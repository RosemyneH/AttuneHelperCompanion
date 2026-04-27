#!/usr/bin/env sh
# Git Bash / WSL / macOS / Linux: Felbite re-scrape hint. Windows: use scripts\felbite_rescrape_hint.bat
# Writes build/tbc_zip_audit.json, prints import_web_catalogs to stderr, exits 1 if no TBC-matching URLs.
set -eu
REPO_ROOT="$(cd "$(dirname "$0")" && cd .. && pwd)"
cd "$REPO_ROOT"
if command -v python3 >/dev/null 2>&1; then
  PY=python3
else
  PY=python
fi
"$PY" scripts/audit_tbc_zip_urls.py --emit-rescrape-bash --no-stdout-json -o "build/tbc_zip_audit.json"
