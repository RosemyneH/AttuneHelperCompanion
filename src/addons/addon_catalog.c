#include "addons/addon_catalog.h"

static const AhcAddon AHC_ADDONS[] = {
    { "ArkInventory", "Scoots", "Inventory", "ArkInventory", "https://github.com/SynScoots/ArkInventory-modified-for-attunements-.git", "Adds support for attunements." },
    { "AtlasLoot Mythic", "imevul", "Loot", "AtlasLoot_Mythic", "https://github.com/imevul/AtlasLoot_Mythic.git", "Adds mythic items to AtlasLoot." },
    { "Attune Helper", "Qt", "Attunement", "AttuneHelper", "https://github.com/RosemyneH/AttuneHelperHosting.git", "Auto-equips attuneable gear and more." },
    { "AttuneProgress", "QT", "Attunement", "AttuneProgress", "https://github.com/RosemyneH/AttuneProgress.git", "Tracks attunement progress in inventory." },
    { "Droms Instance Timer", "Dromkal", "Dungeon", "DromsInstanceTimer", "https://github.com/binnesman/DromsInstanceTimer-1.0.0.git", "Tracks how many instances you've entered in the last hour." },
    { "Droms Kill Counter", "Dromkal", "Loot", "DromsTrashKillCount", "https://github.com/binnesman/DromsKillCounter.git", "Shows attuneable loot in your current zone." },
    { "ElvUI", "QT", "UI Suite", "ElvUI", "https://github.com/RosemyneH/ElvUI_Attune.git", "ElvUI mod for Synastria." },
    { "MehTrajectory", "Meh", "Map", "MehTrajectory", "https://github.com/SynScoots/MehTrajectory.git", "Tracks movement trajectory on the map." },
    { "Postal", "Lulleh", "Utility", "Postal", "https://github.com/binnesman/Postal.git", "Postal mod with faster Open All speed." },
    { "Qt's Auto-Roller", "Qt", "Loot", "qtRoll", "https://github.com/RosemyneH/qtRoll.git", "Automatically rolls for loot while in a group." },
    { "Scoots' Combat Attune Watch", "Scoots", "Combat", "ScootsCombatAttuneWatch", "https://github.com/SynScoots/ScootsCombatAttuneWatch.git", "Tracks attunement progress in combat." },
    { "Scoots' Confirmation Skip", "Scoots", "Quality of Life", "ScootsConfirmationSkip", "https://github.com/SynScoots/ScootsConfirmationSkip.git", "Skips confirmation prompts for BoE equips and BoP looting." },
    { "Scoots' Craft", "Scoots", "Profession", "ScootsCraft", "https://github.com/SynScoots/ScootsCraft.git", "A complete overhaul of the profession UI." },
    { "Scoots' Quick Auction", "Scoots", "Auction", "ScootsQuickAuction", "https://github.com/SynScoots/ScootsQuickAuction.git", "Rapidly creates auctions at a specified price." },
    { "Scoots' Rares", "Scoots", "Map", "ScootsRares", "https://github.com/SynScoots/ScootsRares.git", "Creates a chat link to run .findnpc when near a rare." },
    { "Scoots' Speedrun", "Scoots", "Dungeon", "ScootsSpeedrun", "https://github.com/SynScoots/ScootsSpeedrun.git", "Instantly selects dialogue choices in dungeons." },
    { "Scoots' Stats", "Scoots", "Character", "ScootsStats", "https://github.com/SynScoots/ScootsStats.git", "Adds a stat panel beside the character panel." },
    { "Scoots' Vendor Filter", "Scoots", "Vendor", "ScootsVendorFilter", "https://github.com/SynScoots/ScootsVendorFilter.git", "Converts vendors to a scrollable list." },
    { "Scoots' Vendor Forge", "Scoots", "Vendor", "ScootsVendorForge", "https://github.com/SynScoots/ScootsVendorForge.git", "Continuously purchases vendor items until forged." },
    { "Synastria Build Manager", "Namira", "Builds", "SynastriaBuildManager", "https://github.com/Lubricated/SynastriaBuildManager.git", "Exports and imports perks and talents." },
    { "Synastria Tooltip Enhancer", "Lulleh", "Tooltip", "SynastriaTooltipEnhancer", "https://github.com/binnesman/Synastria-Tooltip-Enhancer.git", "Enhances Synastria item tooltips." },
    { "The Journal", "Qt", "Dungeon", "TheJournal", "https://github.com/RosemyneH/TheJournal.git", "Dungeon journal with item and boss details." },
    { "WarpRing", "Namira", "Utility", "WarpRing", "https://github.com/binnesman/namira_warpring.git", "Warp Ring UI." }
};

const AhcAddon *ahc_addon_catalog_items(void)
{
    return AHC_ADDONS;
}

size_t ahc_addon_catalog_count(void)
{
    return sizeof(AHC_ADDONS) / sizeof(AHC_ADDONS[0]);
}
