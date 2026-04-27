# Synastria monorepo add-ons (catalog home)

The **authoritative** add-on install list for Attune Helper Companion is **[synastria-monorepo-addons](https://github.com/RosemyneH/synastria-monorepo-addons)** on GitHub:

| Location | Path |
|----------|------|
| On disk (default) | Sibling of this repo: `../synastria-monorepo-addons` (e.g. `e:\Repos\synastria-monorepo-addons`) |
| `manifest/addons.json` in that repo | Canonical JSON — **edit there first** |

This companion repository **no longer** hosts `manifest/addons.json` (it only keeps [manifest/presets.json](manifest/presets.json) beside the hub `addons.json` in build output and Android assets).

**CMake** resolves the hub at `../synastria-monorepo-addons/manifest/addons.json` (sibling) or `./synastria-monorepo-addons/...` (submodule). See [CMakeLists.txt](../CMakeLists.txt) (`AHC_HUB_ADDONS_JSON`).

## Remote refresh

The app fetches a cached copy from:

`https://raw.githubusercontent.com/RosemyneH/synastria-monorepo-addons/main/manifest/addons.json`

(after you push the hub to GitHub).

## Vendored trees

Add under `addons/<AddOnFolder>/` in the **hub** repo, then set `repo` to the monorepo HTTPS URL and `source_subdir` (e.g. `addons/RaajikWarpAlias`).

**Upstream-first:** point at public GitHub when it exists; use the monorepo only when there is no upstream.

## Related files in this repo

- [AGENTS.md](../AGENTS.md) — verification and script pointers
- [docs/MANUAL_TEST_CATALOG.md](MANUAL_TEST_CATALOG.md) — manual install checks
- [scripts/emit_curated_addon_manifest.py](../scripts/emit_curated_addon_manifest.py) — optional slim export from a prior JSON dump
- [scripts/list_synastoots_repos.py](../scripts/list_synastoots_repos.py) — list SynScoots rows in the hub manifest
- [scripts/generate_addon_catalog.py](../scripts/generate_addon_catalog.py) — validate + generate baked C; run with `--input` pointing at the hub JSON
