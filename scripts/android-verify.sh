#!/usr/bin/env bash
# Reproduce the CI Android build (requires ANDROID_HOME or android/local.properties).
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT/android"
chmod +x gradlew 2>/dev/null || true
./gradlew assembleDebug --no-daemon --stacktrace
