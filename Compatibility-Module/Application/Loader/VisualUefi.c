#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Protocol/SimpleTextInEx.h>

//
// This file provides definitions and stubs for VisualUefi build compatibility.
//

// EDK2 Globals expected by entry point and glue logic
CONST UINT8  _gDriverUnloadImageCount = 0;
CONST UINT32 _gUefiDriverRevision     = 0x0210;
CONST UINT32 _gDxeRevision           = 0x0210;
CHAR8        *gEfiCallerBaseName     = "Loader";

// Protocol GUIDs used by Loader.c
EFI_GUID gEfiSimpleTextInputExProtocolGuid = EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID;
EFI_GUID gEfiLegacyBiosProtocolGuid        = { 0xdb9c1a96, 0x011b, 0x4164, { 0x82, 0x4b, 0x43, 0x90, 0x11, 0x3e, 0x6e, 0x7a } };
EFI_GUID gEfiGuardDriverProtocolGuid      = { 0x5b212111, 0x6bd0, 0x4761, { 0x80, 0x87, 0x3f, 0x85, 0x85, 0xba, 0xfa, 0x7b } };

// Event GUIDs used by UefiLib/Library code
EFI_GUID gEfiEventLegacyBootGuid          = { 0x27ab5055, 0x111a, 0x4ae7, { 0x93, 0x85, 0x54, 0x33, 0x03, 0xb9, 0x10, 0xce } };
EFI_GUID gEfiEventReadyToBootGuid         = { 0x7ce21531, 0x18a3, 0x4627, { 0x8e, 0x31, 0x07, 0x4e, 0x84, 0x95, 0x47, 0x44 } };

//
// Stubs for ReportStatusCodeLib
//
VOID EFIAPI ReportStatusCode(IN UINT32 Type, IN UINT32 Value) { (VOID)Type; (VOID)Value; }
BOOLEAN EFIAPI ReportProgressCodeEnabled(VOID) { return FALSE; }
BOOLEAN EFIAPI ReportErrorCodeEnabled(VOID) { return FALSE; }
BOOLEAN EFIAPI ReportDebugCodeEnabled(VOID) { return FALSE; }

//
// Stubs for UefiBootManagerLib
//
VOID* EFIAPI EfiBootManagerGetLoadOptions(OUT UINTN *Count, IN UINT32 Type) { if (Count) *Count = 0; return NULL; }
VOID EFIAPI EfiBootManagerFreeLoadOptions(IN VOID *Options, IN UINTN Count) { (VOID)Options; (VOID)Count; }
EFI_STATUS EFIAPI EfiBootManagerConnectAll(VOID) { return EFI_SUCCESS; }
EFI_STATUS EFIAPI EfiBootManagerConnectDevicePath(IN VOID *DevicePath, IN EFI_HANDLE *Handle) { (VOID)DevicePath; (VOID)Handle; return EFI_UNSUPPORTED; }
VOID* EFIAPI EfiBootManagerGetLoadOptionBuffer(IN VOID *DevicePath, IN EFI_GUID *Guid, OUT UINTN *Size) { (VOID)DevicePath; (VOID)Guid; if (Size) *Size = 0; return NULL; }
BOOLEAN EFIAPI BmIsAutoCreateBootOption(IN VOID *Option) { (VOID)Option; return FALSE; }
VOID EFIAPI BmRepairAllControllers(VOID) { }
VOID EFIAPI BmSetMemoryTypeInformationVariable(IN BOOLEAN MemoryTypeInformationChanged) { (VOID)MemoryTypeInformationChanged; }

//
// Placeholder definitions to make linking against BaseSynchronizationLib possible.
//
UINTN EFIAPI InternalGetSpinLockProperties(VOID) { return (UINTN)-1; }
UINT64 EFIAPI GetPerformanceCounter(VOID) { return (UINT64)-1; }
UINT64 EFIAPI GetPerformanceCounterProperties(OUT UINT64 *StartValue OPTIONAL, OUT UINT64 *EndValue OPTIONAL) { if (StartValue) *StartValue = 0; if (EndValue) *EndValue = 0; return (UINT64)-1; }

//
// Entry point redirection for VisualUefi
//
EFI_STATUS EFIAPI UefiUnload(IN EFI_HANDLE ImageHandle) { (VOID)ImageHandle; return EFI_SUCCESS; }
