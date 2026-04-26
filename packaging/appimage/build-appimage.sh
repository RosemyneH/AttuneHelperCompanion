#!/usr/bin/env bash
# Builds a self-contained x86_64 AppImage. Requires: cmake, a C compiler, (ninja optional), libGL/X11 build deps for raylib.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
APPDIR="${AHC_APPDIR:-$ROOT/build-appimage/AppDir}"
BUILD_DIR="${AHC_CMAKE_BUILD:-$ROOT/build-appimage/cmake}"
OUT_DIR="${AHC_OUT:-$ROOT/build-appimage}"
DE="$ROOT/packaging/appimage/attune-helper-companion.desktop"
ICON_RENDER="$OUT_DIR/attune-helper-companion-256.png"
ICON_FILE="$OUT_DIR/attune-helper-companion.png"

VERSION="${AHC_VERSION:-}"
if [ -z "$VERSION" ]; then
  VERSION="$(
    sed -n 's/^[[:space:]]*project(AttuneHelperCompanion VERSION[[:space:]]\{1,\}\([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt" | head -n1
  )"
  [ -z "$VERSION" ] && VERSION="0.1.0"
fi

ARCH="${ARCH:-$(uname -m)}"
if [ "$ARCH" != "x86_64" ]; then
  echo "This script is intended for x86_64. ARCH=$ARCH" >&2
  exit 1
fi

if [ -z "${SKIP_CLEAN:-}" ]; then
  rm -rf "$APPDIR" "$BUILD_DIR"
fi

mkdir -p "$OUT_DIR"

# Icon: images/green_ahc_logo.png first, then pre-rendered, rsvg, or ImageMagick.
if [ -f "$ROOT/images/green_ahc_logo.png" ]; then
  if command -v magick &>/dev/null; then
    magick "$ROOT/images/green_ahc_logo.png" -resize 256x256 "$ICON_RENDER" || true
  elif command -v convert &>/dev/null; then
    convert "$ROOT/images/green_ahc_logo.png" -resize 256x256 "$ICON_RENDER" || true
  else
    cp -f "$ROOT/images/green_ahc_logo.png" "$ICON_RENDER" || true
  fi
fi
if [ ! -f "$ICON_RENDER" ] && [ -f "$ROOT/packaging/appimage/attune-helper-companion-256.png" ]; then
  cp -f "$ROOT/packaging/appimage/attune-helper-companion-256.png" "$ICON_RENDER"
fi
if [ ! -f "$ICON_RENDER" ] && command -v rsvg-convert &>/dev/null; then
  rsvg-convert -w 256 -h 256 "$ROOT/packaging/appimage/attune-helper-companion.svg" -o "$ICON_RENDER"
fi
if [ ! -f "$ICON_RENDER" ] && command -v magick &>/dev/null; then
  magick -size 256x256 "xc:rgb(13,21,32)" -gravity center -pointsize 64 -fill "#c8d8f0" -font "DejaVu-Sans-Bold" -annotate 0 "AHC" "$ICON_RENDER" || true
fi
if [ ! -f "$ICON_RENDER" ] && command -v convert &>/dev/null; then
  convert -size 256x256 "xc:rgb(13,21,32)" -gravity center -pointsize 64 -fill "#c8d8f0" -font "DejaVu-Sans-Bold" -annotate 0 "AHC" "$ICON_RENDER" || true
fi
if [ ! -f "$ICON_RENDER" ]; then
  echo "No icon. Ensure images/green_ahc_logo.png exists" >&2
  echo "  and ImageMagick (magick or convert) is available to produce 256x256, or add packaging/appimage/attune-helper-companion-256.png" >&2
  echo "  or install rsvg-convert, imagemagick, or the ImageMagick 'convert' tool." >&2
  exit 1
fi
cp -f "$ICON_RENDER" "$ICON_FILE"

CMAKE_GEN=()
if command -v ninja &>/dev/null; then
  CMAKE_GEN=(-G Ninja)
fi

cmake -S "$ROOT" -B "$BUILD_DIR" "${CMAKE_GEN[@]}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DAHC_OPT_PROFILE=release \
  -DAHC_BUILD_APP=ON \
  -DAHC_BUILD_BENCH=OFF \
  -DAHC_BUILD_TESTS=OFF \
  -DCMAKE_INSTALL_PREFIX=/usr

cmake --build "$BUILD_DIR" --config Release
DESTDIR="$APPDIR" cmake --install "$BUILD_DIR" --prefix /usr

LINUXDEPLOY="${LINUXDEPLOY:-$OUT_DIR/linuxdeploy-x86_64.AppImage}"
if [ ! -f "$LINUXDEPLOY" ] || [ "${AHC_FETCH_LINUXDEPLOY:-0}" = 1 ]; then
  echo "Downloading linuxdeploy (continuous)..."
  if command -v curl &>/dev/null; then
    curl -fsSL -o "$LINUXDEPLOY" "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
  else
    wget -q -O "$LINUXDEPLOY" "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
  fi
  chmod a+x "$LINUXDEPLOY"
fi
PLUGIN="${LINUXDEPLOY_PLUGIN:-$OUT_DIR/linuxdeploy-plugin-appimage-x86_64.AppImage}"
if [ ! -f "$PLUGIN" ] || [ "${AHC_FETCH_PLUGIN:-0}" = 1 ]; then
  echo "Downloading linuxdeploy-plugin-appimage..."
  if command -v curl &>/dev/null; then
    curl -fsSL -o "$PLUGIN" "https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/releases/download/continuous/linuxdeploy-plugin-appimage-x86_64.AppImage"
  else
    wget -q -O "$PLUGIN" "https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/releases/download/continuous/linuxdeploy-plugin-appimage-x86_64.AppImage"
  fi
  chmod a+x "$PLUGIN"
fi

export ARCH=x86_64
"$LINUXDEPLOY" --appimage-extract-and-run \
  --appdir "$APPDIR" \
  --executable "$APPDIR/usr/bin/attune_helper_companion" \
  --desktop-file "$DE" \
  --icon-file "$ICON_FILE"

# plugin-appimage: drop any prior build product in this folder (keeps downloaded linuxdeploy*.AppImage).
shopt -s nullglob
for f in "$OUT_DIR"/*.{AppImage,appimage}; do
  [ -f "$f" ] || continue
  b="$(basename "$f")"
  case "$b" in
    linuxdeploy*.AppImage|linuxdeploy-plugin*.AppImage) continue ;;
  esac
  rm -f "$f"
done
shopt -u nullglob

# Plugin runs appimagetool; final filename depends on the .desktop Name= field and VERSION.
export VERSION
( cd "$OUT_DIR" && "$PLUGIN" --appimage-extract-and-run --appdir "$APPDIR" )

OUT="$OUT_DIR/Attune-Helper-Companion-${ARCH}-${VERSION}.AppImage"
FOUND=""
shopt -s nullglob
for f in "$OUT_DIR"/*.[Aa]pp[Ii]mage; do
  case "$f" in
    *linuxdeploy*x86_64* | *linuxdeploy-plugin*) continue ;;
  esac
  FOUND="$f"
  break
done
shopt -u nullglob
if [ -n "$FOUND" ] && [ -f "$FOUND" ]; then
  if [ -e "$OUT" ] && [ "$FOUND" != "$OUT" ]; then
    rm -f "$OUT"
  fi
  if [ "$FOUND" != "$OUT" ]; then
    mv -f "$FOUND" "$OUT"
  fi
  echo "Built: $OUT"
  exit 0
fi
echo "AppImage not found. See linuxdeploy logs; install FUSE2 if appimagetool cannot mount: libfuse2 on Debian/Ubuntu, or your distro's appimagetool deps." >&2
exit 1
