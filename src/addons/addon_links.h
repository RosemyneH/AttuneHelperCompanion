#ifndef AHC_ADDON_LINKS_H
#define AHC_ADDON_LINKS_H

#include "addons/addon_catalog.h"

#include <stdbool.h>
#include <stddef.h>

const char *ahc_addon_install_url(const AhcAddon *addon);
bool ahc_addon_install_is_zip(const AhcAddon *addon);
bool ahc_addon_install_is_git_checkout(const AhcAddon *addon);
bool ahc_addon_source_page_url(const AhcAddon *addon, char *out, size_t out_capacity);

#endif
