#include "addons/addon_catalog.h"

static const AhcAddon AHC_ADDONS[] = {
    { "ArkInventory", "Scoots", "ArkInventory", "https://github.com/SynScoots/ArkInventory-modified-for-attunements-.git", "Adds support for attunements." },
    { "AtlasLoot Mythic", "imevul", "AtlasLoot_Mythic", "https://github.com/imevul/AtlasLoot_Mythic.git", "Adds mythic items to AtlasLoot." },
    { "Attune Helper", "Qt", "AttuneHelper", "https://github.com/RosemyneH/AttuneHelperHosting.git", "Auto-equips attuneable gear and more." },
    { "AttuneProgress", "QT", "AttuneProgress", "https://github.com/RosemyneH/AttuneProgress.git", "Tracks attunement progress in inventory." },
    { "Droms Instance Timer", "Dromkal", "DromsInstanceTimer", "https://github.com/binnesman/DromsInstanceTimer-1.0.0.git", "Tracks how many instances you've entered in the last hour." },
    { "Droms Kill Counter", "Dromkal", "DromsTrashKillCount", "https://github.com/binnesman/DromsKillCounter.git", "Shows attuneable loot in your current zone." },
    { "ElvUI", "QT", "ElvUI", "https://github.com/RosemyneH/ElvUI_Attune.git", "ElvUI mod for Synastria." },
    { "MehTrajectory", "Meh", "MehTrajectory", "https://github.com/SynScoots/MehTrajectory.git", "Tracks movement trajectory on the map." },
    { "Postal", "Lulleh", "Postal", "https://github.com/binnesman/Postal.git", "Postal mod with faster Open All speed." },
    { "Qt's Auto-Roller", "Qt", "qtRoll", "https://github.com/RosemyneH/qtRoll.git", "Automatically rolls for loot while in a group." },
    { "Scoots' Combat Attune Watch", "Scoots", "ScootsCombatAttuneWatch", "https://github.com/SynScoots/ScootsCombatAttuneWatch.git", "Tracks attunement progress in combat." },
    { "Scoots' Confirmation Skip", "Scoots", "ScootsConfirmationSkip", "https://github.com/SynScoots/ScootsConfirmationSkip.git", "Skips confirmation prompts for BoE equips and BoP looting." },
    { "Scoots' Craft", "Scoots", "ScootsCraft", "https://github.com/SynScoots/ScootsCraft.git", "A complete overhaul of the profession UI." },
    { "Scoots' Quick Auction", "Scoots", "ScootsQuickAuction", "https://github.com/SynScoots/ScootsQuickAuction.git", "Rapidly creates auctions at a specified price." },
    { "Scoots' Rares", "Scoots", "ScootsRares", "https://github.com/SynScoots/ScootsRares.git", "Creates a chat link to run .findnpc when near a rare." },
    { "Scoots' Speedrun", "Scoots", "ScootsSpeedrun", "https://github.com/SynScoots/ScootsSpeedrun.git", "Instantly selects dialogue choices in dungeons." },
    { "Scoots' Stats", "Scoots", "ScootsStats", "https://github.com/SynScoots/ScootsStats.git", "Adds a stat panel beside the character panel." },
    { "Scoots' Vendor Filter", "Scoots", "ScootsVendorFilter", "https://github.com/SynScoots/ScootsVendorFilter.git", "Converts vendors to a scrollable list." },
    { "Scoots' Vendor Forge", "Scoots", "ScootsVendorForge", "https://github.com/SynScoots/ScootsVendorForge.git", "Continuously purchases vendor items until forged." },
    { "Synastria Build Manager", "Namira", "SynastriaBuildManager", "https://github.com/Lubricated/SynastriaBuildManager.git", "Exports and imports perks and talents." },
    { "Synastria Tooltip Enhancer", "Lulleh", "SynastriaTooltipEnhancer", "https://github.com/binnesman/Synastria-Tooltip-Enhancer.git", "Enhances Synastria item tooltips." },
    { "The Journal", "Qt", "TheJournal", "https://github.com/RosemyneH/TheJournal.git", "Dungeon journal with item and boss details." },
    { "WarpRing", "Namira", "WarpRing", "https://github.com/binnesman/namira_warpring.git", "Warp Ring UI." }
};

const AhcAddon *ahc_addon_catalog_items(void)
{
    return AHC_ADDONS;
}

size_t ahc_addon_catalog_count(void)
{
    return sizeof(AHC_ADDONS) / sizeof(AHC_ADDONS[0]);
}
