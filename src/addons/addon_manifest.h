#ifndef AHC_ADDON_MANIFEST_H
#define AHC_ADDON_MANIFEST_H

#include "addons/addon_catalog.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct AhcAddonManifest {
    AhcAddon *items;
    size_t count;
} AhcAddonManifest;

bool ahc_addon_manifest_load_file(const char *path, AhcAddonManifest *manifest);
void ahc_addon_manifest_free(AhcAddonManifest *manifest);

#endif
