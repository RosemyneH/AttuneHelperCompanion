#!/usr/bin/env python3
"""Build slim upstream-first hub manifest/addons.json (non-interactive; run from repo root)."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from ahc_hub_manifest import resolve_hub_addons_json

ROOT = Path(__file__).resolve().parents[1]

# Addon ids to keep (slim catalog).
KEEP_IDS = frozenset(
    {
        "acp-zero",
        "arkinventory",
        "atlasloot-mythic",
        "attune-helper",
        "attune-progress",
        "mapster-synastria-hosting",
        "mehtrajectory",
        "postal",
        "qt-auto-roller",
        "scoots-combat-attune-watch",
        "scoots-confirmation-skip",
        "scoots-craft",
        "scoots-quick-auction",
        "scoots-rares",
        "scoots-speedrun",
        "scoots-stats",
        "scoots-vendor",
        "synastria-build-manager",
        "the-journal",
        "omniinventory-syn",
        "questie-335",
        "lizard-dmp",
        "lizard-kts",
        "lizard-wwg",
        "lizard-stb",
        "lizard-mmu",
        "lizard-tmo",
        "qtrunner",
    }
)

# Upstream-first entries added (not in previous GitHub-only slice or use override URLs).
NEW_ADDONS: list[dict] = [
    {
        "id": "weakauras-wotlk",
        "name": "WeakAuras",
        "author": "Bunny67",
        "category": "Combat",
        "folder": "WeakAuras",
        "description": "WeakAuras for WotLK 3.3.5 (community port).",
        "repo": "https://github.com/Bunny67/WeakAuras-WotLK.git",
        "install": {"type": "git", "url": "https://github.com/Bunny67/WeakAuras-WotLK.git"},
        "github_release": {
            "owner": "Bunny67",
            "repo": "WeakAuras-WotLK",
            "asset_pattern": "*.zip",
            "enabled": False,
        },
        "direct_zip": None,
        "avatar_url": "https://github.com/Bunny67.png?size=96",
        "version": "main",
    },
    {
        "id": "vendorforgelist",
        "name": "VendorForgeList",
        "author": "APLiquid",
        "category": "Professions",
        "folder": "VendorForgeList",
        "description": "Vendor and forge listing utility for 3.3.5.",
        "repo": "https://github.com/APLiquid/VendorForgeList.git",
        "install": {"type": "git", "url": "https://github.com/APLiquid/VendorForgeList.git"},
        "github_release": {
            "owner": "APLiquid",
            "repo": "VendorForgeList",
            "asset_pattern": "*.zip",
            "enabled": False,
        },
        "direct_zip": None,
        "avatar_url": "https://github.com/APLiquid.png?size=96",
        "version": "main",
    },
    {
        "id": "trufi-gcd",
        "name": "TrufiGCD",
        "author": "robgha01",
        "category": "Combat",
        "folder": "TrufiGCD",
        "description": "GCD / rotation helper for 3.3.5.",
        "repo": "https://github.com/robgha01/TrufiGCD.git",
        "install": {"type": "git", "url": "https://github.com/robgha01/TrufiGCD.git"},
        "github_release": {
            "owner": "robgha01",
            "repo": "TrufiGCD",
            "asset_pattern": "*.zip",
            "enabled": False,
        },
        "direct_zip": None,
        "avatar_url": "https://github.com/robgha01.png?size=96",
        "version": "main",
    },
    {
        "id": "synastria-questie-helper",
        "name": "SynastriaQuestieHelper",
        "author": "Elmegaard",
        "category": "Questing",
        "folder": "SynastriaQuestieHelper",
        "description": "Questie-related helper for Synastria-style servers.",
        "repo": "https://github.com/Elmegaard/SynastriaQuestieHelper.git",
        "install": {"type": "git", "url": "https://github.com/Elmegaard/SynastriaQuestieHelper.git"},
        "github_release": {
            "owner": "Elmegaard",
            "repo": "SynastriaQuestieHelper",
            "asset_pattern": "*.zip",
            "enabled": False,
        },
        "direct_zip": None,
        "avatar_url": "https://github.com/Elmegaard.png?size=96",
        "version": "main",
    },
]


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Emit curated slim addon list into the hub manifest.")
    p.add_argument(
        "--dry-run",
        action="store_true",
        help="Print addon count and hub path; do not write.",
    )
    return p.parse_args()


def main() -> int:
    args = parse_args()
    try:
        manifest_path = resolve_hub_addons_json(ROOT)
    except FileNotFoundError as exc:
        print(f"emit_curated_addon_manifest: {exc}", file=sys.stderr)
        return 1

    data = json.loads(manifest_path.read_text(encoding="utf-8"))
    addons = data.get("addons", [])
    kept: list[dict] = []
    for a in addons:
        if not isinstance(a, dict):
            continue
        aid = a.get("id")
        if aid in KEEP_IDS:
            a = dict(a)
            a.pop("source", None)
            a.pop("page_url", None)
            kept.append(a)
    have = {x.get("id") for x in kept}
    if have != set(KEEP_IDS):
        miss = sorted(set(KEEP_IDS) - have)
        extra = sorted(have - set(KEEP_IDS))
        raise SystemExit(
            f"emit_curated_addon_manifest: id mismatch. missing={miss!r} extra={extra!r}"
        )
    by_id = {a["id"]: a for a in kept}
    ordered: list[dict] = [by_id[i] for i in sorted(KEEP_IDS)]
    for n in NEW_ADDONS:
        if n["id"] in KEEP_IDS:
            raise SystemExit(f"new addon id collides with KEEP_IDS: {n['id']!r}")
    data["addons"] = ordered + NEW_ADDONS
    if "updated_at" in data:
        from datetime import date

        data["updated_at"] = str(date.today())
    if args.dry_run:
        print(f"dry-run: would write {len(data['addons'])} addons to {manifest_path}")
        return 0
    manifest_path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Wrote {len(data['addons'])} addons to {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
