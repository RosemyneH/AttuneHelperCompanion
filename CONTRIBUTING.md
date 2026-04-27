# Contributing to Attune Helper Companion

Thanks for your interest. This project is a native **C** desktop companion and **Android (Kotlin)** helper for [Synastria](https://synastria.com/) players; goals and boundaries are in [docs/scope.md](docs/scope.md) and [README.md](README.md).

## Code of conduct

Participation is governed by the [Code of conduct](CODE_OF_CONDUCT.md). Be constructive and respect other contributors and users.

## How to help

- **Bugs and ideas:** use the [issue templates](.github/ISSUE_TEMPLATE/) (bug / feature) so we get enough context. Search existing issues first.
- **Pull requests:** keep changes **focused** on a single goal; match existing style, naming, and build flows. The [PR template](.github/pull_request_template.md) is shown when you open a pull request on GitHub.
- **Scope:** this app is **local-first**; features that add mandatory cloud services or off-repo account handling are out of scope unless the maintainers and docs explicitly adopt them. See [docs/threat-model.md](docs/threat-model.md).

## What to run before you submit (high level)

| Change | Verify |
|--------|--------|
| C, CMake, core headers | Incremental or full build + **CTest** (see [AGENTS.md](AGENTS.md), [verify build rule](.cursor/rules/verify-build-and-test.mdc)) |
| Hub [synastria-monorepo-addons](https://github.com/RosemyneH/synastria-monorepo-addons) `manifest/addons.json` or [scripts/generate_addon_catalog.py](scripts/generate_addon_catalog.py) | `python scripts/generate_addon_catalog.py --check` (resolves sibling or `./synastria-monorepo-addons/` like CMake; optional `--input` or `AHC_HUB_ADDONS_JSON`) |
| Android app / resources | [android-verify](android-verify.bat) (Windows) or [scripts/android-verify.sh](scripts/android-verify.sh) |

**Windows (cmd, repo root), after CMake is configured:** [verify-incremental.bat](verify-incremental.bat)  
**CI parity (Linux / Git Bash / macOS):** `bash scripts/ci-build.sh` (and `bash scripts/android-verify.sh` for Android).

## License

By contributing, you agree that your contributions are under the same license as the project: [GPL-3.0](LICENSE.md).

## Security

Do **not** use public issues to disclose unfixed security problems. See [SECURITY.md](SECURITY.md).

## Questions

If an issue is not a bug or feature, use **Discussions** if the repository has them enabled, or open a clearly scoped issue. Avoid duplicating work already described in [docs/](docs/) or the README.
