#!/usr/bin/env python3
import json
import re
import sys
import argparse
import urllib.error
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path


USER_AGENT = "attune-helper-companion-web-catalog-import"
DETAIL_WORKERS = 24
CATEGORY_LABELS = [
    "Achievements",
    "Action Bars",
    "Artwork",
    "Auction & Economy",
    "Audio & Video",
    "Bags & Inventory",
    "Boss Encounters",
    "Buffs & Debuffs",
    "Chat & Communication",
    "Class",
    "Combat",
    "Companions",
    "Data Export",
    "Development Tools",
    "Guild",
    "Libraries",
    "Mail",
    "Map & Minimap",
    "Minigames",
    "Miscellaneous",
    "Plugins",
    "Professions",
    "PvP",
    "Quests & Leveling",
    "Roleplay",
    "Tooltip",
    "Unit Frames",
    "Talents",
    "Dungeon",
    "Inventory",
    "Utility",
]

LOOT_KEYWORDS = ("loot", "roll", "atlasloot", "atlas-loot", "atlas", "auction", "drop", "itemrack")
INVENTORY_KEYWORDS = ("bag", "bags", "inventory", "arkinventory", "bagnon", "bank", "bagspace")
MAP_KEYWORDS = ("mapster", "map", "minimap", "cartographer", "coordinates", "coords", "tomtom")
ATTUNEMENT_KEYWORDS = ("attune",)


def log(message: str) -> None:
    print(message, flush=True)


def fetch_text(url: str) -> str | None:
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    try:
        with urllib.request.urlopen(request, timeout=20) as response:
            return response.read().decode("utf-8", errors="replace")
    except (urllib.error.URLError, urllib.error.HTTPError, TimeoutError):
        return None


def slug_to_name(slug: str) -> str:
    words = [part for part in re.split(r"[-_]+", slug) if part]
    return " ".join(word.capitalize() for word in words) if words else slug


def extract_zip_url(html: str) -> str | None:
    if not html:
        return None
    matches = re.findall(r'https://[^"\'\s<>]+\.zip(?:\?[^"\'\s<>]*)?', html, flags=re.IGNORECASE)
    if not matches:
        return None
    preferred = [url for url in matches if "wp-content/uploads" in url or "download" in url.lower()]
    return preferred[0] if preferred else matches[0]


def infer_category_from_context(context: str) -> str | None:
    lowered = context.lower()
    for label in sorted(CATEGORY_LABELS, key=len, reverse=True):
        if label.lower() in lowered:
            return label
    return None


def unique_categories(categories: list[str]) -> list[str]:
    result: list[str] = []
    for category in categories:
        if category and category not in result:
            result.append(category)
    return result


def assign_categories(entry: dict, categories: list[str]) -> None:
    categories = unique_categories(categories) or ["Quality of Life"]
    entry["category"] = categories[0]
    if len(categories) > 1:
        entry["categories"] = categories
    else:
        entry.pop("categories", None)


def curated_felbite_categories(slug: str, name: str, scraped_category: str | None) -> list[str]:
    text = f"{slug} {name} {scraped_category or ''}".lower()
    categories: list[str] = []
    if any(keyword in text for keyword in LOOT_KEYWORDS):
        categories.append("Loot")
    if any(keyword in text for keyword in INVENTORY_KEYWORDS):
        categories.append("Inventory")
    if any(keyword in text for keyword in ATTUNEMENT_KEYWORDS):
        categories.append("Attunement")
    if any(keyword in text for keyword in MAP_KEYWORDS):
        categories.append("Map")
    if not categories:
        categories.append("Quality of Life")
    return unique_categories(categories)


def collect_felbite_urls(max_pages: int = 40) -> tuple[list[str], dict[str, str]]:
    urls: set[str] = set()
    category_by_url: dict[str, str] = {}
    for page in range(1, max_pages + 1):
        page_url = "https://felbite.com/wow-3-3-5-addons/" if page == 1 else f"https://felbite.com/wow-3-3-5-addons/page/{page}/"
        log(f"[Felbite] Listing page {page}/{max_pages}")
        html = fetch_text(page_url)
        if not html:
            log(f"[Felbite] Stopped at page {page}: no response")
            break
        found = set(re.findall(r'https://felbite\.com/addon/\d+-[a-z0-9\-]+/?', html, flags=re.IGNORECASE))
        if not found:
            log(f"[Felbite] Stopped at page {page}: no addon links found")
            break
        before = len(urls)
        urls.update(found)
        for match in re.finditer(r'https://felbite\.com/addon/\d+-[a-z0-9\-]+/?', html, flags=re.IGNORECASE):
            url = match.group(0)
            context = html[max(0, match.start() - 500):match.end() + 500]
            category = infer_category_from_context(context)
            if category and url not in category_by_url:
                category_by_url[url] = category
        log(f"[Felbite] Page {page}: +{len(urls) - before} unique links ({len(urls)} total)")
        if len(urls) == before:
            log(f"[Felbite] Stopped at page {page}: no new links")
            break
    return sorted(urls), category_by_url


def build_felbite_entry(url: str, zip_url: str | None, category: str | None) -> dict:
    slug = url.rstrip("/").split("/")[-1]
    if "-" in slug:
        slug = slug.split("-", 1)[1]
    name = slug_to_name(slug)
    install_type = "direct_zip" if zip_url else "external_page"
    install_url = zip_url if zip_url else url
    repo_url = zip_url if zip_url else url
    entry = {
        "id": f"felbite-{slug}",
        "name": name,
        "author": "Felbite",
        "source": "Felbite",
        "category": "Quality of Life",
        "folder": f"felbite-{slug}",
        "description": "Community addon listing from Felbite (3.3.5 catalog page).",
        "repo": repo_url,
        "page_url": url,
        "install": {
            "type": install_type,
            "url": install_url
        },
        "github_release": None,
        "direct_zip": {
            "url": zip_url,
            "enabled": bool(zip_url)
        } if zip_url else None,
        "avatar_url": "",
        "version": "website"
    }
    assign_categories(entry, curated_felbite_categories(slug, name, category))
    return entry


def collect_zip_urls(detail_urls: list[str]) -> dict[str, str | None]:
    def scrape(url: str) -> tuple[str, str | None]:
        html = fetch_text(url)
        return (url, extract_zip_url(html or ""))

    result: dict[str, str | None] = {}
    zip_hits = 0
    if not detail_urls:
        log("[Detail] No addon detail pages to scan.")
        return result
    log(f"[Detail] Scanning {len(detail_urls)} addon detail pages with {DETAIL_WORKERS} workers")
    with ThreadPoolExecutor(max_workers=DETAIL_WORKERS) as executor:
        future_map = {executor.submit(scrape, url): url for url in detail_urls}
        for index, future in enumerate(as_completed(future_map), start=1):
            url, zip_url = future.result()
            result[url] = zip_url
            if zip_url:
                zip_hits += 1
            if index % 100 == 0 or index == len(detail_urls):
                log(f"[Detail] Scanned {index}/{len(detail_urls)} pages | zip links found: {zip_hits}")
    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Import Felbite addon catalog pages into manifest/addons.json.")
    parser.add_argument(
        "--sources",
        default="felbite",
        help="Comma-separated sources to import. Supported: felbite. Default: felbite",
    )
    parser.add_argument(
        "--detail-workers",
        type=int,
        default=DETAIL_WORKERS,
        help=f"Concurrent workers for detail-page scanning (default: {DETAIL_WORKERS}).",
    )
    return parser.parse_args()


def main() -> None:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(line_buffering=True)
    log("Starting web catalog import...")
    args = parse_args()
    global DETAIL_WORKERS
    if args.detail_workers > 0:
        DETAIL_WORKERS = args.detail_workers

    requested_sources = {part.strip().lower() for part in args.sources.split(",") if part.strip()}
    include_felbite = "felbite" in requested_sources

    manifest_path = Path("manifest/addons.json")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    addons = manifest.get("addons", [])
    existing_ids = {entry.get("id", "") for entry in addons if isinstance(entry, dict)}

    felbite_urls, felbite_categories = collect_felbite_urls() if include_felbite else ([], {})
    log(f"[Summary] Felbite URLs: {len(felbite_urls)}")

    all_detail_urls = list(felbite_urls)
    zip_by_url = collect_zip_urls(all_detail_urls)

    felbite_entries = [build_felbite_entry(url, zip_by_url.get(url), felbite_categories.get(url)) for url in felbite_urls]

    added = 0
    updated = 0
    entry_map = {entry.get("id", ""): entry for entry in addons if isinstance(entry, dict)}
    for entry in felbite_entries:
        entry_id = entry["id"]
        if entry_id in existing_ids:
            current = entry_map.get(entry_id)
            if isinstance(current, dict):
                current["repo"] = entry["repo"]
                current["page_url"] = entry["page_url"]
                current["install"] = entry["install"]
                current["direct_zip"] = entry["direct_zip"]
                current["source"] = entry["source"]
                current["category"] = entry["category"]
                if "categories" in entry:
                    current["categories"] = entry["categories"]
                else:
                    current.pop("categories", None)
                updated += 1
            continue
        addons.append(entry)
        existing_ids.add(entry_id)
        added += 1

    manifest["addons"] = addons
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    zip_total = sum(1 for value in zip_by_url.values() if value)
    log(f"[Summary] Zip links detected: {zip_total}/{len(all_detail_urls)}")
    log(f"[Summary] Added {added} and updated {updated} website catalog addons in {manifest_path}")


if __name__ == "__main__":
    main()
