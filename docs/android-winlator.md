# Android, Winlator-style guests, and the companion

This document describes **practical integration paths (A / B / C)** for people who want **Attune Helper Companion** (or a similar **Synastria + WoW 3.3.5a** workflow) near **Android** or **Windows-in-a-box** emulators. It does **not** document a public API of third-party emulators; treat names as examples and verify against your own build.

## What works in this repo (desktop)

- **Windows / Linux (Wine or Proton):** the companion can launch **WoWExt.exe** (normal path) or **Wow.exe** with **AwesomeWotLK autologin** (`-login`, `-password`, `-realmname` plus optional validated extras).
- **Android folder:** Gradle app + NDK **stub** (`CredentialsStore` JNI) for **arm64-v8a** only, to align future Keystore / native work. The full **raylib** UI is not shipped as a production Android app here.

## Path A — Desktop companion in the same OS as the game (simplest)

Use the **Windows** or **Linux** build in the same environment that can run **Wow.exe** / **WoWExt.exe**.

- **Synastria** path in Settings must be the path **the launcher process** sees.
- **Autologin** stores the password under the companion’s **config directory** (DPAPI on Windows, `0600` file on Linux), not in `settings.ini`.

**Winlator (or similar):** if the companion runs *inside* the emulated Windows/Linux guest, use the **guest** path to Synastria. If the guest user profile is reset, **Save autologin** again so credentials match the new identity.

## Path B — Android host, game in a guest (split worlds)

1. The **Android** build in this repo is currently a **stub** (no full raylib UI; the main screen explains that WoW is not run natively and points to **Winlator’s official project** for users who need a Windows-in-a-box layer). Forks and other emulators are not wired in; users choose their own trusted source.
2. A typical future shape is: **read-only** catalog / docs on the phone + **sideloaded** Windows/Linux companion, or a **remote** session (VNC, Steam Remote Play, or your own) where the real launcher runs.

Risks: no shared DPAPI between Android and a random guest Windows user; do not expect **wow_autologin.cred** from the desktop to “just work” across that boundary.

## Path C — Developer / CI only (this repository)

- Build **ahc_core** and tests with CMake; enable **`AHC_BUILD_ANDROID`** in the top-level **CMake** to pick up the **`ahc_android_stub`** interface for experiments.
- The **app** `ndk { abiFilters "arm64-v8a" }` and **`credentials_ndk`** JNI are **placeholders**; they do not implement a secure enclave or cloud sync.

## Credential notes (all paths)

- **Do not** put **`-password` / `-login` in Launch parameters** when autologin is on; the app strips those patterns on save and blocks launch if they remain.
- Prefer a **dedicated** WoW password; assume **any process running as you** can eventually observe secrets used to start the game.

## See also

- [threat-model.md](threat-model.md)
- [user-testing.md](user-testing.md)
- [scope.md](scope.md)
