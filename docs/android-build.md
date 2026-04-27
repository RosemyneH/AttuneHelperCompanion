# Android app build

The `android/` directory is a standard Gradle (Kotlin) Android app used for the mobile companion. CI always builds a **debug** APK. **Signed** release APKs for GitHub Releases are optional: set repository secrets and environment variables as described in [android-signing.md](android-signing.md). The app `fullBackupContent` policy excludes the attune history `SharedPreferences` file from [Android auto-backup](https://developer.android.com/guide/topics/data/autobackup) (see `res/xml/backup_rules.xml`); the manifest still has `android:allowBackup` enabled for the rest of app data.

## Project layout

| Path | Purpose |
|------|--------|
| `android/settings.gradle.kts` | Project name and `include(":app")` |
| `android/app/build.gradle.kts` | App module, SDK levels, dependencies; `assets` merge `../manifest` (presets) and `../../synastria-monorepo-addons/manifest` (addons.json) |
| `android/gradlew` (and `gradlew.bat`) | Gradle wrapper (version pinned in `gradle/wrapper/`) |

## What the Android app does (v1+)

- **Synastria folder (SAF):** user picks a storage root (for example a shared or copied Synastria tree). The app walks it and reads `WTF/.../SavedVariables/AttuneHelper.lua` to parse the **DailyAttuneSnapshot** and keep a local attune log.
- **Sync:** full history can be **exported / imported** as a text token (`AHC1:` + gzip + Base64). A **QR** encodes **one day** only (compact `AHC-Q1:`) so it fits in a single code; use the long code to move a full log between devices.
- **Add-ons:** pick an add-on from the catalog; the app downloads the GitHub **codeload** zip (`main` or `master`) and extracts the add-on **folder** into `Interface/AddOns/…` under the chosen tree. Some repos with unusual layout or default branches may need a later release to improve.
- **Play:** **Open Winlator** uses `com.winlator` if installed; otherwise opens the official Winlator releases page. The game still runs **inside** Winlator; this host app does not start WoW directly.
- **NFC (P2P NDEF):** **Prepare NFC push** sets an Android-beam style message. Payload order: **attune** (`AHC1` if it fits, else one-day `AHC-Q1`) plus, when it still fits under the ~7 KB message limit, a second MIME part with **gzip+Base64 `AHC-LUA1:`** from the last successfully scanned `AttuneHelper.lua`. **Add-on archives** and an entire **WTF** tree are not transferable over NFC; use the in-app **GitHub zip** flow or file tools. Receiving needs NFC enabled, this activity in the **foreground**, and `singleTop` so `ACTION_NDEF_DISCOVERED` can be delivered.

Build output: debug — `android/app/build/outputs/apk/debug/app-debug.apk`. Release (when signing env is set) — `android/app/build/outputs/apk/release/app-release.apk`.

## Local build

1. Install **JDK 17** (or 21) and the **Android SDK** (Android Studio or `sdkmanager`).
2. Point Gradle at the SDK: set **`ANDROID_HOME`** (or **`ANDROID_SDK_ROOT`**) to the SDK root, **or** create **`android/local.properties`** with `sdk.dir=...` (see [local.properties.example](../android/local.properties.example)). The file is **gitignored**; do not commit it.
3. From the repo root:

   **Windows (CI parity, same as `scripts/android-verify.sh`):**

   ```bat
   android-verify.bat
   ```

   It uses the default Android Studio path `%LOCALAPPDATA%\Android\Sdk` if `ANDROID_HOME` is unset and `local.properties` is missing, and writes `local.properties` for you in that case.

   **macOS / Linux / Git Bash:**

   ```bash
   bash scripts/android-verify.sh
   ```

   **Manual:**

   ```bash
   cd android
   ./gradlew assembleDebug
   ```

   On Windows, use `gradlew.bat` instead of `./gradlew` if you prefer.

## CI: Android APK (GitHub Actions)

The workflow [`.github/workflows/build.yml`](../.github/workflows/build.yml) includes a job **`build_apk`** that:

- Checks out the repository
- Sets up JDK 17 and the Android SDK (`android-actions/setup-android`)
- Uses **`gradle/actions/setup-gradle`** (Gradle user home + caching), then runs `./gradlew assembleDebug` with `working-directory: android` (not the deprecated `gradle-build-action` + `arguments` pattern)
- Uploads the artifact **`attune_helper_companion_android`** (the debug APK)
- On version tags, if `ANDROID_KEYSTORE_B64` (and related secrets) are configured, also builds **`assembleRelease`** and uploads **`attune_helper_companion_android_release`** (`app-release.apk`).

If **cache save** steps occasionally warn with GitHub’s “services aren’t available right now”, that is an infrastructure outage on the runner side, not a misconfiguration of this repo.

**GitHub Releases** (tags `v*`) attach a single Android file named **`AttuneHelperCompanion-android.apk`**: the signed release when available, otherwise the debug APK. See [android-signing.md](android-signing.md) for keystore and secret setup.
