# WotLK curated monorepo (separate repository)

The **vendored add-on monorepo** is a **standalone Git repository** next to this
project, not a subfolder of AttuneHelperCompanion.

| Location | Path |
|----------|------|
| On disk (default) | `e:\Repos\wotlk-curated-addons` (sibling of `AttuneHelperCompanion`) |
| Publish | Create `https://github.com/<org>/wotlk-curated-addons` (or your org’s name) and push `main` |

## First-time publish

```bash
cd /path/to/wotlk-curated-addons
# already: git init + initial commit
git remote add origin https://github.com/<org>/wotlk-curated-addons.git
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
