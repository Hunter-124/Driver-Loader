/*******************************************************************************
*
*  TITLE:       DRVDB.CPP
*
*  VERSION:     1.00
*
*  DATE:        22 Apr 2026
*
*  In-mapper compilation of the KDU provider database (gProvTable / gVersion).
*
*  Historically this data shipped as a sibling DLL `drv64.dll` (project in
*  Mapper/drv-DB) and was loaded at runtime via LoadLibraryEx + GetProcAddress.
*  That forced `drv64.dll` to live on disk next to smap.exe — an extra IP
*  artifact (provider choices / shellcode version mapping) that we don't want
*  to leak. In the packed build the provider table is compiled directly into
*  smap_packed.exe; `KDUProviderLoadDB` points its `gProvTable` pointer at the
*  symbol defined here instead of calling into a dlopen'd library.
*
*  We reuse the existing tanikaze.h by redefining the symbol names it emits
*  (`gProvTable` / `gVersion`) so they don't collide with kduprov.cpp's
*  `gProvTable` pointer. This keeps the provider list single-sourced in
*  Mapper/drv-DB/tanikaze.h — that file is still the ONLY place to add or
*  remove a provider. Rebuild smap_packed.exe and the embedded table updates.
*
*******************************************************************************/

#include "global.h"

//
// Rename the symbols emitted by tanikaze.h so the embedded definitions don't
// clash with the plain `PKDU_DB gProvTable` pointer `kduprov.cpp` owns. The
// #define is scoped to this translation unit only.
//
#define gProvTable      gEmbeddedProvTable
#define gVersion        gEmbeddedVersion

//
// tanikaze.h pulls in `resource.h` relative to its own directory — that
// resolves to Mapper/drv-DB/resource.h, which defines the same IDR_* ids we
// mirror in Mapper/Mapper/resource.h. Macro redefinition warnings are
// silenced globally by global.h (pragma warning disable 4005).
//
#include "../drv-DB/tanikaze.h"

#undef gVersion
#undef gProvTable

//
// Accessors consumed by kduprov.cpp::KDUProviderLoadDB — exposed via the
// KDUGetEmbeddedProviderTable() / KDUGetEmbeddedProviderVersion() helpers
// instead of raw externs so the rename above can't leak.
//
extern "C" PKDU_DB KDUGetEmbeddedProviderTable(VOID)
{
    return &gEmbeddedProvTable;
}

extern "C" PKDU_DB_VERSION KDUGetEmbeddedProviderVersion(VOID)
{
    return &gEmbeddedVersion;
}
