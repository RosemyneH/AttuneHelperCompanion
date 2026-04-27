#!/usr/bin/env python3
"""Reapply Felbite category heuristics to hub manifest/addons.json (no network)."""

import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "scripts"))

from ahc_hub_manifest import resolve_hub_addons_json

import import_web_catalogs as cat


def main() -> None:
    path = resolve_hub_addons_json(REPO)
    data = json.loads(path.read_text(encoding="utf-8"))
    n = 0
    for entry in data.get("addons", []):
        if not isinstance(entry, dict) or entry.get("source") != "Felbite":
            continue
        eid = entry.get("id", "")
        if not eid.startswith("felbite-"):
            continue
        slug = eid[len("felbite-") :]
        name = str(entry.get("name") or cat.slug_to_name(slug))
        new_c = cat.curated_felbite_categories(slug, name, None)
        cat.assign_categories(entry, new_c)
        cat.apply_felbite_addoncontrolpanel_override(entry)
        n += 1
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Reassigned categories for {n} Felbite entries in {path}", flush=True)


if __name__ == "__main__":
    main()
