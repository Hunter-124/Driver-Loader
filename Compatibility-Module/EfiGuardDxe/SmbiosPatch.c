#include "ScootwareCompatDxe.h"
#include "SmbiosPatch.h"
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Guid/Smbios.h>

// SMBIOS 3.0 Table GUID from MdePkg
extern EFI_GUID gEfiSmbiosTableGuid;
extern EFI_GUID gEfiSmbios3TableGuid;

/**
 * Find SMBIOS Entry Point Structure
 */
SMBIOS_STRUCTURE_TABLE_CUSTOM*
SmbiosFindEntry(
    VOID
)
{
    if (gST == NULL || gST->ConfigurationTable == NULL) {
        return NULL;
    }

    for (UINTN i = 0; i < gST->NumberOfTableEntries; i++) {
        if (CompareGuid(&gST->ConfigurationTable[i].VendorGuid, &gEfiSmbios3TableGuid)) {
            return (SMBIOS_STRUCTURE_TABLE_CUSTOM*)gST->ConfigurationTable[i].VendorTable;
        }
        if (CompareGuid(&gST->ConfigurationTable[i].VendorGuid, &gEfiSmbiosTableGuid)) {
            return (SMBIOS_STRUCTURE_TABLE_CUSTOM*)gST->ConfigurationTable[i].VendorTable;
        }
    }

    return NULL;
}

/**
 * Calculate total length of SMBIOS structure (including string table)
 */
STATIC
UINT16
SmbiosTableLength(
    IN SMBIOS_STRUCTURE_POINTER_CUSTOM Table
)
{
    if (Table.Raw == NULL) return 0;
    
    // Header length + strings
    CHAR8* Pointer = (CHAR8*)(Table.Raw + Table.Hdr->Length);
    
    // Find double-null terminator (end of string table)
    // Add safety limit to prevent infinite loops on corrupt tables
    for (UINTN i = 0; i < 4096; i++) {
        if (Pointer[i] == 0 && Pointer[i + 1] == 0) {
            return (UINT16)((UINTN)&Pointer[i] - (UINTN)Table.Raw + 2);
        }
    }
    
    return 0;
}

/**
 * Find SMBIOS structure by type
 */
STATIC
SMBIOS_STRUCTURE_POINTER_CUSTOM
SmbiosFindTableByType(
    IN SMBIOS_STRUCTURE_TABLE_CUSTOM* Entry,
    IN UINT8 Type,
    IN UINTN Index
)
{
    SMBIOS_STRUCTURE_POINTER_CUSTOM Table;
    Table.Raw = NULL;

    if (Entry == NULL || Entry->StructureTableAddress == 0) {
        return Table;
    }

    Table.Raw = (UINT8*)((UINTN)Entry->StructureTableAddress);
    UINTN FoundCount = 0;

    for (UINTN i = 0; i < Entry->NumberOfStructures; i++) {
        if (Table.Hdr->Type == Type) {
            if (FoundCount == Index) {
                return Table;
            }
            FoundCount++;
        }

        if (Table.Hdr->Type == 127) { // End of Table
            break;
        }

        UINT16 Len = SmbiosTableLength(Table);
        if (Len == 0) break; // Error in table

        Table.Raw += Len;
    }

    Table.Raw = NULL;
    return Table;
}

/**
 * Read SMBIOS string from structure
 */
STATIC
VOID
SmbiosReadString(
    IN SMBIOS_STRUCTURE_POINTER_CUSTOM Table,
    IN UINT8 StringIndex,
    OUT char* Output,
    IN UINTN MaxLength
)
{
    if (Table.Raw == NULL || StringIndex == 0 || Output == NULL || MaxLength == 0) {
        if (Output && MaxLength > 0) Output[0] = 0;
        return;
    }

    CHAR8* Pointer = (CHAR8*)(Table.Raw + Table.Hdr->Length);
    UINT8 CurrentIndex = 1;

    while (CurrentIndex < StringIndex) {
        while (*Pointer != 0) Pointer++;
        Pointer++;
        CurrentIndex++;
        
        if (*Pointer == 0) { // End of string table
            Output[0] = 0;
            return;
        }
    }

    // Copy string
    UINTN i;
    for (i = 0; i < MaxLength - 1 && Pointer[i] != 0; i++) {
        Output[i] = Pointer[i];
    }
    Output[i] = 0;
}

/**
 * Edit string directly in SMBIOS table (in-place)
 */
STATIC
VOID
SmbiosEditString(
    IN SMBIOS_STRUCTURE_POINTER_CUSTOM Table,
    IN UINT8 StringIndex,
    IN CONST char* Buffer
)
{
    if (Table.Raw == NULL || StringIndex == 0 || Buffer == NULL) {
        return;
    }

    CHAR8* Pointer = (CHAR8*)(Table.Raw + Table.Hdr->Length);
    UINT8 CurrentIndex = 1;

    while (CurrentIndex < StringIndex) {
        while (*Pointer != 0) Pointer++;
        Pointer++;
        CurrentIndex++;
        
        if (*Pointer == 0) return;
    }

    UINTN ExistingLen = 0;
    while (Pointer[ExistingLen] != 0 && ExistingLen < 256) ExistingLen++;

    UINTN NewLen = 0;
    while (Buffer[NewLen] != 0 && NewLen < 256) NewLen++;

    // In-place edit: we can only copy as many bytes as there is room.
    // We don't implement table resizing to avoid moving structures and breaking pointers.
    UINTN CopyLen = (NewLen < ExistingLen) ? NewLen : ExistingLen;
    
    CopyMem(Pointer, Buffer, CopyLen);
    
    // Pad with spaces if new string is shorter
    if (NewLen < ExistingLen) {
        SetMem(Pointer + NewLen, ExistingLen - NewLen, ' ');
    }
    
    // Ensure null terminator remains at the original end
    Pointer[ExistingLen] = 0;
}

/**
 * Capture current HWID values into Cfg
 */
VOID
SmbiosCaptureCurrent(
    IN OUT SCOOTWARE_EFI_CONFIG* Cfg
)
{
    if (!gHeadless) Print(L"[SMBIOS] Capturing current HWID...\r\n");

    SMBIOS_STRUCTURE_TABLE_CUSTOM* Entry = SmbiosFindEntry();
    if (Entry == NULL) {
        if (!gHeadless) Print(L"[SMBIOS] Failed to find SMBIOS entry point.\r\n");
        return;
    }

    SMBIOS_STRUCTURE_POINTER_CUSTOM Table;

    // Type 1: System Information (UUID, Serial)
    Table = SmbiosFindTableByType(Entry, 1, 0);
    if (Table.Raw != NULL) {
        CopyMem(Cfg->HwIdSpoofUUID, Table.Type1->UUID, 16);
        SmbiosReadString(Table, Table.Type1->SerialNumber, Cfg->HwIdSpoofSystemSerial, 128);
    }

    // Type 2: Baseboard Information (Serial)
    Table = SmbiosFindTableByType(Entry, 2, 0);
    if (Table.Raw != NULL) {
        SmbiosReadString(Table, Table.Type2->SerialNumber, Cfg->HwIdSpoofBaseboardSerial, 128);
    }

    // Type 4: Processor Information (Serial)
    Table = SmbiosFindTableByType(Entry, 4, 0);
    if (Table.Raw != NULL) {
        SmbiosReadString(Table, Table.Type4->SerialNumber, Cfg->HwIdSpoofProcessorSerial, 128);
    }

    Cfg->HwIdCaptured = 1;
    if (!gHeadless) Print(L"[SMBIOS] HWID capture complete.\r\n");
}

/**
 * Apply spoofed values to SMBIOS tables
 */
VOID
SmbiosApplySpoofs(
    IN CONST SCOOTWARE_EFI_CONFIG* Cfg
)
{
    if (!gHeadless) Print(L"[SMBIOS] Applying HWID spoofs...\r\n");

    SMBIOS_STRUCTURE_TABLE_CUSTOM* Entry = SmbiosFindEntry();
    if (Entry == NULL) {
        if (!gHeadless) Print(L"[SMBIOS] Failed to find SMBIOS entry point for spoofing.\r\n");
        return;
    }

    SMBIOS_STRUCTURE_POINTER_CUSTOM Table;

    // Type 1: System Information
    Table = SmbiosFindTableByType(Entry, 1, 0);
    if (Table.Raw != NULL) {
        // Apply UUID
        CopyMem(Table.Type1->UUID, Cfg->HwIdSpoofUUID, 16);
        // Apply Serial
        SmbiosEditString(Table, Table.Type1->SerialNumber, Cfg->HwIdSpoofSystemSerial);
    }

    // Type 2: Baseboard Information
    Table = SmbiosFindTableByType(Entry, 2, 0);
    if (Table.Raw != NULL) {
        SmbiosEditString(Table, Table.Type2->SerialNumber, Cfg->HwIdSpoofBaseboardSerial);
    }

    // Type 4: Processor Information
    Table = SmbiosFindTableByType(Entry, 4, 0);
    if (Table.Raw != NULL) {
        SmbiosEditString(Table, Table.Type4->SerialNumber, Cfg->HwIdSpoofProcessorSerial);
    }

    if (!gHeadless) Print(L"[SMBIOS] HWID spoofs applied successfully.\r\n");
}
