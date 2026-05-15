@echo off
setlocal enabledelayedexpansion

::
:: smap_packed.exe build script.
::
:: Flow:
::   1. Build PCOMP.exe (Mapper\Utils\PCOMP) if it doesn't already exist.
::   2. Run PCOMP against BIN\drv.sys with the project's PROVIDER_RES_KEY
::      (0xF62E6CE0, see Mapper\Shared\consts.h). Produces drv.bin and moves
::      it to Mapper\Mapper\res\drv.bin so the RC compiler can embed it as
::      IDR_EMBEDDED_DRIVER.
::   3. MSBuild smap.sln in Release^|x64. Produces:
::        Mapper\Mapper\output\x64\Release\smap_packed.exe  (our IP, packed)
::        Mapper\drv-DB\output\x64\Release\drv64.dll        (legacy, unused)
::   4. Copy ONLY smap_packed.exe into BIN\. drv.sys stays in BIN\ as the
::      build-time input (it is never shipped alongside smap_packed.exe -
::      the exe is what gets uploaded to the admin panel as the
::      `driver_loader` product asset).
::
:: Required: Visual Studio 2019 or later with C++ support.
::

echo [*] smap_packed build script
echo.

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [!] vswhere.exe not found. Please ensure Visual Studio is installed.
    pause
    exit /b 1
)

echo [*] Looking for Visual Studio...
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -property installationPath`) do (
    set "VS_PATH=%%i"
)

if "%VS_PATH%"=="" (
    echo [!] Visual Studio installation not found.
    pause
    exit /b 1
)

echo [*] Found Visual Studio at: "%VS_PATH%"

set "MSBUILD_EXE="
if exist "%VS_PATH%\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD_EXE=%VS_PATH%\MSBuild\Current\Bin\MSBuild.exe"
if not defined MSBUILD_EXE if exist "%VS_PATH%\MSBuild\15.0\Bin\MSBuild.exe" set "MSBUILD_EXE=%VS_PATH%\MSBuild\15.0\Bin\MSBuild.exe"

if not defined MSBUILD_EXE (
    echo [!] MSBuild.exe not found in %VS_PATH%.
    pause
    exit /b 1
)

echo [*] Found MSBuild: "%MSBUILD_EXE%"

::
:: Step 1: ensure PCOMP.exe is available.
::
:: PCOMP is a tiny helper that XOR+msdelta-compresses raw drv.sys into the
:: shape KDULoadResource + KDUDecompressResource expect at runtime. Using
:: the same codec the rest of the drv-DB provider blobs use means no new
:: decode path is needed in the mapper.
::
set "PCOMP_SLN=%~dp0Mapper\Utils\PCOMP\PCOMP.sln"
set "PCOMP_EXE=%~dp0Mapper\Utils\PCOMP\output\x64\Release\PCOMP.exe"

::
:: Always rebuild PCOMP.exe. It's a ~10kB tool and MSBuild's incremental
:: engine makes the repeated call a no-op when nothing changed, so the
:: cost is negligible. Doing this unconditionally is important because
:: PCOMP *is* the codec - if it's ever out of date with the mapper's
:: decode path (e.g. a bug fix to its command-line key parser) the
:: encoded drv.bin will silently be wrong and the hollowed mapper will
:: exit with 0x5A07 at runtime. We'd rather burn an extra second at
:: build time than ship a broken smap_packed.exe.
::
if not exist "%PCOMP_SLN%" (
    echo [!] PCOMP solution missing at %PCOMP_SLN%
    pause
    exit /b 1
)

echo [*] Building PCOMP.exe (incremental)...
"%MSBUILD_EXE%" "%PCOMP_SLN%" /p:Configuration=Release /p:Platform=x64 /m /v:m /p:PlatformToolset=v143
if errorlevel 1 (
    echo [!] PCOMP build failed.
    pause
    exit /b 1
)

if not exist "%PCOMP_EXE%" (
    echo [!] PCOMP.exe still not present after build, aborting.
    pause
    exit /b 1
)

echo [+] PCOMP.exe ready: "%PCOMP_EXE%"

::
:: Step 2: pre-encode drv.sys for embedding.
::
set "DRV_INPUT=%~dp0BIN\drv.sys"
set "DRV_ENCODED=%~dp0BIN\drv.bin"
set "DRV_RES_DEST=%~dp0Mapper\Mapper\res\drv.bin"
set "DRV_KEY=0xF62E6CE0"

if not exist "%DRV_INPUT%" (
    echo.
    echo [!] Expected drv.sys at "%DRV_INPUT%".
    echo     Build the kernel driver first and drop it in BIN\drv.sys
    echo     or point DRV_INPUT at wherever your drv.sys lives.
    pause
    exit /b 1
)

echo.
echo [*] Encoding drv.sys with key %DRV_KEY% for embed...
if exist "%DRV_ENCODED%" del /Q "%DRV_ENCODED%" >nul 2>&1

:: PCOMP writes <input-without-ext>.bin in the input's directory, so we
:: cd into BIN first and then back out. Capture the exit code before the
:: cd back because cd resets ERRORLEVEL on success.
pushd "%~dp0BIN"
"%PCOMP_EXE%" drv.sys %DRV_KEY%
set "PCOMP_EC=%ERRORLEVEL%"
popd

if not "%PCOMP_EC%"=="0" (
    echo [!] PCOMP failed with error code %PCOMP_EC%.
    pause
    exit /b %PCOMP_EC%
)

if not exist "%DRV_ENCODED%" (
    echo [!] PCOMP ran but drv.bin was not produced at "%DRV_ENCODED%".
    pause
    exit /b 1
)

::
:: Roundtrip sanity check: decode the encoded blob back through the
:: exact same XOR + MSDelta path the mapper will run at runtime, and
:: confirm it reproduces drv.sys byte-for-byte. This catches every
:: class of encoder/decoder skew (wrong key, codec drift, truncated
:: resource, etc.) at BUILD time instead of at runtime inside the
:: hollowed mapper (where failures surface as the opaque 0x5A07).
::
:: Historical bug this guards against: pcomp used minirtl's decimal-
:: only _strtoul to parse its key argument, so "0xF62E6CE0" quietly
:: parsed as 0 and fell back to PROVIDER_RES_KEY_DEFAULT. drv.bin
:: was encrypted with the wrong key for every build; the mapper's
:: ApplyDeltaB returned FALSE at runtime and the autopilot exited
:: with 0x5A07. A roundtrip check would have caught this on the
:: very first build.
::
echo [*] Verifying encoded resource roundtrips cleanly...
pushd "%~dp0BIN"
"%PCOMP_EXE%" -d drv.bin %DRV_KEY%
set "PCOMP_DEC_EC=%ERRORLEVEL%"
popd

if not "%PCOMP_DEC_EC%"=="0" (
    echo [!] PCOMP decode roundtrip failed with error code %PCOMP_DEC_EC%.
    pause
    exit /b %PCOMP_DEC_EC%
)

set "DRV_ROUNDTRIP=%~dp0BIN\drv.decompressed"
if not exist "%DRV_ROUNDTRIP%" (
    echo [!] PCOMP decode ran but "%DRV_ROUNDTRIP%" was not produced.
    echo     This usually means the key argument failed to parse — check
    echo     that PCOMP.exe was rebuilt with the hex-prefix-aware key parser.
    pause
    exit /b 1
)

::
:: Don't wrap this failure branch in an `if ( ... )` block — any
:: parentheses inside the echoed hint text would close the block
:: prematurely and cmd.exe would bail with "<word> was unexpected at
:: this time". Using `if errorlevel 1 goto :label` keeps the echoes
:: out of a parenthesized block entirely, which is why the hint text
:: below is free to mention things like "0x5A07 AP_EXIT_RESOURCE_
:: DECODE_FAILED" and filesystem paths without escaping.
::
fc /b "%DRV_INPUT%" "%DRV_ROUNDTRIP%" >nul
if errorlevel 1 goto :roundtrip_mismatch

del /Q "%DRV_ROUNDTRIP%" >nul 2>&1
echo [+] Roundtrip verified - drv.bin decodes back to drv.sys byte-for-byte.
goto :roundtrip_done

:roundtrip_mismatch
echo.
echo [!] ROUNDTRIP MISMATCH: "%DRV_ROUNDTRIP%" does not match "%DRV_INPUT%".
echo     drv.bin would decrypt/decompress into something OTHER than the
echo     original drv.sys. Do NOT ship this build - the hollowed mapper
echo     will exit with 0x5A07 AP_EXIT_RESOURCE_DECODE_FAILED at runtime.
echo.
echo     Common causes:
echo       - PROVIDER_RES_KEY in Mapper\Shared\consts.h differs from
echo         the DRV_KEY used by this script
echo       - PCOMP.exe is stale and still using the decimal-only key
echo         parser - delete output\x64\Release\PCOMP.exe and retry
echo       - drv.sys was replaced between encode and verify
echo.
del /Q "%DRV_ROUNDTRIP%" >nul 2>&1
pause
exit /b 1

:roundtrip_done

if not exist "%~dp0Mapper\Mapper\res" mkdir "%~dp0Mapper\Mapper\res"
move /Y "%DRV_ENCODED%" "%DRV_RES_DEST%" >nul
if errorlevel 1 (
    echo [!] Failed to move drv.bin into Mapper\Mapper\res.
    pause
    exit /b 1
)

echo [+] Embedded-driver resource ready: "%DRV_RES_DEST%"

::
:: Step 3: build smap_packed.exe.
::
set "SLN_PATH=%~dp0Mapper\smap.sln"
if not exist "%SLN_PATH%" (
    echo [!] Solution file not found at %SLN_PATH%
    pause
    exit /b 1
)

echo.
echo [*] Building smap_packed (Release x64)...
echo.

"%MSBUILD_EXE%" "%SLN_PATH%" /p:Configuration=Release /p:Platform=x64 /m /v:m /p:PlatformToolset=v143
if errorlevel 1 (
    echo.
    echo [!] Build failed.
    pause
    exit /b 1
)

echo.
echo [*] Build successful!

::
:: Step 4: ship only smap_packed.exe.
::
:: drv64.dll is still produced by the Tanikaze project as a build side-effect
:: but it is no longer used - the mapper carries the provider table compiled
:: in via drvdb.cpp. We intentionally do NOT copy drv64.dll to BIN\.
::
set "BIN_DIR=%~dp0BIN"
set "SMAP_EXE=%~dp0Mapper\Mapper\output\x64\Release\smap_packed.exe"

echo.
echo [*] Copying packed artefact to BIN folder...

if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"

if not exist "%SMAP_EXE%" (
    echo     [!] smap_packed.exe not found at "%SMAP_EXE%"
    pause
    exit /b 1
)

copy /Y "%SMAP_EXE%" "%BIN_DIR%\" >nul
if errorlevel 1 (
    echo     [!] Failed to copy smap_packed.exe
    pause
    exit /b 1
)
echo     [+] smap_packed.exe  -^> BIN\smap_packed.exe

::
:: House-keeping: remove the older split artefacts so there's no stale
:: smap.exe / drv64.dll sitting next to the new packed build on developer
:: boxes that had the old build.
::
if exist "%BIN_DIR%\smap.exe" (
    echo     [~] Removing stale BIN\smap.exe
    del /Q "%BIN_DIR%\smap.exe" >nul 2>&1
)
if exist "%BIN_DIR%\drv64.dll" (
    echo     [~] Removing stale BIN\drv64.dll
    del /Q "%BIN_DIR%\drv64.dll" >nul 2>&1
)

echo.
echo [*] Done. Upload BIN\smap_packed.exe to the admin panel as the
echo     `driver_loader` asset for each product.
echo.
pause
