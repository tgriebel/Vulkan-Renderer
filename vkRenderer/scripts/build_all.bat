@echo off
setlocal

REM Build everything: AssetBaker, renderer Debug, renderer Release ^(all x64^).
REM Delegates to the per-target scripts; bails on the first failure.
REM Usage: build_all.bat

set SCRIPT_DIR=%~dp0

call "%SCRIPT_DIR%build_baker.bat"
if errorlevel 1 exit /b 1

call "%SCRIPT_DIR%build_debug.bat"
if errorlevel 1 exit /b 1

call "%SCRIPT_DIR%build_release.bat"
if errorlevel 1 exit /b 1

exit /b 0
