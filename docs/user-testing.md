# User Testing Checklist

**Automated coverage:** Core and library behavior is exercised by CTest in CI and locally (`build-test.bat`, `build-app.bat`, or `bash scripts/ci-build.sh`). **This document** is for manual, production-style checks the automated suite does not cover (tray, fullscreen, catalog UX, and real game folder integration).

Use a Release build of `attune_helper_companion.exe` from `build/` or a GitHub Actions artifact.

## Startup
- Launching the app should open only the companion window, without a debug console.
- The app should remember or detect the Synastria folder path.
- The Play Game button should stay visible and usable.

## Addons
- The Addons tab should stay responsive while scrolling and filtering.
- Community Favorites should be readable and visible near the top of the catalog.
- Category filters should show addons that belong to multiple categories.
- Installing, replacing, updating, and uninstalling managed addons should create backups when expected.

## Tray And Windowing
- Minimize/hide to tray should keep the app running.
- Left-clicking the tray icon should restore the app.
- Right-clicking the tray icon should show Open, Hide to tray, and Close.
- Fullscreen should toggle with the UI button, F11, and Alt+Enter.

## Snapshot History
- Scanning should load the current AttuneHelper saved variables snapshot.
- The status bar should report the loaded snapshot date or a clear setup/error message.

## Reporting Issues
Include Windows version, Synastria install path shape, what tab was open, the last action taken, and whether the app was visible, minimized, or hidden to tray.
