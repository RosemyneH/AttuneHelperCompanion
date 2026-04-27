# Add-on index (curated / upstream)

| Add-on (UI name) / folder | In-app id | Upstream (preferred) or vendored path |
|----------------------------|-------------|----------------------------------------|
| Addon Control Panel (ACP-Zero) | `acp-zero` | <https://github.com/DivideByZeroToDeleteWorld/ACP-Zero> (use this instead of legacy ACP) |
| WeakAuras (WotLK) | `weakauras-wotlk` | <https://github.com/Bunny67/WeakAuras-WotLK> |
| VendorForgeList | `vendorforgelist` | <https://github.com/APLiquid/VendorForgeList> |
| TrufiGCD | `trufi-gcd` | <https://github.com/robgha01/TrufiGCD> |
| SynastriaQuestieHelper | `synastria-questie-helper` | <https://github.com/Elmegaard/SynastriaQuestieHelper> |
| Questie 3.3.5 | `questie-335` | <https://github.com/Netrinil/Questie-335> |
| RaajikWarpAlias (when vendored) | (future) | Copy into `addons/RaajikWarpAlias/` in this monorepo when published; manifest row after content exists. |

The companion manifest in the main app repo is the source of truth for
installable entries. This file is a human-friendly map and curation log.

## Felbite

Bulk Felbite scraping is no longer the default **catalog** source. Prefer Git
clone targets above or a vendored tree in this monorepo.
