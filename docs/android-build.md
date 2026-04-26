# Android app build

The `android/` directory is a standard Gradle (Kotlin) Android app used for the mobile companion. CI builds a **debug** APK on every workflow run; no in-repo keys or `secrets` are used.

## Project layout

| Path | Purpose |
|------|--------|
| `android/settings.gradle.kts` | Project name and `include(":app")` |
| `android/app/build.gradle.kts` | App module, SDK levels, dependencies; `assets` include `../manifest/addons.json` for the in-app add-on list |
| `android/gradlew` (and `gradlew.bat`) | Gradle wrapper (version pinned in `gradle/wrapper/`) |

## What the Android app does (v1+)

- **Synastria folder (SAF):** user picks a storage root (for example a shared or copied Synastria tree). The app walks it and reads `WTF/.../SavedVariables/AttuneHelper.lua` to parse the **DailyAttuneSnapshot** and keep a local attune log.
- **Sync:** full history can be **exported / imported** as a text token (`AHC1:` + gzip + Base64). A **QR** encodes **one day** only (compact `AHC-Q1:`) so it fits in a single code; use the long code to move a full log between devices.
- **Add-ons:** pick an add-on from the catalog; the app downloads the GitHub **codeload** zip (`main` or `master`) and extracts the add-on **folder** into `Interface/AddOns/…` under the chosen tree. Some repos with unusual layout or default branches may need a later release to improve.
- **Play:** **Open Winlator** uses `com.winlator` if installed; otherwise opens the official Winlator releases page. The game still runs **inside** Winlator; this host app does not start WoW directly.
- **NFC (P2P NDEF):** **Prepare NFC push** sets an Android-beam style message. Payload order: **attune** (`AHC1` if it fits, else one-day `AHC-Q1`) plus, when it still fits under the ~7 KB message limit, a second MIME part with **gzip+Base64 `AHC-LUA1:`** from the last successfully scanned `AttuneHelper.lua`. **Add-on archives** and an entire **WTF** tree are not transferable over NFC; use the in-app **GitHub zip** flow or file tools. Receiving needs NFC enabled, this activity in the **foreground**, and `singleTop` so `ACTION_NDEF_DISCOVERED` can be delivered.

Build output (local or CI): `android/app/build/outputs/apk/debug/app-debug.apk`

## Local build

1. Install **JDK 17** (or 21) and the **Android SDK** (Android Studio or `sdkmanager`).
2. From the repo root:

   ```bash
   cd android
   ./gradlew assembleDebug
   ```

3. On Windows, use `gradlew.bat` instead of `./gradlew` if you prefer.

`local.properties` with `sdk.dir=...` is optional if `ANDROID_HOME` is set; do not commit `local.properties` (it is usually gitignored for local paths).

## CI: Android APK (GitHub Actions)

The workflow [`.github/workflows/build.yml`](../.github/workflows/build.yml) includes a job **`build_apk`** that:

- Checks out the repository
- Sets up JDK 17 and the Android SDK (`android-actions/setup-android`)
- Uses **`gradle/actions/setup-gradle`** (Gradle user home + caching), then runs `./gradlew assembleDebug` with `working-directory: android` (not the deprecated `gradle-build-action` + `arguments` pattern)
- Uploads the artifact **`attune_helper_companion_android`** (the debug APK)

If **cache save** steps occasionally warn with GitHub’s “services aren’t available right now”, that is an infrastructure outage on the runner side, not a misconfiguration of this repo.

Releases for tags `v*` also attach the same debug APK to the GitHub Release. The plan [`.cursor/plans/android_pipeline_and_autologin_5f53a863.plan.md`](../.cursor/plans/android_pipeline_and_autologin_5f53a863.plan.md) keeps a short summary of the job name, trigger, and artifact for discoverability.
