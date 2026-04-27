#!/usr/bin/env python3
# Audit manifest entries whose install/repo/zip URLs look like a TBC (2.x) build
# instead of WotLK 3.3.5, for manual review and re-scraping.
#
# Run (repo root):
#   Windows (cmd):  python scripts\audit_tbc_zip_urls.py
#   Git Bash / WSL:  python scripts/audit_tbc_zip_urls.py
#
# With --emit-rescrape-bash, JSON still goes to stdout unless you use -o and --no-stdout-json
#   python scripts/audit_tbc_zip_urls.py -o build/tbc_zip_audit.json --no-stdout-json --emit-rescrape-bash
# Or: scripts\felbite_rescrape_hint.bat  |  scripts/felbite_rescrape_hint.sh
from __future__ import annotations

import argparse
import json
import sys
from collections.abc import Iterator
from pathlib import Path
from typing import Any
from urllib.parse import unquote, urlparse

REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = REPO_ROOT / "manifest" / "addons.json"
DEFAULT_OUT = REPO_ROOT / "build" / "tbc_zip_audit.json"
MARKER_STEM = "tbc_zip_audit_had_matches"

# Felbite and similar: "-tbc-" in path, or basename contains "tbc" (covers *-tbc.zip, *-tbc-*.zip).
TBC_DASH = "-tbc-"


def is_str(v: Any) -> bool:
    return isinstance(v, str) and bool(v.strip())


def url_field_reason(url: str) -> str | None:
    u = url.strip()
    if not u:
        return None
    lower = u.lower()
    if TBC_DASH in lower:
        return f"{TBC_DASH!r} in url"
    parsed = urlparse(u)
    path = unquote(parsed.path or "")
    if not path:
        return None
    base = path.rsplit("/", 1)[-1]
    if not base:
        return None
    if "tbc" in base.lower():
        return "filename *tbc* (basename contains tbc)"
    return None


def collect_url_fingerprints(
    entry: dict[str, Any]
) -> Iterator[tuple[str, str, str | None]]:
    repo = entry.get("repo")
    if is_str(repo):
        yield "repo", repo, url_field_reason(repo)
    install = entry.get("install")
    if isinstance(install, dict) and is_str(install.get("url")):
        u = str(install["url"]).strip()
        yield "install.url", u, url_field_reason(u)
    direct = entry.get("direct_zip")
    if isinstance(direct, dict) and is_str(direct.get("url")):
        u = str(direct["url"]).strip()
        yield "direct_zip.url", u, url_field_reason(u)


def entry_tbc_hit(entry: dict[str, Any]) -> tuple[list[dict[str, str]], str | None]:
    reasons: list[dict[str, str]] = []
    seen: set[str] = set()
    for field, url, reason in collect_url_fingerprints(entry):
        if not reason or url in seen:
            continue
        seen.add(url)
        reasons.append({"field": field, "url": url, "reason": reason})
    purl: str | None = None
    if reasons and is_str(entry.get("page_url")):
        purl = str(entry["page_url"]).strip()
    return reasons, purl


def build_output_record(entry: dict[str, Any], hits: list[dict[str, str]], page_url: str | None) -> dict[str, Any]:
    install = entry.get("install")
    inst_url: str | None = None
    if isinstance(install, dict) and is_str(install.get("url")):
        inst_url = str(install["url"]).strip() or None
    dz = entry.get("direct_zip")
    dz_url: str | None = None
    if isinstance(dz, dict) and is_str(dz.get("url")):
        dz_url = str(dz["url"]).strip() or None
    return {
        "id": entry.get("id", ""),
        "name": entry.get("name", ""),
        "repo": (str(entry["repo"]).strip() if is_str(entry.get("repo")) else None),
        "install": {"url": inst_url} if inst_url is not None else None,
        "direct_zip": {"url": dz_url} if dz_url is not None else None,
        "page_url": page_url or (str(entry["page_url"]).strip() if is_str(entry.get("page_url")) else None),
        "tbc_hits": hits,
    }


def find_matches(manifest: dict[str, Any]) -> list[dict[str, Any]]:
    out: list[dict[str, Any]] = []
    addons = manifest.get("addons")
    if not isinstance(addons, list):
        return out
    for raw in addons:
        if not isinstance(raw, dict):
            continue
        hits, purl = entry_tbc_hit(raw)
        if not hits:
            continue
        out.append(build_output_record(raw, hits, purl))
    return out


def load_manifest(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def print_rescrape_block(match_count: int) -> None:
    py = "python" if sys.platform == "win32" else "python3"
    lines = [
        "# Re-scrape Felbite 3.3.5 catalog into manifest (updates zip URLs; review git diff).",
        "# Windows (cmd, from repo root):",
        f"#   {py} scripts\\import_web_catalogs.py",
        "# Git Bash / WSL / macOS / Linux (from repo root):",
        f"#   {py} scripts/import_web_catalogs.py",
        "",
        f'"{py}" scripts/import_web_catalogs.py' if sys.platform != "win32" else f"{py} scripts\\import_web_catalogs.py",
    ]
    text = "\n".join(lines) + "\n"
    print(text, end="", file=sys.stderr)


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="List manifest addons whose repo / install.url / direct_zip.url look TBC (zip audit for 3.3.5)."
    )
    p.add_argument(
        "--manifest",
        type=Path,
        default=DEFAULT_MANIFEST,
        help=f"Path to manifest JSON (default: {DEFAULT_MANIFEST.relative_to(REPO_ROOT)})",
    )
    p.add_argument(
        "-o",
        "--output",
        type=Path,
        help=f"Write JSON to this file (default if omitted: print only to stdout; suggested: {DEFAULT_OUT.relative_to(REPO_ROOT)})",
    )
    p.add_argument(
        "--emit-rescrape-bash",
        action="store_true",
        help="After audit, print a ready-to-run import_web_catalogs.py command (hint on stderr; see script header for cmd vs bash).",
    )
    p.add_argument(
        "--marker",
        type=Path,
        help="If matches > 0, write a tiny JSON marker; if 0, remove the file if it exists.",
    )
    p.add_argument(
        "--stdout-json",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Print the audit JSON to stdout (default: true; use --no-stdout-json with -o to only write a file).",
    )
    return p.parse_args()


def main() -> int:
    args = parse_args()
    manifest_path = args.manifest
    if not manifest_path.is_file():
        print(f"error: manifest not found: {manifest_path}", file=sys.stderr)
        return 2

    manifest = load_manifest(manifest_path)
    entries = find_matches(manifest)
    payload: dict[str, Any] = {
        "schema": "tbc_zip_url_audit",
        "manifest": str(manifest_path.resolve()),
        "match_count": len(entries),
        "entries": entries,
    }
    out_text = json.dumps(payload, indent=2, ensure_ascii=False) + "\n"

    if args.stdout_json:
        sys.stdout.write(out_text)
    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(out_text, encoding="utf-8")

    n = len(entries)
    if args.marker is not None:
        if n > 0:
            args.marker.parent.mkdir(parents=True, exist_ok=True)
            args.marker.write_text(
                json.dumps({"tbc_url_matches": n, "marker": MARKER_STEM}, indent=2) + "\n",
                encoding="utf-8",
            )
        elif args.marker.is_file():
            try:
                args.marker.unlink()
            except OSError:
                pass

    if args.emit_rescrape_bash:
        print_rescrape_block(n)
        return 0 if n > 0 else 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
