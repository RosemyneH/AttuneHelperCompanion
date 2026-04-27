# Agent notes — Attune Helper Companion

## Verification

- **Project rule (always on)**: [.cursor/rules/verify-build-and-test.mdc](.cursor/rules/verify-build-and-test.mdc) — when changing C, CMake, manifest, the addon catalog script, or Android resources, run the build and tests (or the catalog / Android verify script) as described there.
- **Post-edit hook**: [.cursor/hooks.json](.cursor/hooks.json) — on file edits, runs [.cursor/hooks/after-c-edit-test.ps1](.cursor/hooks/after-c-edit-test.ps1), which for `.c`/`.h` source paths calls [verify-incremental.bat](verify-incremental.bat) (incremental `cmake --build` + `ctest` against `build` or `AHC_BUILD_DIR`). First-time setup: run [build-test.bat](build-test.bat) or [build-app.bat](build-app.bat) so a CMake build directory exists.
- **Batch entry points**: [build-test.bat](build-test.bat) (core tests), [build-app.bat](build-app.bat) (full app + tests + launch), [verify-incremental.bat](verify-incremental.bat) (fast re-verify), [android-verify.bat](android-verify.bat) (Android **assembleDebug**, same as GitHub CI).
- **Manual “production” / UI checks** (tray, fullscreen, addons tab): [docs/user-testing.md](docs/user-testing.md) — not automated; run when changing behavior users see in the app.
- **Android signed releases** (tags `v*` on GitHub): [docs/android-signing.md](docs/android-signing.md) — set `ANDROID_KEYSTORE_B64` and related repository secrets; attach `AttuneHelperCompanion-android.apk` from the signed `assembleRelease` output (falls back to debug if secrets are absent).

## Useful paths

- Core library and tests: [CMakeLists.txt](CMakeLists.txt), [src/](src/), [tests/](tests/)
- CI script: [scripts/ci-build.sh](scripts/ci-build.sh)
- Add-on catalog: upstream-first [manifest/addons.json](manifest/addons.json) (curated / git clone targets), [scripts/generate_addon_catalog.py](scripts/generate_addon_catalog.py) (validate + baked C), optional [scripts/emit_curated_addon_manifest.py](scripts/emit_curated_addon_manifest.py) to rebuild the slim list from a prior export, and [docs/curated-monorepo.md](docs/curated-monorepo.md) for the **separate** `wotlk-curated-addons` git repo (sibling directory; publish with its own remote). Felbite bulk import: [scripts/import_web_catalogs.py](scripts/import_web_catalogs.py) with `--sources felbite` only.
- Android debug build (CI parity): [android-verify.bat](android-verify.bat) (Windows; uses default Studio SDK path when `local.properties` is missing) or [scripts/android-verify.sh](scripts/android-verify.sh) (Bash) — JDK + [Android SDK](docs/android-build.md#local-build) required.
- Android release signing: [docs/android-signing.md](docs/android-signing.md)
