#ifndef AHC_ADDON_MANIFEST_H
#define AHC_ADDON_MANIFEST_H

#include "addons/addon_catalog.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct AhcAddonManifest {
    AhcAddon *items;
    size_t count;
    void *storage;
} AhcAddonManifest;

bool ahc_addon_manifest_load_file(const char *path, AhcAddonManifest *manifest);
void ahc_addon_manifest_free(AhcAddonManifest *manifest);
size_t ahc_addon_manifest_storage_bytes(const AhcAddonManifest *manifest);

#endif
