# Android release signing (GitHub)

## Generate a keystore locally (outside the repo)

The script [scripts/generate-android-keystore.ps1](../scripts/generate-android-keystore.ps1) creates:

- `E:\Security\AttuneHelperCompanion\attune-release.jks`
- `E:\Security\AttuneHelperCompanion\CREDENTIALS-READ-ONCE-STORE-SAFELY.txt` (passwords + Base64 for `ANDROID_KEYSTORE_B64`)

**Requirements:** `keytool` from a JDK. If it is not on your `PATH`, the script still tries **Microsoft OpenJDK** and **Android Studio `jbr\bin`**.

**Install OpenJDK 17 (includes `keytool`) with winget:**

```powershell
winget install --id Microsoft.OpenJDK.17 -e
```

(There is no separate `keytool` package: it ships inside the JDK.) Then either open a **new** terminal so `PATH` updates, or rely on the script’s path discovery.

Run from the repo root:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\generate-android-keystore.ps1
```

To **replace** an existing `attune-release.jks` in `E:\Security\AttuneHelperCompanion\` (and regenerate the credentials file), add **`-Force`**.

```powershell
powershell -ExecutionPolicy Bypass -File scripts\generate-android-keystore.ps1 -Force
```

This is not a “seed” in the crypto-wallet sense: Android uses a **Java keystore file** and passwords. Keep `E:\Security\…` off any git remote; back it up to something only you control.

---

Release builds use a keystore that **never** lives in the repository. Local and CI both read the same environment variable names as Gradle (see [android/app/build.gradle.kts](../android/app/build.gradle.kts)).

## Environment variables (Gradle / CI)

| Variable | Purpose |
| -------- | -------- |
| `ANDROID_KEYSTORE_FILE` | Absolute path to the `.jks` / `.keystore` file |
| `ANDROID_KEYSTORE_PASSWORD` | Keystore password |
| `ANDROID_KEY_ALIAS` | Signing key alias |
| `ANDROID_KEY_PASSWORD` | Key password (often same as keystore for a single-key store) |

If all four are set and the keystore file exists, `release` uses that `signingConfig`. Otherwise the release type is still built but is **unsigned** (or not signed in the sense of a release key—use `assembleDebug` for day-to-day work).

## GitHub Actions (tags `v*`)

Add these **repository secrets** (and optional variable for the file payload):

| Secret | Purpose |
| ------ | -------- |
| `ANDROID_KEYSTORE_B64` | Base64 encoding of the keystore **file** (entire binary) |
| `ANDROID_KEYSTORE_PASSWORD` | Keystore password |
| `ANDROID_KEY_ALIAS` | Key alias |
| `ANDROID_KEY_PASSWORD` | Key password |

On a version tag, the workflow decodes `ANDROID_KEYSTORE_B64` to `android/release-ci.keystore`, runs `assembleRelease` with the env vars above, and uploads the signed `app-release.apk`. The **GitHub Release** job copies that to `AttuneHelperCompanion-android.apk` when present; if secrets are missing, it falls back to the debug APK and logs a message in the job log.

Fork pull requests do not receive these secrets, so they only produce unsigned debug artifacts.

## Local signed release (optional)

1. Create or use an existing keystore (e.g. `keytool`).
2. Set the four `ANDROID_KEYSTORE_*` and `ANDROID_KEY_ALIAS` / `ANDROID_KEY_PASSWORD` environment variables in your shell or IDE, pointing `ANDROID_KEYSTORE_FILE` at your keystore path.
3. `cd android && ./gradlew assembleRelease` (Windows: `gradlew.bat`).

Install the result from `app/build/outputs/apk/release/` on a device and allow “install unknown apps” as usual for side-loaded APKs.

Do not commit keystore files; `.gitignore` already ignores `*.jks` and `*.keystore` at the repo root and `keystore.properties` under `android/`.
