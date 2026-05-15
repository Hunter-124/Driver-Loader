/*******************************************************************************
*
*  TITLE:       AUTOPILOT.CPP
*
*  VERSION:     1.00
*
*  DATE:        22 Apr 2026
*
*  Autopilot implementation — see autopilot.h for the high-level contract.
*
*  This file is the packed-build equivalent of KDUProcessDrvMapSwitch: it
*  walks the same KDU provider pipeline, but sources the target driver from
*  an in-memory resource instead of a file path so drv.sys never touches
*  disk. The rest of the provider lifecycle (vulnerable-driver service
*  install / exploit / unload) is unchanged — those are public wormhole
*  drivers staged from their own embedded RCDATA blobs by the existing
*  KDUProvLoadVulnerableDriver path.
*
*******************************************************************************/

#include "global.h"
#include "autopilot.h"

#include <stdarg.h>

//
// File-based diagnostic channel for the autopilot.
//
// Rationale: when smap_packed.exe is invoked via the loader's RunPE
// hollowing path, its stdout/stderr is wired to pipes the loader creates
// but never actually pumps (see Loader\src\memory\runpe.cpp). Every
// supPrintfEvent / printf_s line the mapper emits therefore goes into a
// pipe-shaped black hole and nothing survives process exit.
//
// For the zero-argv autopilot flow that means a single consolidated
// non-zero exit code (historically always 0x1F / ERROR_GEN_FAILURE)
// is the ONLY signal that reaches the loader diag, and we can't tell
// whether the failure was "resource missing" vs "map returned FALSE"
// vs "PE layout build rejected the driver image" from the outside.
//
// This writes a tiny timestamped trace into %TEMP%\scootware-mapper-
// diag.log using direct WinAPI — no CRT, no printf state, so it stays
// safe even if whatever broke us also broke the CRT. Best-effort on
// every call: if GetTempPathA or CreateFileA fails we silently skip,
// because losing a diag line should never make a working map fail.
//
static VOID KDUAutopilotDiagWrite(_In_ LPCSTR Format, ...)
{
    CHAR  tempDir[MAX_PATH] = { 0 };
    CHAR  logPath[MAX_PATH] = { 0 };
    CHAR  line[1024]        = { 0 };

    DWORD tlen = GetTempPathA(MAX_PATH, tempDir);
    if (tlen == 0 || tlen >= MAX_PATH) {
        return;
    }

    if (FAILED(StringCchPrintfA(logPath, MAX_PATH,
        "%sscootware-mapper-diag.log", tempDir))) {
        return;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);

    CHAR header[96] = { 0 };
    StringCchPrintfA(header, sizeof(header),
        "%04u-%02u-%02u %02u:%02u:%02u.%03u  mapper-pid=%lu  ",
        (unsigned)st.wYear, (unsigned)st.wMonth, (unsigned)st.wDay,
        (unsigned)st.wHour, (unsigned)st.wMinute, (unsigned)st.wSecond,
        (unsigned)st.wMilliseconds,
        (unsigned long)GetCurrentProcessId());

    va_list args;
    va_start(args, Format);
    CHAR body[896] = { 0 };
    StringCchVPrintfA(body, sizeof(body), Format, args);
    va_end(args);

    StringCchPrintfA(line, sizeof(line), "%s%s\r\n", header, body);

    HANDLE h = CreateFileA(logPath, FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD written = 0;
    WriteFile(h, line, (DWORD)strlen(line), &written, NULL);
    CloseHandle(h);
}

//
// Unique exit codes for each autopilot failure path.
//
// The loader's driver_bringup.cpp reads these off the mapper's process
// exit code and surfaces them in scootware-diag.log. Keeping every path
// on its own number is what makes "the mapper died with 0x...." a
// self-describing diagnostic instead of a binary-search exercise.
//
// These are intentionally NOT Win32 error codes (they all sit above
// 0x5A00) so nothing downstream can confuse them with e.g.
// ERROR_GEN_FAILURE (0x1F) or ERROR_ACCESS_DENIED (0x05) that used to
// be the catch-all returns from this file.
//
#define KDU_AP_EXIT_UNKNOWN                     0x5A00  // default, should never leak
#define KDU_AP_EXIT_INVALID_PROVIDER_ID         0x5A01
#define KDU_AP_EXIT_SELF_MODULE_HANDLE_NULL     0x5A02
#define KDU_AP_EXIT_PE_HEADER_INVALID           0x5A03
#define KDU_AP_EXIT_RESOURCE_DIR_MISSING        0x5A04
#define KDU_AP_EXIT_LDR_FIND_RESOURCE_FAILED    0x5A05
#define KDU_AP_EXIT_LDR_ACCESS_RESOURCE_FAILED  0x5A06
#define KDU_AP_EXIT_RESOURCE_DECODE_FAILED      0x5A07
#define KDU_AP_EXIT_PE_LAYOUT_FAILED            0x5A08
#define KDU_AP_EXIT_PROVIDER_CREATE_FAILED      0x5A09
#define KDU_AP_EXIT_MAP_DRIVER_FAILED           0x5A0A

/*
* KDUAutopilotLoadEmbeddedDriver
*
* Purpose:
*
* Decode the IDR_EMBEDDED_DRIVER RCDATA resource (pcomp-encoded drv.sys) into
* a plaintext heap buffer. Caller owns the returned pointer and MUST release
* it with supHeapFree + RtlSecureZeroMemory.
*
* Checksum verification is off: drv.sys is a hand-built driver and its PE
* OptionalHeader.CheckSum is frequently stamped 0 by link.exe. The downstream
* shellcode pipeline doesn't rely on the CheckSum field either, so there's
* nothing to gain from enforcing it here.
*
* FailureCode (out) receives one of KDU_AP_EXIT_* describing *which* step
* of the resource-load pipeline tripped. NULL-safe.
*
*/
static PBYTE KDUAutopilotLoadEmbeddedDriver(
    _Out_ PULONG DriverSize,
    _Out_opt_ PINT FailureCode
)
{
    PBYTE   drvBuffer = NULL;
    ULONG   drvSize   = 0;
    HMODULE selfBase  = GetModuleHandleW(NULL);

    *DriverSize = 0;
    if (FailureCode) {
        *FailureCode = KDU_AP_EXIT_UNKNOWN;
    }

    KDUAutopilotDiagWrite("[load] entered; selfBase=0x%p",
        (PVOID)selfBase);

    if (selfBase == NULL) {
        KDUAutopilotDiagWrite(
            "[load] FAILED: GetModuleHandleW(NULL) returned NULL — PEB "
            "ImageBaseAddress not patched by the hollowing loader?");
        supPrintfEvent(kduEventError,
            "[!] Autopilot: could not resolve self module handle\r\n");
        if (FailureCode) *FailureCode = KDU_AP_EXIT_SELF_MODULE_HANDLE_NULL;
        return NULL;
    }

    //
    // Sanity-check the PE headers at selfBase BEFORE calling the NTDLL
    // resource helpers. If the hollowing step mis-mapped the image or
    // truncated the headers, LdrFindResource_U will return
    // STATUS_RESOURCE_DATA_NOT_FOUND and we'd have no idea why — these
    // logs tell us immediately.
    //
    PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)selfBase;
    if (pDos->e_magic != IMAGE_DOS_SIGNATURE) {
        KDUAutopilotDiagWrite(
            "[load] FAILED: selfBase has bad DOS magic 0x%04X (expected "
            "0x5A4D) — image not correctly mapped",
            (unsigned)pDos->e_magic);
        if (FailureCode) *FailureCode = KDU_AP_EXIT_PE_HEADER_INVALID;
        return NULL;
    }

    PIMAGE_NT_HEADERS64 pNt =
        (PIMAGE_NT_HEADERS64)((PBYTE)selfBase + pDos->e_lfanew);
    if (pNt->Signature != IMAGE_NT_SIGNATURE) {
        KDUAutopilotDiagWrite(
            "[load] FAILED: selfBase NT signature 0x%08X (expected "
            "0x00004550) at e_lfanew=0x%X",
            (unsigned)pNt->Signature, (unsigned)pDos->e_lfanew);
        if (FailureCode) *FailureCode = KDU_AP_EXIT_PE_HEADER_INVALID;
        return NULL;
    }

    ULONG rsrcRva  = pNt->OptionalHeader.DataDirectory
        [IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress;
    ULONG rsrcSize = pNt->OptionalHeader.DataDirectory
        [IMAGE_DIRECTORY_ENTRY_RESOURCE].Size;

    KDUAutopilotDiagWrite(
        "[load] PE OK: SizeOfImage=0x%X ImageBase=0x%llX "
        "ResourceDir RVA=0x%X size=%lu",
        (unsigned)pNt->OptionalHeader.SizeOfImage,
        (ULONGLONG)pNt->OptionalHeader.ImageBase,
        (unsigned)rsrcRva, (unsigned long)rsrcSize);

    if (rsrcRva == 0 || rsrcSize == 0) {
        KDUAutopilotDiagWrite(
            "[load] FAILED: resource data directory empty — rc.exe never "
            "embedded IDR_EMBEDDED_DRIVER (res\\drv.bin missing at build "
            "time, or wrong smap_packed.exe uploaded)");
        if (FailureCode) *FailureCode = KDU_AP_EXIT_RESOURCE_DIR_MISSING;
        return NULL;
    }

    //
    // Call LdrFindResource_U / LdrAccessResource directly so we can
    // capture the exact NTSTATUS each step produces. The wrapper in
    // ntsup.c swallows the status on failure, which is the main reason
    // the pre-autopilot builds gave us no diagnostic traction.
    //
    ULONG_PTR idPath[3];
    idPath[0] = (ULONG_PTR)RT_RCDATA;
    idPath[1] = (ULONG_PTR)IDR_EMBEDDED_DRIVER;
    idPath[2] = 0;

    IMAGE_RESOURCE_DATA_ENTRY* dataEntry = NULL;
    NTSTATUS ntStatus = LdrFindResource_U(
        selfBase, (ULONG_PTR*)idPath, 3, &dataEntry);
    KDUAutopilotDiagWrite(
        "[load] LdrFindResource_U(id=%u) status=0x%08X entry=0x%p",
        (unsigned)IDR_EMBEDDED_DRIVER, (unsigned)ntStatus,
        (PVOID)dataEntry);

    if (!NT_SUCCESS(ntStatus) || dataEntry == NULL) {
        KDUAutopilotDiagWrite(
            "[load] FAILED: LdrFindResource_U; the resource tree does "
            "not expose an RCDATA id %u node. Verify BIN\\smap_packed.exe "
            "contains the embedded driver via:\r\n"
            "        powershell -f check_rsrc.ps1",
            (unsigned)IDR_EMBEDDED_DRIVER);
        if (FailureCode) *FailureCode = KDU_AP_EXIT_LDR_FIND_RESOURCE_FAILED;
        return NULL;
    }

    PVOID rawData  = NULL;
    ULONG rawSize  = 0;
    ntStatus = LdrAccessResource(
        selfBase, dataEntry, &rawData, &rawSize);
    KDUAutopilotDiagWrite(
        "[load] LdrAccessResource status=0x%08X data=0x%p size=%lu",
        (unsigned)ntStatus, rawData, (unsigned long)rawSize);

    if (!NT_SUCCESS(ntStatus) || rawData == NULL || rawSize == 0) {
        KDUAutopilotDiagWrite(
            "[load] FAILED: LdrAccessResource returned no data");
        if (FailureCode) *FailureCode = KDU_AP_EXIT_LDR_ACCESS_RESOURCE_FAILED;
        return NULL;
    }

    //
    // Resource data is addressable — now run it through the
    // XOR + MSDelta pipeline. KDULoadResource -> KDUDecompressResource
    // uses supHeapAlloc for the temp + result buffers and calls
    // ApplyDeltaB from msdelta.dll. If msdelta.dll failed to load into
    // the child (EnsureDllInChild path in the loader), ApplyDeltaB's
    // IAT slot will be bogus and we'll face-plant here.
    //
    drvBuffer = (PBYTE)KDULoadResource(
        IDR_EMBEDDED_DRIVER,
        selfBase,
        &drvSize,
        PROVIDER_RES_KEY,
        FALSE);

    KDUAutopilotDiagWrite(
        "[load] KDULoadResource returned buffer=0x%p decompressedSize=%lu",
        (PVOID)drvBuffer, (unsigned long)drvSize);

    if (drvBuffer == NULL || drvSize == 0) {
        KDUAutopilotDiagWrite(
            "[load] FAILED: KDULoadResource decode returned NULL/zero — "
            "ApplyDeltaB most likely failed. Check that msdelta.dll is "
            "available in the hollowed child (EnsureDllInChild must have "
            "loaded it before IAT resolution)");
        supPrintfEvent(kduEventError,
            "[!] Autopilot: embedded drv.sys resource missing or failed to "
            "decode (RCDATA id %u). Did the pcomp pre-build step run and drop "
            "res\\drv.bin before rc.exe?\r\n",
            (unsigned)IDR_EMBEDDED_DRIVER);
        if (FailureCode) *FailureCode = KDU_AP_EXIT_RESOURCE_DECODE_FAILED;
        return NULL;
    }

    *DriverSize = drvSize;
    if (FailureCode) {
        *FailureCode = 0;
    }
    return drvBuffer;
}

/*
* KDUAutopilotControlDSE
*
* Purpose:
*
* Flip the kernel's CI g_CiOptions variable to the supplied value via the
* same provider + primitive set `-dse <value>` would. Used to disable DSE
* around the manual map (DSEValue = 0) and restore it afterwards
* (DSEValue = 6).
*
* This mirrors KDUProcessDSEFixSwitch in main.cpp almost verbatim — it is
* intentionally duplicated instead of called directly so the autopilot
* keeps ownership of its diagnostics (every log line is tagged
* "[dse]"/"[autopilot]") and so a future policy split (e.g. use a
* different provider for DSE than for map) is a one-file edit.
*
* Returns TRUE iff the kernel variable was actually flipped; FALSE if we
* couldn't locate it or the provider lacks ControlDSE support. A FALSE
* return on the "disable" call is recoverable — the caller may still
* attempt the map (it will just run with DSE live, which is fine for
* providers whose MapDriver path is already DSE-agnostic). A FALSE return
* on the "restore" call gets logged loudly because leaving g_CiOptions=0
* after exit is a visible system-state change.
*
*/
static BOOL KDUAutopilotControlDSE(
    _In_ ULONG HvciEnabled,
    _In_ ULONG NtBuildNumber,
    _In_ ULONG ProviderId,
    _In_ ULONG DSEValue,
    _In_ LPCSTR StepLabel
)
{
    PKDU_CONTEXT  dseContext;
    ULONG_PTR     ciVarAddress;
    BOOL          bResult = FALSE;

    dseContext = KDUProviderCreate(
        ProviderId,
        HvciEnabled,
        NtBuildNumber,
        KDU_SHELLCODE_NONE,
        ActionTypeDSECorruption);

    if (dseContext == NULL) {
        supPrintfEvent(kduEventError,
            "[!] Autopilot [dse/%s]: KDUProviderCreate failed (provider %lu)\r\n",
            StepLabel, ProviderId);
        return FALSE;
    }

    do {

        if (dseContext->Provider->Callbacks.ControlDSE == NULL) {
            supPrintfEvent(kduEventError,
                "[!] Autopilot [dse/%s]: provider %lu has no ControlDSE "
                "callback — skipping DSE toggle\r\n",
                StepLabel, ProviderId);
            break;
        }

        ciVarAddress = KDUQueryCodeIntegrityVariableSymbol(NtBuildNumber);
        if (ciVarAddress == 0) {
            ciVarAddress = KDUQueryCodeIntegrityVariableAddress(NtBuildNumber);
        }

        if (ciVarAddress == 0) {
            supPrintfEvent(kduEventError,
                "[!] Autopilot [dse/%s]: could not resolve g_CiOptions "
                "address on build %lu\r\n",
                StepLabel, NtBuildNumber);
            break;
        }

        printf_s("[*] Autopilot [dse/%s]: writing 0x%lX to g_CiOptions @ 0x%p\r\n",
            StepLabel, DSEValue, (PVOID)ciVarAddress);

        bResult = dseContext->Provider->Callbacks.ControlDSE(
            dseContext,
            DSEValue,
            ciVarAddress) != 0;

        if (!bResult) {
            supPrintfEvent(kduEventError,
                "[!] Autopilot [dse/%s]: ControlDSE callback returned FALSE\r\n",
                StepLabel);
        }

    } while (FALSE);

    KDUProviderRelease(dseContext);
    return bResult;
}

/*
* KDUAutopilotMapOnce
*
* Purpose:
*
* Run one end-to-end map attempt using the autopilot's baked-in provider +
* shellcode. Factored out of KDUAutopilot so we can keep the outer function
* focused on lifecycle (provider release, buffer wipe, error reporting).
*
* DSE contract:
*   - The caller is responsible for bracketing this with DSE-off / DSE-on
*     calls. That separation exists so the outer KDUAutopilot can still
*     restore DSE even when KDUAutopilotMapOnce early-returns on a provider
*     creation failure.
*
*/
static INT KDUAutopilotMapOnce(
    _In_ ULONG HvciEnabled,
    _In_ ULONG NtBuildNumber,
    _In_ ULONG ProviderId,
    _In_ PVOID MappedImage
)
{
    printf_s("[*] Autopilot: provider internal=%lu, shellcode V%lu\r\n",
        ProviderId,
        (ULONG)KDU_AUTOPILOT_SHELLCODE_VERSION);

    PKDU_CONTEXT provContext = KDUProviderCreate(
        ProviderId,
        HvciEnabled,
        NtBuildNumber,
        KDU_AUTOPILOT_SHELLCODE_VERSION,
        ActionTypeMapDriver);

    if (provContext == NULL) {
        //
        // KDUProviderCreate already logs the specific failure reason (HVCI
        // mismatch, unsupported build, etc.). We just return non-zero so
        // the loader's ExpectCleanExit path reports a mapper failure.
        //
        KDUAutopilotDiagWrite(
            "[map] FAILED: KDUProviderCreate(prov=%lu) returned NULL — "
            "see the kdu/provider logs above for the specific rejection "
            "reason (HVCI mismatch, unsupported build, service install "
            "failure, vulnerable driver blocked by MSFT block list, ...)",
            (unsigned long)ProviderId);
        supPrintfEvent(kduEventError,
            "[!] Autopilot: KDUProviderCreate failed for provider %lu\r\n",
            ProviderId);
        return KDU_AP_EXIT_PROVIDER_CREATE_FAILED;
    }

    KDUAutopilotDiagWrite(
        "[map] KDUProviderCreate OK (prov=%lu, shellcode=V%lu) — "
        "calling MapDriver", (unsigned long)ProviderId,
        (unsigned long)KDU_AUTOPILOT_SHELLCODE_VERSION);

    INT retVal = 0;
    BOOL mapOk = provContext->Provider->Callbacks.MapDriver(
        provContext, MappedImage);
    if (mapOk) {
        KDUAutopilotDiagWrite("[map] MapDriver returned TRUE — success");
        retVal = 0;
    } else {
        KDUAutopilotDiagWrite(
            "[map] FAILED: MapDriver callback returned FALSE — provider "
            "loaded fine but the shellcode-driven kernel map did not land. "
            "Typical causes: (1) vulnerable helper driver blocked after "
            "install, (2) shellcode version %lu unsupported by provider, "
            "(3) drv.sys image rejected by the kernel-side validator.",
            (unsigned long)KDU_AUTOPILOT_SHELLCODE_VERSION);
        supPrintfEvent(kduEventError,
            "[!] Autopilot: MapDriver callback returned FALSE\r\n");
        retVal = KDU_AP_EXIT_MAP_DRIVER_FAILED;
    }

    //
    // Release unconditionally — provider release tears down the vulnerable
    // helper driver + closes any device handle, regardless of whether the
    // inner MapDriver succeeded. Skipping this on failure would leak a
    // partially-initialised vuln driver registration.
    //
    KDUProviderRelease(provContext);

    return retVal;
}

INT KDUAutopilot(
    _In_ ULONG HvciEnabled,
    _In_ ULONG NtBuildNumber
)
{
    //
    // Default retVal is KDU_AP_EXIT_UNKNOWN (0x5A00), not ERROR_GEN_FAILURE
    // (0x1F), so a "retVal never got overwritten" bug is impossible to
    // confuse with a real downstream failure code. Every break path below
    // sets a distinct KDU_AP_EXIT_* before leaving the do/while(FALSE).
    //
    INT     retVal      = KDU_AP_EXIT_UNKNOWN;
    PBYTE   drvBuffer   = NULL;
    ULONG   drvSize     = 0;
    PVOID   mappedImg   = NULL;
    BOOL    dseDisabled = FALSE;
    INT     loadFailure = 0;

    FUNCTION_ENTER_MSG(__FUNCTION__);

    KDUAutopilotDiagWrite(
        "[autopilot] entered: HVCI=%lu NtBuild=%lu cmdline='%ws'",
        (unsigned long)HvciEnabled, (unsigned long)NtBuildNumber,
        GetCommandLineW() ? GetCommandLineW() : L"(null)");

    printf_s("[*] Autopilot: starting zero-argv kernel driver mapping\r\n");

    //
    // Validate + resolve the baked provider up front so we can reuse the
    // internal id for BOTH the DSE toggle and the map. Keeping them on the
    // same provider is important: creating two different provider contexts
    // would install two different vulnerable drivers as services, doubling
    // our kernel-state footprint and our unload surface.
    //
    ULONG publicId   = KDU_AUTOPILOT_PROVIDER_PUBLIC_ID;
    ULONG providerId = KDUPublicIdToInternal(publicId);

    if (!KDUIsValidPublicId(publicId)) {
        KDUAutopilotDiagWrite(
            "[autopilot] FAILED: baked provider public id %lu out of range",
            (unsigned long)publicId);
        supPrintfEvent(kduEventError,
            "[!] Autopilot: baked provider public id %lu is out of range "
            "(public id table changed without a matching rebuild?)\r\n",
            publicId);
        FUNCTION_LEAVE_MSG(__FUNCTION__);
        return KDU_AP_EXIT_INVALID_PROVIDER_ID;
    }

    KDUAutopilotDiagWrite(
        "[autopilot] provider public=%lu -> internal=%lu",
        (unsigned long)publicId, (unsigned long)providerId);
    printf_s("[*] Autopilot: provider public=%lu -> internal=%lu\r\n",
        publicId, providerId);

    do {
        //
        // 1. Decode the embedded drv.sys into a plaintext buffer.
        //
        KDUAutopilotDiagWrite(
            "[autopilot] step 1/4: loading embedded driver (RCDATA id=%u, "
            "xor key=0x%08X)", (unsigned)IDR_EMBEDDED_DRIVER,
            (unsigned)PROVIDER_RES_KEY);

        drvBuffer = KDUAutopilotLoadEmbeddedDriver(&drvSize, &loadFailure);
        if (drvBuffer == NULL) {
            retVal = (loadFailure != 0)
                ? loadFailure
                : KDU_AP_EXIT_RESOURCE_DECODE_FAILED;
            KDUAutopilotDiagWrite(
                "[autopilot] step 1 FAILED -> exiting with 0x%X", retVal);
            break;
        }

        KDUAutopilotDiagWrite(
            "[autopilot] step 1 OK: decoded drv.sys (%lu bytes)", drvSize);
        printf_s("[+] Autopilot: decoded embedded driver, %lu bytes\r\n",
            drvSize);

        //
        // 2. Build the mapped-image layout in user RAM (no file on disk).
        //
        KDUAutopilotDiagWrite("[autopilot] step 2/4: supLoadFileForMapping"
            "FromMemory on %lu-byte driver image", drvSize);

        NTSTATUS mapStatus = supLoadFileForMappingFromMemory(
            drvBuffer,
            drvSize,
            &mappedImg);

        if (!NT_SUCCESS(mapStatus) || mappedImg == NULL) {
            KDUAutopilotDiagWrite(
                "[autopilot] step 2 FAILED: supLoadFileForMapping"
                "FromMemory status=0x%08X mappedImg=0x%p",
                (unsigned)mapStatus, mappedImg);
            supShowHardError(
                "[!] Autopilot: in-memory PE layout build failed",
                mapStatus);
            retVal = KDU_AP_EXIT_PE_LAYOUT_FAILED;
            break;
        }

        KDUAutopilotDiagWrite(
            "[autopilot] step 2 OK: driver image at 0x%p", mappedImg);
        printf_s("[+] Autopilot: driver image prepared at 0x%p (no disk I/O)\r\n",
            mappedImg);

        //
        // 3. DSE off — mirrors `kdu.exe -prv N -dse 0` before the map.
        //
        // The shellcode-based MapDriver path doesn't itself need DSE
        // disabled (it writes straight into kernel memory), but drv.sys
        // can call kernel APIs at DriverEntry and beyond that perform
        // runtime signature checks (any path that goes through
        // CiValidateImageHeader / SeValidateImageData). Those would fail
        // against a manually-mapped, unsigned image and either crash the
        // system or cause drv.sys to early-return into a broken state.
        //
        // We intentionally continue even if ControlDSE fails: the classic
        // KDU shellcode map works without DSE manipulation on most
        // builds, and aborting here would leave the user with a strictly
        // worse outcome than letting the map try. The loud log line from
        // KDUAutopilotControlDSE is the trigger for triage if anything
        // downstream trips on CI.
        //
        KDUAutopilotDiagWrite(
            "[autopilot] step 3/4: DSE off (CiOptions=%lu)",
            (unsigned long)KDU_AUTOPILOT_DSE_OFF_VALUE);
        printf_s("[*] Autopilot: disabling DSE before map (CiOptions = %lu)\r\n",
            (ULONG)KDU_AUTOPILOT_DSE_OFF_VALUE);

        dseDisabled = KDUAutopilotControlDSE(
            HvciEnabled,
            NtBuildNumber,
            providerId,
            KDU_AUTOPILOT_DSE_OFF_VALUE,
            "disable");

        KDUAutopilotDiagWrite(
            "[autopilot] step 3 result: dseDisabled=%s — %s",
            dseDisabled ? "TRUE" : "FALSE",
            dseDisabled ? "proceeding with DSE off"
                        : "continuing without DSE toggle");

        if (dseDisabled) {
            printf_s("[+] Autopilot: DSE disabled, proceeding to map\r\n");
        } else {
            supPrintfEvent(kduEventError,
                "[!] Autopilot: DSE disable failed — continuing, but "
                "drv.sys may trip on runtime CI checks\r\n");
        }

        //
        // 4. Run the provider + shellcode map.
        //
        KDUAutopilotDiagWrite(
            "[autopilot] step 4/4: KDUAutopilotMapOnce (prov=%lu, sc=V%lu)",
            (unsigned long)providerId,
            (unsigned long)KDU_AUTOPILOT_SHELLCODE_VERSION);

        retVal = KDUAutopilotMapOnce(
            HvciEnabled,
            NtBuildNumber,
            providerId,
            mappedImg);

        KDUAutopilotDiagWrite(
            "[autopilot] step 4 returned 0x%X (%s)", retVal,
            (retVal == 0) ? "SUCCESS" : "FAILURE");

    } while (FALSE);

    //
    // 5. DSE restore — mirrors `kdu.exe -prv N -dse 6` after the map.
    //
    //    Runs on EVERY exit path (success AND failure) as long as we
    //    previously observed a successful disable; otherwise g_CiOptions
    //    would stay at 0 after the mapper exits, which is a highly visible
    //    system-state change (system-wide unsigned driver load allowed,
    //    tamper-triggers in HVCI / anti-cheat policy engines). The classic
    //    `kdu.exe -dse 6` sequence is what we're replicating here.
    //
    //    Kept outside the do/while(FALSE) `break` body so a break from any
    //    earlier step still restores DSE. We use our own provider context
    //    (KDUAutopilotControlDSE creates a fresh one) instead of reusing
    //    the map context because the map context has already been released
    //    by KDUAutopilotMapOnce.
    //
    if (dseDisabled) {
        printf_s("[*] Autopilot: restoring DSE after map (CiOptions = %lu)\r\n",
            (ULONG)KDU_AUTOPILOT_DSE_RESTORE_VALUE);

        BOOL dseRestored = KDUAutopilotControlDSE(
            HvciEnabled,
            NtBuildNumber,
            providerId,
            KDU_AUTOPILOT_DSE_RESTORE_VALUE,
            "restore");

        if (!dseRestored) {
            //
            // Don't override retVal with this failure if the map already
            // succeeded — the driver IS live and usable, and forcing a
            // loader-visible failure here would cascade into tearing the
            // whole session down for a post-map cleanup miss. Log it
            // loudly and let the operator decide. If the map ALREADY
            // failed, retVal stays non-zero, which is the correct signal
            // to the loader regardless of what DSE ended up at.
            //
            supPrintfEvent(kduEventError,
                "[!] Autopilot: DSE restore FAILED — g_CiOptions left at "
                "0x%lX. Reboot to return to normal DSE state.\r\n",
                (ULONG)KDU_AUTOPILOT_DSE_OFF_VALUE);
        } else {
            printf_s("[+] Autopilot: DSE restored to 0x%lX\r\n",
                (ULONG)KDU_AUTOPILOT_DSE_RESTORE_VALUE);
        }
    }

    //
    // 6. Secure-zero + release both buffers regardless of outcome. Even on
    //    success the hollowed host process keeps running for the cheat's
    //    lifetime (loader doesn't kill it until after the map completes,
    //    see driver_bringup.cpp's waitTimeoutMs=30000 and the cheat spawn),
    //    and we don't want a RAM dump mid-session to surface plaintext
    //    driver bytes.
    //
    if (mappedImg) {
        supUnmapMemoryMappedImage(mappedImg);
        mappedImg = NULL;
    }

    if (drvBuffer) {
        RtlSecureZeroMemory(drvBuffer, drvSize);
        supHeapFree(drvBuffer);
        drvBuffer = NULL;
        drvSize   = 0;
    }

    if (retVal == 0) {
        KDUAutopilotDiagWrite(
            "[autopilot] SUCCESS — drv.sys mapped, exiting cleanly");
        supPrintfEvent(kduEventInformation,
            "[+] Autopilot: success — drv.sys mapped, exiting cleanly\r\n");
    } else {
        KDUAutopilotDiagWrite(
            "[autopilot] FAILURE — returning exit code 0x%X (%d). See "
            "earlier [load]/[map]/[autopilot] lines in this file for the "
            "originating step.", retVal, retVal);
        supPrintfEvent(kduEventError,
            "[!] Autopilot: failure, returning 0x%X\r\n", retVal);
    }

    FUNCTION_LEAVE_MSG(__FUNCTION__);
    return retVal;
}
