<img width="1376" height="768" alt="image" src="https://github.com/user-attachments/assets/905f5589-30f9-4ff9-b9e5-b1c012f6096e" />

**STILL IN WIP** Post any issues in Issues

A **native C** companion for **Synastria** players: manage add-ons, read **AttuneHelper** daily snapshot history from local `SavedVariables`, and (on desktop) launch the game with optional user-provided launch parameters. A separate **Android (Kotlin)** app covers mobile workflows (folder access, attune log, share codes, catalog installs, Winlator handoff). The project keeps data **local**—it does not download the game client, manage user accounts, or store user files on a project-operated server.

- **In scope:** add-on management from static manifests and zip packages, local `WTF` / add-on backups under a chosen Synastria tree (`AttuneHelperBackup`), parsing **DailyAttuneSnapshot** from `AttuneHelper.lua`, and safe URL / zip handling in line with a documented [threat model](docs/threat-model.md).
- **Out of scope:** cloud sync of your game or credentials by this app, remote file hosting beyond what you configure (e.g. GitHub URLs in the catalog), and storing real game passwords on **Android** until a Keystore-backed design exists (JNI is currently a **stub**).

**License:** [GPL-3.0](LICENSE.md).

---

## What’s in the repo

| Area | Description |
|------|-------------|
| **Desktop app** | `attune_helper_companion` — **raylib** + **raygui** UI (Windows and Linux: Wine/Proton supported for launch paths). |
| **Core library** | `ahc_c` in `src/` — attune snapshot parsing, add-on manifest, arena helpers, safe URLs, and shared launch helpers (see `CMakeLists.txt`). |
| **Tests** | CTest targets under `tests/`; CI runs the same flow as [scripts/ci-build.sh](scripts/ci-build.sh). |
| **Add-on catalog** | **[synastria-monorepo-addons](https://github.com/RosemyneH/synastria-monorepo-addons) `manifest/addons.json`** (sibling clone) baked at build time via [scripts/generate_addon_catalog.py](scripts/generate_addon_catalog.py) — see [docs/synastria-monorepo-addons.md](docs/synastria-monorepo-addons.md). |
| **Android** | Gradle app under `android/` — Material UI, SAF to a Synastria root, attune log, `AHC1:` / QR / NFC flows, GitHub codeload zip install into `Interface/AddOns`. See [docs/android-build.md](docs/android-build.md). |
| **Packaging** | Linux **AppImage** build script under `packaging/appimage/`. |

For product goals and boundaries in one page, see [docs/scope.md](docs/scope.md). For desktop + Android trust boundaries and credential storage, see [docs/threat-model.md](docs/threat-model.md). For Winlator / split host–guest workflows, see [docs/android-winlator.md](docs/android-winlator.md).

---

## Features (summary)

**Desktop (raylib app)**

- **Synastria folder** — point at the install; the app reads `WTF/.../SavedVariables/AttuneHelper.lua` and shows attune **snapshot history** in the status area.
- **Add-ons** — browse the baked catalog (filters, “Community Favorites,” install / update / uninstall with backups where designed).
- **Play Game** — launches **WoWExt.exe** / **WowExt.exe** with the Launch parameters entered in Settings.
- **Tray and windowing** — hide to tray, restore, fullscreen (UI / F11 / Alt+Enter).

**Android**

- Pick a **storage root** (SAF), walk to **AttuneHelper** saved variables, keep a local attune log.
- **Sync:** export/import a text token (`AHC1:…`); **QR** for a single compact day (`AHC-Q1:`) when a full log does not fit.
- **Add-ons** — download from GitHub **codeload** zips per catalog entry into `Interface/AddOns` (size limits and path checks apply).
- **Play** — open **Winlator** if installed; the game still runs in the guest environment.
- **NFC** — push attune payload (and optional Lua part when it fits under limits); read [docs/android-build.md](docs/android-build.md) for constraints.

**NDK / CMake:** optional `AHC_BUILD_ANDROID` links stub native pieces for future work; the shipped Android experience is the Kotlin app, not the full raylib UI on device.

---

## Build requirements

- **CMake** 3.24+
- **C11** compiler (MSVC with Desktop C++, or GCC/Clang on Linux)
- **Python 3** (add-on catalog generation and `--check` in CI)
- **Ninja** (used by the provided Windows and CI scripts; other generators can work if you adapt flags)
- **Desktop app:** [raylib](https://github.com/raysan5/raylib) 5.5 and [raygui](https://github.com/raysan5/raygui) 4.0 are **fetched by CMake** when `AHC_BUILD_APP` is on.

**Linux (desktop)** additionally needs the usual OpenGL / X11 (or Wayland) dev packages for raylib—see [scripts/ci-build.sh](scripts/ci-build.sh) for an `apt-get` list used in CI.

**Android:** JDK 17+ and Android SDK; `ANDROID_HOME` or `android/local.properties` with `sdk.dir`. See [docs/android-build.md](docs/android-build.md).

---

## Build and test (local)

**Windows (from repo root):**

- First-time or clean configure: [build-test.bat](build-test.bat) (core tests only) or [build-app.bat](build-app.bat) (full app, tests, launch).
- After `CMakeCache.txt` exists: [verify-incremental.bat](verify-incremental.bat) (incremental build + `ctest`). You can set `%AHC_BUILD_DIR%` if the build directory is not `build`.

**Linux / macOS / Git Bash (CI parity):**

```bash
bash scripts/ci-build.sh
```

This runs `generate_addon_catalog.py --check`, configures with **Ninja**, **Release**, **LTO** (`-DAHC_OPT_PROFILE=lto`), builds, and runs **ctest**.

**Android (CI parity with `assembleDebug`):**

```bash
bash scripts/android-verify.sh
```

---

## Linux AppImage (x86_64)

From a clean clone, on a machine with OpenGL / X11 dev libraries (as for raylib), a C compiler, `cmake`, and `curl` or `wget`:

```bash
bash packaging/appimage/build-appimage.sh
```

The script uses [linuxdeploy](https://github.com/linuxdeploy/linuxdeploy) to bundle dependencies. Add an icon at `packaging/appimage/attune-helper-companion-256.png` or install `librsvg2-bin` (for `rsvg-convert`) or ImageMagick to render the SVG. Output appears under `build-appimage/`.

---

## GitHub Actions and releases

[`.github/workflows/build.yml`](.github/workflows/build.yml) runs a **matrix** on `windows-2022` and `ubuntu-22.04` and invokes [`scripts/ci-build.sh`](scripts/ci-build.sh) (on Windows, after [msvc-dev-cmd](https://github.com/ilammy/msvc-dev-cmd) so MSVC is on `PATH`). A separate job builds the **debug Android APK** via [scripts/android-verify.sh](scripts/android-verify.sh). On `ubuntu-22.04`, the Linux job can also run the AppImage script and upload `AttuneHelperCompanion-Linux-x86_64.AppImage`.

**Artifacts vs GitHub Releases:** every workflow run uploads **workflow artifacts** per job; those are not the same as [GitHub Releases](https://docs.github.com/en/repositories/releasing-projects-on-github).

**Automated release (tags):** on a **tag** whose name starts with `v` (e.g. `v0.1.0`), a release job can publish a **Release** and attach the Windows `.exe`, Linux **AppImage**, and **`AttuneHelperCompanion-android.apk`** (signed release if keystore [secrets](docs/android-signing.md) are set; otherwise the debug APK). The repository must allow the default `GITHUB_TOKEN` to **write** contents where the workflow requires it. Example:

```bash
git tag v0.1.0
git push origin v0.1.0
```

---

## Manual checks (not covered by CTest)

Tray behavior, fullscreen, add-ons tab UX, and real Synastria folder integration are **manual**; use [docs/user-testing.md](docs/user-testing.md).

---

## Documentation index

| Document | Purpose |
|----------|---------|
| [docs/scope.md](docs/scope.md) | Product scope and distribution model |
| [docs/threat-model.md](docs/threat-model.md) | Trust boundaries, launch behavior, Android vs desktop |
| [docs/user-testing.md](docs/user-testing.md) | Manual QA checklist |
| [docs/android-build.md](docs/android-build.md) | Gradle, CI APK, NFC overview |
| [docs/android-winlator.md](docs/android-winlator.md) | Android + Winlator integration paths (A / B / C) |
| [docs/android-signing.md](docs/android-signing.md) | Release keystore and GitHub Actions secrets |
| [AGENTS.md](AGENTS.md) | Contributor/agent notes (hooks, verify commands) |
| [CONTRIBUTING.md](CONTRIBUTING.md) | How to contribute, what to run before a PR, license |
| [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) | Community code of conduct |
| [SECURITY.md](SECURITY.md) | How to report security issues responsibly |

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines, the [code of conduct](CODE_OF_CONDUCT.md), and PR/issue expectations. Quick checks: hub catalog `python scripts/generate_addon_catalog.py --check --input ../synastria-monorepo-addons/manifest/addons.json` when the catalog or generator changes; [verify build and test](.cursor/rules/verify-build-and-test.mdc) and [AGENTS.md](AGENTS.md) for local verification.

---

## GitHub metadata (for maintainers)

The [description](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/managing-settings-for-your-repository#adding-a-repository-description) is set in **Settings → General** (not in git). Suggested one-liner for **About**:

*Native C + Android companion for Synastria: local add-on management, AttuneHelper daily snapshots, desktop launch helpers; data stays on your machine. GPL-3.0.*

Optional [topics](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/managing-repository-settings#adding-topics) for search: e.g. `synastria`, `wow`, `attune`, `raylib`, `android`, `cmake`, `game-addon`. Issue forms and the PR template are under [`.github/`](.github/). In **Settings → [Code security]**, enable [private vulnerability reporting](https://docs.github.com/en/code-security/security-advisories/guidance-on-reporting-and-writing/privately-reporting-a-security-vulnerability) so [SECURITY.md](SECURITY.md) can be fully effective on GitHub.
