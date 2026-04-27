@echo off
setlocal

set "ROOT=%~dp0"
set "ROOT=%ROOT:~0,-1%"
set "BUILD_DIR=%ROOT%\build"
set "APP_EXE=%BUILD_DIR%\attune_helper_companion.exe"
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

echo Closing any running Attune Helper Companion before rebuilding.
taskkill /f /im attune_helper_companion.exe >nul 2>nul

call "%ROOT%\scripts\ensure-synastria-hub.bat" "%ROOT%"
if errorlevel 1 exit /b 1
echo Hub clone is not auto-updated on each run; to check freshness use scripts\verify-synastria-hub-freshness.bat

echo Configuring app build. The first run downloads raylib and raygui.
:: Release + AHC_OPT_PROFILE=lto: CMAKE_BUILD_TYPE=Release; MSVC /O2 + /GL (LTO) on ahc_core (see AHC_OPT_PROFILE in CMakeLists.txt).
cmake -S "%ROOT%" -B "%BUILD_DIR%" --fresh -G Ninja -DCMAKE_BUILD_TYPE=Release -DAHC_OPT_PROFILE=lto -DAHC_BUILD_APP=ON -DAHC_BUILD_TESTS=ON
if errorlevel 1 exit /b 1

echo Building app and tests.
cmake --build "%BUILD_DIR%"
if errorlevel 1 exit /b 1

echo Running ctest.
ctest --test-dir "%BUILD_DIR%" --output-on-failure
if errorlevel 1 exit /b 1

echo App build completed successfully. CMake configure prints AHC_HUB_ADDONS_JSON (the file that bakes the catalog).
echo If the add-on list in the app does not match that file, delete AppData\AttuneHelperCompanion\addon_catalog_cache.json or ensure build\manifest\addons.json is newer than the cache.

if not exist "%APP_EXE%" (
    echo Expected app executable was not found: %APP_EXE%
    exit /b 1
)

echo Launching Attune Helper Companion.
start "" "%APP_EXE%"
