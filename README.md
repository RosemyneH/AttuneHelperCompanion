# AttuneHelperCompanion

## AppImage (Linux x86_64)

From a clean clone, on a machine with OpenGL / X11 dev libraries (raylib’s usual Linux deps), a C compiler, `cmake`, and `curl` or `wget`:

```bash
bash packaging/appimage/build-appimage.sh
```

The script uses [linuxdeploy](https://github.com/linuxdeploy/linuxdeploy) to bundle dependencies; place an icon at `packaging/appimage/attune-helper-companion-256.png` or install `librsvg2-bin` (for `rsvg-convert`) or ImageMagick to render the SVG. The finished file appears under `build-appimage/`.
