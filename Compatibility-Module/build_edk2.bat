@echo off
setlocal EnableExtensions EnableDelayedExpansion

:: ── EDK2 Build Script for ScootwareCompat ──────────────────────────────────
:: Automates EDK2 BaseTools setup and builds ScootwareCompatModulePkg.dsc.
:: Run from repo after VisualUefi + edk2 tree exists under this module.
::
:: Prerequisites: NASM, Python, Visual Studio (same toolchain EDK2 expects).
:: Optional: SCOOTWARE_NO_PAUSE=1 to skip pauses on error/success.
:: ─────────────────────────────────────────────────────────────────────────

cd /d "%~dp0"
set "ROOT=%CD%"

call "%~dp0..\..\build\lib\env.bat"
if errorlevel 1 (
    if not "!SCOOTWARE_NO_PAUSE!"=="1" pause
    exit /b 1
)

set "EDK_PATH=%ROOT%\VisualUefi\edk2"

if not defined NASM_PREFIX (
    echo [*] Searching for NASM...
    if exist "C:\Program Files\NASM\nasm.exe" (
        set "NASM_PREFIX=C:\Program Files\NASM\"
    ) else if exist "C:\Program Files (x86)\NASM\nasm.exe" (
        set "NASM_PREFIX=C:\Program Files (x86)\NASM\"
    ) else (
        echo [-] ERROR: NASM not found. Install NASM or set NASM_PREFIX.
        if not "!SCOOTWARE_NO_PAUSE!"=="1" pause
        exit /b 1
    )
)
if not "!NASM_PREFIX:~-1!"=="\" set "NASM_PREFIX=!NASM_PREFIX!\"
echo [*] NASM_PREFIX: !NASM_PREFIX!

if not exist "%ROOT%\EfiGuardDxe\Zydis\src\Zydis.c" (
    echo [!] WARNING: Zydis submodule appears to be missing or empty.
    echo     Initialize git submodules or vendor Zydis into EfiGuardDxe\Zydis.
    echo.
)

set "VS_VERSION="
set "VS_PATH="

if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VS_VERSION=VS2022"
    set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community"
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VS_VERSION=VS2019"
    set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community"
)

if not defined VS_PATH (
    if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
        for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set "VS_PATH=%%i"
    )
)

if not defined VS_PATH (
    echo [-] ERROR: Visual Studio not found.
    if not "!SCOOTWARE_NO_PAUSE!"=="1" pause
    exit /b 1
)

if not defined VS_VERSION (
    echo !VS_PATH! | findstr /i "2022" >nul && set "VS_VERSION=VS2022"
)
if not defined VS_VERSION (
    echo !VS_PATH! | findstr /i "2019" >nul && set "VS_VERSION=VS2019"
)
if not defined VS_VERSION set "VS_VERSION=VS2022"

echo [*] Using Visual Studio tag: !VS_VERSION! at "!VS_PATH!"

if not defined VCINSTALLDIR (
    echo [*] Initializing Visual Studio environment...
    call "!VS_PATH!\VC\Auxiliary\Build\vcvars64.bat"
)

set "WORKSPACE=%ROOT%"
set "EDK_TOOLS_PATH=%EDK_PATH%\BaseTools"
set "BASE_TOOLS_PATH=%EDK_PATH%\BaseTools"
set "PYTHON_COMMAND=python"

echo [*] Workspace: %WORKSPACE%
echo [*] EDK2 Path: %EDK_PATH%
echo [*] Staging BIN: %BIN%

pushd "%EDK_PATH%"

if not exist "%EDK_TOOLS_PATH%\Bin\Win32\VfrCompile.exe" (
    echo [!] BaseTools C-binaries missing. Building BaseTools...
    call edksetup.bat Rebuild
) else (
    call edksetup.bat
)

if not exist "%EDK_TOOLS_PATH%\Bin\Win32\build.exe" (
    set "PATH=%EDK_TOOLS_PATH%\BinWrappers\WindowsLike;%PATH%"
    set "BUILD_EXE=build"
) else (
    set "BUILD_EXE=build.exe"
)

echo [*] Starting EDK2 build for !VS_VERSION! using !BUILD_EXE!...
!BUILD_EXE! -a X64 -t !VS_VERSION! -p ..\..\ScootwareCompatModulePkg.dsc -b RELEASE
set "BUILD_RESULT=!ERRORLEVEL!"
popd

if "%BUILD_RESULT%"=="0" (
    echo [+] EDK2 build succeeded.
    if not exist "%BIN%" mkdir "%BIN%"
    set "EDK_OUT=%ROOT%\Build\ScootwareCompatModule\RELEASE_!VS_VERSION!\X64"
    if exist "!EDK_OUT!\ScootwareCompatDxe.efi" (
        copy /Y "!EDK_OUT!\ScootwareCompatDxe.efi" "%BIN%\"
        echo [+] Copied ScootwareCompatDxe.efi to BIN
    )
    if exist "!EDK_OUT!\Loader.efi" (
        copy /Y "!EDK_OUT!\Loader.efi" "%BIN%\"
        echo [+] Copied Loader.efi to BIN
    )
) else (
    echo [-] EDK2 build failed with exit code %BUILD_RESULT%.
)

if not "%BUILD_RESULT%"=="0" if not "!SCOOTWARE_NO_PAUSE!"=="1" pause
exit /b %BUILD_RESULT%
