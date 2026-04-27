"""Resolve synastria-monorepo-addons manifest/addons.json (same rules as CMakeLists.txt)."""

from __future__ import annotations

import os
from pathlib import Path


def resolve_hub_addons_json(companion_root: Path) -> Path:
    """Return absolute path to hub addons.json.

    Order: environment AHC_HUB_ADDONS_JSON, then ./synastria-monorepo-addons/...,
    then ../synastria-monorepo-addons/... relative to companion_root.
    """
    env = os.environ.get("AHC_HUB_ADDONS_JSON", "").strip()
    if env:
        p = Path(env).expanduser()
        if not p.is_file():
            msg = f"AHC_HUB_ADDONS_JSON is set but file not found: {p}"
            raise FileNotFoundError(msg)
        return p.resolve()

    in_repo = companion_root / "synastria-monorepo-addons" / "manifest" / "addons.json"
    sibling = companion_root.parent / "synastria-monorepo-addons" / "manifest" / "addons.json"
    if in_repo.is_file():
        return in_repo.resolve()
    if sibling.is_file():
        return sibling.resolve()

    raise FileNotFoundError(
        "Could not find synastria-monorepo-addons/manifest/addons.json. "
        "Clone the hub next to this repo or as ./synastria-monorepo-addons, "
        f"or set AHC_HUB_ADDONS_JSON. Tried:\n  {in_repo}\n  {sibling}"
    )
