# Android app build

The `android/` directory is a standard Gradle (Kotlin) Android app used for the mobile companion. CI builds a **debug** APK on every workflow run; no in-repo keys or `secrets` are used.

## Project layout

| Path | Purpose |
|------|--------|
| `android/settings.gradle.kts` | Project name and `include(":app")` |
| `android/app/build.gradle.kts` | App module, SDK levels, dependencies |
| `android/gradlew` (and `gradlew.bat`) | Gradle wrapper (version pinned in `gradle/wrapper/`) |

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
- Runs `assembleDebug` in `android/`
- Uploads the artifact **`attune_helper_companion_android`** (the debug APK)

Releases for tags `v*` also attach the same debug APK to the GitHub Release. The plan [`.cursor/plans/android_pipeline_and_autologin_5f53a863.plan.md`](../.cursor/plans/android_pipeline_and_autologin_5f53a863.plan.md) keeps a short summary of the job name, trigger, and artifact for discoverability.
