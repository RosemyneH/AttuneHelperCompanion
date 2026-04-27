#!/usr/bin/env python3
"""List SynScoots github.com URLs from the hub manifest for manual deprecated review."""
import json
import sys
from pathlib import Path

from ahc_hub_manifest import resolve_hub_addons_json

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    try:
        path = resolve_hub_addons_json(ROOT)
    except FileNotFoundError as exc:
        print(exc, file=sys.stderr)
        return 1
    data = json.loads(path.read_text(encoding="utf-8"))
    rows = []
    for a in data.get("addons", []):
        if not isinstance(a, dict):
            continue
        repo = a.get("repo") or ""
        if "github.com/SynScoots/" in repo or "/SynScoots/" in repo:
            rows.append((a.get("id", ""), a.get("name", ""), repo, a.get("description", "")[:80]))
    for tid, name, repo, desc in sorted(rows, key=lambda x: x[0]):
        line = f"{tid}\t{name}\t{repo}"
        if "deprecated" in (name or "").lower() or "deprecated" in (desc or "").lower():
            line += "\t[flag: deprecated?]"
        print(line)
    print(f"# {len(rows)} SynScoots rows from {path}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
