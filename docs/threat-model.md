# Threat model: companion, autologin, and Android stub

This is a lightweight model for the **Attune Helper Companion** desktop app and the experimental **Android** JNI placeholder. It is not a formal security audit.

## Goals

- Keep **game account credentials** out of `settings.ini` and off unencrypted cloud sync of that file.
- **Reduce accidental injection** of `-login` / `-password` in “Launch parameters” when autologin supplies those flags.
- Make **data-at-rest** for the password depend on a normal user session (Windows **DPAPI** user scope) or a **user-only** file (Linux `0600`) where the OS enforces access.

## Trust boundaries

| Boundary | What crosses it | Risk |
| -------- | --------------- | ---- |
| **User’s Windows/Linux session** → **Synastria / Wow.exe** | Process command line and environment (Wine/Proton on Linux) | Malware in the same session can often read process memory, command lines, or key material; use a **unique** WoW password. |
| **Companion app** → **Config directory** | `settings.ini` (no password), `wow_autologin.cred` (password blob) | Anyone with the user account and that file (or a backup) can try to recover secrets; **Clear companion data** removes the cred file. |
| **Android stub (future)** | No production Keystore path in this repo yet; JNI is a **placeholder** | Do not store real passwords until a concrete Keystore or hardware-backed design exists. |

## Out of scope / limitations

- **No protection** against a compromised machine or a malicious high-privilege user: DPAPI and file permissions are not a sandbox against same-user malware.
- **Process listing** and **debuggers** on Windows can observe command lines while Wow.exe starts; the legacy client also receives `-password` on the command line.
- **Emulators** (e.g. Winlator-style) may have their own user profiles; DPAPI in a guest may not map to the host. See [android-winlator.md](android-winlator.md).

## References

- [user-testing.md](user-testing.md) — manual checks and autologin behavior
- [android-winlator.md](android-winlator.md) — host/guest and integration paths (A/B/C)
