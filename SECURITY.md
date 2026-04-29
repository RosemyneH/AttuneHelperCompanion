# Security policy

## Supported versions


| Version / channel              | Supported                                                                                        |
| ------------------------------ | ------------------------------------------------------------------------------------------------ |
| Latest `main` / default branch | Yes — security issues are triaged for current development head.                                  |
| **Tagged releases** (`v`*)     | **Best-effort** — critical fixes are considered for the most recent release line when practical. |
| Older tags / forks             | No guarantee — use current releases and verify checksums.                                        |


## Reporting a vulnerability

**Please do not** open a public issue for unfixed security vulnerabilities. That can put users at risk.

1. **Preferred:** use [GitHub **Private vulnerability reporting](https://docs.github.com/en/code-security/security-advisories/guidance-on-reporting-and-writing-information-about-vulnerabilities/privately-reporting-a-security-vulnerability)** for this repository (if enabled in **Settings → Security**).
2. If private reporting is unavailable, contact **repository maintainers** only through a private channel (for example, a direct message workflow your org provides). Do not post exploit details in Discussions, Issues, or public PRs until a fix is coordinated.

### What to include

- Affected component (desktop, Android, core `ahc_c`, build tooling, etc.).
- Steps to reproduce and impact (local only vs broader).
- Suggested fix or patch, if you have one (optional).

We aim to acknowledge reasonable reports in a timely manner. Response time depends on maintainer availability; this is a volunteer-run open source project.

## Scope and expectations

- **In scope:** bugs in *this* repository that could lead to information disclosure, unsafe file handling, or other security impact when used as designed (see [docs/threat-model.md](docs/threat-model.md)).
- **Out of scope (examples):** third-party game clients or add-ons not maintained here, generic OS misconfiguration, or issues that require the victim to run untrusted code with full trust in the app.

## Expected runtime behavior

During normal operation, security tools may report activity that is expected for this app:

- HTTPS requests to GitHub manifest endpoints on `raw.githubusercontent.com` for add-on catalog/preset refresh.
- Temporary cache file writes in the app config directory before validated JSON is promoted to cache.
- Clipboard read/write calls used by profile import/export UX.

These signals can resemble ATT&CK heuristics in automated sandboxes; treat them as suspicious only when they diverge from documented hosts, paths, or app features.

## Safe Harbor

We support good-faith security research. Do not test against systems you do not own or lack permission to test, and do not exfiltrate, modify, or destroy data beyond what is needed to demonstrate the issue to maintainers.