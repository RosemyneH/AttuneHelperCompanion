@echo off
setlocal

set "ROOT=%~dp0"
set "ROOT=%ROOT:~0,-1%"
set "BUILD_DIR=%ROOT%\build-tests"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if not exist "%VSWHERE%" (
    echo Visual Studio Installer vswhere.exe was not found.
    echo Install Visual Studio Build Tools with C++ support.
    exit /b 1
)

for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%i"

if not defined VS_PATH (
    echo Visual Studio C++ build tools were not found.
    echo Install the Desktop development with C++ workload.
    exit /b 1
)

call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b 1

where cmake >nul 2>nul
if errorlevel 1 (
    echo CMake was not found in PATH.
    exit /b 1
)

where ninja >nul 2>nul
if errorlevel 1 (
    echo Ninja was not found in PATH.
    echo Restart your terminal after installing Ninja, then run this script again.
    exit /b 1
)

call "%ROOT%\scripts\ensure-synastria-hub.bat" "%ROOT%"
if errorlevel 1 exit /b 1

:: Release + AHC_OPT_PROFILE=lto: CMAKE_BUILD_TYPE=Release; MSVC /O2 + /GL (LTO) on ahc_core (see AHC_OPT_PROFILE in CMakeLists.txt).
cmake -S "%ROOT%" -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DAHC_OPT_PROFILE=lto -DAHC_BUILD_APP=OFF -DAHC_BUILD_TESTS=ON
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%"
if errorlevel 1 exit /b 1

ctest --test-dir "%BUILD_DIR%" --output-on-failure
if errorlevel 1 exit /b 1

echo Core build and tests completed successfully.
