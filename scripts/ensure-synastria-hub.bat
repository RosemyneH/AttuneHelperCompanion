@echo off
setlocal EnableExtensions
:: ʕ •ᴥ•ʔ Ensures synastria-monorepo-addons manifest exists (clone if missing) and runs generate_addon_catalog --check.
:: This script does not run git pull: an existing in-repo or sibling clone is left as-is. In-repo path wins over sibling for CMake
:: when AHC_HUB_ADDONS_JSON is unset. To compare your clone to origin/main, run: scripts\verify-synastria-hub-freshness.bat "%~dp0.."
set "ROOT=%~1"
if not defined ROOT (
    echo ensure-synastria-hub: missing repo root argument.
    exit /b 1
)

set "HUB_IN=%ROOT%\synastria-monorepo-addons\manifest\addons.json"
set "HUB_SIB=%ROOT%\..\synastria-monorepo-addons\manifest\addons.json"

if exist "%HUB_IN%" set "HUB_MANIFEST=%HUB_IN%"
if not defined HUB_MANIFEST if exist "%HUB_SIB%" set "HUB_MANIFEST=%HUB_SIB%"

if defined HUB_MANIFEST goto run_check

where git >nul 2>nul
if errorlevel 1 (
    echo git was not found in PATH. Install Git for Windows to auto-clone synastria-monorepo-addons.
    exit /b 1
)

echo Cloning synastria-monorepo-addons shallow clone into "%ROOT%\synastria-monorepo-addons" ...
git clone --depth 1 https://github.com/RosemyneH/synastria-monorepo-addons.git "%ROOT%\synastria-monorepo-addons"
if errorlevel 1 exit /b 1

set "HUB_MANIFEST=%ROOT%\synastria-monorepo-addons\manifest\addons.json"
if not exist "%HUB_MANIFEST%" (
    echo Expected hub manifest not found after clone: %HUB_MANIFEST%
    exit /b 1
)

:run_check
where python >nul 2>nul
if errorlevel 1 (
    echo Python not in PATH; skipping generate_addon_catalog.py --check ^(CMake will still validate at configure^).
    exit /b 0
)

python "%ROOT%\scripts\generate_addon_catalog.py" --check --input "%HUB_MANIFEST%"
if errorlevel 1 exit /b 1
exit /b 0
