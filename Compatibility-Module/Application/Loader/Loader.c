#include <Uefi.h>
#include <Pi/PiDxeCis.h>
#include <Guid/GlobalVariable.h>

#ifndef EFI_BOOT_CURRENT_VARIABLE_NAME
#define EFI_BOOT_CURRENT_VARIABLE_NAME L"BootCurrent"
#endif

#ifndef EFI_BOOT_ORDER_VARIABLE_NAME
#define EFI_BOOT_ORDER_VARIABLE_NAME L"BootOrder"
#endif

#include <Protocol/ScootwareCompat.h>
#include <Protocol/ScootwareConfig.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/LegacyBios.h>
#include <Library/PcdLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/ReportStatusCodeLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>


//
// Paths to the driver to try
//
#define EFIGUARD_DRIVER_FILENAME		L"ScootwareCompatDxe.efi"
STATIC CHAR16* mDriverPaths[] = {
	L"\\EFI\\Boot\\" EFIGUARD_DRIVER_FILENAME,
	L"\\EFI\\" EFIGUARD_DRIVER_FILENAME,
	L"\\" EFIGUARD_DRIVER_FILENAME
};

STATIC EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *mTextInputEx = NULL;

VOID
EFIAPI
BmRepairAllControllers(
	IN UINTN ReconnectRepairCount
	);

VOID
EFIAPI
BmSetMemoryTypeInformationVariable(
	IN BOOLEAN Boot
	);

BOOLEAN
EFIAPI
BmIsAutoCreateBootOption(
	IN EFI_BOOT_MANAGER_LOAD_OPTION *BootOption
	);

STATIC
VOID
ResetTextInput(
	VOID
	)
{
	if (mTextInputEx != NULL)
		mTextInputEx->Reset(mTextInputEx, FALSE);
	else
		gST->ConIn->Reset(gST->ConIn, FALSE);
}

STATIC
UINT16
EFIAPI
WaitForKey(
	VOID
	)
{
	EFI_KEY_DATA KeyData = { 0 };
	UINTN Index = 0;
	if (mTextInputEx != NULL)
	{
		gBS->WaitForEvent(1, &mTextInputEx->WaitForKeyEx, &Index);
		mTextInputEx->ReadKeyStrokeEx(mTextInputEx, &KeyData);
	}
	else
	{
		gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
		gST->ConIn->ReadKeyStroke(gST->ConIn, &KeyData.Key);
	}
	return KeyData.Key.ScanCode;
}

STATIC
UINT16
EFIAPI
WaitForKeyWithTimeout(
	IN UINTN Milliseconds
	)
{
	ResetTextInput();
	gBS->Stall(Milliseconds * 1000);

	EFI_KEY_DATA KeyData = { 0 };
	if (mTextInputEx != NULL)
		mTextInputEx->ReadKeyStrokeEx(mTextInputEx, &KeyData);
	else
		gST->ConIn->ReadKeyStroke(gST->ConIn, &KeyData.Key);

	ResetTextInput();
	return KeyData.Key.ScanCode;
}

STATIC
UINT16
EFIAPI
PromptInput(
	IN CONST UINT16* AcceptedChars,
	IN UINTN NumAcceptedChars,
	IN UINT16 DefaultSelection
	)
{
	UINT16 SelectedChar;

	while (TRUE)
	{
		SelectedChar = CHAR_NULL;

		EFI_KEY_DATA KeyData = { 0 };
		UINTN Index = 0;
		if (mTextInputEx != NULL)
		{
			gBS->WaitForEvent(1, &mTextInputEx->WaitForKeyEx, &Index);
			mTextInputEx->ReadKeyStrokeEx(mTextInputEx, &KeyData);
		}
		else
		{
			gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
			gST->ConIn->ReadKeyStroke(gST->ConIn, &KeyData.Key);
		}

		if (KeyData.Key.UnicodeChar == CHAR_LINEFEED || KeyData.Key.UnicodeChar == CHAR_CARRIAGE_RETURN)
		{
			SelectedChar = DefaultSelection;
			break;
		}

		for (UINTN i = 0; i < NumAcceptedChars; ++i)
		{
			if (KeyData.Key.UnicodeChar == AcceptedChars[i])
			{
				SelectedChar = KeyData.Key.UnicodeChar;
				break;
			}
		}

		if (SelectedChar != CHAR_NULL)
			break;
	}

	Print(L"%c\r\n\r\n", SelectedChar);
	return SelectedChar;
}

STATIC
CONST CHAR16*
EFIAPI
StriStr(
	IN CONST CHAR16 *String1,
	IN CONST CHAR16 *String2
	)
{
	if (*String2 == L'\0')
		return String1;

	while (*String1 != L'\0')
	{
		CONST CHAR16* FirstMatch = String1;
		CONST CHAR16* String2Ptr = String2;
		CHAR16 String1Char = CharToUpper(*String1);
		CHAR16 String2Char = CharToUpper(*String2Ptr);

		while (String1Char == String2Char && String1Char != L'\0')
		{
			String1++;
			String2Ptr++;

			String1Char = CharToUpper(*String1);
			String2Char = CharToUpper(*String2Ptr);
		}

		if (String2Char == L'\0')
			return FirstMatch;

		if (String1Char == L'\0')
			return NULL;

		String1 = FirstMatch + 1;
	}
	return NULL;
}

// 
// Try to find a file by browsing each device
// 
STATIC
EFI_STATUS
LocateFile(
	IN CHAR16* ImagePath,
	OUT EFI_DEVICE_PATH** DevicePath
	)
{
	*DevicePath = NULL;

	UINTN NumHandles;
	EFI_HANDLE* Handles;
	EFI_STATUS Status = gBS->LocateHandleBuffer(ByProtocol,
												&gEfiSimpleFileSystemProtocolGuid,
												NULL,
												&NumHandles,
												&Handles);
	if (EFI_ERROR(Status))
		return Status;

	DEBUG((DEBUG_INFO, "[LOADER] Number of UEFI Filesystem Devices: %llu\r\n", NumHandles));

	for (UINTN i = 0; i < NumHandles; i++)
	{
		EFI_FILE_IO_INTERFACE *IoDevice;
		Status = gBS->OpenProtocol(Handles[i],
									&gEfiSimpleFileSystemProtocolGuid,
									(VOID**)&IoDevice,
									gImageHandle,
									NULL,
									EFI_OPEN_PROTOCOL_GET_PROTOCOL);
		if (Status != EFI_SUCCESS)
			continue;

		EFI_FILE_HANDLE VolumeHandle;
		Status = IoDevice->OpenVolume(IoDevice, &VolumeHandle);
		if (EFI_ERROR(Status))
			continue;

		EFI_FILE_HANDLE FileHandle;
		Status = VolumeHandle->Open(VolumeHandle,
									&FileHandle,
									ImagePath,
									EFI_FILE_MODE_READ,
									EFI_FILE_READ_ONLY);
		if (!EFI_ERROR(Status))
		{
			FileHandle->Close(FileHandle);
			VolumeHandle->Close(VolumeHandle);
			*DevicePath = FileDevicePath(Handles[i], ImagePath);
			CHAR16 *PathString = ConvertDevicePathToText(*DevicePath, TRUE, TRUE);
			DEBUG((DEBUG_INFO, "[LOADER] Found file at %S.\r\n", PathString));
			if (PathString != NULL)
				FreePool(PathString);
			break;
		}
		VolumeHandle->Close(VolumeHandle);
	}

	FreePool((VOID*)Handles);

	return Status;
}

//
// Find the optimal available console output mode and set it if it's not already the current mode
//
STATIC
EFI_STATUS
EFIAPI
SetHighestAvailableTextMode(
	VOID
	)
{
	if (gST->ConOut == NULL)
		return EFI_NOT_READY;

	INT32 MaxModeNum = 0;
	UINTN Cols, Rows, MaxWeightedColsXRows = 0;
	EFI_STATUS Status = EFI_SUCCESS;

	for (INT32 ModeNum = 0; ModeNum < gST->ConOut->Mode->MaxMode; ModeNum++)
	{
		Status = gST->ConOut->QueryMode(gST->ConOut, ModeNum, &Cols, &Rows);
		if (EFI_ERROR(Status))
			continue;

		// Accept only modes where the total of (Rows * Columns) >= the previous known best.
		// Use 16:10 as an arbitrary weighting that lies in between the common 4:3 and 16:9 ratios
		CONST UINTN WeightedColsXRows = (16 * Rows) * (10 * Cols);
		if (WeightedColsXRows >= MaxWeightedColsXRows)
		{
			MaxWeightedColsXRows = WeightedColsXRows;
			MaxModeNum = ModeNum;
		}
	}

	if (gST->ConOut->Mode->Mode != MaxModeNum)
	{
		Status = gST->ConOut->SetMode(gST->ConOut, MaxModeNum);
	}

	// Clear screen and enable cursor
	gST->ConOut->ClearScreen(gST->ConOut);
	gST->ConOut->EnableCursor(gST->ConOut, TRUE);

	return Status;
}

// ─────────────────────────────────────────────────────────────────────────────
// Config-file helpers
// ─────────────────────────────────────────────────────────────────────────────

//
// Open the config file from the same volume the running Loader.efi lives on.
// Returns EFI_SUCCESS + populated FileHandle on success; caller must Close().
//
STATIC
EFI_STATUS
OpenConfigFile(
    IN  BOOLEAN          WriteAccess,
    OUT EFI_FILE_HANDLE *OutFileHandle
    )
{
    *OutFileHandle = NULL;

    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage = NULL;
    EFI_STATUS Status = gBS->HandleProtocol(gImageHandle,
                                            &gEfiLoadedImageProtocolGuid,
                                            (VOID **)&LoadedImage);
    if (EFI_ERROR(Status)) {
        Print(L"[CFG] HandleProtocol(LoadedImage) failed: %r\r\n", Status);
        return Status;
    }

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs = NULL;
    Status = gBS->HandleProtocol(LoadedImage->DeviceHandle,
                                 &gEfiSimpleFileSystemProtocolGuid,
                                 (VOID **)&Fs);
    if (EFI_ERROR(Status)) {
        Print(L"[CFG] HandleProtocol(SimpleFileSystem) failed: %r\r\n", Status);
        return Status;
    }

    EFI_FILE_HANDLE Root = NULL;
    Status = Fs->OpenVolume(Fs, &Root);
    if (EFI_ERROR(Status)) {
        Print(L"[CFG] OpenVolume failed: %r\r\n", Status);
        return Status;
    }

    UINT64 Mode = EFI_FILE_MODE_READ |
                  (WriteAccess ? (EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE) : 0);

    EFI_FILE_HANDLE File = NULL;
    Status = Root->Open(Root, &File, SCOOTWARE_CFG_PATH, Mode, 0);
    Root->Close(Root);

    if (EFI_ERROR(Status)) {
        // EFI_NOT_FOUND is expected on first run before Windows loader wrote the file
        if (Status != EFI_NOT_FOUND)
            Print(L"[CFG] Open(%S) failed: %r\r\n", SCOOTWARE_CFG_PATH, Status);
        return Status;
    }

    *OutFileHandle = File;
    return EFI_SUCCESS;
}

//
// Read and validate the config file.
// Returns EFI_SUCCESS + populated Cfg on success.
// Returns EFI_NOT_FOUND if the file does not exist (silent).
// Returns EFI_COMPROMISED_DATA if the file exists but is invalid (logged).
//
STATIC
EFI_STATUS
TryReadConfig(
    OUT SCOOTWARE_EFI_CONFIG *Cfg
    )
{
    EFI_FILE_HANDLE File = NULL;
    EFI_STATUS Status = OpenConfigFile(FALSE, &File);
    if (EFI_ERROR(Status))
        return Status;   // EFI_NOT_FOUND → caller uses defaults, no noise

    UINTN ReadSize = SCOOTWARE_CFG_SIZE;
    Status = File->Read(File, &ReadSize, Cfg);
    File->Close(File);

    if (EFI_ERROR(Status)) {
        Print(L"[CFG] Read failed: %r — using defaults\r\n", Status);
        return Status;
    }

    if (ReadSize != SCOOTWARE_CFG_SIZE) {
        Print(L"[CFG] File size mismatch (%llu != %llu) — using defaults\r\n",
              (UINT64)ReadSize, (UINT64)SCOOTWARE_CFG_SIZE);
        return EFI_COMPROMISED_DATA;
    }

    if (!ScootwConfigIsValid(Cfg)) {
        Print(L"[CFG] Validation failed (magic/version/checksum) — using defaults\r\n");
        return EFI_COMPROMISED_DATA;
    }

    return EFI_SUCCESS;
}

//
// Rewrite the config file with Cfg (checksum is recomputed here).
//
STATIC
EFI_STATUS
WriteConfig(
    IN SCOOTWARE_EFI_CONFIG *Cfg
    )
{
    // Recompute checksum before writing
    Cfg->Checksum = 0;
    Cfg->Checksum = ScootwConfigChecksum(Cfg);

    EFI_FILE_HANDLE File = NULL;
    EFI_STATUS Status = OpenConfigFile(TRUE, &File);
    if (EFI_ERROR(Status)) {
        Print(L"[CFG] Cannot open config for write: %r\r\n", Status);
        return Status;
    }

    // Seek to start in case the file already existed
    Status = File->SetPosition(File, 0);
    if (EFI_ERROR(Status)) {
        Print(L"[CFG] SetPosition failed: %r\r\n", Status);
        File->Close(File);
        return Status;
    }

    UINTN WriteSize = SCOOTWARE_CFG_SIZE;
    Status = File->Write(File, &WriteSize, Cfg);
    File->Flush(File);
    File->Close(File);

    if (EFI_ERROR(Status)) {
        Print(L"[CFG] Write failed: %r\r\n", Status);
        return Status;
    }

    if (WriteSize != SCOOTWARE_CFG_SIZE) {
        Print(L"[CFG] Short write (%llu != %llu)\r\n",
              (UINT64)WriteSize, (UINT64)SCOOTWARE_CFG_SIZE);
        return EFI_DEVICE_ERROR;
    }

    return EFI_SUCCESS;
}

// ─────────────────────────────────────────────────────────────────────────────
// Boot-order removal
// ─────────────────────────────────────────────────────────────────────────────

//
// Remove our own entry (BootCurrent) from the BootOrder UEFI NVRAM variable
// so we do not execute automatically on subsequent boots.
//
// Non-fatal: on any error we print a diagnostic and return without aborting.
//
STATIC
VOID
RemoveSelfFromBootOrder(
    VOID
    )
{
    // Read BootCurrent to find our option number
    UINT16 BootCurrent = 0xFFFF;
    UINTN  CurSize     = sizeof(BootCurrent);
    UINT32 Attributes  = 0;

    EFI_STATUS Status = gRT->GetVariable(EFI_BOOT_CURRENT_VARIABLE_NAME,
                                         &gEfiGlobalVariableGuid,
                                         &Attributes,
                                         &CurSize,
                                         &BootCurrent);
    if (EFI_ERROR(Status)) {
        Print(L"[CFG] GetVariable(BootCurrent) failed: %r — skipping boot-order removal\r\n",
              Status);
        return;
    }

    // Read the current BootOrder
    UINTN   OrderSize = 0;
    UINT32  OrderAttr = 0;
    Status = gRT->GetVariable(EFI_BOOT_ORDER_VARIABLE_NAME,
                              &gEfiGlobalVariableGuid,
                              &OrderAttr,
                              &OrderSize,
                              NULL);
    if (Status != EFI_BUFFER_TOO_SMALL) {
        Print(L"[CFG] GetVariable(BootOrder) size probe failed: %r — skipping boot-order removal\r\n",
              Status);
        return;
    }

    UINT16 *BootOrder = AllocatePool(OrderSize);
    if (BootOrder == NULL) {
        Print(L"[CFG] AllocatePool(%llu) for BootOrder failed\r\n", (UINT64)OrderSize);
        return;
    }

    Status = gRT->GetVariable(EFI_BOOT_ORDER_VARIABLE_NAME,
                              &gEfiGlobalVariableGuid,
                              &OrderAttr,
                              &OrderSize,
                              BootOrder);
    if (EFI_ERROR(Status)) {
        Print(L"[CFG] GetVariable(BootOrder) failed: %r — skipping boot-order removal\r\n",
              Status);
        FreePool(BootOrder);
        return;
    }

    // Remove all occurrences of BootCurrent from the array
    UINTN Count    = OrderSize / sizeof(UINT16);
    UINTN NewCount = 0;
    BOOLEAN Found  = FALSE;
    UINTN i;
    for (i = 0; i < Count; ++i) {
        if (BootOrder[i] == BootCurrent) {
            Found = TRUE;
        } else {
            BootOrder[NewCount++] = BootOrder[i];
        }
    }

    if (!Found) {
        // Already absent from BootOrder — still try to delete the Boot#### variable
        // in case a previous partial cleanup left a dangling entry.
        FreePool(BootOrder);
    } else {
        UINTN NewSize = NewCount * sizeof(UINT16);
        if (NewSize == 0) {
            // Deleting the variable entirely would be catastrophic; leave a warning
            Print(L"[CFG] WARNING: removing ourselves would empty BootOrder — skipping\r\n");
            FreePool(BootOrder);
            return;
        }

        Status = gRT->SetVariable(EFI_BOOT_ORDER_VARIABLE_NAME,
                                   &gEfiGlobalVariableGuid,
                                   OrderAttr,
                                   NewSize,
                                   BootOrder);
        FreePool(BootOrder);

        if (EFI_ERROR(Status)) {
            Print(L"[CFG] SetVariable(BootOrder) failed: %r — entry may still appear in firmware UI\r\n",
                  Status);
            // Don't return; still attempt to delete the Boot#### variable below
        } else {
            Print(L"[CFG] Removed boot entry %04X from BootOrder\r\n", BootCurrent);
        }
    }

    //
    // Delete the Boot#### NVRAM variable itself so the firmware stops probing
    // the (possibly missing) file on every subsequent boot. This is the primary
    // fix for the "stutter even after uninstall" symptom: an orphaned Boot####
    // entry causes many firmware implementations to time out trying to access
    // a file that no longer exists before falling through to the next boot entry.
    //
    CHAR16 BootVarName[9]; // "Boot####" + NUL
    UnicodeSPrint(BootVarName, sizeof(BootVarName), L"Boot%04X", BootCurrent);
    Status = gRT->SetVariable(BootVarName,
                              &gEfiGlobalVariableGuid,
                              0,    // Attributes = 0 → delete
                              0,    // DataSize   = 0 → delete
                              NULL);
    if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) {
        Print(L"[CFG] Failed to delete %s: %r\r\n", BootVarName, Status);
    } else if (Status == EFI_SUCCESS) {
        Print(L"[CFG] Deleted NVRAM variable %s\r\n", BootVarName);
    }
}

// ─────────────────────────────────────────────────────────────────────────────

STATIC
EFI_STATUS
EFIAPI
StartEfiGuard(
	VOID
	)
{
	EFIGUARD_DRIVER_PROTOCOL* EfiGuardDriverProtocol;
	EFI_DEVICE_PATH *DriverDevicePath = NULL;

	// 
	// Check if the driver is loaded 
	// 
	EFI_STATUS Status = gBS->LocateProtocol(&gEfiGuardDriverProtocolGuid,
											NULL,
											(VOID**)&EfiGuardDriverProtocol);
	ASSERT((!EFI_ERROR(Status) || Status == EFI_NOT_FOUND));
	if (Status == EFI_NOT_FOUND)
	{
		Print(L"[LOADER] Locating and loading driver file %S...\r\n", EFIGUARD_DRIVER_FILENAME);
		for (UINT32 i = 0; i < ARRAY_SIZE(mDriverPaths); ++i)
		{
			Status = LocateFile(mDriverPaths[i], &DriverDevicePath);
			if (!EFI_ERROR(Status))
				break;
		}
		if (EFI_ERROR(Status))
		{
			Print(L"[LOADER] Failed to find driver file %S.\r\n", EFIGUARD_DRIVER_FILENAME);
			goto Exit;
		}

		EFI_HANDLE DriverHandle = NULL;
		Status = gBS->LoadImage(FALSE, // Request is not from boot manager
								gImageHandle,
								DriverDevicePath,
								NULL,
								0,
								&DriverHandle);
		if (EFI_ERROR(Status))
		{
			Print(L"[LOADER] LoadImage failed: %llx (%r).\r\n", Status, Status);
			goto Exit;
		}

		Status = gBS->StartImage(DriverHandle, NULL, NULL);
		if (EFI_ERROR(Status))
		{
			Print(L"[LOADER] StartImage failed: %llx (%r).\r\n", Status, Status);
			goto Exit;
		}
	}
	else
	{
		ASSERT_EFI_ERROR(Status);
		Print(L"[LOADER] The driver is already loaded.\r\n");
	}

	Status = gBS->LocateProtocol(&gEfiGuardDriverProtocolGuid,
								NULL,
								(VOID**)&EfiGuardDriverProtocol);
	if (EFI_ERROR(Status))
	{
		// Non-fatal: the driver's LoadImage hook is already active (it was installed
		// before StartImage returned), so the kernel will still be patched at boot.
		// We just cannot push scootware.cfg settings to it; it will run with its
		// compiled-in defaults (DSE_DISABLE_SETVARIABLE_HOOK, no wait-for-key).
		Print(L"[LOADER] Warning: LocateProtocol failed: %llx (%r) — driver active with defaults.\r\n", Status, Status);
		EfiGuardDriverProtocol = NULL;
		Status = EFI_SUCCESS;
	}

	//
	// Build configuration from the persistent config file.
	// If the file is absent or invalid we silently fall back to safe defaults.
	// The legacy interactive HOME-key path is intentionally removed; all
	// configuration is now driven by the Windows-side loader writing scootware.cfg.
	//
	{
		EFIGUARD_CONFIGURATION_DATA ConfigData;
		SCOOTWARE_EFI_CONFIG        FileCfg;
		BOOLEAN                     HaveFile = FALSE;

		EFI_STATUS CfgStatus = TryReadConfig(&FileCfg);
		if (CfgStatus == EFI_SUCCESS) {
			HaveFile = TRUE;
			Print(L"[CFG] Config loaded: DseMethod=%u WaitKey=%u Flags=0x%X Build=%u\r\n",
			      FileCfg.DseBypassMethod, (UINT32)FileCfg.WaitForKeyPress,
			      FileCfg.Flags, FileCfg.OsBuildNumber);
		} else {
			// EFI_NOT_FOUND is expected on first run; anything else is anomalous
			if (CfgStatus != EFI_NOT_FOUND)
				Print(L"[CFG] Config read error %r — using defaults\r\n", CfgStatus);
			else
				Print(L"[CFG] No config file found — using defaults\r\n");
		}

		// Push config to driver only if we could locate its protocol
		if (EfiGuardDriverProtocol != NULL)
		{
			// Populate ConfigData from file or defaults
			if (HaveFile) {
				switch (FileCfg.DseBypassMethod) {
				case 0:  ConfigData.DseBypassMethod = DSE_DISABLE_NONE;            break;
				case 1:  ConfigData.DseBypassMethod = DSE_DISABLE_AT_BOOT;         break;
				case 2:  ConfigData.DseBypassMethod = DSE_DISABLE_SETVARIABLE_HOOK; break;
				case 3:  // Auto: pick based on the OS build number written by the Windows loader
				default:
					if (FileCfg.OsBuildNumber == 0 || FileCfg.OsBuildNumber >= 17134) {
						// Win10 1803+ / Win11 / unknown → SetVariable hook is well-tested
						ConfigData.DseBypassMethod = DSE_DISABLE_SETVARIABLE_HOOK;
					} else {
						// Win8/8.1 / early Win10 → AtBoot patch is safer
						ConfigData.DseBypassMethod = DSE_DISABLE_AT_BOOT;
					}
					break;
				}
				ConfigData.WaitForKeyPress = (BOOLEAN)(FileCfg.WaitForKeyPress != 0);
			} else {
				ConfigData.DseBypassMethod = DSE_DISABLE_SETVARIABLE_HOOK;
				ConfigData.WaitForKeyPress = FALSE;
			}

			EFI_STATUS CfgPushStatus = EfiGuardDriverProtocol->Configure(&ConfigData);
			if (EFI_ERROR(CfgPushStatus)) {
				Print(L"[LOADER] Driver Configure() returned error %llx (%r).\r\n", CfgPushStatus, CfgPushStatus);
				// Non-fatal: driver will use its compiled-in defaults
			}
		}

		//
		// First-run housekeeping: clear the FIRST_RUN flag in the config file.
		//
		if (HaveFile && (FileCfg.Flags & SCOOTWARE_CFG_FLAG_FIRST_RUN)) {
			Print(L"[CFG] First-run detected — clearing flag\r\n");
			FileCfg.Flags &= ~SCOOTWARE_CFG_FLAG_FIRST_RUN;
			EFI_STATUS WriteStatus = WriteConfig(&FileCfg);
			if (EFI_ERROR(WriteStatus))
				Print(L"[CFG] WARNING: could not clear FIRST_RUN flag: %r\r\n", WriteStatus);
		}

		//
		// Always remove ourselves from BootOrder and delete our Boot#### NVRAM
		// variable on every run. This ensures no orphaned firmware entries remain
		// after uninstall, which is the primary cause of post-uninstall boot stutter.
		//
		RemoveSelfFromBootOrder();
	}

Exit:
	if (DriverDevicePath != NULL)
		FreePool(DriverDevicePath);

	return Status;
}

//
// Attempt to boot each Windows boot option in the BootOptions array.
// This function is a combined and simplified version of BootBootOptions (BdsDxe) and EfiBootManagerBoot (UefiBootManagerLib),
// except for the fact that we are of course not in the BDS phase and also not a driver or the platform boot manager.
// The Windows boot manager doesn't have to know about all this, that would only confuse it
//
STATIC
BOOLEAN
TryBootOptionsInOrder(
	IN EFI_BOOT_MANAGER_LOAD_OPTION *BootOptions,
	IN UINTN BootOptionCount,
	IN UINT16 CurrentBootOptionIndex,
	IN BOOLEAN OnlyBootWindows
	)
{
	//
	// Iterate over the boot options 'in BootOrder order'
	//
	EFI_DEVICE_PATH_PROTOCOL* FullPath;
	for (UINTN Index = 0; Index < BootOptionCount; ++Index)
	{
		//
		// This is us
		//
		if (BootOptions[Index].OptionNumber == CurrentBootOptionIndex)
			continue;

		//
		// No LOAD_OPTION_ACTIVE, no load
		//
		if ((BootOptions[Index].Attributes & LOAD_OPTION_ACTIVE) == 0)
			continue;

		//
		// Ignore LOAD_OPTION_CATEGORY_APP entries
		//
		if ((BootOptions[Index].Attributes & LOAD_OPTION_CATEGORY) != LOAD_OPTION_CATEGORY_BOOT)
			continue;

		//
		// Ignore legacy (BBS) entries, unless non-Windows entries are allowed (second boot attempt)
		//
		const BOOLEAN IsLegacy = DevicePathType(BootOptions[Index].FilePath) == BBS_DEVICE_PATH &&
			DevicePathSubType(BootOptions[Index].FilePath) == BBS_BBS_DP;
		if (OnlyBootWindows && IsLegacy)
			continue;

		//
		// Filter out non-Windows boot entries.
		// Check the description first as "Windows Boot Manager" entries are obviously going to boot Windows.
		// However the inverse is not true, i.e. not all entries that boot Windows will have this description.
		//
		BOOLEAN MaybeWindows = FALSE;
		if (BootOptions[Index].Description != NULL &&
			StrStr(BootOptions[Index].Description, L"Windows Boot Manager") != NULL)
		{
			MaybeWindows = TRUE;
		}

		// We need the full path to LoadImage the file with BootPolicy = TRUE.
		UINTN FileSize;
		VOID* FileBuffer = EfiBootManagerGetLoadOptionBuffer(BootOptions[Index].FilePath, &FullPath, &FileSize);
		if (FileBuffer != NULL)
			FreePool(FileBuffer);

		// EDK2's EfiBootManagerGetLoadOptionBuffer will sometimes give a NULL "full path"
		// from an originally non-NULL file path. If so, swap it back (and don't free it).
		if (FullPath == NULL)
			FullPath = BootOptions[Index].FilePath;

		// Get the text representation of the device path
		CHAR16* ConvertedPath = ConvertDevicePathToText(FullPath, FALSE, FALSE);

		// If this is not a named "Windows Boot Manager" entry, apply some heuristics based on the device path,
		// which must end in "bootmgfw.efi" or "bootx64.efi". In the latter case we may get false positives,
		// but for some types of boots the filename will always be bootx64.efi, so this can't be avoided.
		if (!MaybeWindows &&
			ConvertedPath != NULL &&
			(StriStr(ConvertedPath, L"bootmgfw.efi") != NULL || StriStr(ConvertedPath, L"bootx64.efi") != NULL))
		{
			MaybeWindows = TRUE;
		}

		if (OnlyBootWindows && !MaybeWindows)
		{
			if (FullPath != BootOptions[Index].FilePath)
				FreePool(FullPath);
			if (ConvertedPath != NULL)
				FreePool(ConvertedPath);
			
			// Not Windows; skip this entry
			continue;
		}

		// Print what we're booting
		if (ConvertedPath != NULL)
		{
			Print(L"Booting \"%S\"...\r\n    -> %S = %S\r\n",
				(BootOptions[Index].Description != NULL ? BootOptions[Index].Description : L"<null description>"),
				IsLegacy ? L"Legacy path" : L"Path", ConvertedPath);
			FreePool(ConvertedPath);
		}

		//
		// Boot this image.
		//
		// DO NOT: call EfiBootManagerBoot(BootOption) to 'simplify' this process.
		// The driver will not work in this case due to EfiBootManagerBoot calling BmSetMemoryTypeInformationVariable(),
		// which performs a warm reset of the system if, for example, the category of the current boot option changed
		// from 'app' to 'boot'. Which is precisely what we are doing...
		//
		// Change the BootCurrent variable to the option number for our boot selection
		UINT16 OptionNumber = (UINT16)BootOptions[Index].OptionNumber;
		EFI_STATUS Status = gRT->SetVariable(EFI_BOOT_CURRENT_VARIABLE_NAME,
											&gEfiGlobalVariableGuid,
											EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
											sizeof(UINT16),
											&OptionNumber);
		ASSERT_EFI_ERROR(Status);

		// Signal the EVT_SIGNAL_READY_TO_BOOT event
		EfiSignalEventReadyToBoot();
		REPORT_STATUS_CODE(EFI_PROGRESS_CODE, (EFI_SOFTWARE_DXE_BS_DRIVER | EFI_SW_DXE_BS_PC_READY_TO_BOOT_EVENT));

		// Repair system through DriverHealth protocol
		BmRepairAllControllers(0);

		// Save the memory map in the MemoryTypeInformation variable for resuming from ACPI S4 (hibernate)
		BmSetMemoryTypeInformationVariable((BootOptions[Index].Attributes & LOAD_OPTION_CATEGORY) == LOAD_OPTION_CATEGORY_BOOT);

		// Handle BBS entries
		if (IsLegacy)
		{
			Print(L"\r\nNOTE: ScootwareCompat does not support legacy (non-UEFI) Windows installations.\r\n"
				L"The legacy OS will be booted, but ScootwareCompat will not work.\r\nPress any key to acknowledge...\r\n");
			WaitForKey();

			EFI_LEGACY_BIOS_PROTOCOL *LegacyBios;
			Status = gBS->LocateProtocol(&gEfiLegacyBiosProtocolGuid,
										NULL,
										(VOID**)&LegacyBios);
			ASSERT_EFI_ERROR(Status);

			BootOptions[Index].Status = LegacyBios->LegacyBoot(LegacyBios,
															(BBS_BBS_DEVICE_PATH*)BootOptions[Index].FilePath,
															BootOptions[Index].OptionalDataSize,
															BootOptions[Index].OptionalData);
			return !EFI_ERROR(BootOptions[Index].Status);
		}

		// Ensure the image path is connected end-to-end by Dispatch()ing any required drivers through DXE services
		EfiBootManagerConnectDevicePath(BootOptions[Index].FilePath, NULL);

		// Instead of creating a ramdisk and reading the file into it (¿que?), just pass the path we saved earlier.
		// This is the point where the driver kicks in via its LoadImage hook.
		REPORT_STATUS_CODE(EFI_PROGRESS_CODE, PcdGet32(PcdProgressCodeOsLoaderLoad));
		EFI_HANDLE ImageHandle = NULL;
		Status = gBS->LoadImage(TRUE,
								gImageHandle,
								FullPath,
								NULL,
								0,
								&ImageHandle);

		if (FullPath != BootOptions[Index].FilePath)
			FreePool(FullPath);

		if (EFI_ERROR(Status))
		{
			// Unload if execution could not be deferred to avoid a resource leak
			if (Status == EFI_SECURITY_VIOLATION)
				gBS->UnloadImage(ImageHandle);

			Print(L"LoadImage error %llx (%r)\r\n", Status, Status);
			BootOptions[Index].Status = Status;
			continue;
		}

		// Get loaded image info
		EFI_LOADED_IMAGE_PROTOCOL* ImageInfo;
		Status = gBS->OpenProtocol(ImageHandle,
									&gEfiLoadedImageProtocolGuid,
									(VOID**)&ImageInfo,
									gImageHandle,
									NULL,
									EFI_OPEN_PROTOCOL_GET_PROTOCOL);
		ASSERT_EFI_ERROR(Status);

		// Set image load options from the boot option
		if (!BmIsAutoCreateBootOption(&BootOptions[Index]))
		{
			ImageInfo->LoadOptionsSize = BootOptions[Index].OptionalDataSize;
			ImageInfo->LoadOptions = BootOptions[Index].OptionalData;
		}

		// "Clean to NULL because the image is loaded directly from the firmware's boot manager." (EDK2) Good call, I agree
		ImageInfo->ParentHandle = NULL;

		// Enable the Watchdog Timer for 5 minutes before calling the image
		gBS->SetWatchdogTimer((UINTN)(5 * 60), 0x0000, 0x00, NULL);

		// Start the image and set the return code in the boot option status
		REPORT_STATUS_CODE(EFI_PROGRESS_CODE, PcdGet32(PcdProgressCodeOsLoaderStart));
		Status = gBS->StartImage(ImageHandle,
								&BootOptions[Index].ExitDataSize,
								&BootOptions[Index].ExitData);
		BootOptions[Index].Status = Status;
		if (EFI_ERROR(Status))
		{
			Print(L"StartImage error %llx (%r)\r\n", Status, Status);
			continue;
		}

		//
		// Success. Code below is never executed
		//

		// Clear the watchdog timer after the image returns
		gBS->SetWatchdogTimer(0x0000, 0x0000, 0x0000, NULL);

		// Clear the BootCurrent variable
		gRT->SetVariable(EFI_BOOT_CURRENT_VARIABLE_NAME,
						&gEfiGlobalVariableGuid,
						0,
						0,
						NULL);

		if (BootOptions[Index].Status == EFI_SUCCESS)
			return TRUE;
	}

	// All boot attempts failed, or no suitable entries were found
	return FALSE;
}

EFI_STATUS
EFIAPI
UefiMain(
	IN EFI_HANDLE ImageHandle,
	IN EFI_SYSTEM_TABLE* SystemTable
	)
{
	//
	// Connect all drivers to all controllers
	//
	EfiBootManagerConnectAll();

	//
	// Set the highest available console mode and clear the screen
	//
	SetHighestAvailableTextMode();

	//
	// Turn off the watchdog timer
	//
	gBS->SetWatchdogTimer(0, 0, 0, NULL);

	//
	// Query the console input handle for the Simple Text Input Ex protocol
	//
	gBS->HandleProtocol(gST->ConsoleInHandle, &gEfiSimpleTextInputExProtocolGuid, (VOID **)&mTextInputEx);

	//
	// Locate, load, start and configure the driver using the persistent config file.
	// Configuration is fully file-driven; no interactive prompt is needed.
	//
	CONST EFI_STATUS DriverStatus = StartEfiGuard();
	if (EFI_ERROR(DriverStatus))
	{
		Print(L"\r\nERROR: driver load failed with status %llx (%r).\r\n"
			L"Press any key to continue, or press ESC to return to the firmware or shell.\r\n",
			DriverStatus, DriverStatus);
		if (WaitForKey() == SCAN_ESC)
		{
			gBS->Exit(gImageHandle, DriverStatus, 0, NULL);
			return DriverStatus;
		}
	}

	//
	// Start the "boot through" procedure to boot Windows.
	//
	// First obtain our own boot option number, since we don't want to boot ourselves again
	UINT16 CurrentBootOptionIndex;
	UINT32 Attributes = EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS;
	UINTN Size = sizeof(CurrentBootOptionIndex);
	CONST EFI_STATUS Status = gRT->GetVariable(EFI_BOOT_CURRENT_VARIABLE_NAME,
												&gEfiGlobalVariableGuid,
												&Attributes,
												&Size,
												&CurrentBootOptionIndex);
	if (EFI_ERROR(Status))
	{
		CurrentBootOptionIndex = 0xFFFF;
		Print(L"WARNING: failed to query the current boot option index variable.\r\n"
			L"This could lead to the current device being booted recursively.\r\n"
			L"If you booted from a removable device, it is recommended that you remove it now.\r\n"
			L"\r\nPress any key to continue...\r\n");
		WaitForKey();
	}

	// Query all boot options, and try each following the order set in the "BootOrder" variable, except
	// (1) Do not boot ourselves again, and
	// (2) The description or filename must indicate the boot option is some form of Windows.
	// First try to boot the configured exact path for bootmgfw.efi
	BOOLEAN BootSuccess = FALSE;
	SCOOTWARE_EFI_CONFIG FileCfg;
	if (TryReadConfig(&FileCfg) == EFI_SUCCESS && FileCfg.BootmgfwPath[0] != L'\0')
	{
		Print(L"[LOADER] Trying configured BootmgfwPath: %s\r\n", (CHAR16*)FileCfg.BootmgfwPath);
		EFI_DEVICE_PATH *CfgPath = NULL;
		EFI_STATUS CfgStatus = LocateFile((CHAR16*)FileCfg.BootmgfwPath, &CfgPath);
		if (!EFI_ERROR(CfgStatus) && CfgPath != NULL)
		{
			EFI_HANDLE CfgHandle = NULL;
			CfgStatus = gBS->LoadImage(TRUE, gImageHandle, CfgPath, NULL, 0, &CfgHandle);
			if (!EFI_ERROR(CfgStatus))
			{
				EfiSignalEventReadyToBoot();
				gBS->SetWatchdogTimer((UINTN)(5 * 60), 0, 0, NULL);
				UINTN ExitDataSize = 0;
				CHAR16 *ExitData = NULL;
				CfgStatus = gBS->StartImage(CfgHandle, &ExitDataSize, &ExitData);
				if (!EFI_ERROR(CfgStatus))
					BootSuccess = TRUE;
				else
					Print(L"[LOADER] StartImage failed: %r\r\n", CfgStatus);
			}
			else
			{
				if (CfgStatus == EFI_SECURITY_VIOLATION)
					gBS->UnloadImage(CfgHandle);
				Print(L"[LOADER] LoadImage failed: %r\r\n", CfgStatus);
			}
			FreePool(CfgPath);
		}
	}

	UINTN BootOptionCount = 0;
	EFI_BOOT_MANAGER_LOAD_OPTION* BootOptions = NULL;

	if (!BootSuccess)
	{
		BootOptions = EfiBootManagerGetLoadOptions(&BootOptionCount, LoadOptionTypeBoot);
		BootSuccess = TryBootOptionsInOrder(BootOptions,
												BootOptionCount,
												CurrentBootOptionIndex,
												TRUE);
	}

	if (!BootSuccess && BootOptions != NULL)
	{
		// We did not find any Windows boot entry; retry without the "must be Windows" restriction.
		BootSuccess = TryBootOptionsInOrder(BootOptions,
											BootOptionCount,
											CurrentBootOptionIndex,
											FALSE);
	}
	
	if (BootOptions != NULL)
	{
		EfiBootManagerFreeLoadOptions(BootOptions, BootOptionCount);
	}

	if (!BootSuccess)
	{
		// BootOrder may be empty (e.g. after first-run cleanup removed the only entry).
		// Fall back to well-known Windows/generic EFI paths directly.
		STATIC CONST CHAR16* FallbackPaths[] = {
			L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi",
			L"\\EFI\\Boot\\bootx64.efi",
		};
		for (UINTN fi = 0; fi < ARRAY_SIZE(FallbackPaths) && !BootSuccess; ++fi)
		{
			EFI_DEVICE_PATH *FbPath = NULL;
			EFI_STATUS FbStatus = LocateFile((CHAR16*)FallbackPaths[fi], &FbPath);
			if (EFI_ERROR(FbStatus) || FbPath == NULL)
				continue;

			Print(L"[LOADER] Trying fallback: %s\r\n", FallbackPaths[fi]);
			EFI_HANDLE FbHandle = NULL;
			FbStatus = gBS->LoadImage(TRUE, gImageHandle, FbPath, NULL, 0, &FbHandle);
			FreePool(FbPath);
			if (EFI_ERROR(FbStatus))
			{
				if (FbStatus == EFI_SECURITY_VIOLATION)
					gBS->UnloadImage(FbHandle);
				Print(L"[LOADER] LoadImage failed: %r\r\n", FbStatus);
				continue;
			}

			EfiSignalEventReadyToBoot();
			gBS->SetWatchdogTimer((UINTN)(5 * 60), 0, 0, NULL);
			UINTN ExitDataSize = 0;
			CHAR16 *ExitData = NULL;
			FbStatus = gBS->StartImage(FbHandle, &ExitDataSize, &ExitData);
			if (!EFI_ERROR(FbStatus))
				BootSuccess = TRUE;
			else
				Print(L"[LOADER] StartImage failed: %r\r\n", FbStatus);
		}
	}

	if (BootSuccess)
		return EFI_SUCCESS;

	// We should never reach this unless something is seriously wrong (no boot device / partition table corrupted / catastrophic boot manager failure...)
	Print(L"Failed to boot anything. This is super bad!\r\n"
		L"Press any key to return to the firmware or shell,\r\nwhich will surely fix this and not make things worse.\r\n");
	WaitForKey();

	gBS->Exit(gImageHandle, EFI_SUCCESS, 0, NULL);

	return EFI_SUCCESS;
}
