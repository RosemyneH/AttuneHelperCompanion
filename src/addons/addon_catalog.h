#ifndef AHC_ADDON_CATALOG_H
#define AHC_ADDON_CATALOG_H

#include <stddef.h>

typedef struct AhcAddon {
    const char *name;
    const char *author;
    const char *category;
    const char *folder;
    const char *repo;
    const char *description;
} AhcAddon;

const AhcAddon *ahc_addon_catalog_items(void);
size_t ahc_addon_catalog_count(void);

#endif
