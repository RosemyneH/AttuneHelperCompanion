#include "addons/addon_manifest.h"

#include <stdio.h>
#include <string.h>

static const AhcAddon *find_addon(const AhcAddonManifest *manifest, const char *name)
{
    for (size_t i = 0; i < manifest->count; i++) {
        if (strcmp(manifest->items[i].name, name) == 0) {
            return &manifest->items[i];
        }
    }
    return NULL;
}

static const AhcAddon *find_addon_by_id(const AhcAddonManifest *manifest, const char *id)
{
    for (size_t i = 0; i < manifest->count; i++) {
        if (manifest->items[i].id && strcmp(manifest->items[i].id, id) == 0) {
            return &manifest->items[i];
        }
    }
    return NULL;
}

static int addon_has_category(const AhcAddon *addon, const char *category)
{
    if (addon->categories && addon->category_count > 0u) {
        for (size_t i = 0; i < addon->category_count; i++) {
            if (strcmp(addon->categories[i], category) == 0) {
                return 1;
            }
        }
        return 0;
    }
    return addon->category && strcmp(addon->category, category) == 0;
}

static int baked_catalog_has_source(const char *source)
{
    const AhcAddon *addons = ahc_addon_catalog_items();
    size_t count = ahc_addon_catalog_count();
    for (size_t i = 0; i < count; i++) {
        if (addons[i].source && strcmp(addons[i].source, source) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    if (ahc_addon_catalog_count() < 30u) {
        fprintf(stderr, "Baked addon catalog is too small (curated list).\n");
        return 1;
    }
    if (baked_catalog_has_source("Warperia")) {
        fprintf(stderr, "Baked catalog should not include Warperia entries.\n");
        return 1;
    }

    AhcAddonManifest manifest = { 0 };
    if (!ahc_addon_manifest_load_file(AHC_TEST_MANIFEST_PATH, &manifest)) {
        fprintf(stderr, "Failed to load addon manifest: %s\n", AHC_TEST_MANIFEST_PATH);
        return 1;
    }

    if (manifest.count < 30) {
        fprintf(stderr, "Expected at least 30 curated addons, got %zu\n", manifest.count);
        ahc_addon_manifest_free(&manifest);
        return 1;
    }

    const AhcAddon *questie = find_addon(&manifest, "Questie 3.3.5");
    if (!questie || !questie->source_subdir || strcmp(questie->source_subdir, "Questie-335") != 0) {
        fprintf(stderr, "Questie source_subdir was not parsed correctly.\n");
        ahc_addon_manifest_free(&manifest);
        return 1;
    }

    const AhcAddon *lizard = find_addon(&manifest, "LizardDMP");
    if (!lizard || strcmp(lizard->repo, "https://github.com/Versicoloris/LizardDMP.git") != 0) {
        fprintf(stderr, "LizardDMP repo was not parsed correctly.\n");
        ahc_addon_manifest_free(&manifest);
        return 1;
    }

    const AhcAddon *scoots_stats = find_addon(&manifest, "Scoots' Stats");
    if (!scoots_stats
        || scoots_stats->category_count != 3u
        || !addon_has_category(scoots_stats, "Attunement")
        || !addon_has_category(scoots_stats, "Quality of Life")
        || !addon_has_category(scoots_stats, "Community Favorites")) {
        fprintf(stderr, "Scoots' Stats categories were not parsed correctly.\n");
        ahc_addon_manifest_free(&manifest);
        return 1;
    }

    if (!questie->categories || questie->category_count != 1u || !addon_has_category(questie, "Questing")) {
        fprintf(stderr, "Legacy Questie category was not synthesized correctly.\n");
        ahc_addon_manifest_free(&manifest);
        return 1;
    }

    {
        const AhcAddon *acp_zero = find_addon_by_id(&manifest, "acp-zero");
        if (!acp_zero
            || strcmp(acp_zero->repo, "https://github.com/DivideByZeroToDeleteWorld/ACP-Zero.git") != 0) {
            fprintf(stderr, "acp-zero repo (manifest install merge) was not parsed correctly.\n");
            ahc_addon_manifest_free(&manifest);
            return 1;
        }
    }

    ahc_addon_manifest_free(&manifest);
    return 0;
}
