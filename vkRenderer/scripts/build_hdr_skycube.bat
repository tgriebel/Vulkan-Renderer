@echo off
setlocal

REM Precompute env cubemap, diffuse IBL, and specular IBL from a source .hdr file.
REM Usage: build_hdr_skycube.bat <source.hdr> <cubemap_name>
REM
REM <source.hdr>    is resolved relative to vkRenderer\ (the renderer's working directory).
REM <cubemap_name>  prefix for output files in code_assets\:
REM                   <name>_env.img, <name>_diffuseIbl.img, <name>_specIbl.img

if "%~1"=="" (
    echo Usage: build_hdr_skycube.bat ^<source.hdr^> ^<cubemap_name^>
    exit /b 1
)
if "%~2"=="" (
    echo Usage: build_hdr_skycube.bat ^<source.hdr^> ^<cubemap_name^>
    exit /b 1
)

set HDR_SOURCE=%~1
set CUBEMAP_NAME=%~2

set SCRIPT_DIR=%~dp0
set VK_RENDERER_DIR=%SCRIPT_DIR%..
set SOLUTION_OUT=%SCRIPT_DIR%..\..\x64
set RENDERER_EXE=%SOLUTION_OUT%\Release\renderer.exe

REM Build renderer if the Release exe is missing.
if not exist "%RENDERER_EXE%" (
    call "%SCRIPT_DIR%build_tools.bat"
    if errorlevel 1 exit /b 1
)

if not exist "%RENDERER_EXE%" (
    echo ERROR: renderer still missing at "%RENDERER_EXE%" after build attempt.
    exit /b 1
)

echo Building sky cube from: %HDR_SOURCE%
echo Output name:            %CUBEMAP_NAME%

pushd "%VK_RENDERER_DIR%"
"%RENDERER_EXE%" "RenderIBL.ini" "r_hdrCubemapSource=%HDR_SOURCE%" "r_cubemapName=%CUBEMAP_NAME%"
set EXIT_CODE=%ERRORLEVEL%
popd

if %EXIT_CODE% neq 0 (
    echo ERROR: renderer exited with code %EXIT_CODE%.
    exit /b %EXIT_CODE%
)

echo Done.
exit /b 0
