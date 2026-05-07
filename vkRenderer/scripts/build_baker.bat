@echo off
setlocal

REM Build Release x64 of the AssetBaker tool.
REM Usage: build_baker.bat

set SCRIPT_DIR=%~dp0
set SOLUTION_DIR=%SCRIPT_DIR%..\..
set SOLUTION_FILE=%SOLUTION_DIR%\vkRenderer.sln

set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
if not exist "%VSWHERE%" (
    echo ERROR: vswhere.exe not found at "%VSWHERE%".
    echo Install Visual Studio, or build the AssetBaker project ^(Release^|x64^) manually.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set MSBUILD=%%i
if not defined MSBUILD (
    echo ERROR: MSBuild not found via vswhere.
    echo Build the AssetBaker project ^(Release^|x64^) manually.
    exit /b 1
)

echo Building AssetBaker ^(Release^|x64^)...
"%MSBUILD%" "%SOLUTION_FILE%" /t:AssetBaker /p:Configuration=Release /p:Platform=x64 /m /nologo /v:minimal
if errorlevel 1 (
    echo ERROR: AssetBaker Release build failed.
    exit /b 1
)

echo Done.
exit /b 0
