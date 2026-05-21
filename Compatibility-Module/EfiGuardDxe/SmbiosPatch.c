#include "ScootwareCompatDxe.h"
#include "SmbiosPatch.h"

#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Guid/Smbios.h>

extern EFI_GUID gEfiSmbiosTableGuid;
extern EFI_GUID gEfiSmbios3TableGuid;

// SMBIOS_TYPE_END_OF_TABLE comes from <IndustryStandard/SmBios.h> (value 0x7F);
// no local redef needed.

//
// Maximum bytes we will scan past a record header looking for the double-NULL
// that terminates the string area. SMBIOS strings are capped at 64 bytes each
// by the spec; a few KB is a generous safety bound for ~32 strings.
//
#define SMBIOS_STRINGS_AREA_MAX     4096

//
// Maximum number of structures we will walk if NumberOfSmbiosStructures looks
// implausibly large (e.g. SMBIOS 3.x reports 0). Acts as a backstop in case
// the End-Of-Table marker is missing.
//
#define SMBIOS_WALK_MAX_STRUCTURES  4096

VOID
SmbiosFindEntry(
    OUT SMBIOS_ENTRY* Entry
    )
{
    ZeroMem(Entry, sizeof(*Entry));

    if (gST == NULL || gST->ConfigurationTable == NULL)
        return;

    SMBIOS_TABLE_3_0_ENTRY_POINT* V3 = NULL;
    SMBIOS_TABLE_ENTRY_POINT*     V2 = NULL;

    for (UINTN i = 0; i < gST->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE* T = &gST->ConfigurationTable[i];
        if (CompareGuid(&T->VendorGuid, &gEfiSmbios3TableGuid) && V3 == NULL)
            V3 = (SMBIOS_TABLE_3_0_ENTRY_POINT*)T->VendorTable;
        else if (CompareGuid(&T->VendorGuid, &gEfiSmbiosTableGuid) && V2 == NULL)
            V2 = (SMBIOS_TABLE_ENTRY_POINT*)T->VendorTable;
    }

    // Prefer SMBIOS 3.x; both may be present on modern systems and they refer
    // to the same logical table content, but 3.x can address >4GiB.
    if (V3 != NULL && V3->TableAddress != 0) {
        Entry->Kind         = SmbiosEntryPoint3X;
        Entry->u.V3         = V3;
        Entry->TableAddress = (UINT8*)(UINTN)V3->TableAddress;
        Entry->TableLength  = V3->TableMaximumSize;
        Entry->NumStructures= 0;  // not provided in 3.x
        return;
    }

    if (V2 != NULL && V2->TableAddress != 0) {
        Entry->Kind         = SmbiosEntryPoint2X;
        Entry->u.V2         = V2;
        Entry->TableAddress = (UINT8*)(UINTN)V2->TableAddress;
        Entry->TableLength  = (UINT32)V2->TableLength;
        Entry->NumStructures= V2->NumberOfSmbiosStructures;
        return;
    }
}

//
// Compute the full byte length of an SMBIOS record: header + formatted area
// + string set + terminating double-NULL.
//
STATIC
UINT32
SmbiosRecordLength(
    IN UINT8* RecordStart
    )
{
    SMBIOS_STRUCTURE* Hdr = (SMBIOS_STRUCTURE*)RecordStart;
    UINT8* Strings = RecordStart + Hdr->Length;

    for (UINTN i = 0; i < SMBIOS_STRINGS_AREA_MAX; ++i) {
        // End of strings area is double-NULL. If a record has no strings
        // at all, the formatted area is followed by two NULLs (i.e. i==0).
        if (Strings[i] == 0 && Strings[i + 1] == 0) {
            return (UINT32)(Hdr->Length + i + 2);
        }
    }
    return 0;  // malformed
}

//
// Walk the SMBIOS table and return a pointer to the Nth structure of the
// requested type, or NULL if not present.
//
STATIC
UINT8*
SmbiosFindByType(
    IN CONST SMBIOS_ENTRY* Entry,
    IN UINT8 Type,
    IN UINTN Index
    )
{
    if (Entry == NULL || Entry->TableAddress == NULL || Entry->Kind == SmbiosEntryPointNone)
        return NULL;

    UINT8* Cur = Entry->TableAddress;
    UINT8* End = Entry->TableLength != 0
                 ? Entry->TableAddress + Entry->TableLength
                 : (UINT8*)~(UINTN)0;
    UINTN  FoundCount = 0;

    // Iteration cap: prefer NumStructures from a 2.x entry; otherwise bound
    // by SMBIOS_WALK_MAX_STRUCTURES. Either way we also stop on type 127.
    UINTN MaxIter = Entry->NumStructures != 0
                    ? Entry->NumStructures
                    : SMBIOS_WALK_MAX_STRUCTURES;

    for (UINTN i = 0; i < MaxIter && Cur + sizeof(SMBIOS_STRUCTURE) <= End; ++i) {
        SMBIOS_STRUCTURE* Hdr = (SMBIOS_STRUCTURE*)Cur;

        if (Hdr->Length < sizeof(SMBIOS_STRUCTURE))
            return NULL;  // malformed record

        if (Hdr->Type == Type) {
            if (FoundCount == Index)
                return Cur;
            FoundCount++;
        }

        if (Hdr->Type == SMBIOS_TYPE_END_OF_TABLE)
            return NULL;

        UINT32 Len = SmbiosRecordLength(Cur);
        if (Len == 0 || Cur + Len > End)
            return NULL;

        Cur += Len;
    }
    return NULL;
}

//
// Copy SMBIOS string at StringIndex (1-based) into Output. Output is
// always NUL-terminated. If StringIndex == 0 (no string) or not found,
// Output is set to empty.
//
STATIC
VOID
SmbiosReadString(
    IN UINT8* Record,
    IN UINT8 StringIndex,
    OUT CHAR8* Output,
    IN UINTN MaxLength
    )
{
    if (Output == NULL || MaxLength == 0)
        return;
    Output[0] = '\0';

    if (Record == NULL || StringIndex == 0)
        return;

    SMBIOS_STRUCTURE* Hdr = (SMBIOS_STRUCTURE*)Record;
    CHAR8* Strings = (CHAR8*)(Record + Hdr->Length);

    UINT8 Cur = 1;
    while (Cur < StringIndex) {
        if (*Strings == 0) return;  // index out of range
        while (*Strings != 0) Strings++;
        Strings++;
        Cur++;
    }

    if (*Strings == 0) return;

    UINTN i;
    for (i = 0; i < MaxLength - 1 && Strings[i] != 0; i++)
        Output[i] = Strings[i];
    Output[i] = '\0';
}

//
// Edit an SMBIOS string in place at StringIndex (1-based). The slot length is
// fixed (we can't relocate the table), so the new string is truncated to fit
// and padded with spaces if shorter than the original. The original
// NUL-terminator position is preserved.
//
STATIC
VOID
SmbiosEditString(
    IN UINT8* Record,
    IN UINT8 StringIndex,
    IN CONST CHAR8* Buffer
    )
{
    if (Record == NULL || StringIndex == 0 || Buffer == NULL)
        return;

    SMBIOS_STRUCTURE* Hdr = (SMBIOS_STRUCTURE*)Record;
    CHAR8* Strings = (CHAR8*)(Record + Hdr->Length);

    UINT8 Cur = 1;
    while (Cur < StringIndex) {
        if (*Strings == 0) return;
        while (*Strings != 0) Strings++;
        Strings++;
        Cur++;
    }
    if (*Strings == 0) return;

    UINTN ExistingLen = AsciiStrLen(Strings);
    UINTN NewLen      = AsciiStrLen(Buffer);
    UINTN CopyLen     = (NewLen < ExistingLen) ? NewLen : ExistingLen;

    CopyMem(Strings, Buffer, CopyLen);
    if (CopyLen < ExistingLen)
        SetMem(Strings + CopyLen, ExistingLen - CopyLen, ' ');

    Strings[ExistingLen] = '\0';  // preserve original terminator location
}

VOID
SmbiosCaptureCurrent(
    IN OUT SCOOTWARE_EFI_CONFIG* Cfg
    )
{
    if (Cfg == NULL) return;

    // Defensive: never overwrite spoof fields when the caller has staged a
    // spoof for this boot (HwIdApply != 0). Capture is also a one-shot:
    // skip if we've already done it.
    if (Cfg->HwIdCaptured != 0 || Cfg->HwIdApply != 0)
        return;

    SMBIOS_ENTRY Entry;
    SmbiosFindEntry(&Entry);
    if (Entry.Kind == SmbiosEntryPointNone) {
        DEBUG((DEBUG_INFO, "[SMBIOS] capture: no entry point\n"));
        return;
    }

    // Type 1: System Information — UUID (offset 0x08) + Serial string
    UINT8* T1 = SmbiosFindByType(&Entry, SMBIOS_TYPE_SYSTEM_INFORMATION, 0);
    if (T1 != NULL) {
        SMBIOS_STRUCTURE* Hdr = (SMBIOS_STRUCTURE*)T1;
        if (Hdr->Length >= 0x18) {
            CopyMem(Cfg->HwIdSpoofUUID, T1 + 0x08, 16);
            UINT8 SerialIdx = T1[0x07];  // Type1.SerialNumber
            SmbiosReadString(T1, SerialIdx,
                             Cfg->HwIdSpoofSystemSerial,
                             sizeof(Cfg->HwIdSpoofSystemSerial));
        }
    }

    // Type 2: Baseboard — Serial at string index offset 0x07
    UINT8* T2 = SmbiosFindByType(&Entry, SMBIOS_TYPE_BASEBOARD_INFORMATION, 0);
    if (T2 != NULL) {
        SMBIOS_STRUCTURE* Hdr = (SMBIOS_STRUCTURE*)T2;
        if (Hdr->Length >= 0x08) {
            UINT8 SerialIdx = T2[0x07];
            SmbiosReadString(T2, SerialIdx,
                             Cfg->HwIdSpoofBaseboardSerial,
                             sizeof(Cfg->HwIdSpoofBaseboardSerial));
        }
    }

    // Type 4: Processor — Serial at offset 0x20 (string index)
    UINT8* T4 = SmbiosFindByType(&Entry, SMBIOS_TYPE_PROCESSOR_INFORMATION, 0);
    if (T4 != NULL) {
        SMBIOS_STRUCTURE* Hdr = (SMBIOS_STRUCTURE*)T4;
        if (Hdr->Length >= 0x21) {
            UINT8 SerialIdx = T4[0x20];
            SmbiosReadString(T4, SerialIdx,
                             Cfg->HwIdSpoofProcessorSerial,
                             sizeof(Cfg->HwIdSpoofProcessorSerial));
        }
    }

    Cfg->HwIdCaptured = 1;
    DEBUG((DEBUG_INFO, "[SMBIOS] captured baseline HWID into config\n"));
}

VOID
SmbiosApplySpoofs(
    IN CONST SCOOTWARE_EFI_CONFIG* Cfg
    )
{
    if (Cfg == NULL) return;

    SMBIOS_ENTRY Entry;
    SmbiosFindEntry(&Entry);
    if (Entry.Kind == SmbiosEntryPointNone) {
        DEBUG((DEBUG_INFO, "[SMBIOS] apply: no entry point\n"));
        return;
    }

    BOOLEAN WpEnabled, CetEnabled;
    DisableWriteProtect(&WpEnabled, &CetEnabled);

    // Type 1: System Information — overwrite UUID and edit Serial string
    UINT8* T1 = SmbiosFindByType(&Entry, SMBIOS_TYPE_SYSTEM_INFORMATION, 0);
    if (T1 != NULL) {
        SMBIOS_STRUCTURE* Hdr = (SMBIOS_STRUCTURE*)T1;
        if (Hdr->Length >= 0x18) {
            CopyMem(T1 + 0x08, Cfg->HwIdSpoofUUID, 16);
            UINT8 SerialIdx = T1[0x07];
            SmbiosEditString(T1, SerialIdx, Cfg->HwIdSpoofSystemSerial);
        }
    }

    UINT8* T2 = SmbiosFindByType(&Entry, SMBIOS_TYPE_BASEBOARD_INFORMATION, 0);
    if (T2 != NULL) {
        SMBIOS_STRUCTURE* Hdr = (SMBIOS_STRUCTURE*)T2;
        if (Hdr->Length >= 0x08) {
            UINT8 SerialIdx = T2[0x07];
            SmbiosEditString(T2, SerialIdx, Cfg->HwIdSpoofBaseboardSerial);
        }
    }

    UINT8* T4 = SmbiosFindByType(&Entry, SMBIOS_TYPE_PROCESSOR_INFORMATION, 0);
    if (T4 != NULL) {
        SMBIOS_STRUCTURE* Hdr = (SMBIOS_STRUCTURE*)T4;
        if (Hdr->Length >= 0x21) {
            UINT8 SerialIdx = T4[0x20];
            SmbiosEditString(T4, SerialIdx, Cfg->HwIdSpoofProcessorSerial);
        }
    }

    EnableWriteProtect(WpEnabled, CetEnabled);
    DEBUG((DEBUG_INFO, "[SMBIOS] spoofs applied\n"));
}
