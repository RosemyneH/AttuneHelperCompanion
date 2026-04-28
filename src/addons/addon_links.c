#include "addons/addon_links.h"

#include <stdio.h>
#include <string.h>

static bool has_http_scheme(const char *url)
{
    return url && (strncmp(url, "https://", 8) == 0 || strncmp(url, "http://", 7) == 0);
}

static bool is_synastria_hub_url(const char *url)
{
    return url && strstr(url, "github.com/RosemyneH/synastria-monorepo-addons") != NULL;
}

static bool github_owner_from_avatar(const char *avatar_url, char *out, size_t out_capacity)
{
    if (!avatar_url || !out || out_capacity == 0) {
        return false;
    }
    const char *needle = strstr(avatar_url, "github.com/");
    if (!needle) {
        return false;
    }
    needle += strlen("github.com/");
    if (!*needle) {
        return false;
    }
    size_t i = 0;
    while (needle[i] && needle[i] != '.' && needle[i] != '/' && needle[i] != '?' && i + 1 < out_capacity) {
        out[i] = needle[i];
        i++;
    }
    out[i] = '\0';
    return i > 0;
}

static bool source_subdir_hub_tree_url(const AhcAddon *addon, char *out, size_t out_capacity)
{
    if (!addon || !addon->source_subdir || !addon->source_subdir[0] || !out || out_capacity == 0) {
        return false;
    }
    const char *source_subdir = addon->source_subdir;
    while (*source_subdir == '/' || *source_subdir == '\\') {
        source_subdir++;
    }
    if (!source_subdir[0]) {
        return false;
    }
    int n = snprintf(out, out_capacity, "https://github.com/RosemyneH/synastria-monorepo-addons/tree/main/%s", source_subdir);
    return n > 0 && (size_t)n < out_capacity;
}

static bool infer_upstream_repo_url(const AhcAddon *addon, char *out, size_t out_capacity)
{
    if (!addon || !addon->folder || !addon->folder[0] || !addon->source_subdir || !addon->source_subdir[0]) {
        return false;
    }
    if (strncmp(addon->source_subdir, "addons/", 7) != 0) {
        return false;
    }
    char owner[128];
    if (!github_owner_from_avatar(addon->avatar_url, owner, sizeof(owner))) {
        return false;
    }
    if (strcmp(owner, "RosemyneH") == 0) {
        return false;
    }
    int n = snprintf(out, out_capacity, "https://github.com/%s/%s", owner, addon->folder);
    return n > 0 && (size_t)n < out_capacity;
}

const char *ahc_addon_install_url(const AhcAddon *addon)
{
    if (!addon) {
        return NULL;
    }
    if (addon->install_url && addon->install_url[0]) {
        return addon->install_url;
    }
    return addon->repo;
}

bool ahc_addon_install_is_zip(const AhcAddon *addon)
{
    const char *install_url = ahc_addon_install_url(addon);
    return install_url && strstr(install_url, ".zip") != NULL;
}

bool ahc_addon_install_is_git_checkout(const AhcAddon *addon)
{
    const char *install_url = ahc_addon_install_url(addon);
    if (!install_url || !install_url[0]) {
        return false;
    }
    return strstr(install_url, "github.com/") != NULL || strstr(install_url, ".git") != NULL;
}

bool ahc_addon_source_page_url(const AhcAddon *addon, char *out, size_t out_capacity)
{
    if (!addon || !out || out_capacity == 0) {
        return false;
    }
    if (addon->page_url && addon->page_url[0]) {
        if (is_synastria_hub_url(addon->page_url) && infer_upstream_repo_url(addon, out, out_capacity)) {
            return true;
        }
        if (is_synastria_hub_url(addon->page_url) && source_subdir_hub_tree_url(addon, out, out_capacity)) {
            return true;
        }
        snprintf(out, out_capacity, "%s", addon->page_url);
        return true;
    }
    if (has_http_scheme(addon->repo)) {
        if (is_synastria_hub_url(addon->repo) && infer_upstream_repo_url(addon, out, out_capacity)) {
            return true;
        }
        if (is_synastria_hub_url(addon->repo) && source_subdir_hub_tree_url(addon, out, out_capacity)) {
            return true;
        }
        if (strstr(addon->repo, ".zip") != NULL) {
            return false;
        }
        snprintf(out, out_capacity, "%s", addon->repo);
        return true;
    }
    return false;
}
