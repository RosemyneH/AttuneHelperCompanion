#include "addons/addon_catalog.h"
#include "addons/addon_manifest.h"
#include "attune/attune_snapshot.h"

#include "raylib.h"
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4100 4244 4267 4456 4996)
#endif
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <direct.h>
#define AHC_MKDIR(path) _mkdir(path)
#define AHC_RMDIR(path) _rmdir(path)
#define AHC_STRICMP _stricmp
#else
#include <sys/stat.h>
#include <unistd.h>
#define AHC_MKDIR(path) mkdir(path, 0755)
#define AHC_RMDIR(path) rmdir(path)
#define AHC_STRICMP strcasecmp
#endif

#define AHC_PATH_CAPACITY 768
#define AHC_STATUS_CAPACITY 256
#define AHC_SCAN_MAX_DIRECTORIES 6000
#define AHC_HISTORY_CAPACITY 512
#define AHC_CATEGORY_CAPACITY 64

#if !defined(_WIN32)
#include <strings.h>
#endif

typedef enum CompanionTab {
    COMPANION_TAB_ATTUNES = 0,
    COMPANION_TAB_ADDONS = 1,
    COMPANION_TAB_SETTINGS = 2
} CompanionTab;

typedef enum GraphMetric {
    GRAPH_METRIC_TOTAL = 0,
    GRAPH_METRIC_WARFORGED = 1,
    GRAPH_METRIC_LIGHTFORGED = 2,
    GRAPH_METRIC_TITANFORGED = 3
} GraphMetric;

typedef enum GraphDisplay {
    GRAPH_DISPLAY_BARS = 0,
    GRAPH_DISPLAY_PLOT = 1
} GraphDisplay;

typedef struct CompanionState {
    CompanionTab tab;
    char config_dir[AHC_PATH_CAPACITY];
    char config_path[AHC_PATH_CAPACITY];
    char history_path[AHC_PATH_CAPACITY];
    char synastria_path[AHC_PATH_CAPACITY];
    char snapshot_path[AHC_PATH_CAPACITY];
    char status[AHC_STATUS_CAPACITY];
    AhcDailyAttuneSnapshot snapshot;
    AhcDailyAttuneSnapshot history[AHC_HISTORY_CAPACITY];
    size_t history_count;
    AhcAddonManifest addon_manifest;
    const AhcAddon *addons;
    size_t addon_count;
    bool addon_manifest_loaded;
    bool synastria_path_editing;
    int addon_scroll;
    GraphMetric graph_metric;
    GraphDisplay graph_display;
    char selected_addon_category[64];
    char addon_catalog_source[AHC_PATH_CAPACITY];
} CompanionState;

static const Color AHC_BACKGROUND = { 10, 8, 7, 255 };
static const Color AHC_PANEL = { 31, 27, 22, 255 };
static const Color AHC_PANEL_HOVER = { 45, 38, 29, 255 };
static const Color AHC_PANEL_DARK = { 16, 14, 13, 255 };
static const Color AHC_BORDER = { 132, 98, 45, 255 };
static const Color AHC_BORDER_BRIGHT = { 214, 169, 80, 255 };
static const Color AHC_TEXT = { 248, 232, 190, 255 };
static const Color AHC_MUTED = { 188, 170, 132, 255 };
static const Color AHC_ACCENT = { 89, 196, 255, 255 };
static const Color AHC_ACCENT_DARK = { 20, 91, 135, 255 };
static const Color AHC_PARCHMENT = { 68, 51, 31, 255 };
static const Color AHC_TF = { 143, 139, 255, 255 };
static const Color AHC_WF = { 255, 150, 86, 255 };
static const Color AHC_LF = { 255, 226, 115, 255 };

static Font g_ui_font;
static bool g_has_ui_font;

static void draw_text(const char *text, int x, int y, int size, Color color)
{
    if (g_has_ui_font) {
        DrawTextEx(g_ui_font, text, (Vector2){ (float)x, (float)y }, (float)size, 1.0f, color);
        return;
    }

    DrawText(text, x, y, size, color);
}

static int measure_text_width(const char *text, int size)
{
    if (g_has_ui_font) {
        Vector2 measured = MeasureTextEx(g_ui_font, text, (float)size, 1.0f);
        return (int)measured.x;
    }

    return MeasureText(text, size);
}

static int content_left(void)
{
    return 26;
}

static int content_right(void)
{
    int right = GetScreenWidth() - 26;
    return right > content_left() + 420 ? right : content_left() + 420;
}

static int content_top(void)
{
    return 204;
}

static int content_bottom(void)
{
    int bottom = GetScreenHeight() - 66;
    return bottom > content_top() + 300 ? bottom : content_top() + 300;
}

static Rectangle content_rect(void)
{
    return (Rectangle){
        (float)content_left(),
        (float)content_top(),
        (float)(content_right() - content_left()),
        (float)(content_bottom() - content_top())
    };
}

static void draw_centered_text(const char *label, Rectangle bounds, int size, Color color)
{
    int text_width = measure_text_width(label, size);
    int x = (int)(bounds.x + (bounds.width - (float)text_width) * 0.5f);
    int y = (int)(bounds.y + (bounds.height - (float)size) * 0.5f) - 1;
    draw_text(label, x, y, size, color);
}

static bool draw_wow_button(const char *label, Rectangle bounds, bool active, int text_size)
{
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);
    bool pressed = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    Color fill = active ? AHC_ACCENT_DARK : AHC_PANEL_DARK;
    Color border = active ? AHC_ACCENT : AHC_BORDER;
    Color text = active ? RAYWHITE : AHC_TEXT;

    if (hovered) {
        fill = active ? (Color){ 30, 117, 164, 255 } : AHC_PANEL_HOVER;
        border = active ? (Color){ 142, 221, 255, 255 } : AHC_BORDER_BRIGHT;
        text = RAYWHITE;
    }
    if (pressed) {
        fill = active ? (Color){ 12, 76, 116, 255 } : AHC_PARCHMENT;
    }

    DrawRectangleRounded((Rectangle){ bounds.x + 3.0f, bounds.y + 4.0f, bounds.width, bounds.height }, 0.16f, 8, (Color){ 0, 0, 0, 120 });
    DrawRectangleRounded(bounds, 0.16f, 8, fill);
    DrawRectangleRoundedLines(bounds, 0.16f, 8, border);
    DrawRectangle((int)bounds.x + 4, (int)bounds.y + 3, (int)bounds.width - 8, 1, (Color){ 255, 238, 174, hovered ? 75 : 35 });
    draw_centered_text(label, bounds, text_size, text);

    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void load_ui_font(void)
{
    const char *font_paths[] = {
#if defined(_WIN32)
        "C:/Windows/Fonts/segoeuib.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arialbd.ttf",
        "C:/Windows/Fonts/arial.ttf",
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
#endif
        NULL
    };

    for (int i = 0; font_paths[i]; i++) {
        if (!FileExists(font_paths[i])) {
            continue;
        }

        g_ui_font = LoadFontEx(font_paths[i], 34, NULL, 0);
        if (g_ui_font.texture.id != 0) {
            SetTextureFilter(g_ui_font.texture, TEXTURE_FILTER_BILINEAR);
            GuiSetFont(g_ui_font);
            g_has_ui_font = true;
            return;
        }
    }
}

static void set_status(CompanionState *state, const char *message);
static void path_join(char *out, size_t out_capacity, const char *left, const char *right);
static bool validate_synastria_path(const char *path);
static bool addon_has_source_subdir(const AhcAddon *addon)
{
    return addon->source_subdir && addon->source_subdir[0];
}
static bool resolve_addons_path(const CompanionState *state, char *out, size_t out_capacity)
{
    if (!validate_synastria_path(state->synastria_path)) {
        return false;
    }

    char interface_path[AHC_PATH_CAPACITY];
    path_join(interface_path, sizeof(interface_path), state->synastria_path, "Interface");
    path_join(out, out_capacity, interface_path, "AddOns");

    if (!DirectoryExists(interface_path)) {
        AHC_MKDIR(interface_path);
    }
    if (!DirectoryExists(out)) {
        AHC_MKDIR(out);
    }

    return DirectoryExists(out);
}

static bool addon_install_path(const CompanionState *state, const AhcAddon *addon, char *out, size_t out_capacity)
{
    char addons_path[AHC_PATH_CAPACITY];
    if (!resolve_addons_path(state, addons_path, sizeof(addons_path))) {
        return false;
    }

    path_join(out, out_capacity, addons_path, addon->folder);
    return true;
}

static bool addon_is_installed(const CompanionState *state, const AhcAddon *addon)
{
    char path[AHC_PATH_CAPACITY];
    return addon_install_path(state, addon, path, sizeof(path)) && DirectoryExists(path);
}

static bool addon_is_git_checkout(const CompanionState *state, const AhcAddon *addon)
{
    char path[AHC_PATH_CAPACITY];
    char git_path[AHC_PATH_CAPACITY];
    if (!addon_install_path(state, addon, path, sizeof(path))) {
        return false;
    }

    path_join(git_path, sizeof(git_path), path, ".git");
    return DirectoryExists(git_path);
}

static bool addon_has_managed_marker(const CompanionState *state, const AhcAddon *addon)
{
    char path[AHC_PATH_CAPACITY];
    char marker_path[AHC_PATH_CAPACITY];
    if (!addon_install_path(state, addon, path, sizeof(path))) {
        return false;
    }

    path_join(marker_path, sizeof(marker_path), path, ".attune-helper-companion");
    return FileExists(marker_path);
}

static bool addon_is_managed(const CompanionState *state, const AhcAddon *addon)
{
    return addon_is_git_checkout(state, addon) || addon_has_managed_marker(state, addon);
}

static bool remove_directory_tree(const char *path)
{
    if (!DirectoryExists(path)) {
        return true;
    }

    FilePathList entries = LoadDirectoryFiles(path);
    bool removed = true;

    for (unsigned int i = 0; i < entries.count; i++) {
        const char *entry = entries.paths[i];
        const char *name = GetFileName(entry);
        if (AHC_STRICMP(name, ".") == 0 || AHC_STRICMP(name, "..") == 0) {
            continue;
        }

        if (DirectoryExists(entry)) {
            if (!remove_directory_tree(entry)) {
                removed = false;
            }
        } else if (remove(entry) != 0) {
            removed = false;
        }
    }

    UnloadDirectoryFiles(entries);

    if (AHC_RMDIR(path) != 0) {
        removed = false;
    }

    return removed;
}

static bool addon_backup_root(const CompanionState *state, char *out, size_t out_capacity)
{
    char addons_path[AHC_PATH_CAPACITY];
    if (!resolve_addons_path(state, addons_path, sizeof(addons_path))) {
        return false;
    }

    path_join(out, out_capacity, addons_path, "_AttuneHelperCompanionBackups");
    if (!DirectoryExists(out)) {
        AHC_MKDIR(out);
    }

    return DirectoryExists(out);
}

static bool addon_staging_root(const CompanionState *state, char *out, size_t out_capacity)
{
    char addons_path[AHC_PATH_CAPACITY];
    if (!resolve_addons_path(state, addons_path, sizeof(addons_path))) {
        return false;
    }

    path_join(out, out_capacity, addons_path, "_AttuneHelperCompanionStaging");
    if (!DirectoryExists(out)) {
        AHC_MKDIR(out);
    }

    return DirectoryExists(out);
}

static void make_backup_path(const CompanionState *state, const AhcAddon *addon, const char *reason, char *out, size_t out_capacity)
{
    char root[AHC_PATH_CAPACITY];
    if (!addon_backup_root(state, root, sizeof(root))) {
        out[0] = '\0';
        return;
    }

    char name[256];
    snprintf(name, sizeof(name), "%s-%s-%lld", addon->folder, reason, (long long)time(NULL));
    path_join(out, out_capacity, root, name);
}

static bool move_installed_addon_to_backup(CompanionState *state, const AhcAddon *addon, const char *path, const char *reason, char *backup_path, size_t backup_path_capacity)
{
    make_backup_path(state, addon, reason, backup_path, backup_path_capacity);
    if (!backup_path[0]) {
        set_status(state, "Could not create addon backup folder.");
        return false;
    }

    if (rename(path, backup_path) != 0) {
        set_status(state, "Could not move addon to backup folder.");
        return false;
    }

    return true;
}

static bool git_clone_to_path(const AhcAddon *addon, const char *path)
{
    char command[AHC_PATH_CAPACITY * 3];
    snprintf(command, sizeof(command), "git clone --depth 1 \"%s\" \"%s\"", addon->repo, path);
    return system(command) == 0;
}

static void write_addon_managed_marker(const AhcAddon *addon, const char *path)
{
    char marker_path[AHC_PATH_CAPACITY];
    path_join(marker_path, sizeof(marker_path), path, ".attune-helper-companion");

    FILE *file = fopen(marker_path, "wb");
    if (!file) {
        return;
    }

    fprintf(file, "repo=%s\n", addon->repo);
    fprintf(file, "source_subdir=%s\n", addon_has_source_subdir(addon) ? addon->source_subdir : "");
    fclose(file);
}

static bool download_addon_to_path(CompanionState *state, const AhcAddon *addon, const char *path)
{
    if (!addon_has_source_subdir(addon)) {
        return git_clone_to_path(addon, path);
    }

    char staging_root[AHC_PATH_CAPACITY];
    char clone_name[256];
    char clone_path[AHC_PATH_CAPACITY];
    char source_path[AHC_PATH_CAPACITY];

    if (!addon_staging_root(state, staging_root, sizeof(staging_root))) {
        return false;
    }

    snprintf(clone_name, sizeof(clone_name), "%s-repo-%lld", addon->folder, (long long)time(NULL));
    path_join(clone_path, sizeof(clone_path), staging_root, clone_name);
    remove_directory_tree(clone_path);

    if (!git_clone_to_path(addon, clone_path)) {
        remove_directory_tree(clone_path);
        return false;
    }

    path_join(source_path, sizeof(source_path), clone_path, addon->source_subdir);
    if (!DirectoryExists(source_path)) {
        remove_directory_tree(clone_path);
        return false;
    }

    remove_directory_tree(path);
    if (rename(source_path, path) != 0) {
        remove_directory_tree(clone_path);
        return false;
    }

    remove_directory_tree(clone_path);
    write_addon_managed_marker(addon, path);
    return DirectoryExists(path);
}

static void install_or_update_addon(CompanionState *state, const AhcAddon *addon)
{
    char path[AHC_PATH_CAPACITY];
    char command[AHC_PATH_CAPACITY * 3];

    if (!addon_install_path(state, addon, path, sizeof(path))) {
        set_status(state, "Set a valid Synastria folder before managing addons.");
        return;
    }

    bool was_installed = DirectoryExists(path);
    if (was_installed) {
        bool was_managed = addon_is_managed(state, addon);
        if (!addon_is_git_checkout(state, addon)) {
            char staging_root[AHC_PATH_CAPACITY];
            char staging_path[AHC_PATH_CAPACITY];
            char backup_path[AHC_PATH_CAPACITY];

            if (!addon_staging_root(state, staging_root, sizeof(staging_root))) {
                set_status(state, "Could not create addon staging folder.");
                return;
            }

            path_join(staging_path, sizeof(staging_path), staging_root, addon->folder);
            remove_directory_tree(staging_path);
            if (!download_addon_to_path(state, addon, staging_path)) {
                remove_directory_tree(staging_path);
                set_status(state, "Could not download addon update from GitHub.");
                return;
            }
            if (!move_installed_addon_to_backup(state, addon, path, was_managed ? "managed-update" : "manual-backup", backup_path, sizeof(backup_path))) {
                remove_directory_tree(staging_path);
                return;
            }

            if (rename(staging_path, path) != 0) {
                rename(backup_path, path);
                remove_directory_tree(staging_path);
                set_status(state, "Could not promote downloaded addon. Manual install was restored.");
                return;
            }
            char message[AHC_STATUS_CAPACITY];
            snprintf(message, sizeof(message), "%s %s. Backup saved.", was_managed ? "Updated" : "Updated manual", addon->name);
            set_status(state, message);
            return;
        }

        snprintf(command, sizeof(command), "git -C \"%s\" pull --ff-only", path);
    }

    int result = was_installed ? system(command) : (download_addon_to_path(state, addon, path) ? 0 : 1);
    char message[AHC_STATUS_CAPACITY];
    if (result == 0) {
        snprintf(message, sizeof(message), "%s %s.", was_installed ? "Updated" : "Installed", addon->name);
    } else {
        snprintf(message, sizeof(message), "Git command failed for %s.", addon->name);
    }
    set_status(state, message);
}

static void uninstall_addon(CompanionState *state, const AhcAddon *addon)
{
    char path[AHC_PATH_CAPACITY];
    char backup_path[AHC_PATH_CAPACITY];

    if (!addon_install_path(state, addon, path, sizeof(path)) || !DirectoryExists(path)) {
        set_status(state, "Addon is not installed.");
        return;
    }

    if (!move_installed_addon_to_backup(state, addon, path, "uninstalled", backup_path, sizeof(backup_path))) {
        return;
    }

    char message[AHC_STATUS_CAPACITY];
    snprintf(message, sizeof(message), "Uninstalled %s. Backup saved.", addon->name);
    set_status(state, message);
}

static void set_status(CompanionState *state, const char *message)
{
    snprintf(state->status, sizeof(state->status), "%s", message);
}

static void path_join(char *out, size_t out_capacity, const char *left, const char *right)
{
    size_t left_length = strlen(left);
    const char *separator = (left_length > 0 && (left[left_length - 1] == '/' || left[left_length - 1] == '\\')) ? "" : "/";
    snprintf(out, out_capacity, "%s%s%s", left, separator, right);
}

static void trim_line(char *text)
{
    size_t length = strlen(text);
    while (length > 0 && (text[length - 1] == '\r' || text[length - 1] == '\n' || text[length - 1] == ' ' || text[length - 1] == '\t')) {
        text[length - 1] = '\0';
        length--;
    }
}

static void resolve_config_paths(CompanionState *state)
{
#if defined(_WIN32)
    const char *base = getenv("APPDATA");
    if (!base || !base[0]) {
        base = ".";
    }
    path_join(state->config_dir, sizeof(state->config_dir), base, "AttuneHelperCompanion");
#else
    const char *base = getenv("XDG_CONFIG_HOME");
    if (base && base[0]) {
        path_join(state->config_dir, sizeof(state->config_dir), base, "attune-helper-companion");
    } else {
        const char *home = getenv("HOME");
        if (!home || !home[0]) {
            home = ".";
        }
        char config_root[AHC_PATH_CAPACITY];
        path_join(config_root, sizeof(config_root), home, ".config");
        path_join(state->config_dir, sizeof(state->config_dir), config_root, "attune-helper-companion");
    }
#endif
    path_join(state->config_path, sizeof(state->config_path), state->config_dir, "settings.ini");
    path_join(state->history_path, sizeof(state->history_path), state->config_dir, "attune-history.csv");
}

static void load_settings(CompanionState *state)
{
    FILE *file = fopen(state->config_path, "rb");
    if (!file) {
        return;
    }

    char line[AHC_PATH_CAPACITY + 32];
    while (fgets(line, sizeof(line), file)) {
        trim_line(line);
        if (strncmp(line, "synastria_path=", 15) == 0) {
            snprintf(state->synastria_path, sizeof(state->synastria_path), "%s", line + 15);
        }
    }

    fclose(file);
}

static bool save_settings(CompanionState *state)
{
    AHC_MKDIR(state->config_dir);

    FILE *file = fopen(state->config_path, "wb");
    if (!file) {
        set_status(state, "Could not save settings.");
        return false;
    }

    fprintf(file, "synastria_path=%s\n", state->synastria_path);
    fclose(file);
    set_status(state, "Settings saved.");
    return true;
}

static bool try_load_addon_manifest(CompanionState *state, const char *path)
{
    AhcAddonManifest manifest = { 0 };
    if (!ahc_addon_manifest_load_file(path, &manifest)) {
        return false;
    }

    ahc_addon_manifest_free(&state->addon_manifest);
    state->addon_manifest = manifest;
    state->addons = state->addon_manifest.items;
    state->addon_count = state->addon_manifest.count;
    state->addon_manifest_loaded = true;
    snprintf(state->addon_catalog_source, sizeof(state->addon_catalog_source), "%s", path);
    state->selected_addon_category[0] = '\0';
    state->addon_scroll = 0;
    return true;
}

static void load_addon_catalog(CompanionState *state)
{
    state->addons = ahc_addon_catalog_items();
    state->addon_count = ahc_addon_catalog_count();
    state->addon_manifest_loaded = false;
    snprintf(state->addon_catalog_source, sizeof(state->addon_catalog_source), "built-in fallback");

    const char *manifest_paths[] = {
        "manifest/addons.json",
        "../manifest/addons.json",
#if defined(AHC_SOURCE_MANIFEST_PATH)
        AHC_SOURCE_MANIFEST_PATH,
#endif
        NULL
    };

    for (int i = 0; manifest_paths[i]; i++) {
        if (try_load_addon_manifest(state, manifest_paths[i])) {
            return;
        }
    }
}

static int compare_snapshot_dates(const void *left, const void *right)
{
    const AhcDailyAttuneSnapshot *a = (const AhcDailyAttuneSnapshot *)left;
    const AhcDailyAttuneSnapshot *b = (const AhcDailyAttuneSnapshot *)right;
    int date_compare = strcmp(a->date, b->date);
    if (date_compare != 0) {
        return date_compare;
    }

    return a->account - b->account;
}

static void sort_history(CompanionState *state)
{
    qsort(state->history, state->history_count, sizeof(state->history[0]), compare_snapshot_dates);
}

static bool save_history(CompanionState *state)
{
    AHC_MKDIR(state->config_dir);

    FILE *file = fopen(state->history_path, "wb");
    if (!file) {
        set_status(state, "Could not save attune history.");
        return false;
    }

    fprintf(file, "date,account,warforged,lightforged,titanforged\n");
    for (size_t i = 0; i < state->history_count; i++) {
        const AhcDailyAttuneSnapshot *snapshot = &state->history[i];
        fprintf(file, "%s,%d,%d,%d,%d\n",
            snapshot->date,
            snapshot->account,
            snapshot->warforged,
            snapshot->lightforged,
            snapshot->titanforged);
    }

    fclose(file);
    return true;
}

static void load_history(CompanionState *state)
{
    FILE *file = fopen(state->history_path, "rb");
    if (!file) {
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        trim_line(line);
        if (!line[0] || strncmp(line, "date,", 5) == 0) {
            continue;
        }

        AhcDailyAttuneSnapshot snapshot;
        memset(&snapshot, 0, sizeof(snapshot));
        snapshot.found = true;

        int parsed = sscanf(line, "%15[^,],%d,%d,%d,%d",
            snapshot.date,
            &snapshot.account,
            &snapshot.warforged,
            &snapshot.lightforged,
            &snapshot.titanforged);

        if (parsed == 5 && state->history_count < AHC_HISTORY_CAPACITY) {
            state->history[state->history_count++] = snapshot;
        }
    }

    fclose(file);
    sort_history(state);
}

static void upsert_history_snapshot(CompanionState *state, const AhcDailyAttuneSnapshot *snapshot)
{
    if (!snapshot->found || !snapshot->date[0]) {
        return;
    }

    for (size_t i = 0; i < state->history_count; i++) {
        AhcDailyAttuneSnapshot *existing = &state->history[i];
        if (existing->account == snapshot->account && strcmp(existing->date, snapshot->date) == 0) {
            *existing = *snapshot;
            sort_history(state);
            save_history(state);
            return;
        }
    }

    if (state->history_count >= AHC_HISTORY_CAPACITY) {
        set_status(state, "Attune history is full.");
        return;
    }

    state->history[state->history_count++] = *snapshot;
    sort_history(state);
    save_history(state);
}

static void seed_test_history(CompanionState *state)
{
    const AhcDailyAttuneSnapshot samples[] = {
        { true, "2026-04-20", 10126, 100, 20, 400 },
        { true, "2026-04-21", 10126, 140, 30, 520 },
        { true, "2026-04-22", 10126, 140, 30, 520 },
        { true, "2026-04-23", 10126, 260, 65, 900 },
        { true, "2026-04-24", 10126, 265, 65, 900 },
        { true, "2026-04-25", 10126, 20, 5, 80 },
        { true, "2026-04-26", 10126, 20, 5, 80 },
        { true, "2026-04-27", 10126, 90, 15, 230 },
        { true, "2026-04-28", 10126, 150, 40, 500 }
    };

    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
        upsert_history_snapshot(state, &samples[i]);
    }

    state->snapshot = samples[(sizeof(samples) / sizeof(samples[0])) - 1];
    set_status(state, "Loaded test history with daily spikes and no-spike days.");
}

static bool validate_synastria_path(const char *path)
{
    if (!path[0] || !DirectoryExists(path)) {
        return false;
    }
    char wowext_path[AHC_PATH_CAPACITY];
    path_join(wowext_path, sizeof(wowext_path), path, "WoWExt.exe");

    return FileExists(wowext_path);
}


static bool should_skip_directory(const char *path)
{
    const char *name = GetFileName(path);
    return AHC_STRICMP(name, ".") == 0
        || AHC_STRICMP(name, "..") == 0
        || AHC_STRICMP(name, ".git") == 0
        || AHC_STRICMP(name, "AppData") == 0
        || AHC_STRICMP(name, "Windows") == 0
        || AHC_STRICMP(name, "System Volume Information") == 0
        || AHC_STRICMP(name, "$Recycle.Bin") == 0
        || AHC_STRICMP(name, "build") == 0
        || AHC_STRICMP(name, "build-tests") == 0
        || AHC_STRICMP(name, "_deps") == 0;
}

static bool find_wowext_recursive(const char *root, int depth, int *visited_directories, char *out, size_t out_capacity)
{
    if (!root[0] || !DirectoryExists(root) || depth < 0 || *visited_directories >= AHC_SCAN_MAX_DIRECTORIES) {
        return false;
    }

    (*visited_directories)++;

    char direct_wowext[AHC_PATH_CAPACITY];
    path_join(direct_wowext, sizeof(direct_wowext), root, "WoWExt.exe");
    if (FileExists(direct_wowext)) {
        snprintf(out, out_capacity, "%s", root);
        return true;
    }

    if (depth == 0) {
        return false;
    }

    FilePathList entries = LoadDirectoryFiles(root);
    bool found = false;

    for (unsigned int i = 0; i < entries.count && !found; i++) {
        const char *entry = entries.paths[i];
        if (!DirectoryExists(entry) || should_skip_directory(entry)) {
            continue;
        }

        found = find_wowext_recursive(entry, depth - 1, visited_directories, out, out_capacity);
    }

    UnloadDirectoryFiles(entries);
    return found;
}

static bool try_scan_candidate(const char *candidate, int depth, char *out, size_t out_capacity)
{
    if (!candidate || !candidate[0] || !DirectoryExists(candidate)) {
        return false;
    }

    int visited_directories = 0;
    return find_wowext_recursive(candidate, depth, &visited_directories, out, out_capacity);
}

static bool try_auto_detect_synastria(CompanionState *state, bool persist)
{
    char detected_path[AHC_PATH_CAPACITY] = { 0 };
    const char *working_directory = GetWorkingDirectory();

    if (try_scan_candidate(working_directory, 4, detected_path, sizeof(detected_path))) {
        snprintf(state->synastria_path, sizeof(state->synastria_path), "%s", detected_path);
    }

#if defined(_WIN32)
    const char *user_profile = getenv("USERPROFILE");
    const char *program_files = getenv("ProgramFiles");
    const char *program_files_x86 = getenv("ProgramFiles(x86)");

    char candidate[AHC_PATH_CAPACITY];
    const char *fixed_candidates[] = {
        "C:/Synastria",
        "C:/Games",
        "D:/Synastria",
        "D:/Games",
        "E:/Synastria",
        "E:/Games",
        NULL
    };

    for (int i = 0; !state->synastria_path[0] && fixed_candidates[i]; i++) {
        if (try_scan_candidate(fixed_candidates[i], 6, detected_path, sizeof(detected_path))) {
            snprintf(state->synastria_path, sizeof(state->synastria_path), "%s", detected_path);
        }
    }

    if (!state->synastria_path[0] && user_profile && user_profile[0]) {
        const char *user_subdirs[] = { "Downloads", "Desktop", "Documents", "Games", NULL };
        for (int i = 0; !state->synastria_path[0] && user_subdirs[i]; i++) {
            path_join(candidate, sizeof(candidate), user_profile, user_subdirs[i]);
            if (try_scan_candidate(candidate, 5, detected_path, sizeof(detected_path))) {
                snprintf(state->synastria_path, sizeof(state->synastria_path), "%s", detected_path);
            }
        }
    }

    if (!state->synastria_path[0] && program_files && program_files[0] && try_scan_candidate(program_files, 4, detected_path, sizeof(detected_path))) {
        snprintf(state->synastria_path, sizeof(state->synastria_path), "%s", detected_path);
    }

    if (!state->synastria_path[0] && program_files_x86 && program_files_x86[0] && try_scan_candidate(program_files_x86, 4, detected_path, sizeof(detected_path))) {
        snprintf(state->synastria_path, sizeof(state->synastria_path), "%s", detected_path);
    }
#else
    const char *home = getenv("HOME");
    char candidate[AHC_PATH_CAPACITY];

    if (!state->synastria_path[0] && home && home[0]) {
        const char *home_subdirs[] = { "Games", "Downloads", "Desktop", "Documents", NULL };
        for (int i = 0; !state->synastria_path[0] && home_subdirs[i]; i++) {
            path_join(candidate, sizeof(candidate), home, home_subdirs[i]);
            if (try_scan_candidate(candidate, 5, detected_path, sizeof(detected_path))) {
                snprintf(state->synastria_path, sizeof(state->synastria_path), "%s", detected_path);
            }
        }
    }

    if (!state->synastria_path[0] && try_scan_candidate("/opt", 5, detected_path, sizeof(detected_path))) {
        snprintf(state->synastria_path, sizeof(state->synastria_path), "%s", detected_path);
    }
#endif

    if (!state->synastria_path[0]) {
        set_status(state, "Auto-detect did not find WoWExt.exe. Paste the Synastria folder path.");
        return false;
    }

    if (persist) {
        save_settings(state);
    }

    char message[AHC_STATUS_CAPACITY];
    snprintf(message, sizeof(message), "Detected Synastria at %s.", state->synastria_path);
    set_status(state, message);
    return true;
}

static bool scan_attunehelper_snapshot(CompanionState *state)
{
    if (!validate_synastria_path(state->synastria_path)) {
        set_status(state, "Set a valid Synastria folder first.");
        return false;
    }

    char account_root[AHC_PATH_CAPACITY];
    path_join(account_root, sizeof(account_root), state->synastria_path, "WTF/Account");

    if (!DirectoryExists(account_root)) {
        set_status(state, "No WTF/Account folder found.");
        return false;
    }

    FilePathList accounts = LoadDirectoryFiles(account_root);
    bool loaded = false;

    for (unsigned int i = 0; i < accounts.count && !loaded; i++) {
        const char *account_path = accounts.paths[i];
        if (!DirectoryExists(account_path)) {
            continue;
        }

        char saved_variables_path[AHC_PATH_CAPACITY];
        char snapshot_path[AHC_PATH_CAPACITY];
        path_join(saved_variables_path, sizeof(saved_variables_path), account_path, "SavedVariables");
        path_join(snapshot_path, sizeof(snapshot_path), saved_variables_path, "AttuneHelper.lua");

        if (FileExists(snapshot_path) && ahc_parse_daily_attune_snapshot_file(snapshot_path, &state->snapshot)) {
            snprintf(state->snapshot_path, sizeof(state->snapshot_path), "%s", snapshot_path);
            loaded = true;
        }
    }

    UnloadDirectoryFiles(accounts);

    if (loaded) {
        upsert_history_snapshot(state, &state->snapshot);
        char message[AHC_STATUS_CAPACITY];
        snprintf(message, sizeof(message), "Loaded AttuneHelper snapshot for %s.", state->snapshot.date);
        set_status(state, message);
        return true;
    }

    set_status(state, "AttuneHelper.lua was not found under WTF/Account.");
    return false;
}

static void draw_card(Rectangle bounds, const char *title)
{
    DrawRectangleRounded((Rectangle){ bounds.x + 5.0f, bounds.y + 7.0f, bounds.width, bounds.height }, 0.05f, 10, (Color){ 0, 0, 0, 135 });
    DrawRectangleRounded(bounds, 0.035f, 8, AHC_PANEL);
    DrawRectangleRounded((Rectangle){ bounds.x + 2.0f, bounds.y + 2.0f, bounds.width - 4.0f, 42.0f }, 0.035f, 8, (Color){ 50, 37, 23, 255 });
    DrawRectangle((int)bounds.x + 12, (int)bounds.y + 43, (int)bounds.width - 24, 1, AHC_BORDER_BRIGHT);
    DrawRectangleRoundedLines(bounds, 0.035f, 8, AHC_BORDER);
    DrawRectangleRoundedLines((Rectangle){ bounds.x + 3.0f, bounds.y + 3.0f, bounds.width - 6.0f, bounds.height - 6.0f }, 0.03f, 8, (Color){ 82, 56, 25, 180 });
    draw_text(title, (int)bounds.x + 18, (int)bounds.y + 13, 20, AHC_LF);
}

static bool draw_tab_button(const char *label, Rectangle bounds, bool active)
{
    return draw_wow_button(label, bounds, active, 18);
}

static void draw_header(const CompanionState *state)
{
    DrawRectangleGradientH(0, 0, GetScreenWidth(), 126, (Color){ 72, 45, 20, 255 }, (Color){ 12, 16, 24, 255 });
    DrawCircle(GetScreenWidth() - 120, 34, 90, (Color){ 214, 169, 80, 32 });
    DrawCircle(GetScreenWidth() - 42, 82, 54, (Color){ 89, 196, 255, 30 });
    DrawRectangle(0, 124, GetScreenWidth(), 2, AHC_BORDER_BRIGHT);

    draw_text("Attune Helper Companion", 26, 20, 42, AHC_TEXT);
    draw_text("Synastria addon installs, updates, and daily attune history.", 28, 72, 20, (Color){ 228, 204, 153, 255 });

    if (state->synastria_path[0]) {
        char path_line[AHC_PATH_CAPACITY + 32];
        snprintf(path_line, sizeof(path_line), "Synastria: %s", state->synastria_path);
        draw_text(path_line, 28, 100, 16, validate_synastria_path(state->synastria_path) ? (Color){ 180, 230, 205, 255 } : GOLD);
    } else {
        draw_text("Setup needed: open Settings and paste your Synastria folder path.", 28, 100, 16, GOLD);
    }
}

static void draw_tabs(CompanionState *state)
{
    const float y = 142.0f;
    if (draw_tab_button("Attunes", (Rectangle){ (float)content_left(), y, 164.0f, 44.0f }, state->tab == COMPANION_TAB_ATTUNES)) {
        state->tab = COMPANION_TAB_ATTUNES;
    }
    if (draw_tab_button("Addons", (Rectangle){ (float)content_left() + 176.0f, y, 164.0f, 44.0f }, state->tab == COMPANION_TAB_ADDONS)) {
        state->tab = COMPANION_TAB_ADDONS;
    }
    if (draw_tab_button("Settings", (Rectangle){ (float)content_left() + 352.0f, y, 164.0f, 44.0f }, state->tab == COMPANION_TAB_SETTINGS)) {
        state->tab = COMPANION_TAB_SETTINGS;
    }
}

static void draw_snapshot_card(const CompanionState *state, Rectangle bounds)
{
    char line[160];
    draw_card(bounds, "Latest snapshot");

    if (!state->snapshot.found) {
        draw_text("No daily snapshot loaded.", (int)bounds.x + 20, (int)bounds.y + 56, 24, AHC_TEXT);
        draw_text("Set your Synastria folder once, then scan.", (int)bounds.x + 20, (int)bounds.y + 92, 18, AHC_MUTED);
        draw_text("Expected file:", (int)bounds.x + 20, (int)bounds.y + 140, 16, AHC_MUTED);
        draw_text("WTF/Account/*/SavedVariables/AttuneHelper.lua", (int)bounds.x + 20, (int)bounds.y + 164, 16, AHC_ACCENT);
        return;
    }

    snprintf(line, sizeof(line), "%s", state->snapshot.date[0] ? state->snapshot.date : "Unknown date");
    draw_text(line, (int)bounds.x + 20, (int)bounds.y + 58, 32, AHC_TEXT);

    snprintf(line, sizeof(line), "Account %d", state->snapshot.account);
    draw_text(line, (int)bounds.x + 20, (int)bounds.y + 98, 18, AHC_MUTED);

    snprintf(line, sizeof(line), "TF  %d", state->snapshot.titanforged);
    draw_text(line, (int)bounds.x + 20, (int)bounds.y + 146, 24, AHC_TF);

    snprintf(line, sizeof(line), "WF  %d", state->snapshot.warforged);
    draw_text(line, (int)bounds.x + 20, (int)bounds.y + 182, 24, AHC_WF);

    snprintf(line, sizeof(line), "LF  %d", state->snapshot.lightforged);
    draw_text(line, (int)bounds.x + 20, (int)bounds.y + 218, 24, AHC_LF);
}

static int snapshot_metric_value(const AhcDailyAttuneSnapshot *snapshot, GraphMetric metric)
{
    switch (metric) {
        case GRAPH_METRIC_WARFORGED:
            return snapshot->warforged;
        case GRAPH_METRIC_LIGHTFORGED:
            return snapshot->lightforged;
        case GRAPH_METRIC_TITANFORGED:
            return snapshot->titanforged;
        case GRAPH_METRIC_TOTAL:
        default:
            return snapshot->warforged + snapshot->lightforged + snapshot->titanforged;
    }
}

static const char *graph_metric_label(GraphMetric metric)
{
    switch (metric) {
        case GRAPH_METRIC_WARFORGED:
            return "WF / day";
        case GRAPH_METRIC_LIGHTFORGED:
            return "LF / day";
        case GRAPH_METRIC_TITANFORGED:
            return "TF / day";
        case GRAPH_METRIC_TOTAL:
        default:
            return "Account attunes / day";
    }
}

static Color graph_metric_color(GraphMetric metric)
{
    switch (metric) {
        case GRAPH_METRIC_WARFORGED:
            return AHC_WF;
        case GRAPH_METRIC_LIGHTFORGED:
            return AHC_LF;
        case GRAPH_METRIC_TITANFORGED:
            return AHC_TF;
        case GRAPH_METRIC_TOTAL:
        default:
            return AHC_ACCENT;
    }
}

static int daily_metric_value(const CompanionState *state, size_t index, GraphMetric metric)
{
    if (index == 0 || index >= state->history_count) {
        return 0;
    }

    const AhcDailyAttuneSnapshot *previous_snapshot = &state->history[index - 1];
    const AhcDailyAttuneSnapshot *current_snapshot = &state->history[index];
    if (previous_snapshot->account != current_snapshot->account) {
        return 0;
    }

    int previous = snapshot_metric_value(previous_snapshot, metric);
    int current = snapshot_metric_value(current_snapshot, metric);
    return current < previous ? current : current - previous;
}

static bool draw_metric_chip(const char *label, Rectangle bounds, bool active)
{
    return draw_wow_button(label, bounds, active, 15);
}

static void draw_graph_card(CompanionState *state, Rectangle bounds)
{
    draw_card(bounds, "Daily attune history");

    const float chip_gap = 8.0f;
    const float total_chip_width = 70.0f + 52.0f + 52.0f + 52.0f + chip_gap * 3.0f;
    float chip_x = bounds.x + bounds.width - total_chip_width - 18.0f;
    float chip_y = bounds.y + 8.0f;
    if (chip_x < bounds.x + 250.0f) {
        chip_x = bounds.x + 18.0f;
        chip_y = bounds.y + 52.0f;
    }

    if (draw_metric_chip("Total", (Rectangle){ chip_x, chip_y, 70.0f, 32.0f }, state->graph_metric == GRAPH_METRIC_TOTAL)) {
        state->graph_metric = GRAPH_METRIC_TOTAL;
    }
    chip_x += 70.0f + chip_gap;
    if (draw_metric_chip("TF", (Rectangle){ chip_x, chip_y, 52.0f, 32.0f }, state->graph_metric == GRAPH_METRIC_TITANFORGED)) {
        state->graph_metric = GRAPH_METRIC_TITANFORGED;
    }
    chip_x += 52.0f + chip_gap;
    if (draw_metric_chip("WF", (Rectangle){ chip_x, chip_y, 52.0f, 32.0f }, state->graph_metric == GRAPH_METRIC_WARFORGED)) {
        state->graph_metric = GRAPH_METRIC_WARFORGED;
    }
    chip_x += 52.0f + chip_gap;
    if (draw_metric_chip("LF", (Rectangle){ chip_x, chip_y, 52.0f, 32.0f }, state->graph_metric == GRAPH_METRIC_LIGHTFORGED)) {
        state->graph_metric = GRAPH_METRIC_LIGHTFORGED;
    }

    float display_y = chip_y + 40.0f;
    float display_x = bounds.x + bounds.width - 150.0f;
    if (draw_metric_chip("Bars", (Rectangle){ display_x, display_y, 66.0f, 28.0f }, state->graph_display == GRAPH_DISPLAY_BARS)) {
        state->graph_display = GRAPH_DISPLAY_BARS;
    }
    if (draw_metric_chip("Plot", (Rectangle){ display_x + 76.0f, display_y, 66.0f, 28.0f }, state->graph_display == GRAPH_DISPLAY_PLOT)) {
        state->graph_display = GRAPH_DISPLAY_PLOT;
    }

    int plot_left = (int)bounds.x + 62;
    int plot_right = (int)(bounds.x + bounds.width) - 34;
    int plot_top = (int)bounds.y + (chip_y > bounds.y + 20.0f ? 132 : 96);
    int base_y = (int)(bounds.y + bounds.height) - 52;
    if (base_y < plot_top + 90) {
        base_y = plot_top + 90;
    }
    int plot_height = base_y - plot_top;

    DrawLine(plot_left, plot_top, plot_left, base_y, AHC_BORDER);
    DrawLine(plot_left, base_y, plot_right, base_y, AHC_BORDER);

    if (state->history_count == 0) {
        draw_text("Scan AttuneHelper to save daily points here.", plot_left + 12, plot_top + 42, 18, AHC_MUTED);
        draw_text("Hover a spike to see TF / WF / LF for that date.", plot_left + 12, plot_top + 72, 16, AHC_MUTED);
        return;
    }

    int max_delta = 1;
    for (size_t i = 0; i < state->history_count; i++) {
        int delta = daily_metric_value(state, i, state->graph_metric);
        if (delta > max_delta) {
            max_delta = delta;
        }
    }

    const Color metric_color = graph_metric_color(state->graph_metric);
    int hovered_index = -1;
    Vector2 mouse = GetMousePosition();
    char axis_label[128];
    snprintf(axis_label, sizeof(axis_label), "Daily gain scale 0-%d  |  %s to %s",
        max_delta,
        state->history[0].date,
        state->history[state->history_count - 1].date);
    draw_text(axis_label, plot_left, plot_top - 24, 14, AHC_MUTED);

    for (int grid = 1; grid <= 3; grid++) {
        int y = base_y - (plot_height * grid) / 4;
        DrawLine(plot_left + 1, y, plot_right, y, (Color){ 132, 98, 45, 70 });
    }

    float slot_width = (float)(plot_right - plot_left) / (float)state->history_count;
    float bar_width = slot_width * 0.46f;
    if (bar_width < 6.0f) {
        bar_width = 6.0f;
    }
    if (bar_width > 30.0f) {
        bar_width = 30.0f;
    }

    int label_step = (int)(state->history_count / 6) + 1;
    Vector2 previous_point = { 0.0f, 0.0f };
    bool has_previous_point = false;
    for (size_t i = 0; i < state->history_count; i++) {
        float x = (float)plot_left + slot_width * (float)i + slot_width * 0.5f;
        int delta = daily_metric_value(state, i, state->graph_metric);
        float bar_height = delta > 0 ? ((float)delta / (float)max_delta) * (float)(plot_height - 12) : 3.0f;
        float point_y = (float)base_y - ((float)delta / (float)max_delta) * (float)(plot_height - 12);
        Vector2 point = { x, point_y };
        Rectangle hit = { x - slot_width * 0.5f, (float)plot_top, slot_width, (float)plot_height + 10.0f };
        Rectangle bar = { x - bar_width * 0.5f, (float)base_y - bar_height, bar_width, bar_height };
        bool hovered = CheckCollisionPointRec(mouse, hit);

        if (hovered) {
            hovered_index = (int)i;
            DrawRectangleRounded((Rectangle){ hit.x + 2.0f, hit.y, hit.width - 4.0f, hit.height }, 0.12f, 6, (Color){ 89, 196, 255, 34 });
        }
        if (state->graph_display == GRAPH_DISPLAY_PLOT) {
            if (has_previous_point) {
                DrawLineEx(previous_point, point, 2.0f, metric_color);
            }
            DrawCircleV(point, hovered ? 6.5f : 5.0f, metric_color);
            DrawCircleLines((int)point.x, (int)point.y, hovered ? 9.0f : 7.0f, hovered ? RAYWHITE : AHC_BORDER_BRIGHT);
            previous_point = point;
            has_previous_point = true;
        } else {
            if (delta > 0) {
                DrawRectangleRounded(bar, 0.35f, 8, metric_color);
                DrawRectangleRoundedLines(bar, 0.35f, 8, hovered ? RAYWHITE : AHC_BORDER_BRIGHT);
            } else {
                DrawRectangleRounded((Rectangle){ x - 5.0f, (float)base_y - 3.0f, 10.0f, 5.0f }, 0.45f, 6, hovered ? RAYWHITE : AHC_MUTED);
            }
        }
        if ((int)i % label_step == 0 || i + 1 == state->history_count) {
            draw_text(state->history[i].date + 5, (int)(x - 16.0f), base_y + 12, 12, AHC_MUTED);
        }
    }

    draw_text(graph_metric_label(state->graph_metric), plot_left, (int)(bounds.y + bounds.height) - 30, 16, AHC_MUTED);

    if (hovered_index >= 0) {
        const AhcDailyAttuneSnapshot *hovered = &state->history[hovered_index];
        char line[128];
        int daily_value = daily_metric_value(state, (size_t)hovered_index, state->graph_metric);
        int width = 252;
        int x = (int)mouse.x + 14;
        int y = (int)mouse.y - 184;
        if (x + width > GetScreenWidth() - 16) {
            x = GetScreenWidth() - width - 16;
        }
        if (y < 16) {
            y = 16;
        }

        DrawRectangleRounded((Rectangle){ (float)x + 4.0f, (float)y + 5.0f, (float)width, 170.0f }, 0.10f, 8, (Color){ 0, 0, 0, 130 });
        DrawRectangleRounded((Rectangle){ (float)x, (float)y, (float)width, 170.0f }, 0.10f, 8, (Color){ 24, 18, 13, 248 });
        DrawRectangleRoundedLines((Rectangle){ (float)x, (float)y, (float)width, 170.0f }, 0.10f, 8, AHC_BORDER_BRIGHT);
        snprintf(line, sizeof(line), "%s  |  Account %d", hovered->date, hovered->account);
        draw_text(line, x + 14, y + 12, 17, AHC_TEXT);
        snprintf(line, sizeof(line), "Daily %s: %d", graph_metric_label(state->graph_metric), daily_value);
        draw_text(line, x + 14, y + 42, 16, metric_color);
        snprintf(line, sizeof(line), "Cumulative total: %d", hovered->titanforged + hovered->warforged + hovered->lightforged);
        draw_text(line, x + 14, y + 68, 16, AHC_MUTED);
        snprintf(line, sizeof(line), "TF: %d", hovered->titanforged);
        draw_text(line, x + 14, y + 96, 17, AHC_TF);
        snprintf(line, sizeof(line), "WF: %d", hovered->warforged);
        draw_text(line, x + 14, y + 122, 17, AHC_WF);
        snprintf(line, sizeof(line), "LF: %d", hovered->lightforged);
        draw_text(line, x + 14, y + 148, 17, AHC_LF);
    }
}

static void draw_attunes_tab(CompanionState *state)
{
    Rectangle content = content_rect();
    float button_height = 42.0f;
    float button_gap = 20.0f;
    float card_height = content.height - button_height - button_gap;
    if (card_height < 300.0f) {
        card_height = 300.0f;
    }
    float gap = 22.0f;
    float snapshot_width = 360.0f;
    if (content.width < 860.0f) {
        snapshot_width = 320.0f;
    }
    float graph_width = content.width - snapshot_width - gap;
    if (graph_width < 420.0f) {
        graph_width = 420.0f;
    }

    Rectangle snapshot_bounds = { content.x, content.y, snapshot_width, card_height };
    Rectangle graph_bounds = { content.x + snapshot_width + gap, content.y, graph_width, card_height };
    draw_snapshot_card(state, snapshot_bounds);
    draw_graph_card(state, graph_bounds);

    float button_y = content.y + card_height + button_gap;
    if (draw_wow_button("Scan now", (Rectangle){ content.x, button_y, 166.0f, button_height }, false, 18)) {
        scan_attunehelper_snapshot(state);
    }
    if (draw_wow_button("Seed test data", (Rectangle){ content.x + 180.0f, button_y, 180.0f, button_height }, false, 18)) {
        seed_test_history(state);
    }
}

static bool addon_matches_selected_category(const CompanionState *state, const AhcAddon *addon)
{
    return !state->selected_addon_category[0] || strcmp(addon->category, state->selected_addon_category) == 0;
}

static size_t addon_filtered_count(const CompanionState *state, const AhcAddon *addons, size_t count)
{
    size_t filtered = 0;
    for (size_t i = 0; i < count; i++) {
        if (addon_matches_selected_category(state, &addons[i])) {
            filtered++;
        }
    }
    return filtered;
}

static bool category_seen(const char *const *categories, size_t count, const char *category)
{
    for (size_t i = 0; i < count; i++) {
        if (strcmp(categories[i], category) == 0) {
            return true;
        }
    }
    return false;
}

static int draw_category_filters(CompanionState *state, const AhcAddon *addons, size_t count, float x, float y, float right)
{
    const char *categories[AHC_CATEGORY_CAPACITY];
    size_t category_count = 0;
    for (size_t i = 0; i < count && category_count < AHC_CATEGORY_CAPACITY; i++) {
        if (addons[i].category && addons[i].category[0] && !category_seen(categories, category_count, addons[i].category)) {
            categories[category_count++] = addons[i].category;
        }
    }

    float cursor_x = x;
    float cursor_y = y;
    int row = 0;

    const char *labels[AHC_CATEGORY_CAPACITY + 1];
    labels[0] = "All";
    for (size_t i = 0; i < category_count; i++) {
        labels[i + 1] = categories[i];
    }

    for (size_t i = 0; i < category_count + 1; i++) {
        const char *label = labels[i];
        float width = (float)measure_text_width(label, 14) + 26.0f;
        if (width < 58.0f) {
            width = 58.0f;
        }
        if (width > 146.0f) {
            width = 146.0f;
        }
        if (cursor_x + width > right && cursor_x > x) {
            cursor_x = x;
            cursor_y += 34.0f;
            row++;
        }

        bool active = (i == 0 && !state->selected_addon_category[0])
            || (i > 0 && strcmp(state->selected_addon_category, label) == 0);
        if (draw_metric_chip(label, (Rectangle){ cursor_x, cursor_y, width, 28.0f }, active)) {
            if (i == 0) {
                state->selected_addon_category[0] = '\0';
            } else {
                snprintf(state->selected_addon_category, sizeof(state->selected_addon_category), "%s", label);
            }
            state->addon_scroll = 0;
        }

        cursor_x += width + 8.0f;
    }

    return row + 1;
}

static void draw_addon_card(CompanionState *state, const AhcAddon *addon, Rectangle bounds)
{
    bool installed = addon_is_installed(state, addon);
    bool git_checkout = addon_is_git_checkout(state, addon);
    bool managed = addon_is_managed(state, addon);
    bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    DrawRectangleRounded(bounds, 0.035f, 8, hovered ? AHC_PANEL_HOVER : AHC_PANEL);
    DrawRectangleRoundedLines(bounds, 0.035f, 8, hovered ? AHC_BORDER_BRIGHT : AHC_BORDER);
    draw_text(addon->name, (int)bounds.x + 16, (int)bounds.y + 10, 21, AHC_TEXT);

    char meta[256];
    snprintf(meta, sizeof(meta), "%s  |  By %s  |  Folder: %s", addon->category, addon->author, addon->folder);
    draw_text(meta, (int)bounds.x + 16, (int)bounds.y + 39, 15, AHC_MUTED);

    draw_text(addon->description, (int)bounds.x + 16, (int)bounds.y + 66, 17, AHC_TEXT);
    draw_text(installed ? (managed ? "Managed" : "Manual") : "Not installed", (int)bounds.x + (int)bounds.width - 230, (int)bounds.y + 16, 15, installed ? GREEN : GOLD);

    const char *primary_label = installed ? (managed || git_checkout ? "Update" : "Replace") : "Install";
    float primary_x = installed ? bounds.x + bounds.width - 184.0f : bounds.x + bounds.width - 88.0f;
    if (draw_wow_button(primary_label, (Rectangle){ primary_x, bounds.y + 54.0f, 74.0f, 30.0f }, false, 15)) {
        install_or_update_addon(state, addon);
    }
    if (installed && draw_wow_button("Uninstall", (Rectangle){ bounds.x + bounds.width - 96.0f, bounds.y + 54.0f, 82.0f, 30.0f }, false, 15)) {
        uninstall_addon(state, addon);
    }
}

static void draw_addons_tab(CompanionState *state)
{
    const AhcAddon *addons = state->addons ? state->addons : ahc_addon_catalog_items();
    const size_t count = state->addons ? state->addon_count : ahc_addon_catalog_count();
    const size_t filtered_count = addon_filtered_count(state, addons, count);

    char title[128];
    if (state->selected_addon_category[0]) {
        snprintf(title, sizeof(title), "Addon catalog (%zu/%zu) - %s", filtered_count, count, state->selected_addon_category);
    } else {
        snprintf(title, sizeof(title), "Addon catalog (%zu)", count);
    }

    Rectangle content = content_rect();
    draw_card(content, title);

    char source_line[AHC_PATH_CAPACITY + 96];
    snprintf(source_line, sizeof(source_line), "%s catalog: %s", state->addon_manifest_loaded ? "Local manifest" : "Fallback", state->addon_catalog_source);
    draw_text(source_line, (int)content.x + 20, (int)content.y + 42, 15, AHC_MUTED);
    draw_text("Direct GitHub mode: installs clone repos; manual addons are backed up before replacement.", (int)content.x + 20, (int)content.y + 64, 15, AHC_MUTED);


    int category_rows = draw_category_filters(state, addons, count, content.x + 20.0f, content.y + 94.0f, content.x + content.width - 20.0f);
    float list_y = content.y + 94.0f + (float)(category_rows * 34) + 8.0f;
    float list_height = content.y + content.height - list_y - 38.0f;
    if (list_height < 88.0f) {
        list_height = 88.0f;
    }

    int wheel = (int)GetMouseWheelMove();
    Rectangle list_bounds = { content.x + 16.0f, list_y, content.width - 32.0f, list_height };
    if (wheel != 0 && CheckCollisionPointRec(GetMousePosition(), list_bounds)) {
        state->addon_scroll -= wheel;
    }
    int column_count = content.width >= 1100.0f ? 2 : 1;
    const float column_gap = 12.0f;
    const float row_stride = 108.0f;
    const float addon_card_height = 96.0f;
    float column_width = (list_bounds.width - (float)(column_count - 1) * column_gap) / (float)column_count;
    int visible_rows = (int)(list_height / row_stride);
    if (visible_rows < 1) {
        visible_rows = 1;
    }
    size_t visible_slots = (size_t)(visible_rows * column_count);
    int total_rows = filtered_count == 0 ? 0 : (int)((filtered_count + (size_t)column_count - 1) / (size_t)column_count);
    const int max_scroll = total_rows > visible_rows ? total_rows - visible_rows : 0;
    if (state->addon_scroll < 0) {
        state->addon_scroll = 0;
    }
    if (state->addon_scroll > max_scroll) {
        state->addon_scroll = max_scroll;
    }
    if (filtered_count == 0) {
        draw_text("No addons found in this category.", (int)list_bounds.x + 4, (int)list_y + 20, 18, AHC_MUTED);
    } else {
        int skipped = 0;
        int slot = 0;
        int skip_slots = state->addon_scroll * column_count;
        BeginScissorMode((int)list_bounds.x, (int)list_bounds.y, (int)list_bounds.width, (int)list_bounds.height);
        for (size_t i = 0; i < count && (size_t)slot < visible_slots; i++) {
            if (!addon_matches_selected_category(state, &addons[i])) {
                continue;
            }
            if (skipped < skip_slots) {
                skipped++;
                continue;
            }
            int row = slot / column_count;
            int column = slot % column_count;
            float x = list_bounds.x + 4.0f + (float)column * (column_width + column_gap);
            float y = list_y + 4.0f + (float)row * row_stride;
            draw_addon_card(state, &addons[i], (Rectangle){ x, y, column_width - 8.0f, addon_card_height });
            slot++;
        }
        EndScissorMode();
    }
    draw_text("For larger catalogs, release ZIP support will avoid GitHub clone friction.", (int)content.x + 20, (int)(content.y + content.height - 28.0f), 15, AHC_MUTED);
}

static void draw_settings_tab(CompanionState *state)
{
    Rectangle content = content_rect();
    Rectangle card = { content.x, content.y, content.width, 322.0f };
    draw_card(card, "Setup");

    draw_text("Synastria folder", (int)card.x + 20, (int)card.y + 50, 22, AHC_TEXT);
    draw_text("Paste the folder that contains WoWExt.exe, or let the app detect it.", (int)card.x + 20, (int)card.y + 82, 17, AHC_MUTED);

    bool paste_requested = state->synastria_path_editing
        && (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER))
        && IsKeyPressed(KEY_V);

    if (paste_requested) {
        const char *clipboard = GetClipboardText();
        if (clipboard && clipboard[0]) {
            snprintf(state->synastria_path, sizeof(state->synastria_path), "%s", clipboard);
            trim_line(state->synastria_path);
            set_status(state, "Pasted Synastria path from clipboard.");
        }
    }

    Rectangle textbox = { card.x + 20.0f, card.y + 122.0f, card.width - 146.0f, 40.0f };
    if (textbox.width < 320.0f) {
        textbox.width = 320.0f;
    }
    if (GuiTextBox(textbox, state->synastria_path, (int)sizeof(state->synastria_path), state->synastria_path_editing)) {
        state->synastria_path_editing = !state->synastria_path_editing;
    }

    if (draw_wow_button("Save", (Rectangle){ textbox.x + textbox.width + 18.0f, textbox.y, 108.0f, 40.0f }, false, 18)) {
        if (validate_synastria_path(state->synastria_path)) {
            save_settings(state);
            scan_attunehelper_snapshot(state);
        } else {
            set_status(state, "That folder does not look like a Synastria install.");
        }
    }

    if (draw_wow_button("Scan snapshots", (Rectangle){ card.x + 20.0f, card.y + 188.0f, 172.0f, 42.0f }, false, 18)) {
        scan_attunehelper_snapshot(state);
    }
    if (draw_wow_button("Auto-detect", (Rectangle){ card.x + 206.0f, card.y + 188.0f, 172.0f, 42.0f }, false, 18)) {
        if (try_auto_detect_synastria(state, true)) {
            scan_attunehelper_snapshot(state);
        }
    }
    draw_text(validate_synastria_path(state->synastria_path) ? "Destination ready: WoWExt.exe found." : "Destination not set: choose the folder with WoWExt.exe.", (int)card.x + 20, (int)card.y + 264, 17, validate_synastria_path(state->synastria_path) ? GREEN : GOLD);
    draw_text("Daily history is stored locally for graphing. No WTF backup or account sync is used.", (int)card.x + 20, (int)card.y + 292, 15, AHC_MUTED);
}

static void init_state(CompanionState *state)
{
    memset(state, 0, sizeof(*state));
    state->tab = COMPANION_TAB_ATTUNES;
    state->graph_metric = GRAPH_METRIC_TOTAL;
    state->graph_display = GRAPH_DISPLAY_BARS;
    load_addon_catalog(state);
    resolve_config_paths(state);
    load_settings(state);
    load_history(state);
    set_status(state, "Ready.");

    if (!state->synastria_path[0]) {
        try_auto_detect_synastria(state, true);
    }

    if (state->synastria_path[0]) {
        scan_attunehelper_snapshot(state);
    }
}

int main(void)
{
    CompanionState state;
    init_state(&state);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 760, "Attune Helper Companion");
    load_ui_font();
    SetTargetFPS(60);
    GuiSetStyle(DEFAULT, TEXT_SIZE, 18);
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, 0x44331fff);
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, 0x84622dff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, 0xf8e8beff);
    GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, 0x5b421eff);
    GuiSetStyle(BUTTON, BORDER_COLOR_FOCUSED, 0xd6a950ff);
    GuiSetStyle(BUTTON, BASE_COLOR_PRESSED, 0x145b87ff);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(AHC_BACKGROUND);
        DrawRectangleGradientV(0, 126, GetScreenWidth(), GetScreenHeight() - 126, (Color){ 24, 19, 14, 255 }, (Color){ 7, 6, 5, 255 });

        draw_header(&state);
        draw_tabs(&state);

        switch (state.tab) {
            case COMPANION_TAB_ATTUNES:
                draw_attunes_tab(&state);
                break;
            case COMPANION_TAB_ADDONS:
                draw_addons_tab(&state);
                break;
            case COMPANION_TAB_SETTINGS:
                draw_settings_tab(&state);
                break;
        }

        DrawRectangle(0, GetScreenHeight() - 46, GetScreenWidth(), 46, (Color){ 16, 12, 9, 238 });
        DrawRectangle(0, GetScreenHeight() - 46, GetScreenWidth(), 1, AHC_BORDER);
        draw_text(state.status, 26, GetScreenHeight() - 32, 17, AHC_MUTED);

        EndDrawing();
    }

    CloseWindow();
    ahc_addon_manifest_free(&state.addon_manifest);
    if (g_has_ui_font) {
        UnloadFont(g_ui_font);
    }
    return 0;
}
