#include "addons/addon_catalog.h"
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
    bool synastria_path_editing;
    int addon_scroll;
    GraphMetric graph_metric;
} CompanionState;

static const Color AHC_BACKGROUND = { 16, 18, 24, 255 };
static const Color AHC_PANEL = { 30, 36, 50, 255 };
static const Color AHC_PANEL_DARK = { 18, 22, 32, 255 };
static const Color AHC_BORDER = { 91, 111, 135, 255 };
static const Color AHC_TEXT = { 244, 248, 252, 255 };
static const Color AHC_MUTED = { 176, 188, 203, 255 };
static const Color AHC_ACCENT = { 98, 208, 255, 255 };
static const Color AHC_ACCENT_DARK = { 32, 126, 169, 255 };
static const Color AHC_TF = { 128, 128, 255, 255 };
static const Color AHC_WF = { 255, 166, 128, 255 };
static const Color AHC_LF = { 255, 255, 166, 255 };

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

            if (!git_clone_to_path(addon, staging_path)) {
                remove_directory_tree(staging_path);
                set_status(state, "Could not download addon update from GitHub.");
                return;
            }

            if (!move_installed_addon_to_backup(state, addon, path, "manual-backup", backup_path, sizeof(backup_path))) {
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
            snprintf(message, sizeof(message), "Updated manual %s. Backup saved.", addon->name);
            set_status(state, message);
            return;
        }

        snprintf(command, sizeof(command), "git -C \"%s\" pull --ff-only", path);
    } else {
        snprintf(command, sizeof(command), "git clone --depth 1 \"%s\" \"%s\"", addon->repo, path);
    }

    int result = system(command);
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
        { true, "2026-04-20", 10126, 900, 180, 3920 },
        { true, "2026-04-21", 10126, 1034, 205, 4210 },
        { true, "2026-04-22", 10126, 1190, 236, 4635 },
        { true, "2026-04-23", 10126, 1340, 272, 5105 },
        { true, "2026-04-24", 10126, 1410, 301, 5480 },
        { true, "2026-04-25", 10126, 80, 20, 310 },
        { true, "2026-04-26", 10126, 190, 54, 690 },
        { true, "2026-04-27", 10126, 320, 88, 1110 },
        { true, "2026-04-28", 10126, 470, 126, 1580 }
    };

    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
        upsert_history_snapshot(state, &samples[i]);
    }

    state->snapshot = samples[(sizeof(samples) / sizeof(samples[0])) - 1];
    set_status(state, "Loaded test history with a prestige reset.");
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
    DrawRectangleRounded((Rectangle){ bounds.x + 4.0f, bounds.y + 6.0f, bounds.width, bounds.height }, 0.06f, 10, (Color){ 4, 7, 14, 115 });
    DrawRectangleRounded(bounds, 0.04f, 8, AHC_PANEL);
    DrawRectangleRounded((Rectangle){ bounds.x + 1.0f, bounds.y + 1.0f, bounds.width - 2.0f, 42.0f }, 0.04f, 8, (Color){ 38, 48, 66, 255 });
    DrawRectangleRoundedLines(bounds, 0.04f, 8, AHC_BORDER);
    draw_text(title, (int)bounds.x + 18, (int)bounds.y + 13, 20, AHC_ACCENT);
}

static bool draw_tab_button(const char *label, Rectangle bounds, bool active)
{
    Color fill = active ? AHC_ACCENT_DARK : AHC_PANEL_DARK;
    Color border = active ? AHC_ACCENT : AHC_BORDER;
    DrawRectangleRounded(bounds, 0.18f, 8, fill);
    DrawRectangleRoundedLines(bounds, 0.18f, 8, border);

    int text_width = measure_text_width(label, 18);
    draw_text(label, (int)(bounds.x + (bounds.width - (float)text_width) * 0.5f), (int)bounds.y + 10, 18, active ? RAYWHITE : AHC_MUTED);
    return CheckCollisionPointRec(GetMousePosition(), bounds) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void draw_header(const CompanionState *state)
{
    DrawRectangleGradientH(0, 0, GetScreenWidth(), 126, (Color){ 18, 95, 126, 255 }, (Color){ 20, 23, 35, 255 });
    DrawCircle(GetScreenWidth() - 120, 34, 90, (Color){ 98, 208, 255, 26 });
    DrawCircle(GetScreenWidth() - 42, 82, 54, (Color){ 128, 128, 255, 28 });
    DrawRectangle(0, 124, GetScreenWidth(), 2, AHC_ACCENT_DARK);

    draw_text("Attune Helper Companion", 26, 20, 42, RAYWHITE);
    draw_text("Synastria addon installs, updates, and daily attune history.", 28, 72, 20, (Color){ 218, 232, 244, 255 });

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
    if (draw_tab_button("Attunes", (Rectangle){ 26.0f, y, 152.0f, 44.0f }, state->tab == COMPANION_TAB_ATTUNES)) {
        state->tab = COMPANION_TAB_ATTUNES;
    }
    if (draw_tab_button("Addons", (Rectangle){ 190.0f, y, 152.0f, 44.0f }, state->tab == COMPANION_TAB_ADDONS)) {
        state->tab = COMPANION_TAB_ADDONS;
    }
    if (draw_tab_button("Settings", (Rectangle){ 354.0f, y, 152.0f, 44.0f }, state->tab == COMPANION_TAB_SETTINGS)) {
        state->tab = COMPANION_TAB_SETTINGS;
    }
}

static void draw_snapshot_card(const CompanionState *state)
{
    char line[160];
    draw_card((Rectangle){ 26.0f, 204.0f, 388.0f, 260.0f }, "Latest snapshot");

    if (!state->snapshot.found) {
        draw_text("No daily snapshot loaded.", 46, 256, 24, AHC_TEXT);
        draw_text("Set your Synastria folder once, then scan.", 46, 292, 18, AHC_MUTED);
        draw_text("Expected file:", 46, 340, 16, AHC_MUTED);
        draw_text("WTF/Account/*/SavedVariables/AttuneHelper.lua", 46, 364, 16, AHC_ACCENT);
        return;
    }

    snprintf(line, sizeof(line), "%s", state->snapshot.date[0] ? state->snapshot.date : "Unknown date");
    draw_text(line, 46, 252, 32, AHC_TEXT);

    snprintf(line, sizeof(line), "Account %d", state->snapshot.account);
    draw_text(line, 46, 292, 18, AHC_MUTED);

    snprintf(line, sizeof(line), "TF  %d", state->snapshot.titanforged);
    draw_text(line, 46, 340, 24, AHC_TF);

    snprintf(line, sizeof(line), "WF  %d", state->snapshot.warforged);
    draw_text(line, 46, 374, 24, AHC_WF);

    snprintf(line, sizeof(line), "LF  %d", state->snapshot.lightforged);
    draw_text(line, 46, 408, 24, AHC_LF);
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

static size_t active_history_start_index(const CompanionState *state)
{
    if (state->history_count == 0) {
        return 0;
    }

    size_t start = 0;
    int previous = snapshot_metric_value(&state->history[0], state->graph_metric);
    for (size_t i = 1; i < state->history_count; i++) {
        int current = snapshot_metric_value(&state->history[i], state->graph_metric);
        if (current < previous) {
            start = i;
        }
        previous = current;
    }

    return start;
}

static bool draw_metric_chip(const char *label, Rectangle bounds, bool active)
{
    DrawRectangleRounded(bounds, 0.28f, 10, active ? AHC_ACCENT_DARK : AHC_PANEL_DARK);
    DrawRectangleRoundedLines(bounds, 0.28f, 10, active ? AHC_ACCENT : AHC_BORDER);
    int text_width = measure_text_width(label, 15);
    draw_text(label, (int)(bounds.x + (bounds.width - (float)text_width) * 0.5f), (int)bounds.y + 7, 15, active ? RAYWHITE : AHC_MUTED);
    return CheckCollisionPointRec(GetMousePosition(), bounds) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void draw_graph_card(CompanionState *state)
{
    draw_card((Rectangle){ 438.0f, 204.0f, 474.0f, 260.0f }, "Daily attune history");

    if (draw_metric_chip("Total", (Rectangle){ 594.0f, 218.0f, 70.0f, 32.0f }, state->graph_metric == GRAPH_METRIC_TOTAL)) {
        state->graph_metric = GRAPH_METRIC_TOTAL;
    }
    if (draw_metric_chip("TF", (Rectangle){ 672.0f, 218.0f, 52.0f, 32.0f }, state->graph_metric == GRAPH_METRIC_TITANFORGED)) {
        state->graph_metric = GRAPH_METRIC_TITANFORGED;
    }
    if (draw_metric_chip("WF", (Rectangle){ 732.0f, 218.0f, 52.0f, 32.0f }, state->graph_metric == GRAPH_METRIC_WARFORGED)) {
        state->graph_metric = GRAPH_METRIC_WARFORGED;
    }
    if (draw_metric_chip("LF", (Rectangle){ 792.0f, 218.0f, 52.0f, 32.0f }, state->graph_metric == GRAPH_METRIC_LIGHTFORGED)) {
        state->graph_metric = GRAPH_METRIC_LIGHTFORGED;
    }

    const int base_y = 410;
    const int axis_x = 500;
    DrawLine(axis_x, 268, axis_x, base_y, AHC_BORDER);
    DrawLine(axis_x, base_y, 868, base_y, AHC_BORDER);

    if (state->history_count == 0) {
        draw_text("Scan AttuneHelper to save daily points here.", 500, 314, 18, AHC_MUTED);
        draw_text("Hover a dot to see TF / WF / LF for that date.", 500, 344, 16, AHC_MUTED);
        return;
    }
    size_t start_index = active_history_start_index(state);
    size_t point_count = state->history_count - start_index;
    int baseline_value = snapshot_metric_value(&state->history[start_index], state->graph_metric);
    int max_value = baseline_value + 1;
    for (size_t i = start_index; i < state->history_count; i++) {
        int value = snapshot_metric_value(&state->history[i], state->graph_metric);
        if (value > max_value) {
            max_value = value;
        }
    }

    const int plot_left = axis_x;
    const int plot_right = 868;
    const int plot_top = 270;
    const int plot_height = base_y - plot_top;
    const Color metric_color = graph_metric_color(state->graph_metric);
    Vector2 previous = { 0.0f, 0.0f };
    bool has_previous = false;
    int hovered_index = -1;
    Vector2 mouse = GetMousePosition();
    int range = max_value - baseline_value;
    if (range <= 0) {
        range = 1;
    }

    char axis_label[128];
    snprintf(axis_label, sizeof(axis_label), "Baseline %d  |  %s to %s",
        baseline_value,
        state->history[start_index].date,
        state->history[state->history_count - 1].date);
    draw_text(axis_label, plot_left, 252, 14, AHC_MUTED);

    for (size_t i = start_index; i < state->history_count; i++) {
        size_t local_index = i - start_index;
        float x = point_count == 1
            ? (float)plot_left
            : (float)plot_left + ((float)local_index / (float)(point_count - 1)) * (float)(plot_right - plot_left);
        int value = snapshot_metric_value(&state->history[i], state->graph_metric);
        float y = (float)base_y - ((float)(value - baseline_value) / (float)range) * (float)(plot_height - 8);
        Vector2 point = { x, y };

        if (has_previous) {
            DrawLineEx(previous, point, 2.0f, metric_color);
        }

        DrawCircleV(point, 6.0f, metric_color);
        DrawCircleLines((int)point.x, (int)point.y, 8.0f, RAYWHITE);

        if (CheckCollisionPointCircle(mouse, point, 12.0f)) {
            hovered_index = (int)i;
        }

        previous = point;
        has_previous = true;
    }

    draw_text(graph_metric_label(state->graph_metric), plot_left, 428, 16, AHC_MUTED);

    if (hovered_index >= 0) {
        const AhcDailyAttuneSnapshot *hovered = &state->history[hovered_index];
        char line[128];
        int width = 238;
        int x = (int)mouse.x + 14;
        int y = (int)mouse.y - 164;
        if (x + width > GetScreenWidth() - 16) {
            x = GetScreenWidth() - width - 16;
        }
        if (y < 16) {
            y = 16;
        }

        DrawRectangleRounded((Rectangle){ (float)x + 4.0f, (float)y + 5.0f, (float)width, 150.0f }, 0.10f, 8, (Color){ 0, 0, 0, 110 });
        DrawRectangleRounded((Rectangle){ (float)x, (float)y, (float)width, 150.0f }, 0.10f, 8, (Color){ 8, 11, 18, 248 });
        DrawRectangleRoundedLines((Rectangle){ (float)x, (float)y, (float)width, 150.0f }, 0.10f, 8, AHC_ACCENT);
        snprintf(line, sizeof(line), "%s  |  Account %d", hovered->date, hovered->account);
        draw_text(line, x + 14, y + 12, 17, AHC_TEXT);
        snprintf(line, sizeof(line), "Total: %d", hovered->titanforged + hovered->warforged + hovered->lightforged);
        draw_text(line, x + 14, y + 42, 16, AHC_MUTED);
        snprintf(line, sizeof(line), "TF: %d", hovered->titanforged);
        draw_text(line, x + 14, y + 70, 17, AHC_TF);
        snprintf(line, sizeof(line), "WF: %d", hovered->warforged);
        draw_text(line, x + 14, y + 96, 17, AHC_WF);
        snprintf(line, sizeof(line), "LF: %d", hovered->lightforged);
        draw_text(line, x + 14, y + 122, 17, AHC_LF);
    }
}

static void draw_attunes_tab(CompanionState *state)
{
    draw_snapshot_card(state);
    draw_graph_card(state);

    if (GuiButton((Rectangle){ 26.0f, 486.0f, 166.0f, 42.0f }, "Scan now")) {
        scan_attunehelper_snapshot(state);
    }
    if (GuiButton((Rectangle){ 206.0f, 486.0f, 166.0f, 42.0f }, "Seed test data")) {
        seed_test_history(state);
    }
}

static void draw_addon_card(CompanionState *state, const AhcAddon *addon, Rectangle bounds)
{
    bool installed = addon_is_installed(state, addon);
    bool git_checkout = addon_is_git_checkout(state, addon);
    DrawRectangleRounded(bounds, 0.04f, 8, AHC_PANEL);
    DrawRectangleRoundedLines(bounds, 0.04f, 8, AHC_BORDER);
    draw_text(addon->name, (int)bounds.x + 16, (int)bounds.y + 12, 20, AHC_TEXT);

    char meta[256];
    snprintf(meta, sizeof(meta), "%s  |  By %s  |  Folder: %s", addon->category, addon->author, addon->folder);
    draw_text(meta, (int)bounds.x + 16, (int)bounds.y + 40, 14, AHC_MUTED);

    draw_text(addon->description, (int)bounds.x + 16, (int)bounds.y + 64, 15, AHC_MUTED);
    draw_text(installed ? (git_checkout ? "Managed" : "Manual") : "Not installed", (int)bounds.x + (int)bounds.width - 330, (int)bounds.y + 18, 14, installed ? GREEN : GOLD);

    const char *primary_label = installed ? (git_checkout ? "Update" : "Replace") : "Install";
    float primary_x = installed ? bounds.x + bounds.width - 204.0f : bounds.x + bounds.width - 102.0f;
    if (GuiButton((Rectangle){ primary_x, bounds.y + 22.0f, 86.0f, 32.0f }, primary_label)) {
        install_or_update_addon(state, addon);
    }

    if (installed && GuiButton((Rectangle){ bounds.x + bounds.width - 104.0f, bounds.y + 22.0f, 86.0f, 32.0f }, "Uninstall")) {
        uninstall_addon(state, addon);
    }
}

static void draw_addons_tab(CompanionState *state)
{
    const AhcAddon *addons = ahc_addon_catalog_items();
    const size_t count = ahc_addon_catalog_count();

    char title[128];
    snprintf(title, sizeof(title), "Addon catalog (%zu)", count);
    draw_card((Rectangle){ 26.0f, 204.0f, 888.0f, 360.0f }, title);
    draw_text("Direct GitHub mode: install clones repos; manual addons can be replaced with a backed-up managed copy.", 46, 246, 17, AHC_MUTED);

    int wheel = (int)GetMouseWheelMove();
    if (wheel != 0 && CheckCollisionPointRec(GetMousePosition(), (Rectangle){ 42.0f, 282.0f, 852.0f, 250.0f })) {
        state->addon_scroll -= wheel;
    }

    const int max_scroll = count > 2 ? (int)count - 2 : 0;
    if (state->addon_scroll < 0) {
        state->addon_scroll = 0;
    }
    if (state->addon_scroll > max_scroll) {
        state->addon_scroll = max_scroll;
    }

    BeginScissorMode(42, 282, 852, 250);
    for (int row = 0; row < 3; row++) {
        int index = state->addon_scroll + row;
        if (index >= (int)count) {
            break;
        }
        draw_addon_card(state, &addons[index], (Rectangle){ 46.0f, 286.0f + (float)(row * 104), 832.0f, 90.0f });
    }
    EndScissorMode();

    draw_text("For larger catalogs, this should move to release ZIPs or a cached manifest to avoid GitHub friction.", 46, 540, 15, AHC_MUTED);
}

static void draw_settings_tab(CompanionState *state)
{
    draw_card((Rectangle){ 26.0f, 204.0f, 888.0f, 322.0f }, "Setup");

    draw_text("Synastria folder", 46, 254, 22, AHC_TEXT);
    draw_text("Paste the folder that contains WoWExt.exe, or let the app detect it.", 46, 286, 17, AHC_MUTED);

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

    if (GuiTextBox((Rectangle){ 46.0f, 326.0f, 708.0f, 40.0f }, state->synastria_path, (int)sizeof(state->synastria_path), state->synastria_path_editing)) {
        state->synastria_path_editing = !state->synastria_path_editing;
    }

    if (GuiButton((Rectangle){ 772.0f, 326.0f, 108.0f, 40.0f }, "Save")) {
        if (validate_synastria_path(state->synastria_path)) {
            save_settings(state);
            scan_attunehelper_snapshot(state);
        } else {
            set_status(state, "That folder does not look like a Synastria install.");
        }
    }

    if (GuiButton((Rectangle){ 46.0f, 392.0f, 172.0f, 42.0f }, "Scan snapshots")) {
        scan_attunehelper_snapshot(state);
    }
    if (GuiButton((Rectangle){ 232.0f, 392.0f, 172.0f, 42.0f }, "Auto-detect")) {
        if (try_auto_detect_synastria(state, true)) {
            scan_attunehelper_snapshot(state);
        }
    }

    draw_text(validate_synastria_path(state->synastria_path) ? "Destination ready: WoWExt.exe found." : "Destination not set: choose the folder with WoWExt.exe.", 46, 468, 17, validate_synastria_path(state->synastria_path) ? GREEN : GOLD);
    draw_text("Daily history is stored locally for graphing. No WTF backup or account sync is used.", 46, 496, 15, AHC_MUTED);
}

static void init_state(CompanionState *state)
{
    memset(state, 0, sizeof(*state));
    state->tab = COMPANION_TAB_ATTUNES;
    state->graph_metric = GRAPH_METRIC_TOTAL;
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
    InitWindow(980, 680, "Attune Helper Companion");
    load_ui_font();
    SetTargetFPS(60);
    GuiSetStyle(DEFAULT, TEXT_SIZE, 18);
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, 0x2a303cff);
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, 0x53667cff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, 0xe8edf3ff);
    GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, 0x275d74ff);
    GuiSetStyle(BUTTON, BASE_COLOR_PRESSED, 0x1f7591ff);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(AHC_BACKGROUND);
        DrawRectangleGradientV(0, 126, GetScreenWidth(), GetScreenHeight() - 126, (Color){ 15, 19, 29, 255 }, (Color){ 7, 10, 17, 255 });

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

        DrawRectangle(0, GetScreenHeight() - 46, GetScreenWidth(), 46, (Color){ 10, 14, 22, 230 });
        DrawRectangle(0, GetScreenHeight() - 46, GetScreenWidth(), 1, AHC_BORDER);
        draw_text(state.status, 26, GetScreenHeight() - 32, 17, AHC_MUTED);

        EndDrawing();
    }

    CloseWindow();
    if (g_has_ui_font) {
        UnloadFont(g_ui_font);
    }
    return 0;
}
