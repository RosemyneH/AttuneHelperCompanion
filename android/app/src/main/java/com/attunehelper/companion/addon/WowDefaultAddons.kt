package com.attunehelper.companion.addon

object WowDefaultAddons {
    private val BLIZZARD_335_FOLDERS: Set<String> = setOf(
        "Blizzard_AchievementUI",
        "Blizzard_ArenaUI",
        "Blizzard_AuctionUI",
        "Blizzard_BarbershopUI",
        "Blizzard_BattlefieldMinimap",
        "Blizzard_BindingUI",
        "Blizzard_Calendar",
        "Blizzard_CombatLog",
        "Blizzard_CombatText",
        "Blizzard_CraftUI",
        "Blizzard_DebugTools",
        "Blizzard_GlyphUI",
        "Blizzard_GMChatUI",
        "Blizzard_GuildBankUI",
        "Blizzard_InspectUI",
        "Blizzard_ItemSocketingUI",
        "Blizzard_MacroUI",
        "Blizzard_RaidUI",
        "Blizzard_TalentUI",
        "Blizzard_TimeManager",
        "Blizzard_TokenUI",
        "Blizzard_TradeSkillUI",
        "Blizzard_TrainerUI",
        "Blizzard_VehicleUI",
    )

    fun isBlizzardDefaultFolder(folder: String): Boolean {
        if (folder.isEmpty()) {
            return false
        }
        return BLIZZARD_335_FOLDERS.any { it.equals(folder, ignoreCase = true) }
    }
}
