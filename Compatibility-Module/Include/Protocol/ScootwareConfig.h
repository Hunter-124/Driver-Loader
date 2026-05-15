#pragma once
#pragma once

#ifndef EFI_SUCCESS
// Not compiling in EDK2 (e.g. from Windows Loader)
#include <stdint.h>
typedef uintptr_t UINTN;
#ifndef UINT32
typedef uint32_t UINT32;
#endif
#ifndef UINT8
typedef uint8_t UINT8;
#endif
#ifndef BOOLEAN
typedef uint8_t BOOLEAN;
#endif
#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif
#endif

#if defined(_MSC_VER) && !defined(__cplusplus)
#define SCOOTW_INLINE __inline
#else
#define SCOOTW_INLINE inline
#endif
#define SCOOTWARE_CFG_MAGIC   0x43474653u  // "SFGC"
#define SCOOTWARE_CFG_VERSION 2u           // bumped: struct extended with HWID spoof fields
#define SCOOTWARE_CFG_FLAG_FIRST_RUN  (1u << 0)  // cleared after first successful boot

#define SCOOTWARE_CFG_PATH    L"\\EFI\\Boot\\scootware.cfg"
#define SCOOTWARE_CFG_SIZE    sizeof(SCOOTWARE_EFI_CONFIG)

// ── Offsets of HWID spoof fields within SCOOTWARE_EFI_CONFIG ──────────────
#define SCOOTWARE_OFF_HWID_CAPTURED           0x3AC
#define SCOOTWARE_OFF_HWID_SPOOF_UUID         0x3B0
#define SCOOTWARE_OFF_HWID_SPOOF_SYS_SERIAL   0x3C0
#define SCOOTWARE_OFF_HWID_SPOOF_BB_SERIAL    0x440
#define SCOOTWARE_OFF_HWID_SPOOF_CPU_SERIAL   0x4C0
#define SCOOTWARE_OFF_HWID_APPLY              0x540
#define SCOOTWARE_OFF_CHECKSUM                0x544

#pragma pack(push, 1)
typedef struct {
    UINT32  Magic;            // 0x000  SCOOTWARE_CFG_MAGIC
    UINT32  Version;          // 0x004  SCOOTWARE_CFG_VERSION
    UINT32  Flags;            // 0x008  SCOOTWARE_CFG_FLAG_FIRST_RUN | ...
    UINT32  DseBypassMethod;  // 0x00C  EFIGUARD_DSE_BYPASS_TYPE (0=None 1=AtBoot 2=SetVarHook 3=Auto)
    BOOLEAN WaitForKeyPress;  // 0x010
    UINT8   Reserved[3];      // 0x011
    UINT32  OsMajorVersion;   // 0x014  written by Windows loader (RtlGetVersion)
    UINT32  OsBuildNumber;    // 0x018  written by Windows loader
    UINT16  BootmgfwPath[256];// 0x01C  exact path to bootmgfw.efi on ESP (512 bytes)
    UINT32  LegacyCfgChecksum;// 0x21C  legacy field; now subsumed by full Checksum at 0x544
    UINT8   Reserved2[0x18C]; // 0x220  padding to reach HWID region at 0x3AC

    // ── EFI HWID Spoof fields ─────────────────────────────────────────────
    UINT8   HwIdCaptured;              // 0x3AC  R/O: module sets 1 after capturing live HWID
    UINT8   HwIdPad[3];               // 0x3AD  alignment padding
    UINT8   HwIdSpoofUUID[16];        // 0x3B0  UUID bytes to write (Type 1 UUID)
    char    HwIdSpoofSystemSerial[128];    // 0x3C0  Type 1 system serial (null-terminated)
    char    HwIdSpoofBaseboardSerial[128]; // 0x440  Type 2 baseboard serial (null-terminated)
    char    HwIdSpoofProcessorSerial[128]; // 0x4C0  Type 4 processor serial (null-terminated)
    UINT8   HwIdApply;                // 0x540  Write 1 to apply spoof on next boot; module clears
    UINT8   HwIdApplyPad[3];          // 0x541  alignment padding

    UINT32  Checksum;                 // 0x544  FNV-1a over bytes 0x000..0x543
} SCOOTWARE_EFI_CONFIG;
#pragma pack(pop)

// Helper: computes FNV-1a checksum over all struct bytes preceding the Checksum field.
// Covers 0x000..0x543 (i.e. the full struct including all HWID fields).
static SCOOTW_INLINE UINT32 ScootwConfigChecksum(const SCOOTWARE_EFI_CONFIG *Cfg) {
    const UINT8 *Bytes = (const UINT8 *)Cfg;
    UINTN End = (UINTN)&Cfg->Checksum - (UINTN)Cfg;
    UINT32 Hash = 0x811c9dc5u; // FNV-1a offset basis
    for (UINTN i = 0; i < End; ++i) {
        Hash ^= Bytes[i];
        Hash *= 0x01000193u;   // FNV prime
    }
    return Hash;
}

// Helper: verifies Magic, Version, and full-struct Checksum.
static SCOOTW_INLINE BOOLEAN ScootwConfigIsValid(const SCOOTWARE_EFI_CONFIG *Cfg) {
    if (Cfg->Magic != SCOOTWARE_CFG_MAGIC || Cfg->Version != SCOOTWARE_CFG_VERSION) {
        return FALSE;
    }
    if (Cfg->Checksum != ScootwConfigChecksum(Cfg)) {
        return FALSE;
    }
    return TRUE;
}
