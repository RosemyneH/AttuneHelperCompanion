@echo off
setlocal EnableExtensions
:: ʕ •ᴥ•ʔ Optional maintainer check: same hub path CMake uses, warn if not at origin/main.
set "ROOT=%~1"
if not defined ROOT for %%I in ("%~dp0..") do set "ROOT=%%~fI"
if not exist "%ROOT%\CMakeLists.txt" (
  echo verify-synastria-hub-freshness: pass the AttuneHelperCompanion repo root as the first argument.
  exit /b 1
)

set "HUB_IN=%ROOT%\synastria-monorepo-addons\manifest\addons.json"
set "HUB_SIB=%ROOT%\..\synastria-monorepo-addons\manifest\addons.json"
if exist "%HUB_IN%" set "HUB_MANIFEST=%HUB_IN%"
if not defined HUB_MANIFEST if exist "%HUB_SIB%" set "HUB_MANIFEST=%HUB_SIB%"
if not defined HUB_MANIFEST (
  echo verify-synastria-hub-freshness: no synastria-monorepo-addons\manifest\addons.json under the repo or sibling. Clone the hub or run ensure-synastria-hub.bat first.
  exit /b 1
)

for %%I in ("%HUB_MANIFEST%") do set "HUB_MANIFEST=%%~fI"
for %%I in ("%HUB_MANIFEST%\..\..") do set "HUB_GIT_ROOT=%%~fI"

if not exist "%HUB_GIT_ROOT%\.git" (
  echo Hub manifest is not inside a .git work tree: %HUB_GIT_ROOT%
  echo Cannot compare to origin/main. CMake still uses: %HUB_MANIFEST%
  goto print_check
)

where git >nul 2>nul
if errorlevel 1 (
  echo git not in PATH; cannot check freshness.
  goto print_check
)

echo When AHC_HUB_ADDONS_JSON is unset, CMake order is in-repo then sibling ^(same as ensure-synastria-hub^).
if exist "%HUB_IN%" (
  if exist "%HUB_SIB%" (echo Both hub copies exist. CMake and this script use in-repo: %HUB_IN%) else (echo In-repo only: %HUB_MANIFEST%)
) else (
  echo Sibling only: %HUB_MANIFEST%
)
echo.

git -C "%HUB_GIT_ROOT%" rev-parse --is-inside-work-tree >nul 2>nul
if errorlevel 1 (
  echo %HUB_GIT_ROOT% is not a valid git work tree.
  goto print_check
)

git -C "%HUB_GIT_ROOT%" fetch -q origin 2>nul
if errorlevel 1 (
  echo Optional ^(warning^): could not run git fetch on the hub. Offline or no remote. Skipping version compare.
  echo Hub path: %HUB_GIT_ROOT%
  goto print_check
)

set "H_LOCAL="
set "H_REMOTE="
for /f "usebackq delims=" %%i in (`git -C "%HUB_GIT_ROOT%" rev-parse HEAD 2^>nul`) do set "H_LOCAL=%%i"
for /f "usebackq delims=" %%i in (`git -C "%HUB_GIT_ROOT%" rev-parse origin/main 2^>nul`) do set "H_REMOTE=%%i"
if not defined H_REMOTE (
  for /f "usebackq delims=" %%i in (`git -C "%HUB_GIT_ROOT%" rev-parse refs/remotes/origin/main 2^>nul`) do set "H_REMOTE=%%i"
)
if not defined H_REMOTE (
  echo Could not resolve origin/main. hub path: %HUB_GIT_ROOT%
  goto print_check
)
if not defined H_LOCAL set "H_LOCAL=missing"

if /i "%H_LOCAL%"=="%H_REMOTE%" (
  echo Hub clone is at origin/main, same commit as the remote.
) else (
  git -C "%HUB_GIT_ROOT%" merge-base --is-ancestor HEAD origin/main 2>nul
  if not errorlevel 1 (
    echo WARNING: local hub is behind origin/main. Update with:  git -C "%HUB_GIT_ROOT%" pull --ff-only origin main
  ) else (
    echo WARNING: local hub is not a fast-forward from origin/main ^(ahead, diverged, or detached^). See:  git -C "%HUB_GIT_ROOT%" status
  )
)

:print_check
echo.
if exist "%ROOT%\scripts\generate_addon_catalog.py" if exist "%HUB_MANIFEST%" (
  python "%ROOT%\scripts\generate_addon_catalog.py" --check --input "%HUB_MANIFEST%"
)
echo.
echo If the app shows a different add-on count than the build, the desktop may be using a fresh 24h remote cache. Delete the cache to force the baked list or a new download:
echo   %%APPDATA%%\AttuneHelperCompanion\addon_catalog_cache.json
echo On Linux, see XDG path attune-helper-companion/addon_catalog_cache.json under the config home used by the app.
echo.
exit /b 0
