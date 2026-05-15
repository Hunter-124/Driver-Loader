@echo off
setlocal EnableExtensions EnableDelayedExpansion

:: ── Unified Build Script ────────────────────────────────────────────────
:: Builds the DXE driver, Loader, and EfiDSEFix in a single pass.
:: The resulting binaries determine their "headless" (silent) status
:: at runtime by reading scootware.cfg from the ESP.
:: ─────────────────────────────────────────────────────────────────────────

:: Script-level headless check (suppress pauses at end / on some errors)
set "HEADLESS=0"
if /I "%1"=="--headless" set "HEADLESS=1"
if "%SCOOTWARE_HEADLESS%"=="1" set "HEADLESS=1"
rem build_all.bat --no-pause sets SCOOTWARE_NO_PAUSE=1 for CI; treat like headless for pause only.
if "!SCOOTWARE_NO_PAUSE!"=="1" set "HEADLESS=1"
cd /d "%~dp0"
set "ROOT=%CD%"

call "%~dp0..\..\build\lib\env.bat"
if errorlevel 1 (
    if "%HEADLESS%"=="0" pause
    exit /b 2
)

set "BIN_DIR=%BIN%"
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"

echo [*] MSBuild: %MSBUILD_EXE%
echo [*] Solution: %ROOT%\ScootwareCompatModule.sln
echo [*] Output dir: %BIN_DIR%
echo.

:: ── Check VisualUefi dependency ────────────────────────────────────────────
:: The .efi projects (ScootwareCompatDxe, Loader) link against VisualUefi\EDK-II
:: libraries under this module ^(see VISUALUEFI_PATH below^).
:: EfiDSEFix.exe does NOT need VisualUefi — it builds with standard CRT.
set "VISUALUEFI_PATH=%ROOT%\VisualUefi"
if not exist "%VISUALUEFI_PATH%" (
    echo [!] WARNING: VisualUefi not found at "%VISUALUEFI_PATH%"
    echo     The ScootwareCompatDxe.efi and Loader.efi projects will FAIL to link
    echo     without VisualUefi libraries prebuilt.
    echo     EfiDSEFix.exe ^(standalone^) will still build.
    echo.
    echo     To set up VisualUefi, clone it into:
    echo       %VISUALUEFI_PATH%
    echo     See: https://github.com/ionescu007/VisualUefi
    echo.
    echo     Attempting build anyway ^(expect linker errors for DXE/Loader^)...
    echo.
)

:: ── Build the solution ─────────────────────────────────────────────────────
echo [1/1] Building ScootwareCompatModule.sln (Release ^| x64)...
"%MSBUILD_EXE%" "%ROOT%\ScootwareCompatModule.sln" /m /v:minimal /p:Configuration=Release /p:Platform=x64
set "BUILD_RESULT=%ERRORLEVEL%"

if "%BUILD_RESULT%"=="0" (
    echo [+] Solution build succeeded.
) else (
    echo [-] Solution build had warnings/errors ^(exit code %BUILD_RESULT%^).
    echo     Individual outputs will be copied if found.
)

echo.

:: ── Copy outputs to BIN directory ──────────────────────────────────────────
set "COPIED_ANY=0"

:: ScootwareCompatDxe.efi — default VS output path
set "DXE_SRC=%ROOT%\x64\Release\ScootwareCompatDxe.efi"
if exist "%DXE_SRC%" (
    copy /Y "%DXE_SRC%" "%BIN_DIR%\ScootwareCompatDxe.efi" >nul
    echo [+] Copied ScootwareCompatDxe.efi
    set "COPIED_ANY=1"
) else (
    echo [-] ScootwareCompatDxe.efi not found at "%DXE_SRC%"
)

:: Loader.efi — default VS output path
set "LOADER_SRC=%ROOT%\x64\Release\Loader.efi"
if exist "%LOADER_SRC%" (
    copy /Y "%LOADER_SRC%" "%BIN_DIR%\Loader.efi" >nul
    echo [+] Copied Loader.efi
    set "COPIED_ANY=1"
) else (
    echo [-] Loader.efi not found at "%ROOT%\x64\Release\Loader.efi"
)

:: EfiDSEFix.exe — has its own OutDir in the vcxproj
set "DSEFIX_SRC=%ROOT%\Application\EfiDSEFix\bin\EfiDSEFix.exe"
if exist "%DSEFIX_SRC%" (
    copy /Y "%DSEFIX_SRC%" "%BIN_DIR%\EfiDSEFix.exe" >nul
    echo [+] Copied EfiDSEFix.exe
    set "COPIED_ANY=1"
) else (
    echo [-] EfiDSEFix.exe not found at "%DSEFIX_SRC%"
)

:: ── Summary ────────────────────────────────────────────────────────────────
echo.
if "%COPIED_ANY%"=="1" (
    echo [+] Outputs copied to "%BIN_DIR%"
    dir "%BIN_DIR%\ScootwareCompat*" "%BIN_DIR%\Loader*" "%BIN_DIR%\EfiDSEFix*" 2>nul
) else (
    echo [!] No outputs were copied. Build may have failed.
    echo     Check the MSBuild output above for errors.
)

if not "%BUILD_RESULT%"=="0" (
    echo.
    echo [!] Build exited with code %BUILD_RESULT%.
    if "%HEADLESS%"=="0" pause
)
