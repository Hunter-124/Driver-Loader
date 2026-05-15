@echo off
setlocal EnableExtensions EnableDelayedExpansion

:: ── EDK2 Build Script for ScootwareCompat ──────────────────────────────────
:: This script automates the EDK2 build process.
:: It ensures that the environment is correctly set up and BaseTools are built.
:: ─────────────────────────────────────────────────────────────────────────

cd /d "%~dp0"
set "ROOT=%CD%"
set "EDK_PATH=%ROOT%\VisualUefi\edk2"

:: ── Check for NASM ───────────────────────────────────────────────────────
if not defined NASM_PREFIX (
    echo [*] Searching for NASM...
    if exist "C:\Program Files\NASM\nasm.exe" (
        set "NASM_PREFIX=C:\Program Files\NASM\"
    ) else if exist "C:\Program Files (x86)\NASM\nasm.exe" (
        set "NASM_PREFIX=C:\Program Files (x86)\NASM\"
    ) else (
        echo [-] ERROR: NASM not found. Please install NASM and set NASM_PREFIX.
        pause
        exit /b 1
    )
)
:: Ensure trailing backslash
if not "%NASM_PREFIX:~-1%"=="\" set "NASM_PREFIX=%NASM_PREFIX%\"
echo [*] NASM_PREFIX: %NASM_PREFIX%

:: ── Check for Zydis Submodule ───────────────────────────────────────────
if not exist "%ROOT%\EfiGuardDxe\Zydis\src\Zydis.c" (
    echo [!] WARNING: Zydis submodule appears to be missing or empty.
    echo     EDK2 build will likely fail. Please ensure submodules are initialized.
    echo     If you downloaded the repo as a ZIP, you must download Zydis separately
    echo     and place it in EfiGuardDxe\Zydis.
    echo.
)

:: ── Find Visual Studio ───────────────────────────────────────────────────
set "VS_VERSION="
set "VS_PATH="

:: Try to find VS 2022
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VS_VERSION=VS2022"
    set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community"
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VS_VERSION=VS2019"
    set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community"
)

:: If not found via common paths, try vswhere
if not defined VS_PATH (
    if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
        for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set "VS_PATH=%%i"
    )
)

if defined VS_PATH (
    :: Rudimentary version detection from path
    echo !VS_PATH! | findstr "2022" >nul && set "VS_VERSION=VS2022"
    echo !VS_PATH! | findstr "2019" >nul && set "VS_VERSION=VS2019"
) else (
    echo [-] ERROR: Visual Studio not found.
    pause
    exit /b 1
)

if not defined VS_VERSION set "VS_VERSION=VS2019"
echo [*] Found Visual Studio: %VS_VERSION% at "%VS_PATH%"

:: ── Initialize VS Environment ───────────────────────────────────────────
if not defined VCINSTALLDIR (
    echo [*] Initializing Visual Studio environment...
    :: Use vcvars64.bat for x64 build
    call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"
)

:: ── Set up EDK2 environment ──────────────────────────────────────────────
set "WORKSPACE=%ROOT%"
set "EDK_TOOLS_PATH=%EDK_PATH%\BaseTools"
set "BASE_TOOLS_PATH=%EDK_PATH%\BaseTools"
set "PYTHON_COMMAND=python"

echo [*] Workspace: %WORKSPACE%
echo [*] EDK2 Path: %EDK_PATH%
echo [*] EDK_TOOLS_PATH: %EDK_TOOLS_PATH%

pushd "%EDK_PATH%"

:: Check if BaseTools binaries exist, if not, try to build them
if not exist "%EDK_TOOLS_PATH%\Bin\Win32\VfrCompile.exe" (
    echo [!] BaseTools C-binaries missing. Building BaseTools...
    call edksetup.bat Rebuild
) else (
    call edksetup.bat
)

:: Ensure BinWrappers is in path if build.exe is missing
if not exist "%EDK_TOOLS_PATH%\Bin\Win32\build.exe" (
    set "PATH=%EDK_TOOLS_PATH%\BinWrappers\WindowsLike;%PATH%"
    set "BUILD_EXE=build"
) else (
    set "BUILD_EXE=build.exe"
)

:: ── Build the project ────────────────────────────────────────────────────
echo [*] Starting EDK2 build for %VS_VERSION% using %BUILD_EXE%...
:: Using relative path to DSC from EDK_PATH
%BUILD_EXE% -a X64 -t %VS_VERSION% -p ..\..\ScootwareCompatModulePkg.dsc -b RELEASE
set "BUILD_RESULT=%ERRORLEVEL%"
popd

if "%BUILD_RESULT%"=="0" (
    echo [+] EDK2 build succeeded.
    
    :: Copy outputs to BIN folder
    set "BIN_DIR=%ROOT%\BIN"
    if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"
    
    set "EDK_OUT=%ROOT%\Build\ScootwareCompatModule\RELEASE_%VS_VERSION%\X64"
    if exist "%EDK_OUT%\ScootwareCompatDxe.efi" (
        copy /Y "%EDK_OUT%\ScootwareCompatDxe.efi" "%BIN_DIR%\"
        echo [+] Copied ScootwareCompatDxe.efi
    )
    if exist "%EDK_OUT%\Loader.efi" (
        copy /Y "%EDK_OUT%\Loader.efi" "%BIN_DIR%\"
        echo [+] Copied Loader.efi
    )
) else (
    echo [-] EDK2 build failed with exit code %BUILD_RESULT%.
)

pause
exit /b %BUILD_RESULT%
