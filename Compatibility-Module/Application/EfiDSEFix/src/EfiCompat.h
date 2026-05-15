#pragma once

// We can't include all of <Uefi/UefiBaseType.h> because MSVC will give some very angry errors, especially w.r.t. GUID types which come from the retarded guiddef.h.
// Instead define the minimum subset required to include <Protocol/ScootwareCompat.h>
#define EFIAPI __cdecl

typedef ULONG_PTR UINTN;
typedef UINTN RETURN_STATUS;
typedef RETURN_STATUS EFI_STATUS;
typedef GUID EFI_GUID;
typedef CHAR CHAR8;
typedef WCHAR CHAR16;
typedef struct
{
	UINT16 Year;
	UINT8 Month;
	UINT8 Day;
	UINT8 Hour;
	UINT8 Minute;
	UINT8 Second;
	UINT8 Pad1;
	UINT32 Nanosecond;
	INT16 TimeZone;
	UINT8 Daylight;
	UINT8 Pad2;
} EFI_TIME;

// For EFI variable attributes
//
// Define the minimum EFI variable attributes needed by <Protocol/ScootwareCompat.h>.
// These are normally provided by <Uefi/UefiMultiPhase.h> from the EDK II (VisualUefi).
// We define them inline to avoid the dependency.
//
#define EFI_VARIABLE_NON_VOLATILE       0x00000001
#define EFI_VARIABLE_BOOTSERVICE_ACCESS 0x00000002
#define EFI_VARIABLE_RUNTIME_ACCESS     0x00000004

#define EFI_GLOBAL_VARIABLE             { 0x8BE4DF61, 0x93CA, 0x11d2, { 0xAA, 0x0D, 0x00, 0xE0, 0x98, 0x03, 0x2B, 0x8C } }
