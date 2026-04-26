# AttuneHelperCompanion

## AppImage (Linux x86_64)

From a clean clone, on a machine with OpenGL / X11 dev libraries (raylib’s usual Linux deps), a C compiler, `cmake`, and `curl` or `wget`:

```bash
bash packaging/appimage/build-appimage.sh
```

The script uses [linuxdeploy](https://github.com/linuxdeploy/linuxdeploy) to bundle dependencies; place an icon at `packaging/appimage/attune-helper-companion-256.png` or install `librsvg2-bin` (for `rsvg-convert`) or ImageMagick to render the SVG. The finished file appears under `build-appimage/`.

## GitHub Actions

[`.github/workflows/build.yml`](.github/workflows/build.yml) runs a **matrix** on `windows-2022` and `ubuntu-22.04` and calls the same [`scripts/ci-build.sh`](scripts/ci-build.sh) (after [msvc-dev-cmd](https://github.com/ilammy/msvc-dev-cmd) on Windows so MSVC is on `PATH`). `RUNNER_OS` is set for you by the runner, so the script can install Linux packages only when `RUNNER_OS=Linux` and `GITHUB_ACTIONS` is set. The Linux job then runs [`packaging/appimage/build-appimage.sh`](packaging/appimage/build-appimage.sh) and uploads `AttuneHelperCompanion-Linux-x86_64.AppImage` (not a plain executable name with no extension).

**Artifacts vs releases:** every run uploads **workflow artifacts** (per OS) you can download from the Actions run. Those are **not** the same as [Releases](https://docs.github.com/en/repositories/releasing-projects-on-github).

**Automated GitHub Release:** on a **tag** whose name starts with `v` (e.g. `v0.1.0`), an extra job publishes a **Release** and attaches the Windows `.exe` and a Linux x86_64 `AttuneHelperCompanion-Linux-x86_64.AppImage` using [softprops/action-gh-release](https://github.com/softprops/action-gh-release), with [generated release notes](https://docs.github.com/en/repositories/releasing-projects-on-github/automatically-generated-release-notes). The repository must allow the default `GITHUB_TOKEN` to **write** contents (Settings → Actions → General → Workflow permissions, or the org default). Example:

```bash
git tag v0.1.0
git push origin v0.1.0
```
