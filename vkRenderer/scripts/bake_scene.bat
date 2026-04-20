@echo off
setlocal

REM Launch the AssetBaker against a scene json.
REM Usage: bake_scene.bat <scene.json>
REM
REM Scene path resolves relative to vkRenderer\ (where ScenePath = ".\scenes\"),
REM so run the exe from that directory regardless of where the script is called from.

set SCRIPT_DIR=%~dp0
set VK_RENDERER_DIR=%SCRIPT_DIR%..
set SOLUTION_OUT=%SCRIPT_DIR%..\..\x64

set BAKER_EXE=%SOLUTION_OUT%\Release\AssetBaker.exe

REM If the Release exe doesn't exist, kick off a Release build.
if not exist "%BAKER_EXE%" (
    call "%SCRIPT_DIR%buildTools.bat"
    if errorlevel 1 exit /b 1
)

if not exist "%BAKER_EXE%" (
    echo ERROR: AssetBaker still missing at "%BAKER_EXE%" after build attempt.
    exit /b 1
)

pushd "%VK_RENDERER_DIR%"
"%BAKER_EXE%" %*
set EXIT_CODE=%ERRORLEVEL%
popd

exit /b %EXIT_CODE%
