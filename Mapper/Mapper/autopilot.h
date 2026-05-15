/*******************************************************************************
*
*  TITLE:       AUTOPILOT.H
*
*  VERSION:     1.00
*
*  DATE:        22 Apr 2026
*
*  Autopilot entry point for the packed, loader-streamed build.
*
*  When smap_packed.exe is launched by the Scootware loader (hollowed into a
*  transient scootware.exe host, see Loader/src/security/driver_bringup.cpp)
*  it is invoked with NO command-line arguments. KDUProcessCommandLine sees
*  the empty argv, calls KDUAutopilot, and the autopilot drives exactly the
*  same `-prv 23 -map drv.sys` sequence the operator would otherwise run by
*  hand — but with drv.sys pulled straight from the embedded RCDATA resource
*  (IDR_EMBEDDED_DRIVER) instead of a disk file, and with the provider table
*  read from the compiled-in drvdb.cpp instead of a sibling drv64.dll.
*
*  The caller is expected to:
*    1. Be already manifest-elevated (admin). The loader guarantees this.
*    2. Have completed all the HVCI / CI / build-number queries in KDUMain.
*  The autopilot itself only needs (HvciEnabled, NtBuildNumber) and knows
*  which provider + shellcode version to use from bake-time constants.
*
*  Return value semantics:
*    0    success — drv.sys mapped into kernel, vulnerable driver unloaded.
*    !=0  failure. Loader's RunPE::ExpectCleanExit reads this off the child's
*         exit code and surfaces the outcome in the loader UI.
*
*******************************************************************************/

#pragma once

//
// Public-id provider to use for the autopilot map. Matches WORKING.txt
// (kdu.exe -prv 23 -map drv.sys). Translated through g_KDUPublicIdTable to
// KDU_PROVIDER_INTEL_PMXDRV (internal id 52).
//
#ifndef KDU_AUTOPILOT_PROVIDER_PUBLIC_ID
#define KDU_AUTOPILOT_PROVIDER_PUBLIC_ID 23
#endif

//
// Shellcode version used for the autopilot map. V1 is the default for every
// provider that doesn't require V2/V3 per its KDUPROV_SC_* mask.
//
#ifndef KDU_AUTOPILOT_SHELLCODE_VERSION
#define KDU_AUTOPILOT_SHELLCODE_VERSION KDU_SHELLCODE_V1
#endif

//
// DSE (Driver Signature Enforcement) state the autopilot should leave on the
// kernel globals while the map is in flight and what it should restore
// afterwards. The classic KDU flow for manually mapped drivers is:
//   -dse 0  (CiOptions = 0 — DSE fully disabled)
//   -map drv.sys
//   -dse 6  (CiOptions = 6 — DSE re-enabled in the usual locked-down state)
//
// Our packed build drives that same sequence automatically so drv.sys can
// call kernel APIs that perform runtime signature checks without tripping
// on its manually-mapped (unsigned) image.
//
#ifndef KDU_AUTOPILOT_DSE_OFF_VALUE
#define KDU_AUTOPILOT_DSE_OFF_VALUE 0
#endif

#ifndef KDU_AUTOPILOT_DSE_RESTORE_VALUE
#define KDU_AUTOPILOT_DSE_RESTORE_VALUE 6
#endif

//
// Drive the packed build's `-prv N -map drv.sys` equivalent without touching
// argv / disk. Returns a Win32-style error code (0 = success).
//
INT KDUAutopilot(
    _In_ ULONG HvciEnabled,
    _In_ ULONG NtBuildNumber);
