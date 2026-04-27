#!/usr/bin/env python3
import json
import re
import sys
from email.utils import parsedate_to_datetime
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

from ahc_hub_manifest import resolve_hub_addons_json


REPO_OWNER_RE = re.compile(r"github\.com/([^/]+)/", re.IGNORECASE)
REPO_NAME_RE = re.compile(r"github\.com/[^/]+/([^/]+?)(?:\.git)?$", re.IGNORECASE)


def github_owner(repo_url: str) -> str | None:
    if not repo_url:
        return None
    match = REPO_OWNER_RE.search(repo_url)
    return match.group(1) if match else None


def github_repo_name(repo_url: str) -> str | None:
    if not repo_url:
        return None
    clean = repo_url.rstrip("/")
    match = REPO_NAME_RE.search(clean)
    if not match:
        return None
    return match.group(1)


def http_json(url: str) -> dict | list | None:
    req = urllib.request.Request(
        url,
        headers={
            "Accept": "application/vnd.github+json",
            "User-Agent": "attune-helper-companion-manifest-refresh",
        },
    )
    try:
        with urllib.request.urlopen(req, timeout=12) as response:
            return json.loads(response.read().decode("utf-8"))
    except (urllib.error.URLError, urllib.error.HTTPError, TimeoutError, json.JSONDecodeError):
        return None


def http_headers(url: str) -> dict[str, str]:
    req = urllib.request.Request(
        url,
        method="HEAD",
        headers={
            "User-Agent": "attune-helper-companion-manifest-refresh",
        },
    )
    try:
        with urllib.request.urlopen(req, timeout=12) as response:
            return {key.lower(): value for key, value in response.headers.items()}
    except (urllib.error.URLError, urllib.error.HTTPError, TimeoutError):
        return {}


def resolve_github_version(owner: str, repo: str) -> str | None:
    release_url = f"https://api.github.com/repos/{urllib.parse.quote(owner)}/{urllib.parse.quote(repo)}/releases/latest"
    release_data = http_json(release_url)
    if isinstance(release_data, dict):
        tag_name = release_data.get("tag_name")
        if isinstance(tag_name, str) and tag_name.strip():
            return tag_name.strip()

    tags_url = f"https://api.github.com/repos/{urllib.parse.quote(owner)}/{urllib.parse.quote(repo)}/tags?per_page=1"
    tags_data = http_json(tags_url)
    if isinstance(tags_data, list) and tags_data:
        first = tags_data[0]
        if isinstance(first, dict):
            tag_name = first.get("name")
            if isinstance(tag_name, str) and tag_name.strip():
                return tag_name.strip()

    branch_url = f"https://api.github.com/repos/{urllib.parse.quote(owner)}/{urllib.parse.quote(repo)}"
    branch_data = http_json(branch_url)
    if not isinstance(branch_data, dict):
        return None
    default_branch = branch_data.get("default_branch")
    if not isinstance(default_branch, str) or not default_branch:
        return None

    commit_url = f"https://api.github.com/repos/{urllib.parse.quote(owner)}/{urllib.parse.quote(repo)}/commits/{urllib.parse.quote(default_branch)}"
    commit_data = http_json(commit_url)
    if isinstance(commit_data, dict):
        sha = commit_data.get("sha")
        if isinstance(sha, str) and len(sha) >= 7:
            return f"commit-{sha[:7]}"
    return None


def resolve_direct_zip_version(url: str) -> str:
    headers = http_headers(url)
    last_modified = headers.get("last-modified")
    if last_modified:
        try:
            dt = parsedate_to_datetime(last_modified)
            return f"zip-{dt:%Y%m%d}"
        except (TypeError, ValueError, IndexError, OverflowError):
            pass

    etag = headers.get("etag")
    if etag:
        clean = re.sub(r"[^A-Za-z0-9]+", "", etag)
        if clean:
            return f"zip-etag-{clean[:12]}"

    return "website"


def main() -> None:
    companion_root = Path(__file__).resolve().parents[1]
    try:
        manifest_path = resolve_hub_addons_json(companion_root)
    except FileNotFoundError as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1) from exc
    data = json.loads(manifest_path.read_text(encoding="utf-8"))
    addons = data.get("addons", [])

    for addon in addons:
        repo = addon.get("repo", "")
        owner = github_owner(repo)
        repo_name = github_repo_name(repo)
        if owner and not addon.get("avatar_url"):
            addon["avatar_url"] = f"https://github.com/{owner}.png?size=96"
        if owner and repo_name:
            resolved = resolve_github_version(owner, repo_name)
            if resolved:
                addon["version"] = resolved
            elif not addon.get("version"):
                addon["version"] = "unknown"
        elif repo.lower().split("?", 1)[0].endswith(".zip"):
            addon["version"] = resolve_direct_zip_version(repo)
        elif not addon.get("version"):
            addon["version"] = "unknown"

    manifest_path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Updated {len(addons)} addons in {manifest_path}")


if __name__ == "__main__":
    main()
