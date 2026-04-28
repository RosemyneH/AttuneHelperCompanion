#include "addons/addon_links.h"
#include "addons/addon_manifest.h"

#include <stdio.h>
#include <string.h>

static const AhcAddon *find_addon_by_id(const AhcAddonManifest *manifest, const char *id)
{
    for (size_t i = 0; i < manifest->count; i++) {
        if (manifest->items[i].id && strcmp(manifest->items[i].id, id) == 0) {
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

    const AhcAddon *acp_zero = find_addon_by_id(&manifest, "acp-zero");
    if (!acp_zero) {
        fprintf(stderr, "acp-zero not found in manifest.\n");
        ahc_addon_manifest_free(&manifest);
        return 1;
    }

    char url[512];
    if (!ahc_addon_source_page_url(acp_zero, url, sizeof(url))) {
        fprintf(stderr, "Could not resolve source page URL for acp-zero.\n");
        ahc_addon_manifest_free(&manifest);
        return 1;
    }
    if (strcmp(url, "https://github.com/DivideByZeroToDeleteWorld/ACP-Zero") != 0) {
        fprintf(stderr, "Unexpected acp-zero source page URL: %s\n", url);
        ahc_addon_manifest_free(&manifest);
        return 1;
    }

    if (!ahc_addon_install_url(acp_zero) || strcmp(ahc_addon_install_url(acp_zero), "https://github.com/RosemyneH/synastria-monorepo-addons.git") != 0) {
        fprintf(stderr, "acp-zero install URL should remain monorepo.\n");
        ahc_addon_manifest_free(&manifest);
        return 1;
    }

    const AhcAddon *attune_helper = find_addon_by_id(&manifest, "attune-helper");
    if (!attune_helper) {
        fprintf(stderr, "attune-helper not found in manifest.\n");
        ahc_addon_manifest_free(&manifest);
        return 1;
    }
    if (!ahc_addon_source_page_url(attune_helper, url, sizeof(url))) {
        fprintf(stderr, "Could not resolve source page URL for attune-helper.\n");
        ahc_addon_manifest_free(&manifest);
        return 1;
    }
    if (strcmp(url, "https://github.com/RosemyneH/AttuneHelper") != 0) {
        fprintf(stderr, "Unexpected attune-helper source page URL: %s\n", url);
        ahc_addon_manifest_free(&manifest);
        return 1;
    }

    ahc_addon_manifest_free(&manifest);
    return 0;
}
