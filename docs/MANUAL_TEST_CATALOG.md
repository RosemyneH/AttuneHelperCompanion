# Manual catalog / install checks

Automated tests cover JSON parse and baked catalog ([addon_manifest_test](../tests/addon_manifest_test.c)). After changing the hub manifest or install paths, run these once on a Release build.

## Paths

- Hub manifest: `../synastria-monorepo-addons/manifest/addons.json` (sibling clone).
- Companion build copies `addons.json` next to the exe under `manifest/`.

## Checks

1. **Upstream git:** Install one add-on whose `repo` points at a normal GitHub URL (e.g. WeakAuras). Confirm clone + `.toc` detection.
2. **Monorepo / source_subdir:** Install **RaajikWarpAlias** only after the hub repo contains the vendored tree under `addons/RaajikWarpAlias/` with a valid `.toc`; confirm `source_subdir` is honored.

See also [user-testing.md](user-testing.md) for general UI checks.
