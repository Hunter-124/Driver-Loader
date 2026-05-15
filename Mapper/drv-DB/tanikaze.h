/*******************************************************************************
*
*  (C) COPYRIGHT AUTHORS, 2020 - 2026
*
*  TITLE:       TANIKAZE.H
*
*  VERSION:     1.46
*
*  DATE:        12 Feb 2026
*
*  Tanikaze helper dll (part of KDU project).
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/
#include <Windows.h>
#include "Shared/consts.h"
#include "Shared/ntos/ntbuilds.h"
#include "Shared/kdubase.h"
#include "resource.h"

#pragma once

KDU_DB_ENTRY gProvEntry[] = {

    // Index 11: KDU_PROVIDER_ENETECHIO64B - MSI Dragon Center
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_ENETECHIO64B,
        KDU_PROVIDER_ENETECHIO64B,
        KDU_VICTIM_DEFAULT,
        SourceBaseWinIo,
        KDUPROV_FLAGS_SIGNATURE_WHQL | KDUPROV_FLAGS_PML4_FROM_LOWSTUB,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"MSI Dragon Center",
        (LPWSTR)L"EneTechIo64",
        (LPWSTR)L"EneTechIo",
        (LPWSTR)L"Microsoft Windows Hardware Compatibility Publisher"
    },

    // Index 20: KDU_PROVIDER_DBUTILDRV2 - Dell BIOS Utility
    {
        NT_WIN10_THRESHOLD1,
        KDU_MAX_NTBUILDNUMBER,
        IDR_DBUTILDRV2,
        KDU_PROVIDER_DBUTILDRV2,
        KDU_VICTIM_DEFAULT,
        SourceBaseNone,
        KDUPROV_FLAGS_SIGNATURE_WHQL | KDUPROV_FLAGS_NO_FORCED_SD,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"CVE-2021-36276",
        (LPWSTR)L"DBUtilDrv2",
        (LPWSTR)L"DBUtil_2_5",
        (LPWSTR)L"Microsoft Windows Hardware Compatibility Publisher"
    },

    // Index 25: KDU_PROVIDER_ZEMANA - WatchDog/MalwareFox/Zemana AM
    {
        NT_WIN8_BLUE,
        KDU_MAX_NTBUILDNUMBER,
        IDR_ZEMANA,
        KDU_PROVIDER_ZEMANA,
        KDU_VICTIM_PE1702,
        SourceBaseNone,
        KDUPROV_FLAGS_SIGNATURE_WHQL | KDUPROV_FLAGS_OPENPROCESS_SUPPORTED,
        KDUPROV_SC_V4,
        (LPWSTR)L"Zemana (CVE-2021-31728, CVE-2022-42045)",
        (LPWSTR)L"ZemanaAntimalware",
        (LPWSTR)L"amsdk",
        (LPWSTR)L"WATCHDOGDEVELOPMENT.COM, LLC"
    },

    // Index 26: KDU_PROVIDER_INPOUTX64 - inputtx64
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_INPOUTX64,
        KDU_PROVIDER_INPOUTX64,
        KDU_VICTIM_DEFAULT,
        SourceBaseWinIo,
        KDUPROV_FLAGS_PML4_FROM_LOWSTUB,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"inpoutx64 Driver Version 1.2",
        (LPWSTR)L"inpoutx64",
        (LPWSTR)L"inpoutx64",
        (LPWSTR)L"Red Fox UK Limited"
    },

    // Index 27: KDU_PROVIDER_PASSMARK_OSF - PassMark OSForensics DirectIO
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_PASSMARK_OSF,
        KDU_PROVIDER_PASSMARK_OSF,
        KDU_VICTIM_DEFAULT,
        SourceBaseNone,
        KDUPROV_FLAGS_SIGNATURE_WHQL | KDUPROV_FLAGS_PML4_FROM_LOWSTUB,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"PassMark OSForensics DirectIO",
        (LPWSTR)L"DirectIo64",
        (LPWSTR)L"DIRECTIO64",
        (LPWSTR)L"PassMark Software Pty Ltd"
    },

    // Index 28: KDU_PROVIDER_ASROCK - AsrDrv106, Phantom Gaming Tuning
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_ASROCKDRV,
        KDU_PROVIDER_ASROCK,
        KDU_VICTIM_DEFAULT,
        SourceBaseRWEverything,
        KDUPROV_FLAGS_SIGNATURE_WHQL | KDUPROV_FLAGS_PHYSICAL_BRUTE_FORCE,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"ASRock IO Driver",
        (LPWSTR)L"AsrDrv106",
        (LPWSTR)L"AsrDrv106",
        (LPWSTR)L"ASROCK Incorporation"
    },

    // Index 29: KDU_PROVIDER_ALCPU - ALSysIO64, Core Temp
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_ALSYSIO64,
        KDU_PROVIDER_ALCPU,
        KDU_VICTIM_DEFAULT,
        SourceBaseNone,
        KDUPROV_FLAGS_SIGNATURE_WHQL | KDUPROV_FLAGS_PHYSICAL_BRUTE_FORCE,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"Core Temp",
        (LPWSTR)L"ALSysIO64",
        (LPWSTR)L"ALSysIO",
        (LPWSTR)L"ALCPU (Arthur Liberman)"
    },

    // Index 30: KDU_PROVIDER_AMD_RYZENMASTER - AMDRyzenMasterDriver
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_AMD_RYZENMASTER,
        KDU_PROVIDER_AMD_RYZENMASTER,
        KDU_VICTIM_DEFAULT,
        SourceBaseNone,
        KDUPROV_FLAGS_SIGNATURE_WHQL | KDUPROV_FLAGS_PHYSICAL_BRUTE_FORCE,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"AMD Ryzen Master Service Driver",
        (LPWSTR)L"AMDRyzenMasterDriver",
        (LPWSTR)L"AMDRyzenMasterDriverV20",
        (LPWSTR)L"Advanced Micro Devices Inc."
    },

    // Index 33: KDU_PROVIDER_DELL_PCDOC - pcdsrvc_x64, Dell PC Doctor
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_PCDSRVC,
        KDU_PROVIDER_DELL_PCDOC,
        KDU_VICTIM_DEFAULT,
        SourceBaseNone,
        KDUPROV_FLAGS_PHYSICAL_BRUTE_FORCE,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"PC-Doctor (CVE-2019-12280)",
        (LPWSTR)L"pcdsrvc_x64",
        (LPWSTR)L"pcdsrvc_x64",
        (LPWSTR)L"PC-Doctor, Inc."
    },

    // Index 34: KDU_PROVIDER_MSI_WINIO - WinIo, MSI Foundation Service
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_MSI_WINIO,
        KDU_PROVIDER_MSI_WINIO,
        KDU_VICTIM_DEFAULT,
        SourceBaseWinIo,
        KDUPROV_FLAGS_SIGNATURE_WHQL | KDUPROV_FLAGS_PML4_FROM_LOWSTUB,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"MSI Foundation Service",
        (LPWSTR)L"WinIo",
        (LPWSTR)L"WinIo",
        (LPWSTR)L"Microsoft Windows Hardware Compatibility Publisher"
    },

    // Index 36: KDU_PROVIDER_KEXPLORE - Kernel Explorer
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_KEXPLORE,
        KDU_PROVIDER_KEXPLORE,
        KDU_VICTIM_DEFAULT,
        SourceBaseNone,
        KDUPROV_FLAGS_PREFER_VIRTUAL,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"Kernel Explorer Driver",
        (LPWSTR)L"KExplore",
        (LPWSTR)L"KExplore",
        (LPWSTR)L"Pavel Yosifovich"
    },

    // Index 40: KDU_PROVIDER_NVOCLOCK - NVidia System Utility Driver
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_NVOCLOCK,
        KDU_PROVIDER_NVOCLOCK,
        KDU_VICTIM_PE1702,
        SourceBaseNone,
        KDUPROV_FLAGS_PHYSICAL_BRUTE_FORCE,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"NVidia System Utility Driver",
        (LPWSTR)L"nvoclock",
        (LPWSTR)L"NVR0Internal",
        (LPWSTR)L"NVIDIA Corporation"
    },

    // Index 42: KDU_PROVIDER_PHYDMACC - PhyDMACC, SLIC ToolKit
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_PHYDMACC,
        KDU_PROVIDER_PHYDMACC,
        KDU_VICTIM_PE1702,
        SourceBaseWinRing0,
        KDUPROV_FLAGS_PHYSICAL_BRUTE_FORCE,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"SLIC ToolKit",
        (LPWSTR)L"PhyDMACC",
        (LPWSTR)L"PhyDMACC_1_2_0",
        (LPWSTR)L"Suzhou Ind. Park ShiSuanKeJi Co., Ltd."
    },

    // Index 43: KDU_PROVIDER_RAZER - rzpnk, Razer Synapse
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_RZPNK,
        KDU_PROVIDER_RAZER,
        KDU_VICTIM_DEFAULT,
        SourceBaseNone,
        KDUPROV_FLAGS_NO_VICTIM | KDUPROV_FLAGS_OPENPROCESS_SUPPORTED,
        KDUPROV_SC_NONE,
        (LPWSTR)L"Razer Overlay Support driver CVE-2017-9769",
        (LPWSTR)L"rzpnk",
        (LPWSTR)L"47CD78C9-64C3-47C2-B80F-677B887CF095",
        (LPWSTR)L"Razer USA Ltd."
    },

    // Index 44: KDU_PROVIDER_AMD_PDFWKRNL - PdFwKrnl, AMD Radeon Software
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_AMD_PDFWKRNL,
        KDU_PROVIDER_AMD_PDFWKRNL,
        KDU_VICTIM_DEFAULT,
        SourceBaseNone,
        KDUPROV_FLAGS_SIGNATURE_WHQL | KDUPROV_FLAGS_PREFER_VIRTUAL,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"AMD USB-C Power Delivery Firmware Update Utility CVE-2023-20598",
        (LPWSTR)L"PdFwKrnl",
        (LPWSTR)L"PdFwKrnl",
        (LPWSTR)L"Advanced Micro Devices Inc."
    },

    // Index 45: KDU_PROVIDER_AMD_AOD215 - AODDriver, AMD OverDrive Driver
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_AMD_AOD215,
        KDU_PROVIDER_AMD_AOD215,
        KDU_VICTIM_DEFAULT,
        SourceBaseNone,
        KDUPROV_FLAGS_PHYSICAL_BRUTE_FORCE,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"AMD OverDrive Driver (same as CVE-2020-12928)",
        (LPWSTR)L"AODDriver",
        (LPWSTR)L"AODDriver",
        (LPWSTR)L"Advanced Micro Devices Inc."
    },

    // Index 46: KDU_PROVIDER_WINCOR - wnBios64, WinBios Driver
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_WNBIOS64,
        KDU_PROVIDER_WINCOR,
        KDU_VICTIM_DEFAULT,
        SourceBaseWinIo,
        KDUPROV_FLAGS_SIGNATURE_WHQL | KDUPROV_FLAGS_PML4_FROM_LOWSTUB,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"WnBios Driver",
        (LPWSTR)L"wnBios64",
        (LPWSTR)L"WNBIOS",
        (LPWSTR)L"Wincor Nixdorf International GmbH"
    },

    // Index 47: KDU_PROVIDER_EVGA_ELEETX1 - EleetX1, EVGA ELEET X1
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_EVGA_ELEETX1,
        KDU_PROVIDER_EVGA_ELEETX1,
        KDU_VICTIM_DEFAULT,
        SourceBaseNone,
        KDUPROV_FLAGS_SIGNATURE_WHQL | KDUPROV_FLAGS_PHYSICAL_BRUTE_FORCE,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"EVGA Low Level Driver",
        (LPWSTR)L"EleetX1",
        (LPWSTR)L"EleetX1",
        (LPWSTR)L"EVGA Corp."
    },

    // Index 48: KDU_PROVIDER_ASROCK2 - AxtuDrv, AsRock Extreme Tuner
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_ASROCKDRV2,
        KDU_PROVIDER_ASROCK2,
        KDU_VICTIM_DEFAULT,
        SourceBaseRWEverything,
        KDUPROV_FLAGS_PHYSICAL_BRUTE_FORCE,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"RW-Everything Read & Write Driver",
        (LPWSTR)L"AxtuDrv",
        (LPWSTR)L"AxtuDrv",
        (LPWSTR)L"ASROCK Incorporation"
    },

    // Index 49: KDU_PROVIDER_ASROCK3 - AppShopDrv103, ASRock APP Shop
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_ASROCKAPPSHOP103,
        KDU_PROVIDER_ASROCK3,
        KDU_VICTIM_DEFAULT,
        SourceBaseRWEverything,
        KDUPROV_FLAGS_PHYSICAL_BRUTE_FORCE,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"AppShopDrv103 Driver",
        (LPWSTR)L"AppShopDrv103",
        (LPWSTR)L"AppShopDrv103",
        (LPWSTR)L"ASROCK Incorporation"
    },

    // Index 50: KDU_PROVIDER_ASROCK4 - AsrDrv107n, ASRock Motherboard Utility
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_ASROCKDRV3,
        KDU_PROVIDER_ASROCK4,
        KDU_VICTIM_DEFAULT,
        SourceBaseRWEverything,
        KDUPROV_FLAGS_SIGNATURE_WHQL | KDUPROV_FLAGS_PHYSICAL_BRUTE_FORCE,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"ASRock IO Driver",
        (LPWSTR)L"AsrDrv107n",
        (LPWSTR)L"AsrDrv107n",
        (LPWSTR)L"ASROCK INC."
    },

    // Index 51: KDU_PROVIDER_ASROCK5 - AsrDrv107, ASRock Motherboard Utility
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_ASROCKDRV4,
        KDU_PROVIDER_ASROCK5,
        KDU_VICTIM_DEFAULT,
        SourceBaseRWEverything,
        KDUPROV_FLAGS_SIGNATURE_WHQL | KDUPROV_FLAGS_PHYSICAL_BRUTE_FORCE,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"ASRock IO Driver",
        (LPWSTR)L"AsrDrv107",
        (LPWSTR)L"AsrDrv107",
        (LPWSTR)L"ASROCK INC."
    },

    // Index 52: KDU_PROVIDER_INTEL_PMXDRV - PmxDrv, Intel ME Tools Driver
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_PMXDRV64,
        KDU_PROVIDER_INTEL_PMXDRV,
        KDU_VICTIM_DEFAULT,
        SourceBaseNone,
        KDUPROV_FLAGS_SIGNATURE_WHQL | KDUPROV_FLAGS_PML4_FROM_LOWSTUB,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"Intel(R) Management Engine Tools Driver",
        (LPWSTR)L"PMxDrv",
        (LPWSTR)L"Pmxdrv",
        (LPWSTR)L"Intel(R) Embedded Subsystems and IP Blocks Group"
    },

    // Index 53: KDU_PROVIDER_HWRWDRVX64 - HwRwDrv, Hardware read & write driver
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_HWRWDRVX64,
        KDU_PROVIDER_HWRWDRVX64,
        KDU_VICTIM_DEFAULT,
        SourceBaseWinRing0,
        KDUPROV_FLAGS_PHYSICAL_BRUTE_FORCE,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"Hardware read & write driver",
        (LPWSTR)L"HwRwDrv.x64",
        (LPWSTR)L"HwRwDrv",
        (LPWSTR)L"Open Source Developer, Jun Liu"
    },

    // Index 54: KDU_PROVIDER_NEACSAFE64 - NeacSafe64 mini-filter driver
    {
        NT_WIN10_THRESHOLD1,
        KDU_MAX_NTBUILDNUMBER,
        IDR_NEACSAFE64,
        KDU_PROVIDER_NEACSAFE64,
        KDU_VICTIM_DEFAULT,
        SourceBaseNone,
        KDUPROV_FLAGS_SIGNATURE_WHQL | KDUPROV_FLAGS_NO_FORCED_SD | KDUPROV_FLAGS_FS_FILTER,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"NeacSafe64 mini-filter driver (CVE-2025-45737)",
        (LPWSTR)L"NeacSafe64",
        (LPWSTR)L"OWNeacSafePort",
        (LPWSTR)L"Microsoft Windows Hardware Compatibility Publisher"
    },

    // Index 55: KDU_PROVIDER_TPUP - ThrottleStop
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_THROTTLESTOP,
        KDU_PROVIDER_TPUP,
        KDU_VICTIM_DEFAULT,
        SourceBaseNone,
        KDUPROV_FLAGS_SIGNATURE_WHQL | KDUPROV_FLAGS_USE_SUPERFETCH,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"TechPowerUp ThrottleStop (CVE-2025-7771)",
        (LPWSTR)L"ThrottleStop",
        (LPWSTR)L"ThrottleStop",
        (LPWSTR)L"TechPowerUp"
    },

    // Index 56: KDU_PROVIDER_TOSHIBA - TPwSav, Toshiba power saving driver
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_TPWSAV,
        KDU_PROVIDER_TOSHIBA,
        KDU_VICTIM_DEFAULT,
        SourceBaseNone,
        KDUPROV_FLAGS_SIGNATURE_WHQL | KDUPROV_FLAGS_USE_SUPERFETCH,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"Toshiba power saving driver for laptops",
        (LPWSTR)L"TPwSav",
        (LPWSTR)L"EBIoDispatch",
        (LPWSTR)L"Compal Electronics"
    },

    // Index 57: KDU_PROVIDER_LENOVOMSRIO - LnvMSRIO, Lenovo filter driver
    {
        KDU_MIN_NTBUILDNUMBER,
        KDU_MAX_NTBUILDNUMBER,
        IDR_LENOVOMSRIO,
        KDU_PROVIDER_LENOVOMSRIO,
        KDU_VICTIM_DEFAULT,
        SourceBaseWinRing0,
        KDUPROV_FLAGS_SIGNATURE_WHQL | KDUPROV_FLAGS_PREFER_PHYSICAL | KDUPROV_FLAGS_USE_SUPERFETCH,
        KDUPROV_SC_ALL_DEFAULT,
        (LPWSTR)L"Lenovo MSR I/O Driver (CVE-2025-8061)",
        (LPWSTR)L"LnvMSRIO",
        (LPWSTR)L"WinMsrDev",
        (LPWSTR)L"Lenovo"
    }

};

#if defined(__cplusplus)
extern "C" {
#endif

    KDU_DB gProvTable = {
        RTL_NUMBER_OF(gProvEntry),
        gProvEntry
    };

    KDU_DB_VERSION gVersion = {
        KDU_VERSION_MAJOR,
        KDU_VERSION_MINOR,
        KDU_VERSION_REVISION,
        KDU_VERSION_BUILD
    };

#ifdef __cplusplus
}
#endif
