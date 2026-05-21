#pragma once

#include <Uefi.h>
#include <IndustryStandard/SmBios.h>
#include <Protocol/Smbios.h>
#include <Protocol/ScootwareConfig.h>

//
// SMBIOS structure type values we care about.
//
#ifndef SMBIOS_TYPE_SYSTEM_INFORMATION
#define SMBIOS_TYPE_SYSTEM_INFORMATION    1
#endif
#ifndef SMBIOS_TYPE_BASEBOARD_INFORMATION
#define SMBIOS_TYPE_BASEBOARD_INFORMATION 2
#endif
#ifndef SMBIOS_TYPE_PROCESSOR_INFORMATION
#define SMBIOS_TYPE_PROCESSOR_INFORMATION 4
#endif

//
// Walker view of an SMBIOS structure record.
// Pointers index into the live firmware table; no copy is made.
//
typedef union {
    UINT8*             Raw;
    SMBIOS_STRUCTURE*  Hdr;
} SMBIOS_RECORD_POINTER;

//
// Unified handle to either a 2.x or 3.x entry point. The two specs use
// different layouts (and 3.x uses a 64-bit table address) so we keep both
// representations and a tag for which one was found.
//
typedef enum {
    SmbiosEntryPointNone = 0,
    SmbiosEntryPoint2X,
    SmbiosEntryPoint3X
} SMBIOS_ENTRY_POINT_KIND;

typedef struct {
    SMBIOS_ENTRY_POINT_KIND       Kind;
    union {
        SMBIOS_TABLE_ENTRY_POINT*       V2;  // points into firmware config table
        SMBIOS_TABLE_3_0_ENTRY_POINT*   V3;
    } u;
    UINT8*  TableAddress;   // resolved table start (handles 32-/64-bit)
    UINT32  TableLength;    // max table length (best effort; may be 0)
    UINT16  NumStructures;  // 2.x: exact; 3.x: not provided (0)
} SMBIOS_ENTRY;

//
// Locate the SMBIOS entry point in the EFI configuration table.
// Prefers SMBIOS 3.0 ("_SM3_") when present, falling back to 2.x.
// Returns Entry.Kind == SmbiosEntryPointNone if not found.
//
VOID
SmbiosFindEntry(
    OUT SMBIOS_ENTRY* Entry
    );

//
// One-time capture of the live HWID fields (Type 1 UUID + serial, Type 2 serial,
// Type 4 serial) into Cfg. Sets Cfg->HwIdCaptured = 1.
// Skips entirely if Cfg->HwIdCaptured is already non-zero, or if HwIdApply is set
// (so the caller-supplied spoof values are never trampled by capture on the same
// boot they're about to be applied).
//
VOID
SmbiosCaptureCurrent(
    IN OUT SCOOTWARE_EFI_CONFIG* Cfg
    );

//
// Write the spoof fields from Cfg back into the live SMBIOS table. Strings are
// edited in place, capped to the existing slot length (no relocation, no
// pointer fixups). UUID is overwritten in full.
//
VOID
SmbiosApplySpoofs(
    IN CONST SCOOTWARE_EFI_CONFIG* Cfg
    );
