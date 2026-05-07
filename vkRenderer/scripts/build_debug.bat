@echo off
setlocal

REM Build Debug x64 of the renderer.
REM Usage: build_debug.bat

set SCRIPT_DIR=%~dp0
set SOLUTION_DIR=%SCRIPT_DIR%..\..
set SOLUTION_FILE=%SOLUTION_DIR%\vkRenderer.sln

set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
if not exist "%VSWHERE%" (
    echo ERROR: vswhere.exe not found at "%VSWHERE%".
    echo Install Visual Studio, or build the renderer project ^(Debug^|x64^) manually.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set MSBUILD=%%i
if not defined MSBUILD (
    echo ERROR: MSBuild not found via vswhere.
    echo Build the renderer project ^(Debug^|x64^) manually.
    exit /b 1
)

echo Building renderer ^(Debug^|x64^)...
"%MSBUILD%" "%SOLUTION_FILE%" /t:renderer /p:Configuration=Debug /p:Platform=x64 /m /nologo /v:minimal
if errorlevel 1 (
    echo ERROR: renderer Debug build failed.
    exit /b 1
)

echo Done.
exit /b 0
