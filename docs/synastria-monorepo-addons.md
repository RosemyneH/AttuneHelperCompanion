# Synastria monorepo add-ons (catalog home)

The **authoritative** add-on install list for Attune Helper Companion is **[synastria-monorepo-addons](https://github.com/RosemyneH/synastria-monorepo-addons)** on GitHub:

| Location | Path |
|----------|------|
| On disk (default) | Sibling of this repo: `../synastria-monorepo-addons` (e.g. `e:\Repos\synastria-monorepo-addons`) |
| `manifest/addons.json` in that repo | Canonical JSON — **edit there first** |

This companion repository **no longer** hosts `manifest/addons.json` (it only keeps [manifest/presets.json](manifest/presets.json) beside the hub `addons.json` in build output and Android assets).

**CMake** resolves the hub in this order: optional **`-DAHC_HUB_ADDONS_JSON=`** (cache path to `addons.json`), else environment **`AHC_HUB_ADDONS_JSON`**, else `./synastria-monorepo-addons/manifest/addons.json` (in-repo), else `../synastria-monorepo-addons/manifest/addons.json` (sibling). See [CMakeLists.txt](../CMakeLists.txt).

On Windows, [build-app.bat](../build-app.bat) and [build-test.bat](../build-test.bat) run [scripts/ensure-synastria-hub.bat](../scripts/ensure-synastria-hub.bat) before configure: if neither in-repo nor sibling manifest exists, they **shallow-clone** `RosemyneH/synastria-monorepo-addons` into `./synastria-monorepo-addons/`, then run **`generate_addon_catalog.py --check`** when `python` is on `PATH`.

## Maintenance scripts

Python helpers under [scripts/](../scripts/) resolve the same default hub path as CMake (in-repo hub first, then sibling). Shared logic lives in [scripts/ahc_hub_manifest.py](../scripts/ahc_hub_manifest.py).

To point at a non-standard checkout, set **`AHC_HUB_ADDONS_JSON`** to the absolute or relative path of `addons.json` (CMake configure or Python).

Examples (from this repo root, with the hub present as usual):

- `python scripts/generate_addon_catalog.py --check` — validate the hub manifest (same schema CMake uses to bake the catalog).
- `python scripts/generate_addon_catalog.py --check --input path/to/addons.json` — explicit path when needed.

[scripts/emit_curated_addon_manifest.py](../scripts/emit_curated_addon_manifest.py) writes the resolved hub manifest by default; use **`--dry-run`** to print the target path and size without writing.

## Remote refresh

The app fetches a cached copy from:

`https://raw.githubusercontent.com/RosemyneH/synastria-monorepo-addons/main/manifest/addons.json`

(after you push the hub to GitHub).

## Vendored trees

Add under `addons/<AddOnFolder>/` in the **hub** repo, then set `repo` to the monorepo HTTPS URL and `source_subdir` (e.g. `addons/RaajikWarpAlias`).

**Upstream-first:** point at public GitHub when it exists; use the monorepo only when there is no upstream.

## Normal workflow: hub plus GitHub (or monorepo paths)

For every catalog change going forward:

1. Edit **`manifest/addons.json` in [synastria-monorepo-addons](https://github.com/RosemyneH/synastria-monorepo-addons)** (clone next to this repo or under `./synastria-monorepo-addons/`).
2. Set each addon’s **`repo`** to the real upstream Git HTTPS URL when one exists.
3. If there is no public upstream, **vendor** the tree under the hub (for example `addons/<Folder>/`) and point **`repo`** at the monorepo plus **`source_subdir`** as documented above.

Attune Helper Companion **does not** define the list elsewhere: CMake bakes from that JSON, install flows use those URLs, and the desktop/Android app refreshes from the same hub on GitHub (`raw.githubusercontent.com/.../synastria-monorepo-addons/.../addons.json`).

## Related files in this repo

- [AGENTS.md](../AGENTS.md) — verification and script pointers
- [docs/MANUAL_TEST_CATALOG.md](MANUAL_TEST_CATALOG.md) — manual install checks
- [scripts/emit_curated_addon_manifest.py](../scripts/emit_curated_addon_manifest.py) — optional slim export from a prior JSON dump
- [scripts/list_synastoots_repos.py](../scripts/list_synastoots_repos.py) — list SynScoots rows in the hub manifest
- [scripts/generate_addon_catalog.py](../scripts/generate_addon_catalog.py) — validate + generate baked C; default `--input` is the hub (or pass a path)
- [scripts/audit_tbc_zip_urls.py](../scripts/audit_tbc_zip_urls.py) — optional: find hub manifest entries whose zip/repo URLs look like TBC-era artifacts (manual review)
