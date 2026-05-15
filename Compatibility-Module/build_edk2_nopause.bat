@echo off
rem Thin wrapper for CI / automation — same as build_edk2.bat without pauses.
set "SCOOTWARE_NO_PAUSE=1"
call "%~dp0build_edk2.bat"
exit /b %ERRORLEVEL%
