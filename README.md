# AttuneHelperCompanion

## AppImage (Linux x86_64)

From a clean clone, on a machine with OpenGL / X11 dev libraries (raylib’s usual Linux deps), a C compiler, `cmake`, and `curl` or `wget`:

```bash
bash packaging/appimage/build-appimage.sh
```

The script uses [linuxdeploy](https://github.com/linuxdeploy/linuxdeploy) to bundle dependencies; place an icon at `packaging/appimage/attune-helper-companion-256.png` or install `librsvg2-bin` (for `rsvg-convert`) or ImageMagick to render the SVG. The finished file appears under `build-appimage/`.

## GitHub Actions

[`.github/workflows/build.yml`](.github/workflows/build.yml) runs a **matrix** on `windows-2022` and `ubuntu-22.04` and calls the same [`scripts/ci-build.sh`](scripts/ci-build.sh) (after [msvc-dev-cmd](https://github.com/ilammy/msvc-dev-cmd) on Windows so MSVC is on `PATH`). `RUNNER_OS` is set for you by the runner, so the script can install Linux packages only when `RUNNER_OS=Linux` and `GITHUB_ACTIONS` is set.
