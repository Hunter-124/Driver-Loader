/*******************************************************************************
*
*  (C) COPYRIGHT AUTHORS, 2020 - 2026
*
*  TITLE:       KDUPLIST.H
*
*  VERSION:     1.47
*
*  DATE:        25 Mar 2026
*
*  Providers global list.
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/

#pragma once

#include "idrv/intel.h"
#include "idrv/winio.h"
#include "idrv/winring0.h"
#include "idrv/directio64.h"
#include "idrv/dell.h"
#include "idrv/zemana.h"
#include "idrv/asrdrv.h"
#include "idrv/alcpu.h"
#include "idrv/amd.h"
#include "idrv/lenovo.h"
#include "idrv/zodiacon.h"
#include "idrv/nvidia.h"
#include "idrv/rzpnk.h"
#include "idrv/evga.h"
#include "idrv/netease.h"
#include "idrv/tpup.h"
#include "idrv/tpw.h"

//
// Victims public array.
//
static KDU_VICTIM_PROVIDER g_KDUVictims[] = {
    {
        (LPCWSTR)PROCEXP152,              // Device and driver name,
        (LPCWSTR)PROCEXP1627_DESC,        // Description
        IDR_PROCEXP1627,                  // Resource id in drivers database
        KDU_VICTIM_PE1627,                // Victim id
        SYNCHRONIZE |
        GENERIC_READ | GENERIC_WRITE,     // Desired access flags used for acquiring victim handle
        KDU_VICTIM_FLAGS_SUPPORT_RELOAD,  // Victim flags, target dependent
        VpCreateCallback,                 // Victim create callback
        VpReleaseCallback,                // Victim release callback
        VpExecuteCallback,                // Victim execute payload callback
        &g_ProcExpSig,                    // Victim dispatch bytes
        sizeof(g_ProcExpSig)              // Victim dispatch bytes size
    },

    {
        (LPCWSTR)PROCEXP152,              // Device and driver name,
        (LPCWSTR)PROCEXP1702_DESC,        // Description
        IDR_PROCEXP1702,                  // Resource id in drivers database
        KDU_VICTIM_PE1702,                // Victim id
        SYNCHRONIZE |
        GENERIC_READ | GENERIC_WRITE,     // Desired access flags used for acquiring victim handle
        KDU_VICTIM_FLAGS_SUPPORT_RELOAD,  // Victim flags, target dependent
        VpCreateCallback,                 // Victim create callback
        VpReleaseCallback,                // Victim release callback
        VpExecuteCallback,                // Victim execute payload callback
        &g_ProcExpSig,                    // Victim dispatch bytes
        sizeof(g_ProcExpSig)              // Victim dispatch bytes size
    }

};

//
// Providers public array, unsupported methods must be set to NULL.
//
static KDU_PROVIDER g_KDUProviders[] =
{
    // Index 0: KDU_PROVIDER_INTEL_NAL - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 1: KDU_PROVIDER_UNWINDER_RTCORE - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 2: KDU_PROVIDER_GIGABYTE_GDRV - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 3: KDU_PROVIDER_ASUSTEK_ATSZIO - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 4: KDU_PROVIDER_PATRIOT_MSIO64 - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 5: KDU_PROVIDER_GLCKIO2 - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 6: KDU_PROVIDER_ENEIO64 - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 7: KDU_PROVIDER_WINRING0 - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 8: KDU_PROVIDER_ENETECHIO64 - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 9: KDU_PROVIDER_PHYMEM64 - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 10: KDU_PROVIDER_RTKIO64 - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 11: KDU_PROVIDER_ENETECHIO64B - MSI Dragon Center (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)WinIoRegisterDriver,
        (provUnregisterDriver)WinIoUnregisterDriver,
        (provPreOpenDriver)WinIoPreOpen,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE,

        (provReadKernelVM)WinIoReadKernelVirtualMemory,
        (provWriteKernelVM)WinIoWriteKernelVirtualMemory,

        (provVirtualToPhysical)WinIoVirtualToPhysical,
        (provQueryPML4)WinIoQueryPML4Value,
        (provReadPhysicalMemory)WinIoReadPhysicalMemory,
        (provWritePhysicalMemory)WinIoWritePhysicalMemory,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)NULL
    },

    // Index 12: KDU_PROVIDER_LHA - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 13: KDU_PROVIDER_ASUSIO2 - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 14: KDU_PROVIDER_DIRECTIO64 - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 15: KDU_PROVIDER_GMER - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 16: KDU_PROVIDER_DBUTIL23 - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 17: KDU_PROVIDER_MIMIDRV - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 18: KDU_PROVIDER_KPH - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 19: KDU_PROVIDER_PROCEXP - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 20: KDU_PROVIDER_DBUTILDRV2 - Dell BIOS Utility (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)DbUtilStartVulnerableDriver,
        (provStopVulnerableDriver)DbUtilStopVulnerableDriver,

        (provRegisterDriver)NULL,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE,

        (provReadKernelVM)DbUtilReadVirtualMemory,
        (provWriteKernelVM)DbUtilWriteVirtualMemory,

        (provVirtualToPhysical)NULL,
        (provQueryPML4)NULL,
        (provReadPhysicalMemory)NULL,
        (provWritePhysicalMemory)NULL,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)NULL
    },

    // Index 21: KDU_PROVIDER_DBK64 - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 22: KDU_PROVIDER_ASUSIO3 - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 23: KDU_PROVIDER_HW64 - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 24: KDU_PROVIDER_SYSDRV3S - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 25: KDU_PROVIDER_ZEMANA - WatchDog/MalwareFox/Zemana AM (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)ZmRegisterDriver,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)NULL,
        (provMapDriver)ZmMapDriver,
        (provControlDSE)ZmControlDSE,

        (provReadKernelVM)NULL,
        (provWriteKernelVM)NULL,

        (provVirtualToPhysical)NULL,
        (provQueryPML4)NULL,
        (provReadPhysicalMemory)NULL,
        (provWritePhysicalMemory)NULL,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)ZmOpenProcess
    },

    // Index 26: KDU_PROVIDER_INPOUTX64 - inputtx64 (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)WinIoRegisterDriver,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE,

        (provReadKernelVM)WinIoReadKernelVirtualMemory,
        (provWriteKernelVM)WinIoWriteKernelVirtualMemory,

        (provVirtualToPhysical)WinIoVirtualToPhysical,
        (provQueryPML4)WinIoQueryPML4Value,
        (provReadPhysicalMemory)WinIoReadPhysicalMemory,
        (provWritePhysicalMemory)WinIoWritePhysicalMemory,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)NULL
    },

    // Index 27: KDU_PROVIDER_PASSMARK_OSF - PassMark OSForensics (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)NULL,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE,

        (provReadKernelVM)DI64ReadKernelVirtualMemory,
        (provWriteKernelVM)DI64WriteKernelVirtualMemory,

        (provVirtualToPhysical)DI64VirtualToPhysical,
        (provQueryPML4)DI64QueryPML4Value,
        (provReadPhysicalMemory)DI64ReadPhysicalMemory,
        (provWritePhysicalMemory)DI64WritePhysicalMemory,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)NULL
    },

    // Index 28: KDU_PROVIDER_ASROCK - AsrDrv106, Phantom Gaming Tuning (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)NULL,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE2,

        (provReadKernelVM)NULL,
        (provWriteKernelVM)NULL,

        (provVirtualToPhysical)NULL,
        (provQueryPML4)NULL,
        (provReadPhysicalMemory)AsrReadPhysicalMemory,
        (provWritePhysicalMemory)AsrWritePhysicalMemory,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)NULL
    },

    // Index 29: KDU_PROVIDER_ALCPU - ALSysIO64, Core Temp (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)NULL,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE2,

        (provReadKernelVM)NULL,
        (provWriteKernelVM)NULL,

        (provVirtualToPhysical)NULL,
        (provQueryPML4)NULL,
        (provReadPhysicalMemory)AlcReadPhysicalMemory,
        (provWritePhysicalMemory)AlcWritePhysicalMemory,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)NULL
    },

    // Index 30: KDU_PROVIDER_AMD_RYZENMASTER - AMDRyzenMasterDriver (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)NULL,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE2,

        (provReadKernelVM)NULL,
        (provWriteKernelVM)NULL,

        (provVirtualToPhysical)NULL,
        (provQueryPML4)NULL,
        (provReadPhysicalMemory)RmReadPhysicalMemory,
        (provWritePhysicalMemory)RmWritePhysicalMemory,

        (provValidatePrerequisites)RmValidatePrerequisites,

        (provOpenProcess)NULL
    },

    // Index 31: KDU_PROVIDER_HR_PHYSMEM - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 32: KDU_PROVIDER_LENOVO_DD - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 33: KDU_PROVIDER_DELL_PCDOC - pcdsrvc_x64, Dell PC Doctor (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)DellRegisterDriver,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE2,

        (provReadKernelVM)NULL,
        (provWriteKernelVM)NULL,

        (provVirtualToPhysical)NULL,
        (provQueryPML4)NULL,
        (provReadPhysicalMemory)DpdReadPhysicalMemory,
        (provWritePhysicalMemory)DpdWritePhysicalMemory,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)NULL
    },

    // Index 34: KDU_PROVIDER_MSI_WINIO - WinIo, MSI Foundation Service (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)WinIoRegisterDriver,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE,

        (provReadKernelVM)WinIoReadKernelVirtualMemory,
        (provWriteKernelVM)WinIoWriteKernelVirtualMemory,

        (provVirtualToPhysical)WinIoVirtualToPhysical,
        (provQueryPML4)WinIoQueryPML4Value,
        (provReadPhysicalMemory)WinIoReadPhysicalMemory,
        (provWritePhysicalMemory)WinIoWritePhysicalMemory,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)NULL
    },

    // Index 35: KDU_PROVIDER_HP_ETDSUPPORT - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 36: KDU_PROVIDER_KEXPLORE - Kernel Explorer (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)NULL,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE,

        (provReadKernelVM)KObExpReadVirtualMemory,
        (provWriteKernelVM)KObExpWriteVirtualMemory,

        (provVirtualToPhysical)NULL,
        (provQueryPML4)NULL,
        (provReadPhysicalMemory)NULL,
        (provWritePhysicalMemory)NULL,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)NULL
    },

    // Index 37: KDU_PROVIDER_KOBJEXP - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 38: KDU_PROVIDER_KREGEXP - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 39: KDU_PROVIDER_ECHODRV - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 40: KDU_PROVIDER_NVOCLOCK - NVidia System Utility Driver (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)NULL,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)NULL,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE2,

        (provReadKernelVM)NULL,
        (provWriteKernelVM)NULL,

        (provVirtualToPhysical)NULL,
        (provQueryPML4)NULL,
        (provReadPhysicalMemory)NvoReadPhysicalMemory,
        (provWritePhysicalMemory)NvoWritePhysicalMemory,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)NULL
    },

    // Index 41: KDU_PROVIDER_BINALYZE_IREC - removed
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

    // Index 42: KDU_PROVIDER_PHYDMACC - SLIC ToolKit (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)NULL,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE2,

        (provReadKernelVM)NULL,
        (provWriteKernelVM)NULL,

        (provVirtualToPhysical)NULL,
        (provQueryPML4)NULL,
        (provReadPhysicalMemory)WRZeroReadPhysicalMemory,
        (provWritePhysicalMemory)WRZeroWritePhysicalMemory,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)NULL
    },

    // Index 43: KDU_PROVIDER_RAZER - rzpnk, Razer Synapse (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)NULL,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)NULL,
        (provMapDriver)NULL,
        (provControlDSE)NULL,

        (provReadKernelVM)NULL,
        (provWriteKernelVM)NULL,

        (provVirtualToPhysical)NULL,
        (provQueryPML4)NULL,
        (provReadPhysicalMemory)NULL,
        (provWritePhysicalMemory)NULL,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)RazerOpenProcess
    },

    // Index 44: KDU_PROVIDER_AMD_PDFWKRNL - PdFwKrnl, AMD Radeon Software (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)NULL,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE,

        (provReadKernelVM)PdFwReadVirtualMemory,
        (provWriteKernelVM)PdFwWriteVirtualMemory,

        (provVirtualToPhysical)NULL,
        (provQueryPML4)NULL,
        (provReadPhysicalMemory)NULL,
        (provWritePhysicalMemory)NULL,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)NULL
    },

    // Index 45: KDU_PROVIDER_AMD_AOD215 - AODDriver, AMD OverDrive Driver (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)NULL,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE2,

        (provReadKernelVM)NULL,
        (provWriteKernelVM)NULL,

        (provVirtualToPhysical)NULL,
        (provQueryPML4)NULL,
        (provReadPhysicalMemory)RmReadPhysicalMemory,
        (provWritePhysicalMemory)RmWritePhysicalMemory,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)NULL
    },

    // Index 46: KDU_PROVIDER_WINCOR - wnBios64, WinBios Driver (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)WinIoRegisterDriver,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE,

        (provReadKernelVM)WinIoReadKernelVirtualMemory,
        (provWriteKernelVM)WinIoWriteKernelVirtualMemory,

        (provVirtualToPhysical)WinIoVirtualToPhysical,
        (provQueryPML4)WinIoQueryPML4Value,
        (provReadPhysicalMemory)WinIoReadPhysicalMemory,
        (provWritePhysicalMemory)WinIoWritePhysicalMemory,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)NULL
    },

    // Index 47: KDU_PROVIDER_EVGA_ELEETX1 - EleetX1, EVGA ELEET X1 (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)NULL,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE2,

        (provReadKernelVM)NULL,
        (provWriteKernelVM)NULL,

        (provVirtualToPhysical)NULL,
        (provQueryPML4)NULL,
        (provReadPhysicalMemory)EvgaReadPhysicalMemory,
        (provWritePhysicalMemory)EvgaWritePhysicalMemory,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)NULL
    },

    // Index 48: KDU_PROVIDER_ASROCK2 - AxtuDrv, AsRock Extreme Tuner (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)AsrRegisterDriver,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE2,

        (provReadKernelVM)NULL,
        (provWriteKernelVM)NULL,

        (provVirtualToPhysical)NULL,
        (provQueryPML4)NULL,
        (provReadPhysicalMemory)RweReadPhysicalMemory,
        (provWritePhysicalMemory)RweWritePhysicalMemory,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)NULL
    },

    // Index 49: KDU_PROVIDER_ASROCK3 - AppShopDrv103, ASRock APP Shop (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)NULL,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE2,

        (provReadKernelVM)NULL,
        (provWriteKernelVM)NULL,

        (provVirtualToPhysical)NULL,
        (provQueryPML4)NULL,
        (provReadPhysicalMemory)AsrReadPhysicalMemory,
        (provWritePhysicalMemory)AsrWritePhysicalMemory,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)NULL
    },

    // Index 50: KDU_PROVIDER_ASROCK4 - AsrDrv107n, ASRock Motherboard Utility (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)AsrRegisterDriver,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE2,

        (provReadKernelVM)NULL,
        (provWriteKernelVM)NULL,

        (provVirtualToPhysical)NULL,
        (provQueryPML4)NULL,
        (provReadPhysicalMemory)RweReadPhysicalMemory,
        (provWritePhysicalMemory)RweWritePhysicalMemory,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)NULL
    },

    // Index 51: KDU_PROVIDER_ASROCK5 - AsrDrv107, ASRock Motherboard Utility (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)NULL,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE2,

        (provReadKernelVM)NULL,
        (provWriteKernelVM)NULL,

        (provVirtualToPhysical)NULL,
        (provQueryPML4)NULL,
        (provReadPhysicalMemory)AsrReadPhysicalMemory,
        (provWritePhysicalMemory)AsrWritePhysicalMemory,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)NULL
    },

    // Index 52: KDU_PROVIDER_INTEL_PMXDRV - PmxDrv, Intel ME Tools Driver (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)NULL,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE,

        (provReadKernelVM)PmxDrvReadKernelVirtualMemory,
        (provWriteKernelVM)PmxDrvWriteKernelVirtualMemory,

        (provVirtualToPhysical)PmxDrvVirtualToPhysical,
        (provQueryPML4)PmxDrvQueryPML4Value,
        (provReadPhysicalMemory)PmxDrvReadPhysicalMemory,
        (provWritePhysicalMemory)PmxDrvWritePhysicalMemory,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)NULL
    },

    // Index 53: KDU_PROVIDER_HWRWDRVX64 - HwRwDrv, Hardware read & write driver (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)NULL,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE2,

        (provReadKernelVM)NULL,
        (provWriteKernelVM)NULL,

        (provVirtualToPhysical)NULL,
        (provQueryPML4)NULL,
        (provReadPhysicalMemory)WRZeroReadPhysicalMemory,
        (provWritePhysicalMemory)WRZeroWritePhysicalMemory,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)NULL
    },

    // Index 54: KDU_PROVIDER_NEACSAFE64 - NeacSafe64 mini-filter driver (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)NetEaseStartVulnerableDriver,
        (provStopVulnerableDriver)NetEaseStopVulnerableDriver,

        (provRegisterDriver)NULL,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)NULL,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE,

        (provReadKernelVM)NetEaseReadVirtualMemory,
        (provWriteKernelVM)NetEaseWriteVirtualMemory,

        (provVirtualToPhysical)NULL,
        (provQueryPML4)NULL,
        (provReadPhysicalMemory)NULL,
        (provWritePhysicalMemory)NULL,

        (provValidatePrerequisites)NULL,

        (provOpenProcess)NULL
    },

    // Index 55: KDU_PROVIDER_TPUP - ThrottleStop (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)NULL,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE,

        (provReadKernelVM)TpupReadKernelVirtualMemory,
        (provWriteKernelVM)TpupWriteKernelVirtualMemory,

        (provVirtualToPhysical)NULL,
        (provQueryPML4)NULL,
        (provReadPhysicalMemory)TpupReadPhysicalMemory,
        (provWritePhysicalMemory)TpupWritePhysicalMemory,

        (provValidatePrerequisites)KDUValidatePrerequisitesForSuperfetch,

        (provOpenProcess)NULL
    },

    // Index 56: KDU_PROVIDER_TOSHIBA - TPwSav, Toshiba power saving driver (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)NULL,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE,

        (provReadKernelVM)TpwReadKernelVirtualMemory,
        (provWriteKernelVM)TpwWriteKernelVirtualMemory,

        (provVirtualToPhysical)NULL,
        (provQueryPML4)NULL,
        (provReadPhysicalMemory)TpwReadPhysicalMemory,
        (provWritePhysicalMemory)TpwWritePhysicalMemory,

        (provValidatePrerequisites)KDUValidatePrerequisitesForSuperfetch,

        (provOpenProcess)NULL
    },

    // Index 57: KDU_PROVIDER_LENOVOMSRIO - LnvMSRIO, Lenovo filter driver (KEEP)
    {
        NULL,

        (provStartVulnerableDriver)KDUProvStartVulnerableDriver,
        (provStopVulnerableDriver)KDUProvStopVulnerableDriver,

        (provRegisterDriver)NULL,
        (provUnregisterDriver)NULL,
        (provPreOpenDriver)NULL,
        (provPostOpenDriver)KDUProviderPostOpen,
        (provMapDriver)KDUMapDriver,
        (provControlDSE)KDUControlDSE,

        (provReadKernelVM)LnvMsrReadKernelVirtualMemory,
        (provWriteKernelVM)LnvMsrWriteKernelVirtualMemory,

        (provVirtualToPhysical)LnvMsrVirtualToPhysical,
        (provQueryPML4)NULL,
        (provReadPhysicalMemory)LnvMsrReadPhysicalMemory,
        (provWritePhysicalMemory)LnvMsrWritePhysicalMemory,

        (provValidatePrerequisites)KDUValidatePrerequisitesForSuperfetch,

        (provOpenProcess)NULL
    }
};
