"""Resolve synastria-monorepo-addons manifest/addons.json (same rules as CMakeLists.txt)."""

from __future__ import annotations

import os
import re
from pathlib import Path

_GITHUB_HTTPS_OWNER_REPO = re.compile(
    r"https?://github\.com/([^/]+)/([^/?.#]+)", re.IGNORECASE
)
_GITHUB_SSH_OWNER_REPO = re.compile(
    r"git@github\.com:([^/]+)/([^/]+?)(?:\.git)?$", re.IGNORECASE
)


def hub_monorepo_github_slug() -> str:
    """Owner/name for the add-on hub (single GitHub repo), e.g. RosemyneH/synastria-monorepo-addons."""
    slug = (os.environ.get("AHC_HUB_MONOREPO_GITHUB_SLUG") or "").strip()
    if slug:
        return slug
    return "RosemyneH/synastria-monorepo-addons"


def parse_github_owner_repo(repo_url: str) -> tuple[str | None, str | None]:
    if not repo_url or not isinstance(repo_url, str):
        return None, None
    u = repo_url.strip().rstrip("/")
    m = _GITHUB_SSH_OWNER_REPO.match(u)
    if m:
        return m.group(1), m.group(2)
    m = _GITHUB_HTTPS_OWNER_REPO.search(u)
    if m:
        return m.group(1), m.group(2)
    return None, None


def repo_is_hub_monorepo(owner: str | None, repo: str | None) -> bool:
    if not owner or not repo:
        return False
    target = hub_monorepo_github_slug().lower()
    return f"{owner}/{repo}".lower() == target


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
