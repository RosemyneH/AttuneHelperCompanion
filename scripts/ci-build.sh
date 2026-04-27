#!/usr/bin/env bash
# Shared build + test for GitHub Actions: set RUNNER_OS=Linux|Windows, or use from workflow matrix.
set -euo pipefail

ROOT="${GITHUB_WORKSPACE:-$(cd "$(dirname "$0")/.." && pwd)}"
cd "$ROOT"
BUILD_DIR="${CI_BUILD_DIR:-"$ROOT/build"}"
if command -v nproc &>/dev/null; then
  PARALLEL="$(nproc)"
else
  PARALLEL="${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
fi
export CTEST_PARALLEL_LEVEL="${CTEST_PARALLEL_LEVEL:-$PARALLEL}"

if [ "${RUNNER_OS:-}" = "Linux" ] && [ -n "${GITHUB_ACTIONS:-}" ] && [ "${AHC_SKIP_APT_DEPS:-0}" != 1 ] && command -v apt-get &>/dev/null; then
  echo "Installing Linux dependencies for raylib..."
  sudo apt-get update
  sudo apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    libgl1-mesa-dev \
    libx11-dev \
    libxrandr-dev \
    libxi-dev \
    libxcursor-dev \
    libxinerama-dev \
    libwayland-dev \
    libxkbcommon-dev \
    libasound2-dev
fi

# Add-on list: synastria-monorepo-addons (sibling, or ./synastria-monorepo-addons in CI)
HUB_MANIFEST="${ROOT}/../synastria-monorepo-addons/manifest/addons.json"
if [ ! -f "$HUB_MANIFEST" ]; then
  HUB_MANIFEST="${ROOT}/synastria-monorepo-addons/manifest/addons.json"
fi
if [ ! -f "$HUB_MANIFEST" ]; then
  echo "ci-build: missing hub manifest. Clone next to this repo or into ./synastria-monorepo-addons/" >&2
  exit 1
fi
"${PYTHON:-python}" scripts/generate_addon_catalog.py --check --input "$HUB_MANIFEST"

CMAKE_CMD=(
  -S "$ROOT"
  -B "$BUILD_DIR"
  -G Ninja
  -DCMAKE_BUILD_TYPE=Release
  -DAHC_OPT_PROFILE=lto
  -DAHC_BUILD_APP=ON
  -DAHC_BUILD_BENCH=OFF
  -DAHC_BUILD_TESTS=ON
)
if [ "${AHC_CMAKE_FRESH:-1}" = 1 ]; then
  cmake --fresh "${CMAKE_CMD[@]}"
else
  cmake "${CMAKE_CMD[@]}"
fi

cmake --build "$BUILD_DIR" --config Release --parallel
ctest --test-dir "$BUILD_DIR" -C Release --output-on-failure
echo "Build finished: $BUILD_DIR"
find "$BUILD_DIR" -maxdepth 1 \( -name "attune_helper_companion" -o -name "attune_helper_companion.exe" \) -print
