# Synastria monorepo add-ons (separate repository)

The **vendored add-on monorepo** is a **standalone Git repository** next to this
project, not a subfolder of AttuneHelperCompanion.

| Location | Path |
|----------|------|
| On disk (default) | `e:\Repos\synastria-monorepo-addons` (sibling of `AttuneHelperCompanion`) |
| Publish | Create `https://github.com/<org>/synastria-monorepo-addons` and push `main` |

(Former name: `wotlk-curated-addons`. If that folder still exists beside this one, remove the duplicate and use only `synastria-monorepo-addons`.)

## First-time publish

```bash
cd /path/to/synastria-monorepo-addons
# already: git init + initial commit
git remote add origin https://github.com/<org>/synastria-monorepo-addons.git
git push -u origin main
```

Vendored trees: add under `addons/<AddOnFolder>/`, then in **this** repository’s
`manifest/addons.json` use `repo` = the monorepo’s HTTPS clone URL and
`source_subdir` = path under the repo (e.g. `addons/RaajikWarpAlias`).

**Upstream-first** policy: keep manifest rows pointed at a public upstream Git
URL when one exists; use the monorepo only for sources that are not (or not
yet) published elsewhere.

## Related files in this repo

- [AGENTS.md](../AGENTS.md) — agent notes and catalog script pointers
- [scripts/emit_curated_addon_manifest.py](../scripts/emit_curated_addon_manifest.py) — rebuild slim list from a prior `addons.json` export
- [manifest/addons.json](../manifest/addons.json) — in-app install catalog
