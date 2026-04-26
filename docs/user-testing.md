# User Testing Checklist

**Automated coverage:** Core and library behavior is exercised by CTest in CI and locally (`build-test.bat`, `build-app.bat`, or `bash scripts/ci-build.sh`). **This document** is for manual, production-style checks the automated suite does not cover (tray, fullscreen, catalog UX, and real game folder integration).

Use a Release build of `attune_helper_companion.exe` from `build/` or a GitHub Actions artifact.

## Startup
- Launching the app should open only the companion window, without a debug console.
- The app should remember or detect the Synastria folder path.
- The Play Game button should stay visible and usable.
- With **AwesomeWotLK autologin** off, launch should behave as before (**WoWExt.exe**; ShellExecute/CreateProcess on Windows, Wine/Proton on Linux).
- With autologin **on**, after **Save autologin**, **Play Game** should start **Wow.exe** (not WoWExt) with `-login` / `-password` / `-realmname` (plus any validated **Launch parameters** as extras). If the password was not stored, **Play Game** should stay disabled and launch should show a clear message, not fail silently.

## Autologin and security (manual)
- The password is **not** written to `settings.ini`. On Windows it is stored with **DPAPI** in `wow_autologin.cred` under the app config directory; on Linux a **0600** file is used. **Clear companion data** should remove that credential file for the current profile. Anyone with access to the machine or a backup of that file may be able to recover stored credentials (DPAPI is tied to the Windows user), so use a **unique game password** and do not hand an untrusted person your user session. The password field in the app is **cleared from the UI buffer after a successful save**; a short time in process memory is still required to launch. **Android** in this repository is a **NDK/JNI placeholder** only until a Keystore-backed path exists; see [android-winlator.md](android-winlator.md) and [threat-model.md](threat-model.md).
- On Linux with Wine, keep **Launch parameters** free of trick `-login` / `-password` tokens when autologin is on; the app rejects such patterns in the extra field.

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
