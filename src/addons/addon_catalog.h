#ifndef AHC_ADDON_CATALOG_H
#define AHC_ADDON_CATALOG_H

#include <stddef.h>

typedef struct AhcAddon {
    const char *id;
    const char *name;
    const char *author;
    const char *category;
    const char *folder;
    const char *repo;
    const char *description;
    const char *source_subdir;
    const char *avatar_url;
    const char *version;
    const char *source;
    const char *page_url;
    const char *install_url;
    const char **categories;
    size_t category_count;
} AhcAddon;

const AhcAddon *ahc_addon_catalog_items(void);
size_t ahc_addon_catalog_count(void);

#endif
