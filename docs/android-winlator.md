# Android, Winlator-style guests, and the companion

This document describes **practical integration paths (A / B / C)** for people who want **Attune Helper Companion** (or a similar **Synastria + WoW 3.3.5a** workflow) near **Android** or **Windows-in-a-box** emulators. It does **not** document a public API of third-party emulators; treat names as examples and verify against your own build.

## What works in this repo (desktop)

- **Windows / Linux (Wine or Proton):** the companion can launch **WoWExt.exe** / **WowExt.exe** with user-entered Launch parameters.
- **Android folder:** Gradle app + NDK **stub** (`CredentialsStore` JNI) for **arm64-v8a** only, to align future Keystore / native work. The full **raylib** UI is not shipped as a production Android app here.

## Path A — Desktop companion in the same OS as the game (simplest)

Use the **Windows** or **Linux** build in the same environment that can run **WoWExt.exe** / **WowExt.exe**.

- **Synastria** path in Settings must be the path **the launcher process** sees.
- **Launch parameters** are passed to the launcher as user-provided extras.

**Winlator (or similar):** if the companion runs *inside* the emulated Windows/Linux guest, use the **guest** path to Synastria.

## Path B — Android host, game in a guest (split worlds)

1. The **Android** app in this repo has a **Material** UI (not the desktop raylib build): you pick a **Synastria root** (storage access) so the app can read **AttuneHelper.lua**, keep a local attune log, **export/import** a shareable code, show a one-day **QR**, install add-ons from the shipped **GitHub codeload** list into `Interface/AddOns`, and **open Winlator** if `com.winlator` is present. The on-device **Play** action does not start WoW; it starts Winlator so the guest can run the real client.
2. A typical **split** path remains: read-only or file-manager style work on the phone + **sideloaded** Windows/Linux companion in the guest, or a **remote** session (VNC, Steam Remote Play, or your own) where the real launcher runs.

Risks: Android host paths and guest paths are separate; do not expect the Android app to start the desktop launcher directly across that boundary.

## Path C — Developer / CI only (this repository)

- Build **ahc_core** and tests with CMake; enable **`AHC_BUILD_ANDROID`** in the top-level **CMake** to pick up the **`ahc_android_stub`** interface for experiments.
- The **app** `ndk { abiFilters "arm64-v8a" }` and **`credentials_ndk`** JNI are **placeholders**; they do not implement a secure enclave or cloud sync.

## Launch notes (all paths)

- Keep Launch parameters to the flags you want the WoWExt launcher to receive.
- Prefer a **dedicated** WoW password; assume **any process running as you** can eventually observe secrets you type or pass to the game.

## See also

- [threat-model.md](threat-model.md)
- [user-testing.md](user-testing.md)
- [scope.md](scope.md)
