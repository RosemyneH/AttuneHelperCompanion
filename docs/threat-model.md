# Threat model: companion launch and Android companion

This is a lightweight model for the **Attune Helper Companion** desktop app and the **Android** (Kotlin) companion. It is not a formal security audit.

## Goals

- Keep desktop launch behavior simple: the app stores the game folder and user-provided Launch parameters, not game account credentials.
- Avoid adding secret-handling paths to `settings.ini` or Android app data until a dedicated credential design exists.

## Trust boundaries

| Boundary | What crosses it | Risk |
| -------- | --------------- | ---- |
| **User’s Windows/Linux session** → **Synastria / WoWExt.exe** | Process command line and environment (Wine/Proton on Linux) | Malware in the same session can often read process memory or command lines; avoid placing secrets in Launch parameters. |
| **Companion app** → **Config directory** | `settings.ini` with folder path, UI scale, and Launch parameters | Anyone with the user account or backups can read these local preferences. |
| **Android (Kotlin)** | Sync codes (`AHC1:`), QR, NFC, GitHub codeload zips, clipboard | Import paths merge **unauthenticated** snapshot data (by design) unless the user cancels (NFC prompt). **GitHub Releases** can ship a **signed** APK if maintainers set keystore secrets; **CI debug** builds are not release-signed. See [android-signing.md](android-signing.md). Auto-backup **excludes** the attune history preferences file; other app data may still be subject to device backup policy. Add-on install caps download and extracted size, resolves zip paths under a temp directory, and rejects `..` in entry names. |
| **Desktop: addon download / extract** | `https://` URLs only, no shell for `curl` / `git`; `tar -tf` preflight before unzipping | Malicious or compromised **zip** from a URL that passes validation could still be a problem at the **trust-the-catalog** layer; the app avoids shell injection and obvious zip slip. Non-`https` repo URLs in the manifest will fail the new download path. |
| **JNI on Android** | `CredentialsStore` is still a **stub** (NDK) | Do not store real game passwords in the Android app until a Keystore-backed design exists. |

## Out of scope / limitations

- **No protection** against a compromised machine or a malicious high-privilege user: local preferences and process state are visible to the same user.
- **Process listing** and **debuggers** on Windows/Linux can observe command lines while the launcher starts.
- **Emulators** (e.g. Winlator-style) may have their own user profiles and path mappings. See [android-winlator.md](android-winlator.md).

## Expected sandbox and EDR telemetry

- **Network fetches:** desktop/Android refreshes of add-on metadata call GitHub-hosted manifests on `raw.githubusercontent.com` over HTTPS.
- **Cache writes:** metadata fetches write temporary files under the app config directory (`addon_catalog_cache.tmp`, `addon_preset_cache.tmp`) and atomically promote validated JSON to cache files.
- **Clipboard activity:** import/export profile features intentionally read and write clipboard data and can be mapped to generic ATT&CK input-capture style heuristics.
- **False-positive ATT&CK tags:** behavioral engines may classify normal UI/process operations (module loading, process inspection, clipboard use) as credential/input techniques even when no credential dumping path exists in this project.

## References

- [user-testing.md](user-testing.md) — manual checks and launch behavior
- [android-winlator.md](android-winlator.md) — host/guest and integration paths (A/B/C)
- [android-build.md](android-build.md) — Gradle, CI, NFC overview
- [android-signing.md](android-signing.md) — release keystore and GitHub secrets
