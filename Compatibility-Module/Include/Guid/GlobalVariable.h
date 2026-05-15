/** @file

  Stub header for EfiDSEFix.exe builds.
  Provides EFI_GLOBAL_VARIABLE GUID definition without requiring the full EDK II (VisualUefi) package.
  The real header lives in MdePkg/Include/Guid/GlobalVariable.h.

  Copyright (c) 2025, Scootware.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __GLOBAL_VARIABLE_GUID_H__
#define __GLOBAL_VARIABLE_GUID_H__

#ifdef __cplusplus
extern "C" {
#endif

///
/// The GlobalVariable GUID.
/// EFI_GLOBAL_VARIABLE  {8BE4DF61-93CA-11d2-AA0D-00E098032B8C}
///
#define EFI_GLOBAL_VARIABLE \
  { \
    0x8BE4DF61, 0x93CA, 0x11d2, { 0xAA, 0x0D, 0x00, 0xE0, 0x98, 0x03, 0x2B, 0x8C } \
  }

extern EFI_GUID gEfiGlobalVariableGuid;

#ifdef __cplusplus
}
#endif

#endif
