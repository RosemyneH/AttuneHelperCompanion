#include "addons/wow_default_addons.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    if (!ahc_is_blizzard_default_addon_folder("Blizzard_AuctionUI")) {
        fprintf(stderr, "expected Blizzard_AuctionUI true\n");
        return 1;
    }
    if (!ahc_is_blizzard_default_addon_folder("blizzard_auctionui")) {
        fprintf(stderr, "expected case fold match\n");
        return 1;
    }
    if (ahc_is_blizzard_default_addon_folder("Attune")) {
        fprintf(stderr, "expected custom folder false\n");
        return 1;
    }
    if (ahc_is_blizzard_default_addon_folder("Blizzard_")) {
        fprintf(stderr, "expected prefix false\n");
        return 1;
    }
    if (ahc_is_blizzard_default_addon_folder("")) {
        fprintf(stderr, "expected empty false\n");
        return 1;
    }
    if (ahc_is_blizzard_default_addon_folder(NULL)) {
        fprintf(stderr, "expected null false\n");
        return 1;
    }
    return 0;
}
