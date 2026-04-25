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

int main(void)
{
    AhcAddonManifest manifest = { 0 };
    if (!ahc_addon_manifest_load_file(AHC_TEST_MANIFEST_PATH, &manifest)) {
        fprintf(stderr, "Failed to load addon manifest: %s\n", AHC_TEST_MANIFEST_PATH);
        return 1;
    }

    if (manifest.count < 34) {
        fprintf(stderr, "Expected at least 34 addons, got %zu\n", manifest.count);
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

    ahc_addon_manifest_free(&manifest);
    return 0;
}
