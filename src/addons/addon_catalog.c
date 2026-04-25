#include "addons/addon_catalog.h"

static const AhcAddon AHC_ADDONS[] = {
    { "ArkInventory", "Scoots", "Inventory", "ArkInventory", "https://github.com/SynScoots/ArkInventory-modified-for-attunements-.git", "Adds support for attunements.", NULL, NULL, NULL, NULL },
    { "AtlasLoot Mythic", "imevul", "Loot", "AtlasLoot_Mythic", "https://github.com/imevul/AtlasLoot_Mythic.git", "Adds mythic items to AtlasLoot.", NULL, NULL, NULL, NULL },
    { "Attune Helper", "Qt", "Attunement", "AttuneHelper", "https://github.com/RosemyneH/AttuneHelperHosting.git", "Auto-equips attuneable gear and more.", NULL, NULL, NULL, NULL },
    { "AttuneProgress", "QT", "Attunement", "AttuneProgress", "https://github.com/RosemyneH/AttuneProgress.git", "Tracks attunement progress in inventory.", NULL, NULL, NULL, NULL },
    { "Droms Instance Timer", "Dromkal", "Dungeon", "DromsInstanceTimer", "https://github.com/binnesman/DromsInstanceTimer-1.0.0.git", "Tracks how many instances you've entered in the last hour.", NULL, NULL, NULL, NULL },
    { "Droms Kill Counter", "Dromkal", "Loot", "DromsTrashKillCount", "https://github.com/binnesman/DromsKillCounter.git", "Shows attuneable loot in your current zone.", NULL, NULL, NULL, NULL },
    { "ElvUI", "QT", "UI Suite", "ElvUI", "https://github.com/RosemyneH/ElvUI_Attune.git", "ElvUI mod for Synastria.", NULL, NULL, NULL, NULL },
    { "MehTrajectory", "Meh", "Map", "MehTrajectory", "https://github.com/SynScoots/MehTrajectory.git", "Tracks movement trajectory on the map.", NULL, NULL, NULL, NULL },
    { "Postal", "Lulleh", "Utility", "Postal", "https://github.com/binnesman/Postal.git", "Postal mod with faster Open All speed.", NULL, NULL, NULL, NULL },
    { "Qt's Auto-Roller", "Qt", "Loot", "qtRoll", "https://github.com/RosemyneH/qtRoll.git", "Automatically rolls for loot while in a group.", NULL, NULL, NULL, NULL },
    { "Scoots' Combat Attune Watch", "Scoots", "Combat", "ScootsCombatAttuneWatch", "https://github.com/SynScoots/ScootsCombatAttuneWatch.git", "Tracks attunement progress in combat.", NULL, NULL, NULL, NULL },
    { "Scoots' Confirmation Skip", "Scoots", "Quality of Life", "ScootsConfirmationSkip", "https://github.com/SynScoots/ScootsConfirmationSkip.git", "Skips confirmation prompts for BoE equips and BoP looting.", NULL, NULL, NULL, NULL },
    { "Scoots' Craft", "Scoots", "Profession", "ScootsCraft", "https://github.com/SynScoots/ScootsCraft.git", "A complete overhaul of the profession UI.", NULL, NULL, NULL, NULL },
    { "Scoots' Quick Auction", "Scoots", "Auction", "ScootsQuickAuction", "https://github.com/SynScoots/ScootsQuickAuction.git", "Rapidly creates auctions at a specified price.", NULL, NULL, NULL, NULL },
    { "Scoots' Rares", "Scoots", "Map", "ScootsRares", "https://github.com/SynScoots/ScootsRares.git", "Creates a chat link to run .findnpc when near a rare.", NULL, NULL, NULL, NULL },
    { "Scoots' Speedrun", "Scoots", "Dungeon", "ScootsSpeedrun", "https://github.com/SynScoots/ScootsSpeedrun.git", "Instantly selects dialogue choices in dungeons.", NULL, NULL, NULL, NULL },
    { "Scoots' Stats", "Scoots", "Character", "ScootsStats", "https://github.com/SynScoots/ScootsStats.git", "Adds a stat panel beside the character panel.", NULL, NULL, NULL, NULL },
    { "Scoots' Vendor", "Scoots", "Vendor", "ScootsVendor", "https://github.com/SynScoots/ScootsVendor.git", "Unified vendor addon that combines filtering and forge workflows.", NULL, NULL, NULL, NULL },
    { "Synastria Build Manager", "Namira", "Builds", "SynastriaBuildManager", "https://github.com/Lubricated/SynastriaBuildManager.git", "Exports and imports perks and talents.", NULL, NULL, NULL, NULL },
    { "Synastria Tooltip Enhancer", "Lulleh", "Tooltip", "SynastriaTooltipEnhancer", "https://github.com/binnesman/Synastria-Tooltip-Enhancer.git", "Enhances Synastria item tooltips.", NULL, NULL, NULL, NULL },
    { "The Journal", "Qt", "Dungeon", "TheJournal", "https://github.com/RosemyneH/TheJournal.git", "Dungeon journal with item and boss details.", NULL, NULL, NULL, NULL },
    { "WarpRing", "Namira", "Utility", "WarpRing", "https://github.com/binnesman/namira_warpring.git", "Warp Ring UI.", NULL, NULL, NULL, NULL },
    { "OmniInventory Syn", "RosemyneH", "Inventory", "OmniInventory", "https://github.com/RosemyneH/OmniInv-Syn.git", "Inventory management addon adapted for Synastria.", "OmniInventory", NULL, NULL, NULL },
    { "BossTracker", "nayalist", "Dungeon", "BossTracker", "https://github.com/nayalist/boss-tracker.git", "Tracks boss kills and dungeon lockout details.", "BossTracker", NULL, NULL, NULL },
    { "Talented Synastria", "RosemyneH", "Talents", "Talented-synastria", "https://github.com/RosemyneH/talented-synastria.git", "Talent build manager adapted for Synastria.", "Talented-synastria", NULL, NULL, NULL },
    { "qtRunner", "RosemyneH", "Dungeon", "qtRunner", "https://github.com/RosemyneH/qtRunner.git", "Dungeon runner helper with bindings and run tracking.", "qtRunner", NULL, NULL, NULL },
    { "Questie 3.3.5", "Netrinil", "Questing", "Questie-335", "https://github.com/Netrinil/Questie-335.git", "Quest helper for the 3.3.5 client.", "Questie-335", NULL, NULL, NULL },
    { "LizardDMP", "Versicoloris", "Gear", "LizardDMP", "https://github.com/Versicoloris/LizardDMP.git", "Fast gear optimizer based on stat priority.", NULL, NULL, NULL, NULL },
    { "LizardKTS", "Versicoloris", "Gear", "LizardKTS", "https://github.com/Versicoloris/LizardKTS.git", "Shows Synastria upgrade chains to prevent vendoring.", NULL, NULL, NULL, NULL },
    { "LizardWWG", "Versicoloris", "Map", "LizardWWG", "https://github.com/Versicoloris/LizardWWG.git", "Right-click warps to instantly open their map.", NULL, NULL, NULL, NULL },
    { "LizardSTB", "Versicoloris", "Utility", "LizardSTB", "https://github.com/Versicoloris/LizardSTB.git", "Save an action bar and automatically share it across all characters.", NULL, NULL, NULL, NULL },
    { "LizardMMU", "Versicoloris", "Mounts", "LizardMMU", "https://github.com/Versicoloris/LizardMMU.git", "Pick your favorite mounts and summon one at random with a single click.", NULL, NULL, NULL, NULL },
    { "LizardTMO", "Versicoloris", "Transmog", "LizardTMO", "https://github.com/Versicoloris/LizardTMO.git", "Store your transmog import codes in-game-no more lost outfits after prestige.", NULL, NULL, NULL, NULL }
};

const AhcAddon *ahc_addon_catalog_items(void)
{
    return AHC_ADDONS;
}

size_t ahc_addon_catalog_count(void)
{
    return sizeof(AHC_ADDONS) / sizeof(AHC_ADDONS[0]);
}
