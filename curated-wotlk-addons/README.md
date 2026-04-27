# WotLK curated add-on distribution (Synastria / Attune)

This tree is a **suggested layout** for a public GitHub repository (for example
`wotlk-curated-addons`) used to **vendor** add-on sources that are not
published on Git, with a **README index** and optional copies under `addons/`.

- **Companion catalog policy** is *upstream-first*: the Attune Helper Companion
manifest should point at each add-on’s **canonical GitHub** URL when it
exists, and use this monorepo only for trees that are **not** on Git
elsewhere.
- **Do not** use git submodules here; the app clones with
`git clone --depth 1` and expects a **flat** directory tree (use
`source_subdir` in the manifest to select a subfolder).

When you create the public repository, add `addons/<FolderName>/` for each
vendored add-on and record it in [ADDONS.md](ADDONS.md). Placeholder paths like
`RaajikWarpAlias` can be filled after copying from a local or legacy layout.

## Related

- [ADDONS.md](ADDONS.md) – index table and upstream / vendored notes
- [AttuneHelperCompanion](https://github.com/RosemyneH/AttuneHelperCompanion) –
`manifest/addons.json` is the in-app catalog