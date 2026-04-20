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

REM Prefer Release; fall back to Debug.
set BAKER_EXE=%SOLUTION_OUT%\Release\AssetBaker.exe
if not exist "%BAKER_EXE%" set BAKER_EXE=%SOLUTION_OUT%\Debug\AssetBaker.exe

if not exist "%BAKER_EXE%" (
    echo AssetBaker not found. Expected at one of:
    echo   %SOLUTION_OUT%\Release\AssetBaker.exe
    echo   %SOLUTION_OUT%\Debug\AssetBaker.exe
    echo Build the AssetBaker project first ^(x64^).
    exit /b 1
)

pushd "%VK_RENDERER_DIR%"
"%BAKER_EXE%" %*
set EXIT_CODE=%ERRORLEVEL%
popd

exit /b %EXIT_CODE%
