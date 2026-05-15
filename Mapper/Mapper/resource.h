//{{NO_DEPENDENCIES}}
// Microsoft Visual C++ generated include file.
// Used by resource.rc
//
#define IDI_ICON1                       1001

//
// The mapper-specific RCDATA blobs (victim stubs) live in the 3000-range
// so they don't collide with the drv-DB resource IDs (folded in below,
// matching Mapper/drv-DB/resource.h's historical numbering).
//
#define IDR_TAIGEI32                    3000
#define IDR_TAIGEI64                    3001

//
// New autopilot-only resources (drv.sys embedded + future per-run metadata).
// Kept at the top of the 3100 range so they don't collide with drv-DB IDs
// and have room to grow.
//
#define IDR_EMBEDDED_DRIVER             3100

//
// ── drv-DB provider resources (merged from Mapper/drv-DB/resource.h) ─────
//
// These IDs MUST match the numeric values hard-coded in Mapper/drv-DB/
// tanikaze.h (the KDU_DB_ENTRY resource-id column). Renumbering them would
// silently break every provider-select path, so keep them as-is.
//
#define IDR_RZPNK                       104
#define IDR_ENETECHIO64B                115
#define IDR_DBUTILDRV2                  123
#define IDR_ZEMANA                      128
#define IDR_INPOUTX64                   129
#define IDR_PASSMARK_OSF                130
#define IDR_ASROCKDRV                   131
#define IDR_ALSYSIO64                   132
#define IDR_AMD_RYZENMASTER             133
#define IDR_PCDSRVC                     136
#define IDR_MSI_WINIO                   137
#define IDR_KEXPLORE                    139
#define IDR_PHYDMACC                    142
#define IDR_NVOCLOCK                    144
#define IDR_AMD_PDFWKRNL                146
#define IDR_AMD_AOD215                  147
#define IDR_WNBIOS64                    148
#define IDR_EVGA_ELEETX1                149
#define IDR_ASROCKDRV2                  150
#define IDR_ASROCKAPPSHOP103            151
#define IDR_ASROCKDRV3                  152
#define IDR_ASROCKDRV4                  153
#define IDR_PMXDRV64                    154
#define IDR_HWRWDRVX64                  155
#define IDR_NEACSAFE64                  156
#define IDR_THROTTLESTOP                157
#define IDR_TPWSAV                      158
#define IDR_LENOVOMSRIO                 159
#define IDR_DATA_DBUTILCAT              1000
#define IDR_DATA_DBUTILINF              1001
#define IDR_DATA_KMUEXE                 1002
#define IDR_DATA_KMUSIG                 1003
#define IDR_DATA_ASUSCERTSERVICE        1004
#define IDR_DATA_NEACSAFEINF            1005
#define IDR_PROCEXP1627                 2000
#define IDR_PROCEXP1702                 2001

// Next default values for new objects
// 
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE        3200
#define _APS_NEXT_COMMAND_VALUE         40001
#define _APS_NEXT_CONTROL_VALUE         1001
#define _APS_NEXT_SYMED_VALUE           101
#endif
#endif
