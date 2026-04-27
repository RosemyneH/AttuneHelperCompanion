#include "addons/wow_default_addons.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

/* ʕ •ᴥ•ʔ 3.3.5a client-bundled Interface add-on folders; match on disk case-insensitively. ʕ •ᴥ•ʔ */
static int streq_ascii_fold(const char *a, const char *b)
{
    for (; *a != '\0' || *b != '\0'; a++, b++) {
        unsigned char ca = (unsigned char)*a;
        unsigned char cb = (unsigned char)*b;
        if (tolower(ca) != tolower(cb)) {
            return 0;
        }
    }
    return 1;
}

static const char *const AHC_WOW_335_DEFAULT_BLIZZARD_FOLDERS[] = {
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
};

bool ahc_is_blizzard_default_addon_folder(const char *folder_name)
{
    if (!folder_name || folder_name[0] == '\0') {
        return false;
    }
    const size_t n = sizeof(AHC_WOW_335_DEFAULT_BLIZZARD_FOLDERS) / sizeof(AHC_WOW_335_DEFAULT_BLIZZARD_FOLDERS[0]);
    for (size_t i = 0; i < n; i++) {
        if (streq_ascii_fold(folder_name, AHC_WOW_335_DEFAULT_BLIZZARD_FOLDERS[i])) {
            return true;
        }
    }
    return false;
}
