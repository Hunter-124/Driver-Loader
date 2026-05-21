#include "ScootwareCompatDxe.h"
#include "SmbiosPatch.h"

#include <Guid/EventGroup.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/SynchronizationLib.h>
#include <Protocol/Shell.h>

//
// EFI Driver Version Protocol
//
EFI_DRIVER_SUPPORTED_EFI_VERSION_PROTOCOL gEfiGuardSupportedEfiVersion = {
    sizeof(EFI_DRIVER_SUPPORTED_EFI_VERSION_PROTOCOL),
    EFI_2_10_SYSTEM_TABLE_REVISION};

//
// Driver unload
//
EFI_STATUS
EFIAPI
EfiGuardUnload(IN EFI_HANDLE ImageHandle);

//
// ScootwareCompat driver protocol
//
EFI_STATUS
EFIAPI
DriverConfigure(IN CONST EFIGUARD_CONFIGURATION_DATA *ConfigurationData);

// ScootwareCompat driver protocol
EFIGUARD_DRIVER_PROTOCOL gEfiGuardDriverProtocol = {DriverConfigure};

//
// Default driver configuration used if Configure() is not called
//
// Default driver configuration
//
EFIGUARD_CONFIGURATION_DATA gDriverConfig = {
    DSE_DISABLE_SETVARIABLE_HOOK, // DseBypassMethod
    FALSE                         // WaitForKeyPress
};

// Default ON so anything that runs before EfiGuardInitialize is silent.
// The init path may flip this OFF after reading scootware.cfg.
BOOLEAN gHeadless = TRUE;

//
// Bootmgfw.efi handle
//
EFI_HANDLE gBootmgfwHandle = NULL;

//
// EFI runtime globals
//
EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *gTextInputEx = NULL;
EFI_EVENT gEfiExitBootServicesEvent = NULL;
BOOLEAN gEfiAtRuntime = FALSE;
EFI_EVENT gEfiVirtualNotifyEvent = NULL;
BOOLEAN gEfiGoneVirtual = FALSE;

//
// Original gBS->LoadImage pointer
//
STATIC EFI_IMAGE_LOAD mOriginalLoadImage = NULL;

//
// Original gRT->SetVariable pointer
//
STATIC EFI_SET_VARIABLE mOriginalSetVariable = NULL;

//
// SMBIOS re-apply state (used by ReadyToBoot callback). Declared up here so
// both the unload path and the init path can reference them.
//
STATIC SCOOTWARE_EFI_CONFIG mPendingCfg;
STATIC BOOLEAN              mPendingCfgValid = FALSE;
STATIC EFI_EVENT            mReadyToBootEvent = NULL;

#if defined(MDE_CPU_X64)
#define MM_SYSTEM_RANGE_START                                                  \
  (VOID *)(0xFFFF080000000000) // Windows XP through 7 value. On newer systems
                               // this is a bit higher, but not that much
#elif defined(MDE_CPU_IA32)
#define MM_SYSTEM_RANGE_START (VOID *)(0x80000000)
#endif

// Title (adapted from original by Dude719)
#define SCOOTWARE_TITLE1	L"\r\n╔██████╗  ╔██████╗  ██████╗  ██████╗ ████████╗██╗    ██╗ █████╗ ██████╗ ███████╗" \
						            	L"\r\n██╔════╝ ║██╔═══   ██╔═══██╗██╔═══██╗╚══██╔══╝██║    ██║██╔══██╗██╔══██╗██╔════╝" \
			            				L"\r\n╚█████╗  ║██║      ██║   ██║██║   ██║   ██║   ██║ █╗ ██║███████║██████╔╝█████╗  " \
			            				L"\r\n ╚═══██╗ ║██║      ██║   ██║██║   ██║   ██║   ██║███╗██║██╔══██║██╔══██╗██╔══╝  " \
			            				L"\r\n██████╔╝ ╚╗██████  ╚██████╔╝╚██████╔╝   ██║   ╚███╔███╔╝██║  ██║██║  ██║███████╗"
#define SCOOTWARE_TITLE2	L"\r\n╚═════╝    ╚════╝   ╚═════╝  ╚═════╝    ╚═╝    ╚══╝╚══╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚══════╝" \
							L"\r\n                                                                           " \
							L"\r\n               Scootware Compatibility Module v1.0                           \r\n"

//
// (Un)hooks a service table pointer, replacing its value with NewFunction and
// returning the original address.
//
VOID *SetServicePointer(IN OUT EFI_TABLE_HEADER *ServiceTableHeader,
                        IN OUT VOID **ServiceTableFunction,
                        IN VOID *NewFunction) {
  if (ServiceTableFunction == NULL || NewFunction == NULL)
    return NULL;

  // If this is really needed after boot time at some point the CRC function is
  // easy enough to reimplement
  ASSERT(gBS != NULL);
  ASSERT(gBS->CalculateCrc32 != NULL);

  CONST EFI_TPL Tpl = gBS->RaiseTPL(TPL_HIGH_LEVEL); // Note: implies cli
  CONST UINTN Cr0 = AsmReadCr0();
  CONST BOOLEAN WpSet = (Cr0 & CR0_WP) != 0;
  if (WpSet)
    AsmWriteCr0(Cr0 & ~CR0_WP);

  VOID *OriginalFunction = InterlockedCompareExchangePointer(
      ServiceTableFunction, *ServiceTableFunction, NewFunction);

  // Recalculate the table checksum
  ServiceTableHeader->CRC32 = 0;
  gBS->CalculateCrc32((UINT8 *)ServiceTableHeader,
                      ServiceTableHeader->HeaderSize,
                      &ServiceTableHeader->CRC32);

  if (WpSet)
    AsmWriteCr0(Cr0);
  gBS->RestoreTPL(Tpl);

  return OriginalFunction;
}

//
// Boot Services LoadImage hook
//
EFI_STATUS
EFIAPI
HookedLoadImage(IN BOOLEAN BootPolicy, IN EFI_HANDLE ParentImageHandle,
                IN EFI_DEVICE_PATH_PROTOCOL *DevicePath,
                IN VOID *SourceBuffer OPTIONAL, IN UINTN SourceSize,
                OUT EFI_HANDLE *ImageHandle) {
  // Try to get a readable file path from the EFI shell protocol if it's
  // available
  EFI_SHELL_PROTOCOL *EfiShellProtocol = NULL;
  CONST EFI_STATUS EfiShellStatus = gBS->LocateProtocol(
      &gEfiShellProtocolGuid, NULL, (VOID **)&EfiShellProtocol);
  CHAR16 *ImagePath = NULL;
  if (!EFI_ERROR(EfiShellStatus)) {
    ImagePath = EfiShellProtocol->GetFilePathFromDevicePath(DevicePath);
  }
  if (ImagePath == NULL) {
    ImagePath = ConvertDevicePathToText(DevicePath, TRUE, TRUE);
  }

  // We only have a filename to go on at this point. We will determine the final
  // 'is this bootmgfw.efi?' status after the image has been loaded
  CONST BOOLEAN MaybeBootmgfw =
      ImagePath != NULL ? StriStr(ImagePath, L"bootmgfw.efi") != NULL ||
                              StriStr(ImagePath, L"Bootmgfw_ms.vc") != NULL ||
                              StriStr(ImagePath, L"bootx64.efi") != NULL
                        : FALSE;
  CONST BOOLEAN IsBoot =
      (MaybeBootmgfw || (BootPolicy == TRUE && SourceBuffer == NULL));

  if (!gHeadless) {
    // GUI (debug) mode: print what's being loaded
    CONST INT32 OriginalAttribute = SetConsoleTextColour(EFI_GREEN, FALSE);
    Print(L"[HookedLoadImage] %S %S\r\n    (ParentImageHandle = %llx)\r\n",
          (IsBoot ? L"Booting" : L"Loading"), ImagePath,
          (UINTN)ParentImageHandle);
    if (ImagePath != NULL)
      FreePool(ImagePath);

    gST->ConOut->SetAttribute(gST->ConOut, OriginalAttribute);
    gST->ConOut->EnableCursor(gST->ConOut, FALSE);
  } else {
    if (ImagePath != NULL)
      FreePool(ImagePath);
  }

  // Q: If we loaded bootmgfw.efi manually, is there any benefit to flipping
  // BootPolicy to TRUE to make it look like the load request came straight from
  // the boot manager?
  if (MaybeBootmgfw) {
    // Let's find out
    BootPolicy = TRUE;
  }

  // Load the image
  CONST EFI_STATUS Status =
      mOriginalLoadImage(BootPolicy, ParentImageHandle, DevicePath,
                         SourceBuffer, SourceSize, ImageHandle);

  // Was this a successful load of an image that's being booted?
  if (!EFI_ERROR(Status) && IsBoot && *ImageHandle != NULL) {
    // Get loaded image info
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage = NULL;
    CONST EFI_STATUS ImageInfoStatus = gBS->OpenProtocol(
        *ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImage,
        gImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(ImageInfoStatus)) {
      if (!gHeadless) {
        Print(L"\r\nHookedLoadImage: failed to get loaded image info. Status: "
              L"%llx (%r)\r\n",
              ImageInfoStatus, ImageInfoStatus);
      }
    } else {
      // Determine the type of file we're loading
      CONST INPUT_FILETYPE FileType =
          GetInputFileType(LoadedImage->ImageBase, LoadedImage->ImageSize);
      ASSERT(FileType == Unknown || FileType == Bootmgr ||
             FileType == BootmgfwEfi);

      if (FileType == BootmgfwEfi) {
        // This is bootmgfw.efi. Save the returned image handle
        gBootmgfwHandle = *ImageHandle;
        LoadedImage->ParentHandle = NULL;

        if (!gHeadless) {
          // Print image info
          PrintLoadedImageInfo(LoadedImage);
        }

        // Nuke it dot it
        PatchBootManager(FileType, LoadedImage->ImageBase,
                         LoadedImage->ImageSize);
      }
    }
  }

  return Status;
}

//
// Runtime Services SetVariable hook
//
EFI_STATUS
EFIAPI
HookedSetVariable(IN CHAR16 *VariableName, IN EFI_GUID *VendorGuid,
                  IN UINT32 Attributes, IN UINTN DataSize, IN VOID *Data) {
  // We should not be hooking the runtime table after ExitBootServices() unless
  // this is the selected DSE bypass method
  ASSERT(!gEfiAtRuntime ||
         (gDriverConfig.DseBypassMethod == DSE_DISABLE_SETVARIABLE_HOOK &&
          gBootmgfwHandle != NULL));

  // Do we have a match for the variable name and vendor GUID?
  if (gEfiAtRuntime && gEfiGoneVirtual && VariableName != NULL &&
      VariableName[0] != CHAR_NULL && VendorGuid != NULL &&
      CompareGuid(VendorGuid, EFIGUARD_BACKDOOR_VARIABLE_GUID) &&
      StrnCmp(VariableName, EFIGUARD_BACKDOOR_VARIABLE_NAME,
              (sizeof(EFIGUARD_BACKDOOR_VARIABLE_NAME) / sizeof(CHAR16)) - 1) ==
          0) {
    // Yep. Do we have any data?
    if (DataSize == 0 && Data == NULL) {
      // Nope. This is the first SetVariable() call from the HAL, intended to
      // wipe the variable. (This call may be skipped if
      // EFI_VARIABLE_APPEND_WRITE is set, but this is version-dependent)
      return EFI_SUCCESS;
    }

    if ((Attributes & EFIGUARD_BACKDOOR_VARIABLE_ATTRIBUTES) ==
            EFIGUARD_BACKDOOR_VARIABLE_ATTRIBUTES &&
        DataSize == EFIGUARD_BACKDOOR_VARIABLE_DATASIZE && Data != NULL) {
      // Yep, and Attributes and DataSize are correct. Check if *Data is a valid
      // input for a backdoor read/write operation
      EFIGUARD_BACKDOOR_DATA *BackdoorData = Data;
      if (BackdoorData->CookieValue == EFIGUARD_BACKDOOR_COOKIE_VALUE &&
          BackdoorData->Size > 0 &&
          (UINTN)BackdoorData->KernelAddress >= (UINTN)MM_SYSTEM_RANGE_START) {
        // For scalars, copy user value to kernel memory and put the old value
        // in BackdoorData->u.XXX
        switch (BackdoorData->Size) {
        case 1: {
          CONST UINT8 NewByte = (UINT8)BackdoorData->u.s.Byte;
          BackdoorData->u.s.Byte = *(UINT8 *)BackdoorData->KernelAddress;
          if (!BackdoorData->ReadOnly)
            CopyWpMem(BackdoorData->KernelAddress, &NewByte, sizeof(NewByte));
          break;
        }
        case 2: {
          CONST UINT16 NewWord = (UINT16)BackdoorData->u.s.Word;
          BackdoorData->u.s.Word = *(UINT16 *)BackdoorData->KernelAddress;
          if (!BackdoorData->ReadOnly)
            CopyWpMem(BackdoorData->KernelAddress, &NewWord, sizeof(NewWord));
          break;
        }
        case 4: {
          CONST UINT32 NewDword = (UINT32)BackdoorData->u.s.Dword;
          BackdoorData->u.s.Dword = *(UINT32 *)BackdoorData->KernelAddress;
          if (!BackdoorData->ReadOnly)
            CopyWpMem(BackdoorData->KernelAddress, &NewDword, sizeof(NewDword));
          break;
        }
        case 8: {
          CONST UINT64 NewQword = BackdoorData->u.Qword;
          BackdoorData->u.Qword = *(UINT64 *)BackdoorData->KernelAddress;
          if (!BackdoorData->ReadOnly)
            CopyWpMem(BackdoorData->KernelAddress, &NewQword, sizeof(NewQword));
          break;
        }
        default: {
          // Arbitrary size memcpy
          if (BackdoorData->u.UserBuffer != NULL) {
            if (BackdoorData->ReadOnly)
              CopyWpMem(BackdoorData->u.UserBuffer, BackdoorData->KernelAddress,
                        BackdoorData->Size);
            else
              CopyWpMem(BackdoorData->KernelAddress, BackdoorData->u.UserBuffer,
                        BackdoorData->Size);
          }
          break;
        }
        }

        // Backdoor complete
        return EFI_SUCCESS;
      }
      // else { /*Invalid EFIGUARD_BACKDOOR_DATA* provided*/ }
    }
    // else { /*Data is NULL, or DataSize/Attributes mismatch*/ }
  }
  // else { /*Not our variable name + vendor GUID, or SetVirtualAddressMap() has
  // not been called yet*/ }

  return mOriginalSetVariable(VariableName, VendorGuid, Attributes, DataSize,
                              Data);
}

//
// ExitBootServices callback
//
VOID EFIAPI ExitBootServicesEvent(IN EFI_EVENT Event, IN VOID *Context) {
  // Close this event now. The boot loader only calls this once.
  gBS->CloseEvent(gEfiExitBootServicesEvent);
  gEfiExitBootServicesEvent = NULL;

  // The message buffer may be empty if the patch process was aborted in one of
  // the earlier stages
  if (gKernelPatchInfo.Buffer[0] != CHAR_NULL) {
    CONST EFI_STATUS Status = gKernelPatchInfo.Status;
    CONST INT32 OriginalAttribute = gST->ConOut->Mode->Attribute;

    // Default to showing a message in case of errors unless we are booting a
    // pre-Vista kernel such as XP, in which case EFI_UNSUPPORTED is expected.
    CONST BOOLEAN ShowErrorMessage =
        gKernelPatchInfo.KernelBuildNumber == 0 ||
        gKernelPatchInfo.KernelBuildNumber >= 6001 || Status != EFI_UNSUPPORTED;
    if (!gHeadless) {
      if (Status == EFI_SUCCESS) {
        SetConsoleTextColour(EFI_GREEN, TRUE);
        PrintKernelPatchInfo();
        Print(L"\r\nSuccessfully patched ntoskrnl.exe.\r\n");

        if (gDriverConfig.WaitForKeyPress) {
          Print(L"\r\nPress any key to continue.\r\n");
          RtlStall(2000);
        }
      } else if (ShowErrorMessage) {
        // Patch failed. Make a fake BSOD
        gST->ConOut->SetAttribute(gST->ConOut, EFI_WHITE | EFI_BACKGROUND_BLUE);
        gST->ConOut->ClearScreen(gST->ConOut);

        Print(L"A problem has been detected and Windows has been paused to "
              L"prevent damage\r\nto your botnets.\r\n\r\n"
              L"BOOTKIT_KERNEL_PATCH_FAILED\r\n\r\n"
              L"Technical information:\r\n\r\n*** STOP: 0X%llX (%r, "
              L"0x%p)\r\n\r\n",
              Status, Status, gKernelPatchInfo.KernelBase);
        PrintKernelPatchInfo();

        RtlStall(2000);

        Print(L"\r\nPress any key to continue anyway, or press ESC to "
              L"reboot.\r\n");
        if (!WaitForKey()) {
          gRT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
        }
      }

      gST->ConOut->SetAttribute(gST->ConOut, OriginalAttribute);
      if (Status != EFI_SUCCESS && ShowErrorMessage)
        gST->ConOut->ClearScreen(gST->ConOut);
    } else {
      // Headless mode: suppress all console output and user interaction.
      // Patches are applied silently and Windows boots without interruption.
      if (Status != EFI_SUCCESS && ShowErrorMessage) {
        // Log failure but continue silently - no BSOD simulation or key wait.
        // The system will boot Windows regardless of patch status.
      }
    }
  }

  // If the DSE bypass method is *not* DSE_DISABLE_SETVARIABLE_HOOK, perform
  // some cleanup now. In principle this should allow linking with
  // /SUBSYSTEM:EFI_BOOT_SERVICE_DRIVER, because our driver image may be freed
  // after this callback returns. Using DSE_DISABLE_SETVARIABLE_HOOK requires
  // linking with /SUBSYSTEM:EFI_RUNTIME_DRIVER, because the image must not be
  // freed.
  if (gDriverConfig.DseBypassMethod != DSE_DISABLE_SETVARIABLE_HOOK ||
      gBootmgfwHandle == NULL) {
    // Uninstall our installed driver protocols
    gBS->UninstallMultipleProtocolInterfaces(
        gImageHandle, &gEfiGuardDriverProtocolGuid, &gEfiGuardDriverProtocol,
        &gEfiDriverSupportedEfiVersionProtocolGuid,
        &gEfiGuardSupportedEfiVersion, NULL);

    // Unregister SetVirtualAddressMap() notification
    if (gEfiVirtualNotifyEvent != NULL) {
      gBS->CloseEvent(gEfiVirtualNotifyEvent);
      gEfiVirtualNotifyEvent = NULL;
    }

    // Unhook gRT->SetVariable
    if (mOriginalSetVariable != NULL) {
      SetServicePointer(&gRT->Hdr, (VOID **)&gRT->SetVariable,
                        (VOID *)mOriginalSetVariable);
      mOriginalSetVariable = NULL;
    }
  }

  // Regardless of which OS is being booted, boot services won't be available
  // after this callback returns
  gBS = NULL;
  mOriginalLoadImage = NULL;
  gEfiAtRuntime = TRUE;
}

//
// SetVirtualAddressMap callback
//
VOID EFIAPI SetVirtualAddressMapEvent(IN EFI_EVENT Event, IN VOID *Context) {
  ASSERT(gEfiAtRuntime == TRUE);
  ASSERT(gBS == NULL);
  gEfiVirtualNotifyEvent = NULL;

  // Convert the original SetVariable pointer to virtual so our hook will
  // continue to work
  EFI_STATUS Status = gRT->ConvertPointer(0, (VOID **)&mOriginalSetVariable);
  ASSERT_EFI_ERROR(Status);

  // Convert the runtime services pointer itself from physical to virtual
  Status = gRT->ConvertPointer(0, (VOID **)&gRT);
  ASSERT_EFI_ERROR(Status);

  // Set the flag indicating virtual addressing mode has been entered
  gEfiGoneVirtual = TRUE;
}

EFI_STATUS
EFIAPI
DriverConfigure(IN CONST EFIGUARD_CONFIGURATION_DATA *ConfigurationData) {
  // Do not allow configure if we are at runtime, or if the Windows boot manager
  // has been loaded
  if (gEfiAtRuntime || gBootmgfwHandle != NULL)
    return EFI_ACCESS_DENIED;

  if (ConfigurationData == NULL)
    return EFI_INVALID_PARAMETER;

  gDriverConfig = *ConfigurationData;

  // Keep gHeadless in sync with the caller-supplied WaitForKeyPress so any
  // later print site honours the updated mode. We intentionally tie the two
  // together: WaitForKeyPress is a "debug build behaviour" toggle, and the
  // pretty-print path is what makes the waits meaningful.
  gHeadless = !gDriverConfig.WaitForKeyPress;

  if (!gHeadless) {
    Print(L"Configuration data accepted.\r\n\r\n");
  }

  return EFI_SUCCESS;
}

//
// Driver unload
//
EFI_STATUS
EFIAPI
EfiGuardUnload(IN EFI_HANDLE ImageHandle) {
  // Do not allow unload if we are at runtime, or if the Windows boot manager
  // has been loaded
  if (gEfiAtRuntime || gBootmgfwHandle != NULL) {
    return EFI_ACCESS_DENIED;
  }

  ASSERT(gBS != NULL);

  // Uninstall our installed driver protocols
  gBS->UninstallMultipleProtocolInterfaces(
      gImageHandle, &gEfiGuardDriverProtocolGuid, &gEfiGuardDriverProtocol,
      &gEfiDriverSupportedEfiVersionProtocolGuid, &gEfiGuardSupportedEfiVersion,
      NULL);

  // Unregister SetVirtualAddressMap() notification
  if (gEfiVirtualNotifyEvent != NULL) {
    gBS->CloseEvent(gEfiVirtualNotifyEvent);
    gEfiVirtualNotifyEvent = NULL;
  }

  // Unregister ExitBootServices() notification
  if (gEfiExitBootServicesEvent != NULL) {
    gBS->CloseEvent(gEfiExitBootServicesEvent);
    gEfiExitBootServicesEvent = NULL;
  }

  // Unregister ReadyToBoot notification (used for SMBIOS re-apply)
  if (mReadyToBootEvent != NULL) {
    gBS->CloseEvent(mReadyToBootEvent);
    mReadyToBootEvent = NULL;
  }

  // Unhook gRT->SetVariable
  if (mOriginalSetVariable != NULL) {
    SetServicePointer(&gRT->Hdr, (VOID **)&gRT->SetVariable,
                      (VOID *)mOriginalSetVariable);
    mOriginalSetVariable = NULL;
  }

  // Unhook gBS->LoadImage
  if (mOriginalLoadImage != NULL) {
    SetServicePointer(&gBS->Hdr, (VOID **)&gBS->LoadImage,
                      (VOID *)mOriginalLoadImage);
    mOriginalLoadImage = NULL;
  }

  return EFI_SUCCESS;
}

//
// Open the volume the driver was loaded from. The config file lives next to
// the driver on the ESP, so we resolve the file system through
// LoadedImage->DeviceHandle rather than scanning all simple-file-system
// handles (which could pick up the wrong volume on multi-disk systems).
//
STATIC
EFI_STATUS
OpenLoaderVolume(OUT EFI_FILE_HANDLE *Root) {
  *Root = NULL;

  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage = NULL;
  EFI_STATUS Status =
      gBS->HandleProtocol(gImageHandle, &gEfiLoadedImageProtocolGuid,
                          (VOID **)&LoadedImage);
  if (EFI_ERROR(Status))
    return Status;

  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs = NULL;
  Status = gBS->HandleProtocol(LoadedImage->DeviceHandle,
                               &gEfiSimpleFileSystemProtocolGuid, (VOID **)&Fs);
  if (EFI_ERROR(Status))
    return Status;

  return Fs->OpenVolume(Fs, Root);
}

//
// Read scootware.cfg from the ESP. Returns EFI_SUCCESS only if the file
// exists, is the expected size, and passes magic/version/checksum validation.
//
STATIC
EFI_STATUS
ReadConfigFromDisk(OUT SCOOTWARE_EFI_CONFIG *Cfg) {
  EFI_FILE_HANDLE Root = NULL;
  EFI_STATUS Status = OpenLoaderVolume(&Root);
  if (EFI_ERROR(Status))
    return Status;

  EFI_FILE_HANDLE File = NULL;
  Status = Root->Open(Root, &File, SCOOTWARE_CFG_PATH, EFI_FILE_MODE_READ, 0);
  Root->Close(Root);
  if (EFI_ERROR(Status))
    return Status;

  UINTN ReadSize = sizeof(*Cfg);
  Status = File->Read(File, &ReadSize, Cfg);
  File->Close(File);

  if (EFI_ERROR(Status))
    return Status;
  if (ReadSize != sizeof(*Cfg))
    return EFI_COMPROMISED_DATA;
  if (!ScootwConfigIsValid(Cfg))
    return EFI_COMPROMISED_DATA;

  return EFI_SUCCESS;
}

//
// Write scootware.cfg to the ESP. Recomputes the checksum before writing.
//
STATIC
EFI_STATUS
SaveConfigToDisk(IN SCOOTWARE_EFI_CONFIG *Cfg) {
  Cfg->Checksum = ScootwConfigChecksum(Cfg);

  EFI_FILE_HANDLE Root = NULL;
  EFI_STATUS Status = OpenLoaderVolume(&Root);
  if (EFI_ERROR(Status))
    return Status;

  EFI_FILE_HANDLE File = NULL;
  Status = Root->Open(
      Root, &File, SCOOTWARE_CFG_PATH,
      EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
  Root->Close(Root);
  if (EFI_ERROR(Status))
    return Status;

  // Rewrite from the start in case the file already existed at a larger size.
  File->SetPosition(File, 0);

  UINTN WriteSize = sizeof(*Cfg);
  Status = File->Write(File, &WriteSize, Cfg);
  File->Flush(File);
  File->Close(File);

  if (!EFI_ERROR(Status) && WriteSize != sizeof(*Cfg))
    Status = EFI_DEVICE_ERROR;
  return Status;
}

//
// Decode the on-disk DseBypassMethod into the driver's enum, resolving the
// "auto" case using OsBuildNumber written by the Windows-side loader.
//
STATIC
EFIGUARD_DSE_BYPASS_TYPE
ResolveDseBypassMethod(IN CONST SCOOTWARE_EFI_CONFIG *Cfg) {
  switch (Cfg->DseBypassMethod) {
  case 0:
    return DSE_DISABLE_NONE;
  case 1:
    return DSE_DISABLE_AT_BOOT;
  case 2:
    return DSE_DISABLE_SETVARIABLE_HOOK;
  case 3:
  default:
    // Win10 1803+ / Win11 / unknown → SetVariable hook (well-tested)
    // Older → AtBoot patch (safer on pre-1803)
    if (Cfg->OsBuildNumber == 0 || Cfg->OsBuildNumber >= 17134)
      return DSE_DISABLE_SETVARIABLE_HOOK;
    return DSE_DISABLE_AT_BOOT;
  }
}

//
// Apply SMBIOS handling from the on-disk config. Capture is one-shot
// (HwIdCaptured guard), and spoof is one-shot per boot (HwIdApply guard
// cleared after a successful apply). Returns TRUE if any field of Cfg was
// mutated and the caller should write it back.
//
STATIC
BOOLEAN
ProcessSmbiosFromConfig(IN OUT SCOOTWARE_EFI_CONFIG *Cfg) {
  BOOLEAN Mutated = FALSE;

  if (Cfg->HwIdCaptured == 0 && Cfg->HwIdApply == 0) {
    // Only capture when no spoof is staged for this boot, so we never trample
    // the caller-supplied spoof values into our captured baseline.
    SmbiosCaptureCurrent(Cfg);
    if (Cfg->HwIdCaptured != 0)
      Mutated = TRUE;
  }

  if (Cfg->HwIdApply != 0) {
    SmbiosApplySpoofs(Cfg);
    Cfg->HwIdApply = 0;
    Mutated = TRUE;
  }

  return Mutated;
}

//
// ReadyToBoot callback: re-apply SMBIOS spoofs (some firmwares re-publish the
// SMBIOS table in BDS, after our early init runs but before the OS loader
// actually reads it). The on-disk config staged for this boot lives in
// mPendingCfg/mPendingCfgValid which are declared at file scope above.
//
STATIC
VOID
EFIAPI
ReadyToBootSmbiosCallback(IN EFI_EVENT Event, IN VOID *Context) {
  // Close-once: this event fires at most once per boot, but be defensive.
  if (mReadyToBootEvent != NULL) {
    gBS->CloseEvent(mReadyToBootEvent);
    mReadyToBootEvent = NULL;
  }
  if (!mPendingCfgValid)
    return;

  // Re-apply spoofs against whatever SMBIOS layout the firmware finalized for
  // ReadyToBoot. We don't reset HwIdApply here because the on-disk file
  // already had that cleared by the init-time pass; this is purely an
  // in-memory re-application against potentially-regenerated tables.
  SmbiosApplySpoofs(&mPendingCfg);
}

//
// Main entry point
//
EFI_STATUS
EFIAPI
EfiGuardInitialize(IN EFI_HANDLE ImageHandle,
                   IN EFI_SYSTEM_TABLE *SystemTable) {
  ASSERT(ImageHandle == gImageHandle);

  //
  // Defaults: HEADLESS + SetVariable hook. Headless is the safe default — if
  // there is no config file (e.g. driver launched directly as a UEFI driver
  // entry, not via Loader.efi), the user wants silence, not a banner-and-wait.
  //
  gDriverConfig.DseBypassMethod = DSE_DISABLE_SETVARIABLE_HOOK;
  gDriverConfig.WaitForKeyPress = FALSE;
  gHeadless = TRUE;

  //
  // Read scootware.cfg from the same volume the driver was loaded from.
  // If anything goes wrong (no file, bad magic/checksum, short read) we just
  // proceed with the headless defaults and skip SMBIOS work entirely.
  //
  SCOOTWARE_EFI_CONFIG FileCfg;
  BOOLEAN HaveFile = (ReadConfigFromDisk(&FileCfg) == EFI_SUCCESS);

  if (HaveFile) {
    gDriverConfig.DseBypassMethod = ResolveDseBypassMethod(&FileCfg);
    gDriverConfig.WaitForKeyPress = (BOOLEAN)(FileCfg.WaitForKeyPress != 0);
    gHeadless = !gDriverConfig.WaitForKeyPress;

    // SMBIOS handling: capture-once + apply-on-request. Helper returns TRUE
    // if Cfg was mutated and needs to be written back; we do exactly one
    // FAT32 round-trip rather than two.
    if (ProcessSmbiosFromConfig(&FileCfg)) {
      SaveConfigToDisk(&FileCfg);
    }

    // Stash for the ReadyToBoot re-apply pass. Use CopyMem rather than struct
    // assignment so the linker doesn't pull in a libc-style memcpy: the EFI
    // toolchain only provides BaseMemoryLib.
    CopyMem(&mPendingCfg, &FileCfg, sizeof(mPendingCfg));
    mPendingCfgValid = TRUE;
  }

  // Check if we're not already loaded.
  EFIGUARD_DRIVER_PROTOCOL *EfiGuardDriverProtocol;
  EFI_STATUS Status = gBS->LocateProtocol(&gEfiGuardDriverProtocolGuid, NULL,
                                          (VOID **)&EfiGuardDriverProtocol);
  if (Status != EFI_NOT_FOUND) {
    if (!gHeadless) {
      Print(L"An instance of the driver is already loaded.\r\n");
    }
    return EFI_ALREADY_STARTED;
  }

  //
  // Install supported EFI version protocol
  //
  Status = gBS->InstallMultipleProtocolInterfaces(
      &gImageHandle, &gEfiDriverSupportedEfiVersionProtocolGuid,
      &gEfiGuardSupportedEfiVersion, NULL);
  if (EFI_ERROR(Status)) {
    if (!gHeadless) {
      Print(L"Failed to install EFI Driver Supported Version protocol. Error: "
            L"%llx (%r)\r\n",
            Status, Status);
    }
    return Status;
  }

  //
  // Query the console input handle for the Simple Text Input Ex protocol
  //
  gBS->HandleProtocol(gST->ConsoleInHandle, &gEfiSimpleTextInputExProtocolGuid,
                      (VOID **)&gTextInputEx);

  //
  // Install ScootwareCompat driver protocol
  //
  Status = gBS->InstallProtocolInterface(
      &gImageHandle, &gEfiGuardDriverProtocolGuid, EFI_NATIVE_INTERFACE,
      &gEfiGuardDriverProtocol);
  if (EFI_ERROR(Status))
    goto Exit;

  if (!gHeadless) {
    //
    // GUI (debug) mode: clear screen, print banner, show hook info
    //
    CONST INT32 OriginalAttribute = SetConsoleTextColour(EFI_GREEN, TRUE);
    Print(L"\r\n\r\n");
    Print(L"%S", SCOOTWARE_TITLE1);
    Print(L"%S", SCOOTWARE_TITLE2);
    gST->ConOut->SetAttribute(gST->ConOut, OriginalAttribute);

    EFI_LOADED_IMAGE_PROTOCOL *LocalImageInfo;
    Status = gBS->OpenProtocol(gImageHandle, &gEfiLoadedImageProtocolGuid,
                               (VOID **)&LocalImageInfo, gImageHandle, NULL,
                               EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (!EFI_ERROR(Status)) {
      PrintLoadedImageInfo(LocalImageInfo);
    }

    //
    // Hook gBS->LoadImage
    //
    mOriginalLoadImage = (EFI_IMAGE_LOAD)SetServicePointer(
        &gBS->Hdr, (VOID **)&gBS->LoadImage, (VOID *)&HookedLoadImage);
    Print(L"Hooked gBS->LoadImage: 0x%p -> 0x%p\r\n",
          (VOID *)mOriginalLoadImage, (VOID *)&HookedLoadImage);

    //
    // Hook gRT->SetVariable
    //
    mOriginalSetVariable = (EFI_SET_VARIABLE)SetServicePointer(
        &gRT->Hdr, (VOID **)&gRT->SetVariable, (VOID *)&HookedSetVariable);
    Print(L"Hooked gRT->SetVariable: 0x%p -> 0x%p\r\n",
          (VOID *)mOriginalSetVariable, (VOID *)&HookedSetVariable);

    // The ASCII banner is very pretty - ensure the user has enough time to
    // admire it
    RtlSleep(300);
  } else {
    //
    // Headless mode: all console output suppressed.
    // Hooks are installed silently.
    //
    mOriginalLoadImage = (EFI_IMAGE_LOAD)SetServicePointer(
        &gBS->Hdr, (VOID **)&gBS->LoadImage, (VOID *)&HookedLoadImage);
    mOriginalSetVariable = (EFI_SET_VARIABLE)SetServicePointer(
        &gRT->Hdr, (VOID **)&gRT->SetVariable, (VOID *)&HookedSetVariable);
  }

  // Register notification callback for ExitBootServices()
  // Non-fatal: some firmware does not support CreateEventEx with event-group
  // GUIDs and returns EFI_NOT_FOUND. The LoadImage/SetVariable hooks are
  // already active and will still patch the kernel at boot time; only the
  // post-boot cleanup/print path is skipped on those systems.
  Status = gBS->CreateEventEx(
      EVT_NOTIFY_SIGNAL, TPL_NOTIFY, ExitBootServicesEvent, NULL,
      &gEfiEventExitBootServicesGuid, &gEfiExitBootServicesEvent);
  if (EFI_ERROR(Status)) {
    // Fallback: some firmware does not support CreateEventEx with event-group
    // GUIDs. Use the legacy EVT_SIGNAL_EXIT_BOOT_SERVICES signal type instead.
    Status = gBS->CreateEvent(EVT_SIGNAL_EXIT_BOOT_SERVICES, TPL_NOTIFY,
                              ExitBootServicesEvent, NULL,
                              &gEfiExitBootServicesEvent);
    if (EFI_ERROR(Status)) {
      if (!gHeadless) {
        Print(L"[DRIVER] Warning: ExitBootServices callback failed both "
              L"methods: %r — gEfiAtRuntime will not be set.\r\n",
              Status);
      }
      gEfiExitBootServicesEvent = NULL;
      Status = EFI_SUCCESS;
    }
  }

  // Register notification callback for SetVirtualAddressMap()
  // Non-fatal for the same reason. If this fails, the SetVariable runtime
  // backdoor (DSE_DISABLE_SETVARIABLE_HOOK) will not work, but boot-time
  // patching is unaffected.
  Status = gBS->CreateEventEx(
      EVT_NOTIFY_SIGNAL, TPL_NOTIFY, SetVirtualAddressMapEvent, NULL,
      &gEfiEventVirtualAddressChangeGuid, &gEfiVirtualNotifyEvent);
  if (EFI_ERROR(Status)) {
    // Fallback: use legacy EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE signal type.
    Status = gBS->CreateEvent(EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE, TPL_NOTIFY,
                              SetVirtualAddressMapEvent, NULL,
                              &gEfiVirtualNotifyEvent);
    if (EFI_ERROR(Status)) {
      if (!gHeadless) {
        Print(L"[DRIVER] Warning: SetVirtualAddressMap callback failed both "
              L"methods: %r — runtime backdoor may not work.\r\n",
              Status);
      }
      gEfiVirtualNotifyEvent = NULL;
      Status = EFI_SUCCESS;
    }
  }

  //
  // Register a ReadyToBoot callback to re-apply SMBIOS spoofs late in BDS.
  // Some firmwares regenerate/relocate the SMBIOS configuration table during
  // BDS (after our init runs but before the OS loader actually reads it), so
  // a single apply at driver-init time can be silently undone. Re-applying
  // at ReadyToBoot is the latest hook that still has gBS available.
  //
  // Best-effort only: if no spoof was staged for this boot, or this firmware
  // doesn't expose the event group, we just skip — the init-time apply is
  // still in effect.
  //
  if (mPendingCfgValid) {
    EFI_STATUS RtbStatus = gBS->CreateEventEx(
        EVT_NOTIFY_SIGNAL, TPL_CALLBACK, ReadyToBootSmbiosCallback, NULL,
        &gEfiEventReadyToBootGuid, &mReadyToBootEvent);
    if (EFI_ERROR(RtbStatus))
      mReadyToBootEvent = NULL;  // non-fatal
  }

  // Initialize the global kernel patch info struct.
  gKernelPatchInfo.Status = EFI_SUCCESS;
  gKernelPatchInfo.BufferSize = 0;
  SetMem64(gKernelPatchInfo.Buffer, sizeof(gKernelPatchInfo.Buffer), 0ULL);
  gKernelPatchInfo.WinloadBuildNumber = 0;
  gKernelPatchInfo.KernelBuildNumber = 0;
  gKernelPatchInfo.KernelBase = NULL;

Exit:
  if (EFI_ERROR(Status)) {
    if (!gHeadless) {
      Print(L"\r\nScootwareCompatDxe initialization failed with status %llx "
            L"(%r)\r\n",
            Status, Status);
    }
    // Because we do not use the driver binding protocol, recovering from a
    // failed load is simple. We can just call the unload function, which will
    // only unload that which was actually installed.
    EfiGuardUnload(gImageHandle);
  }
  return Status;
}
