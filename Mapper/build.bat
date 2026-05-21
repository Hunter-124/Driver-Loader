@echo off
setlocal enabledelayedexpansion

::
:: smap_packed.exe build script.
::
:: Flow:
::   1. Load repo env (REPO_ROOT, BIN, MSBUILD_EXE) via build\lib\env.bat
::   2. Build PCOMP.exe (Utils\PCOMP)
::   3. Run PCOMP against %BIN%\drv.sys (build kernel driver first — see Driver\FINAL-DRV\build.bat).
::      Produces drv.bin and moves it to Mapper\res\drv.bin for RC embedding.
::   4. MSBuild smap.sln Release|x64 -> Mapper\output\x64\Release\smap_packed.exe (under this folder)
::   5. Copy smap_packed.exe to %BIN%
::
:: Required: Visual Studio 2019+ with C++ (v143 toolset for this solution).
::

echo [*] smap_packed build script
echo.

call "%~dp0..\..\build\lib\env.bat"
if errorlevel 1 (
    if not "!SCOOTWARE_NO_PAUSE!"=="1" pause
    exit /b 1
)

echo [*] REPO_ROOT=%REPO_ROOT%
echo [*] BIN=%BIN%
echo [*] MSBuild: %MSBUILD_EXE%
echo.

set "PCOMP_SLN=%~dp0Utils\PCOMP\PCOMP.sln"
set "PCOMP_EXE=%~dp0Utils\PCOMP\output\x64\Release\PCOMP.exe"

if not exist "%PCOMP_SLN%" (
    echo [!] PCOMP solution missing at %PCOMP_SLN%
    if not "!SCOOTWARE_NO_PAUSE!"=="1" pause
    exit /b 1
)

echo [*] Cleaning PCOMP build artifacts...
if exist "%~dp0Utils\PCOMP\output" (
    rmdir /S /Q "%~dp0Utils\PCOMP\output" >nul 2>&1
)

echo [*] Building PCOMP.exe (clean build)...
"%MSBUILD_EXE%" "%PCOMP_SLN%" /p:Configuration=Release /p:Platform=x64 /m /v:m /p:PlatformToolset=v143 /t:Rebuild
if errorlevel 1 (
    echo [!] PCOMP build failed.
    if not "!SCOOTWARE_NO_PAUSE!"=="1" pause
    exit /b 1
)

if not exist "%PCOMP_EXE%" (
    echo [!] PCOMP.exe still not present after build, aborting.
    if not "!SCOOTWARE_NO_PAUSE!"=="1" pause
    exit /b 1
)

echo [+] PCOMP.exe ready: "%PCOMP_EXE%"

set "DRV_INPUT=%BIN%\drv.sys"
set "DRV_ENCODED=%BIN%\drv.bin"
set "DRV_RES_DEST=%~dp0Mapper\res\drv.bin"
set "DRV_KEY=0xF62E6CE0"

if not exist "%DRV_INPUT%" (
    echo.
    echo [!] Expected drv.sys at "%DRV_INPUT%".
    echo     Run Driver\FINAL-DRV\build.bat or build_all.bat first ^(it copies driver.sys as drv.sys^).
    if not "!SCOOTWARE_NO_PAUSE!"=="1" pause
    exit /b 1
)

echo.
echo [*] Encoding drv.sys with key %DRV_KEY% for embed...
if exist "%DRV_ENCODED%" del /Q "%DRV_ENCODED%" >nul 2>&1

pushd "%BIN%"
"%PCOMP_EXE%" drv.sys %DRV_KEY%
set "PCOMP_EC=!ERRORLEVEL!"
popd

if not "!PCOMP_EC!"=="0" (
    echo [!] PCOMP failed with error code !PCOMP_EC!.
    if not "!SCOOTWARE_NO_PAUSE!"=="1" pause
    exit /b !PCOMP_EC!
)

if not exist "%DRV_ENCODED%" (
    echo [!] PCOMP ran but drv.bin was not produced at "%DRV_ENCODED%".
    if not "!SCOOTWARE_NO_PAUSE!"=="1" pause
    exit /b 1
)

echo [*] Verifying encoded resource roundtrips cleanly...
pushd "%BIN%"
"%PCOMP_EXE%" -d drv.bin %DRV_KEY%
set "PCOMP_DEC_EC=!ERRORLEVEL!"
popd

if not "!PCOMP_DEC_EC!"=="0" (
    echo [!] PCOMP decode roundtrip failed with error code !PCOMP_DEC_EC!.
    if not "!SCOOTWARE_NO_PAUSE!"=="1" pause
    exit /b !PCOMP_DEC_EC!
)

set "DRV_ROUNDTRIP=%BIN%\drv.decompressed"
if not exist "%DRV_ROUNDTRIP%" (
    echo [!] PCOMP decode ran but "%DRV_ROUNDTRIP%" was not produced.
    echo     This usually means the key argument failed to parse — check
    echo     that PCOMP.exe was rebuilt with the hex-prefix-aware key parser.
    if not "!SCOOTWARE_NO_PAUSE!"=="1" pause
    exit /b 1
)

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
echo       - PCOMP.exe is stale — delete Utils\PCOMP\output\x64\Release\PCOMP.exe and retry
echo       - drv.sys was replaced between encode and verify
echo.
del /Q "%DRV_ROUNDTRIP%" >nul 2>&1
if not "!SCOOTWARE_NO_PAUSE!"=="1" pause
exit /b 1

:roundtrip_done

if not exist "%~dp0Mapper\res" mkdir "%~dp0Mapper\res"
move /Y "%DRV_ENCODED%" "%DRV_RES_DEST%" >nul
if errorlevel 1 (
    echo [!] Failed to move drv.bin into Mapper\res.
    if not "!SCOOTWARE_NO_PAUSE!"=="1" pause
    exit /b 1
)

echo [+] Embedded-driver resource ready: "%DRV_RES_DEST%"

set "SLN_PATH=%~dp0smap.sln"
if not exist "%SLN_PATH%" (
    echo [!] Solution file not found at %SLN_PATH%
    if not "!SCOOTWARE_NO_PAUSE!"=="1" pause
    exit /b 1
)

echo.
echo [*] Cleaning smap_packed build artifacts...
if exist "%~dp0Mapper\output" (
    rmdir /S /Q "%~dp0Mapper\output" >nul 2>&1
)

echo [*] Building smap_packed (Release x64, clean build)...
echo.

"%MSBUILD_EXE%" "%SLN_PATH%" /p:Configuration=Release /p:Platform=x64 /m /v:m /p:PlatformToolset=v143 /t:Rebuild
if errorlevel 1 (
    echo.
    echo [!] Build failed.
    if not "!SCOOTWARE_NO_PAUSE!"=="1" pause
    exit /b 1
)

echo.
echo [*] Build successful!

set "SMAP_EXE=%~dp0Mapper\output\x64\Release\smap_packed.exe"

echo.
echo [*] Copying packed artefact to BIN folder...

if not exist "%SMAP_EXE%" (
    echo     [!] smap_packed.exe not found at "%SMAP_EXE%"
    if not "!SCOOTWARE_NO_PAUSE!"=="1" pause
    exit /b 1
)

copy /Y "%SMAP_EXE%" "%BIN%\" >nul
if errorlevel 1 (
    echo     [!] Failed to copy smap_packed.exe
    if not "!SCOOTWARE_NO_PAUSE!"=="1" pause
    exit /b 1
)
echo     [+] smap_packed.exe  -^> %BIN%\smap_packed.exe

if exist "%BIN%\smap.exe" (
    echo     [~] Removing stale smap.exe
    del /Q "%BIN%\smap.exe" >nul 2>&1
)
if exist "%BIN%\drv64.dll" (
    echo     [~] Removing stale drv64.dll
    del /Q "%BIN%\drv64.dll" >nul 2>&1
)

echo.
echo [*] Done. Upload %BIN%\smap_packed.exe to the admin panel as the
echo     `driver_loader` asset for each product.
exit /b 0
