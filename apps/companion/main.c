#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(__linux__) && !defined(_DEFAULT_SOURCE) && !defined(_GNU_SOURCE)
#define _DEFAULT_SOURCE
#endif

#include "addons/addon_catalog.h"
#include "addons/addon_manifest.h"
#include "attune/attune_snapshot.h"
#include "attune/attune_sync_codec.h"
#include "qrcodegen.h"
#include "ahc/ahc_wow_autologin.h"
#include "ahc_process.h"
#include "ahc_credentials.h"
#include "ahc_tray.h"

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
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <wchar.h>
__declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(void *hModule, char *lpFilename, unsigned long nSize);
__declspec(dllimport) int __stdcall CloseHandle(void *hObject);
#include <direct.h>
#include <process.h>
#define AHC_MKDIR(path) _mkdir(path)
#define AHC_RMDIR(path) _rmdir(path)
#define AHC_STRICMP _stricmp
#else
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#define AHC_MKDIR(path) mkdir(path, 0755)
#define AHC_RMDIR(path) rmdir(path)
#define AHC_STRICMP strcasecmp
#endif

#define AHC_PATH_CAPACITY 768
#define AHC_STATUS_CAPACITY 256
#define AHC_SCAN_MAX_DIRECTORIES 2000
#define AHC_HISTORY_CAPACITY 256
#define AHC_DISPLAY_HISTORY_CAPACITY 400
#define AHC_CATEGORY_CAPACITY 64
#define AHC_AVATAR_CACHE_CAPACITY 24
#define AHC_AVATAR_TEXTURE_MAX 32
#define AHC_AVATAR_KEY 256
#define AHC_AVATAR_LOAD_INTERVAL_SECONDS 0.25
#define AHC_ADDON_STATUS_CACHE_CAPACITY 4096
#define AHC_ADDON_STATUS_REFRESH_SECONDS 5.0
#define AHC_ADDON_STATIC_FPS 30
#define AHC_HIDDEN_FPS 2
#define AHC_FONT_GLYPHS 95
#define AHC_SNAPSHOT_POLL_SECONDS 5.0
#define AHC_PROCESS_ACTIVE_SECONDS 4.0
#define AHC_CHROME_H 36
#if !defined(_WIN32)
#define AHC_SH_CMD_BYTES 4096
#endif

#if !defined(_WIN32)
#include <strings.h>
#endif

typedef enum CompanionTab {
    COMPANION_TAB_ATTUNES = 0,
    COMPANION_TAB_ADDONS = 1,
    COMPANION_TAB_SETTINGS = 2
} CompanionTab;

typedef enum GraphMetric {
    GRAPH_METRIC_ACCOUNT = 0,
    GRAPH_METRIC_TITANFORGED = 1,
    GRAPH_METRIC_WARFORGED = 2,
    GRAPH_METRIC_LIGHTFORGED = 3
} GraphMetric;

typedef enum GraphDisplay {
    GRAPH_DISPLAY_BARS = 0,
    GRAPH_DISPLAY_PLOT = 1
} GraphDisplay;

typedef struct DisplaySnapshot {
    AhcDailyAttuneSnapshot snapshot;
    bool synthetic;
} DisplaySnapshot;

typedef struct AddonInstallStatus {
    double checked_at;
    bool checked;
    bool installed;
    bool git_checkout;
    bool managed;
} AddonInstallStatus;

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
    bool launch_parameters_editing;
    bool clear_data_confirming;
    int addon_scroll;
    GraphMetric graph_metric;
    GraphDisplay graph_display;
    char selected_addon_category[64];
    char addon_catalog_source[AHC_PATH_CAPACITY];
    const AhcAddon *addon_category_cache_items;
    size_t addon_category_cache_count;
    const char *addon_categories[AHC_CATEGORY_CAPACITY];
    size_t addon_category_count;
    bool addon_categories_valid;
    const AhcAddon *addon_filter_cache_items;
    size_t addon_filter_cache_count;
    char addon_filter_cache_category[64];
    size_t addon_filter_count;
    bool addon_filter_valid;
    const AhcAddon *addon_status_cache_items;
    size_t addon_status_cache_count;
    char addon_status_cache_synastria_path[AHC_PATH_CAPACITY];
    char addon_status_root[AHC_PATH_CAPACITY];
    bool addon_status_root_valid;
    AddonInstallStatus addon_statuses[AHC_ADDON_STATUS_CACHE_CAPACITY];
    char launch_parameters[256];
    bool awesome_wotlk_autologin;
    char autologin_user[AHC_WOW_TRUST_STR_MAX + 1];
    char autologin_realm[AHC_WOW_TRUST_STR_MAX + 1];
    char autologin_password_edit[AHC_CREDENTIAL_PASSWORD_MAX];
    bool autologin_user_editing;
    bool autologin_realm_editing;
    bool autologin_password_editing;
    bool autologin_password_stored;
    char monitor_name[128];
    long snapshot_mod_time;
    double next_snapshot_poll_time;
    char process_action[AHC_STATUS_CAPACITY];
    double process_started_at;
    double process_until;
    volatile int addon_job_running;
    volatile int addon_job_done;
    volatile int addon_job_success;
    char addon_job_progress[AHC_STATUS_CAPACITY];
    char addon_job_result[AHC_STATUS_CAPACITY];
    char addon_job_action[AHC_STATUS_CAPACITY];
#if defined(_WIN32)
    uintptr_t addon_job_thread;
#else
    pthread_t addon_job_thread;
#endif
    bool wow_config_loaded;
    bool wow_width_editing;
    bool wow_height_editing;
    bool wow_refresh_editing;
    bool wow_vsync_editing;
    bool wow_multisampling_editing;
    bool wow_texture_resolution_editing;
    bool wow_environment_detail_editing;
    bool wow_ground_effect_density_editing;
    char wow_width[16];
    char wow_height[16];
    char wow_refresh[16];
    char wow_vsync[16];
    char wow_multisampling[16];
    char wow_texture_resolution[16];
    char wow_environment_detail[16];
    char wow_ground_effect_density[16];
    bool wow_windowed;
    bool wow_borderless;
    int ui_scale_index;
    Texture2D phone_qr_tx;
    bool phone_qr_valid;
} CompanionState;

static void companion_dispose_phone_qr(CompanionState *state);

static const Color AHC_BACKGROUND = { 7, 12, 20, 255 };
static const Color AHC_BACKGROUND_TOP = { 19, 31, 48, 255 };
static const Color AHC_BACKGROUND_BOTTOM = { 3, 7, 13, 255 };
static const Color AHC_PANEL = { 22, 32, 46, 245 };
static const Color AHC_PANEL_HOVER = { 31, 46, 65, 250 };
static const Color AHC_PANEL_DARK = { 14, 23, 35, 250 };
static const Color AHC_BORDER = { 80, 108, 140, 255 };
static const Color AHC_BORDER_BRIGHT = { 146, 182, 224, 255 };
static const Color AHC_TEXT = { 233, 239, 251, 255 };
static const Color AHC_MUTED = { 167, 182, 206, 255 };
static const Color AHC_ACCENT = { 83, 167, 239, 255 };
static const Color AHC_ACCENT_DARK = { 39, 93, 151, 255 };
static const Color AHC_STATUS_WARNING = { 141, 184, 232, 255 };
static const Color AHC_TF = { 143, 139, 255, 255 };
static const Color AHC_WF = { 255, 150, 86, 255 };
static const Color AHC_LF = { 255, 226, 115, 255 };

static Font g_ui_font;
static bool g_has_ui_font;
typedef struct AvatarCacheEntry {
    char key[AHC_AVATAR_KEY];
    char fetch_url[AHC_PATH_CAPACITY];
    Texture2D texture;
    bool loaded;
    bool failed;
    bool load_pending;
} AvatarCacheEntry;
static AvatarCacheEntry g_avatar_cache[AHC_AVATAR_CACHE_CAPACITY];
static size_t g_avatar_cache_count;
static Texture2D g_avatar_placeholder;
static bool g_avatar_placeholder_valid;
static double g_next_avatar_load_time;
static char s_exe_dir[AHC_PATH_CAPACITY];
static bool s_have_exe_dir;

static int g_font_codepoints[AHC_FONT_GLYPHS];
static bool g_chrome_drag;
static Vector2 g_chrome_grab;
static bool g_quit;
static float g_ui_scale = 1.0f;

static float ui_scale_value(int index)
{
    switch (index) {
        case 1: return 1.0f;
        case 2: return 1.25f;
        case 3: return 1.5f;
        default: break;
    }
    float scale = (float)GetScreenHeight() / 1080.0f;
    if (scale < 0.9f) {
        scale = 0.9f;
    }
    if (scale > 1.45f) {
        scale = 1.45f;
    }
    return scale;
}

static void update_ui_scale(const CompanionState *state)
{
    g_ui_scale = ui_scale_value(state->ui_scale_index);
}

static float sf(float value)
{
    return value * g_ui_scale;
}

static int si(int value)
{
    return (int)((float)value * g_ui_scale + 0.5f);
}

static void init_latin_font_codepoints(void)
{
    static bool done;
    if (done) {
        return;
    }
    for (int i = 0; i < AHC_FONT_GLYPHS; i++) {
        g_font_codepoints[i] = 32 + i;
    }
    done = true;
}

static void set_status(CompanionState *state, const char *message);
static void set_action_status(CompanionState *state, const char *status, const char *action);
static void set_process_action(CompanionState *state, const char *message);
static void set_addon_job_progress(CompanionState *state, const char *message);
static double app_time(void);
static void path_join(char *out, size_t out_capacity, const char *left, const char *right);
static void ahc_init_exe_dir_once(void);
static bool validate_synastria_path(const char *path);
static void addon_status_cache_invalidate(CompanionState *state);
static bool addon_current_install_path(const CompanionState *state, const AhcAddon *addon, char *out, size_t out_capacity);
#if !defined(_WIN32)
static bool ahc_posix_wine_or_proton_ready(void);
#endif
static void apply_monitor_defaults(CompanionState *state);
static void draw_window_chrome(CompanionState *state);

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

static int draw_wrapped_text(const char *text, int x, int y, int size, int max_width, int max_lines, Color color)
{
    if (!text || !text[0] || max_width <= 0 || max_lines <= 0) {
        return 0;
    }

    char line[256];
    size_t line_len = 0u;
    int lines = 0;
    int cursor_y = y;
    const char *word = text;
    while (*word && lines < max_lines) {
        while (*word == ' ') {
            word++;
        }
        const char *word_end = word;
        while (*word_end && *word_end != ' ') {
            word_end++;
        }
        size_t word_len = (size_t)(word_end - word);
        if (word_len == 0u) {
            break;
        }

        char candidate[256];
        int written = snprintf(candidate, sizeof(candidate), "%s%s%.*s", line_len ? line : "", line_len ? " " : "", (int)word_len, word);
        if (written < 0) {
            break;
        }

        if (line_len && measure_text_width(candidate, size) > max_width) {
            draw_text(line, x, cursor_y, size, color);
            lines++;
            cursor_y += size + 5;
            line_len = 0u;
            line[0] = '\0';
            continue;
        }

        snprintf(line, sizeof(line), "%s", candidate);
        line_len = strlen(line);
        word = word_end;
    }

    if (line_len && lines < max_lines) {
        draw_text(line, x, cursor_y, size, color);
        lines++;
    }

    return lines;
}

static int color_to_gui_hex(Color color)
{
    return ((int)color.r << 24) | ((int)color.g << 16) | ((int)color.b << 8) | color.a;
}

static void format_int_with_commas(int value, char *out, size_t out_capacity)
{
    char digits[32];
    snprintf(digits, sizeof(digits), "%d", value);
    size_t len = strlen(digits);
    size_t start = 0;
    if (digits[0] == '-') {
        if (out_capacity > 1) {
            out[0] = '-';
            out[1] = '\0';
        }
        start = 1;
    }

    size_t written = start;
    for (size_t i = start; i < len && written + 1 < out_capacity; i++) {
        size_t remain = len - i;
        if (i > start && remain % 3 == 0 && written + 1 < out_capacity) {
            out[written++] = ',';
        }
        out[written++] = digits[i];
    }
    out[written] = '\0';
}

static int content_left(void)
{
    return si(26);
}

static int content_right(void)
{
    int right = GetScreenWidth() - si(26);
    return right > content_left() + 420 ? right : content_left() + 420;
}

static int content_top(void)
{
    return AHC_CHROME_H + si(88);
}

static int content_bottom(void)
{
    int bottom = GetScreenHeight() - si(56);
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
#if defined(PLATFORM_ANDROID)
    Rectangle hit = bounds;
    if (hit.height < 44.0f) {
        float pad = 0.5f * (44.0f - hit.height);
        hit.y -= pad;
        if (hit.y < 0.0f) {
            hit.y = 0.0f;
        }
        hit.height = 44.0f;
    }
#else
    Rectangle hit = bounds;
#endif
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, hit);
    bool pressed = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    Color fill = active ? (Color){ 44, 104, 164, 255 } : AHC_PANEL_DARK;
    Color border = active ? AHC_ACCENT : AHC_BORDER;
    Color text = AHC_TEXT;
    Color top_glint = active ? (Color){ 170, 219, 255, 86 } : (Color){ 198, 218, 245, 52 };
    float border_width = 1.0f;

    if (hovered) {
        fill = active ? (Color){ 58, 123, 184, 255 } : AHC_PANEL_HOVER;
        border = active ? (Color){ 163, 218, 255, 255 } : AHC_BORDER_BRIGHT;
        top_glint.a = 104;
        border_width = 2.0f;
    }
    if (pressed) {
        fill = active ? (Color){ 28, 78, 131, 255 } : (Color){ 16, 27, 41, 255 };
        border = active ? (Color){ 120, 184, 236, 255 } : AHC_BORDER;
        top_glint.a = 36;
    }

    DrawRectangleRounded((Rectangle){ bounds.x + 2.0f, bounds.y + 3.0f, bounds.width, bounds.height }, 0.14f, 8, (Color){ 0, 0, 0, 115 });
    DrawRectangleRounded(bounds, 0.16f, 8, fill);
    DrawRectangleRoundedLinesEx(bounds, 0.16f, 8, border_width, border);
    DrawRectangle((int)bounds.x + 4, (int)bounds.y + 3, (int)bounds.width - 8, 1, top_glint);
    draw_centered_text(label, bounds, text_size, text);

    if (!hovered || !IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        return false;
    }
    return true;
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

    init_latin_font_codepoints();
    for (int i = 0; font_paths[i]; i++) {
        if (!FileExists(font_paths[i])) {
            continue;
        }

        g_ui_font = LoadFontEx(
            font_paths[i], 32, g_font_codepoints, AHC_FONT_GLYPHS
        );
        if (g_ui_font.texture.id == 0) {
            g_ui_font = LoadFontEx(font_paths[i], 32, NULL, 0);
        }
        if (g_ui_font.texture.id != 0) {
            SetTextureFilter(g_ui_font.texture, TEXTURE_FILTER_BILINEAR);
            GuiSetFont(g_ui_font);
            g_has_ui_font = true;
            return;
        }
    }
}

static void load_ui_images(void)
{
    g_avatar_cache_count = 0;
    memset(g_avatar_cache, 0, sizeof(g_avatar_cache));
    g_avatar_placeholder_valid = false;
    g_next_avatar_load_time = 0.0;
    {
        Image ph = GenImageColor(32, 32, (Color){ 40, 58, 86, 255 });
        g_avatar_placeholder = LoadTextureFromImage(ph);
        UnloadImage(ph);
    }
    if (g_avatar_placeholder.id != 0) {
        SetTextureFilter(g_avatar_placeholder, TEXTURE_FILTER_BILINEAR);
        g_avatar_placeholder_valid = true;
    }
}

static bool github_owner_from_repo(const char *repo, char *out, size_t out_capacity)
{
    if (!repo || !out || out_capacity == 0) {
        return false;
    }

    const char *needle = strstr(repo, "github.com/");
    if (!needle) {
        return false;
    }
    needle += strlen("github.com/");

    size_t i = 0;
    while (needle[i] && needle[i] != '/' && i + 1 < out_capacity) {
        out[i] = needle[i];
        i++;
    }
    out[i] = '\0';
    return i > 0;
}

static bool addon_avatar_url(const AhcAddon *addon, char *out, size_t out_capacity)
{
    if (!addon || !out || out_capacity == 0) {
        return false;
    }
    if (addon->avatar_url && addon->avatar_url[0]) {
        snprintf(out, out_capacity, "%s", addon->avatar_url);
        return true;
    }

    char owner[128];
    if (!github_owner_from_repo(addon->repo, owner, sizeof(owner))) {
        return false;
    }

    snprintf(out, out_capacity, "https://github.com/%s.png?size=96", owner);
    return true;
}

static void addon_avatar_cache_key(const AhcAddon *addon, const char *avatar_url, char *out, size_t out_capacity)
{
    if (addon && addon->author && addon->author[0]) {
        size_t written = 0;
        for (size_t i = 0; addon->author[i] && written + 1 < out_capacity; i++) {
            unsigned char ch = (unsigned char)addon->author[i];
            out[written++] = (char)tolower(ch);
        }
        out[written] = '\0';
        return;
    }
    snprintf(out, out_capacity, "%s", avatar_url ? avatar_url : "");
}

static void sanitize_cache_key(const char *key, char *out, size_t out_capacity)
{
    size_t written = 0;
    for (size_t i = 0; key[i] && written + 1 < out_capacity; i++) {
        unsigned char ch = (unsigned char)key[i];
        if (isalnum(ch)) {
            out[written++] = (char)ch;
        } else {
            out[written++] = '_';
        }
    }
    out[written] = '\0';
}

static AvatarCacheEntry *avatar_cache_lookup(const char *key)
{
    for (size_t i = 0; i < g_avatar_cache_count; i++) {
        if (strcmp(g_avatar_cache[i].key, key) == 0) {
            return &g_avatar_cache[i];
        }
    }
    return NULL;
}

static void avatar_cache_evict_oldest(void)
{
    if (g_avatar_cache_count == 0) {
        return;
    }
    if (g_avatar_cache[0].loaded) {
        UnloadTexture(g_avatar_cache[0].texture);
    }
    g_avatar_cache_count--;
    if (g_avatar_cache_count > 0) {
        memmove(
            g_avatar_cache,
            g_avatar_cache + 1,
            g_avatar_cache_count * sizeof(g_avatar_cache[0])
        );
    }
    memset(&g_avatar_cache[g_avatar_cache_count], 0, sizeof(g_avatar_cache[0]));
}

static AvatarCacheEntry *avatar_cache_get_or_create(const char *key)
{
    AvatarCacheEntry *existing = avatar_cache_lookup(key);
    if (existing) {
        return existing;
    }
    while (g_avatar_cache_count >= AHC_AVATAR_CACHE_CAPACITY) {
        avatar_cache_evict_oldest();
    }

    AvatarCacheEntry *entry = &g_avatar_cache[g_avatar_cache_count++];
    memset(entry, 0, sizeof(*entry));
    snprintf(entry->key, sizeof(entry->key), "%s", key);
    return entry;
}

static void ensure_avatar_cache_dir(char *out, size_t out_capacity)
{
    ahc_init_exe_dir_once();
    if (s_have_exe_dir) {
        path_join(out, out_capacity, s_exe_dir, "avatar-cache");
    } else {
        char build_dir[AHC_PATH_CAPACITY];
        snprintf(build_dir, sizeof(build_dir), "build");
        if (!DirectoryExists(build_dir)) {
            AHC_MKDIR(build_dir);
        }
        snprintf(out, out_capacity, "build/avatar-cache");
    }
    if (!DirectoryExists(out)) {
        AHC_MKDIR(out);
    }
}

static bool ahc_complete_avatar_entry(AvatarCacheEntry *entry)
{
    if (entry->loaded) {
        return true;
    }
    if (entry->failed) {
        return true;
    }
    if (entry->fetch_url[0] == '\0') {
        return true;
    }

    char cache_dir[AHC_PATH_CAPACITY];
    ensure_avatar_cache_dir(cache_dir, sizeof(cache_dir));

    char cache_name[AHC_PATH_CAPACITY];
    sanitize_cache_key(entry->fetch_url, cache_name, sizeof(cache_name));

    char cache_path[AHC_PATH_CAPACITY];
    path_join(cache_path, sizeof(cache_path), cache_dir, cache_name);
    strncat(cache_path, ".png", sizeof(cache_path) - strlen(cache_path) - 1);

    if (!FileExists(cache_path) && !ahc_curl_download_file(entry->fetch_url, cache_path)) {
        entry->failed = true;
        entry->load_pending = false;
        return true;
    }

    Image avatar_image = LoadImage(cache_path);
    if (avatar_image.data == NULL || avatar_image.width == 0 || avatar_image.height == 0) {
        entry->failed = true;
        entry->load_pending = false;
        return true;
    }
    if (avatar_image.width > AHC_AVATAR_TEXTURE_MAX || avatar_image.height > AHC_AVATAR_TEXTURE_MAX) {
        const int max_side = AHC_AVATAR_TEXTURE_MAX;
        int w = avatar_image.width;
        int h = avatar_image.height;
        if (w > h) {
            h = (h * max_side) / w;
            w = max_side;
        } else {
            w = (w * max_side) / h;
            h = max_side;
        }
        if (w < 1) {
            w = 1;
        }
        if (h < 1) {
            h = 1;
        }
        ImageResize(&avatar_image, w, h);
    }
    Texture2D texture = LoadTextureFromImage(avatar_image);
    UnloadImage(avatar_image);
    if (texture.id == 0) {
        entry->failed = true;
        entry->load_pending = false;
        return true;
    }

    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    entry->texture = texture;
    entry->loaded = true;
    entry->load_pending = false;
    return true;
}

static void ahc_process_avatar_deferred_loads(void)
{
    double now = app_time();
    if (now < g_next_avatar_load_time) {
        return;
    }

    for (size_t i = 0; i < g_avatar_cache_count; i++) {
        AvatarCacheEntry *e = &g_avatar_cache[i];
        if (e->load_pending && !e->loaded && !e->failed) {
            (void)ahc_complete_avatar_entry(e);
            g_next_avatar_load_time = now + AHC_AVATAR_LOAD_INTERVAL_SECONDS;
            return;
        }
    }
}

static Texture2D *get_addon_avatar_texture(const AhcAddon *addon)
{
    char avatar_url[AHC_PATH_CAPACITY];
    if (!addon_avatar_url(addon, avatar_url, sizeof(avatar_url))) {
        return NULL;
    }

    char cache_key[AHC_PATH_CAPACITY];
    addon_avatar_cache_key(addon, avatar_url, cache_key, sizeof(cache_key));
    AvatarCacheEntry *entry = avatar_cache_get_or_create(cache_key);
    if (!entry || entry->failed) {
        return NULL;
    }
    if (entry->loaded) {
        return &entry->texture;
    }
    snprintf(entry->fetch_url, sizeof(entry->fetch_url), "%s", avatar_url);
    entry->load_pending = true;
    if (g_avatar_placeholder_valid) {
        return &g_avatar_placeholder;
    }
    return NULL;
}

static const char *addon_source_label(const AhcAddon *addon)
{
    if (!addon || !addon->source || !addon->source[0]) {
        return "GitHub";
    }
    return addon->source;
}

static bool addon_is_github_source(const AhcAddon *addon)
{
    return strcmp(addon_source_label(addon), "GitHub") == 0;
}

static bool addon_repo_is_git_checkout(const AhcAddon *addon)
{
    if (!addon || !addon->repo || !addon->repo[0]) {
        return false;
    }
    return strstr(addon->repo, "github.com/") != NULL || strstr(addon->repo, ".git") != NULL;
}

static bool addon_repo_is_zip_url(const AhcAddon *addon)
{
    if (!addon || !addon->repo || !addon->repo[0]) {
        return false;
    }
    return strstr(addon->repo, ".zip") != NULL;
}

static bool addon_has_install_source(const AhcAddon *addon)
{
    return addon_repo_is_git_checkout(addon) || addon_repo_is_zip_url(addon);
}

static bool open_external_url(const char *url)
{
    if (!url || !url[0]) {
        return false;
    }
    return ahc_open_url_hidden(url);
}

static bool addon_url_for_author_link(const AhcAddon *addon, char *out, size_t out_capacity)
{
    if (!addon || !out || out_capacity == 0) {
        return false;
    }
    char owner[128];
    if (github_owner_from_repo(addon->repo, owner, sizeof(owner)) && owner[0]) {
        snprintf(out, out_capacity, "https://github.com/%s", owner);
        return true;
    }
    if (addon->page_url && addon->page_url[0]) {
        snprintf(out, out_capacity, "%s", addon->page_url);
        return true;
    }
    if (addon->repo && (strstr(addon->repo, "https://") || strstr(addon->repo, "http://"))) {
        snprintf(out, out_capacity, "%s", addon->repo);
        return true;
    }
    return false;
}

static bool addon_url_for_source_link(const AhcAddon *addon, char *out, size_t out_capacity)
{
    if (!addon || !out || out_capacity == 0) {
        return false;
    }
    if (addon->page_url && addon->page_url[0]) {
        snprintf(out, out_capacity, "%s", addon->page_url);
        return true;
    }
    if (addon->repo && addon->repo[0] && (strstr(addon->repo, "https://") || strstr(addon->repo, "http://"))) {
        if (addon_repo_is_zip_url(addon)) {
            return false;
        }
        snprintf(out, out_capacity, "%s", addon->repo);
        return true;
    }
    if (AHC_STRICMP(addon_source_label(addon), "Felbite") == 0) {
        snprintf(out, out_capacity, "%s", "https://felbite.com/wow-3-3-5-addons/");
        return true;
    }
    return false;
}

static void try_open_url_with_feedback(CompanionState *state, const char *url, const char *ok_action)
{
    if (open_external_url(url)) {
        set_action_status(state, "Opened in browser.", ok_action ? ok_action : "Link opened");
    } else {
        set_action_status(state, "Could not open link in browser.", "Link failed");
    }
}

static bool draw_interactive_text_link(int x, int y, int size, const char *text, const char *url)
{
    if (!text || !text[0] || !url || !url[0]) {
        return false;
    }
    int tw = measure_text_width(text, size);
    if (tw <= 0) {
        return false;
    }
    Rectangle hit = { (float)x, (float)y, (float)tw, (float)size + 4.0f };
    bool hovered = CheckCollisionPointRec(GetMousePosition(), hit);
    Color base = (Color){ 150, 190, 240, 255 };
    Color hi = (Color){ 200, 235, 255, 255 };
    draw_text(text, x, y, size, hovered ? hi : base);
    if (hovered) {
        int ly = y + size;
        DrawLine((int)hit.x, ly, (int)(hit.x + hit.width), ly, hi);
    }
    if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        return true;
    }
    return false;
}

static void draw_source_badge(CompanionState *state, Rectangle bounds, const AhcAddon *addon)
{
    DrawRectangleRounded(bounds, 0.16f, 8, (Color){ 15, 25, 39, 255 });
    DrawRectangleRoundedLines(bounds, 0.16f, 8, (Color){ 98, 128, 166, 255 });
    DrawRectangleGradientV((int)bounds.x + 1, (int)bounds.y + 1, (int)bounds.width - 2, (int)bounds.height - 2, (Color){ 30, 50, 74, 255 }, (Color){ 12, 21, 34, 255 });

    Texture2D *avatar = get_addon_avatar_texture(addon);
    if (avatar) {
        Rectangle source = { 0.0f, 0.0f, (float)avatar->width, (float)avatar->height };
        float icon_size = bounds.height - 16.0f;
        Rectangle dest = { bounds.x + 9.0f, bounds.y + 8.0f, icon_size, icon_size };
        DrawTexturePro(*avatar, source, dest, (Vector2){ 0.0f, 0.0f }, 0.0f, RAYWHITE);
        DrawRectangleRoundedLines(dest, 0.24f, 8, (Color){ 198, 220, 248, 120 });
    } else {
        DrawCircle((int)bounds.x + 26, (int)bounds.y + 22, 12.0f, (Color){ 236, 243, 255, 220 });
        draw_text("GH", (int)bounds.x + 16, (int)bounds.y + 14, 14, (Color){ 22, 36, 54, 255 });
    }

    char author_url[AHC_PATH_CAPACITY];
    char source_url[AHC_PATH_CAPACITY];
    bool has_aurl = addon_url_for_author_link(addon, author_url, sizeof(author_url));
    bool has_surl = addon_url_for_source_link(addon, source_url, sizeof(source_url));
    int ax = (int)bounds.x + 44;
    int auy = (int)bounds.y + 8;
    int sx = (int)bounds.x + 44;
    int suy = (int)bounds.y + 27;

    if (has_aurl) {
        if (draw_interactive_text_link(ax, auy, 14, addon->author, author_url)) {
            try_open_url_with_feedback(state, author_url, "Author link");
        }
    } else {
        draw_text(addon->author, ax, auy, 14, AHC_TEXT);
    }

    const char *slab = addon_source_label(addon);
    if (has_surl) {
        if (draw_interactive_text_link(sx, suy, 12, slab, source_url)) {
            try_open_url_with_feedback(state, source_url, "Addon page");
        }
    } else {
        draw_text(slab, sx, suy, 12, AHC_MUTED);
    }
}

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

static bool ensure_directory(const char *path)
{
    if (DirectoryExists(path)) {
        return true;
    }

    AHC_MKDIR(path);
    return DirectoryExists(path);
}

static bool backup_base_root(const CompanionState *state, char *out, size_t out_capacity)
{
    if (!validate_synastria_path(state->synastria_path)) {
        return false;
    }

    path_join(out, out_capacity, state->synastria_path, "AttuneHelperBackup");
    return ensure_directory(out);
}

static bool backup_child_root(const CompanionState *state, const char *child, char *out, size_t out_capacity)
{
    char base[AHC_PATH_CAPACITY];
    if (!backup_base_root(state, base, sizeof(base))) {
        return false;
    }

    path_join(out, out_capacity, base, child);
    return ensure_directory(out);
}

static bool resolve_wtf_path(const CompanionState *state, char *out, size_t out_capacity)
{
    if (!validate_synastria_path(state->synastria_path)) {
        return false;
    }

    path_join(out, out_capacity, state->synastria_path, "WTF");
    return DirectoryExists(out);
}

static bool resolve_wow_config_path(const CompanionState *state, char *out, size_t out_capacity)
{
    char wtf_path[AHC_PATH_CAPACITY];
    if (!resolve_wtf_path(state, wtf_path, sizeof(wtf_path))) {
        return false;
    }

    path_join(out, out_capacity, wtf_path, "Config.wtf");
    return true;
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

static void addon_status_cache_invalidate(CompanionState *state)
{
    state->addon_status_cache_items = NULL;
    state->addon_status_cache_count = 0u;
    state->addon_status_cache_synastria_path[0] = '\0';
    state->addon_status_root[0] = '\0';
    state->addon_status_root_valid = false;
    memset(state->addon_statuses, 0, sizeof(state->addon_statuses));
}

static void addon_status_cache_prepare(CompanionState *state, const AhcAddon *addons, size_t count)
{
    size_t capped_count = count < AHC_ADDON_STATUS_CACHE_CAPACITY ? count : AHC_ADDON_STATUS_CACHE_CAPACITY;
    if (state->addon_status_cache_items == addons
        && state->addon_status_cache_count == capped_count
        && strcmp(state->addon_status_cache_synastria_path, state->synastria_path) == 0) {
        return;
    }

    addon_status_cache_invalidate(state);
    state->addon_status_cache_items = addons;
    state->addon_status_cache_count = capped_count;
    snprintf(state->addon_status_cache_synastria_path, sizeof(state->addon_status_cache_synastria_path), "%s", state->synastria_path);
    state->addon_status_root_valid = resolve_addons_path(state, state->addon_status_root, sizeof(state->addon_status_root));
}

static AddonInstallStatus addon_install_status_for_index(CompanionState *state, const AhcAddon *addons, size_t count, size_t index)
{
    AddonInstallStatus status = { 0 };
    if (!addons || index >= count) {
        return status;
    }

    addon_status_cache_prepare(state, addons, count);
    if (index >= state->addon_status_cache_count) {
        status.installed = addon_is_installed(state, &addons[index]);
        status.git_checkout = addon_is_git_checkout(state, &addons[index]);
        status.managed = status.git_checkout || addon_has_managed_marker(state, &addons[index]);
        status.checked = true;
        return status;
    }

    AddonInstallStatus *cached = &state->addon_statuses[index];
    double now = app_time();
    if (cached->checked && now - cached->checked_at < AHC_ADDON_STATUS_REFRESH_SECONDS) {
        return *cached;
    }

    memset(cached, 0, sizeof(*cached));
    cached->checked_at = now;
    cached->checked = true;
    if (!state->addon_status_root_valid) {
        return *cached;
    }

    char git_path[AHC_PATH_CAPACITY];
    char marker_path[AHC_PATH_CAPACITY];
    char path[AHC_PATH_CAPACITY];
    cached->installed = addon_current_install_path(state, &addons[index], path, sizeof(path));
    if (cached->installed) {
        path_join(git_path, sizeof(git_path), path, ".git");
        path_join(marker_path, sizeof(marker_path), path, ".attune-helper-companion");
        cached->git_checkout = DirectoryExists(git_path);
        cached->managed = cached->git_checkout || FileExists(marker_path);
    }
    return *cached;
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

static bool copy_file_contents(const char *source, const char *destination)
{
    FILE *input = fopen(source, "rb");
    if (!input) {
        return false;
    }

    FILE *output = fopen(destination, "wb");
    if (!output) {
        fclose(input);
        return false;
    }

    char buffer[16384];
    bool copied = true;
    size_t read_count = 0;
    while ((read_count = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        if (fwrite(buffer, 1, read_count, output) != read_count) {
            copied = false;
            break;
        }
    }

    if (ferror(input)) {
        copied = false;
    }

    fclose(input);
    if (fclose(output) != 0) {
        copied = false;
    }

    return copied;
}

static bool copy_directory_tree(const char *source, const char *destination)
{
    if (!DirectoryExists(source) || !ensure_directory(destination)) {
        return false;
    }

    FilePathList entries = LoadDirectoryFiles(source);
    bool copied = true;

    for (unsigned int i = 0; i < entries.count; i++) {
        const char *entry = entries.paths[i];
        const char *name = GetFileName(entry);
        if (AHC_STRICMP(name, ".") == 0 || AHC_STRICMP(name, "..") == 0) {
            continue;
        }

        char destination_entry[AHC_PATH_CAPACITY];
        path_join(destination_entry, sizeof(destination_entry), destination, name);
        if (DirectoryExists(entry)) {
            if (!copy_directory_tree(entry, destination_entry)) {
                copied = false;
            }
        } else if (!copy_file_contents(entry, destination_entry)) {
            copied = false;
        }
    }

    UnloadDirectoryFiles(entries);
    return copied;
}

static void make_timestamped_folder_name(const char *label, char *out, size_t out_capacity)
{
    snprintf(out, out_capacity, "%s-%lld", label, (long long)time(NULL));
}

static bool backup_directory_snapshot(CompanionState *state, const char *source, const char *backup_child, const char *label)
{
    char root[AHC_PATH_CAPACITY];
    char folder[128];
    char destination[AHC_PATH_CAPACITY];

    if (!backup_child_root(state, backup_child, root, sizeof(root))) {
        set_action_status(state, "Could not create backup folder.", "Backup folder failed");
        return false;
    }

    make_timestamped_folder_name(label, folder, sizeof(folder));
    path_join(destination, sizeof(destination), root, folder);

    if (!copy_directory_tree(source, destination)) {
        set_action_status(state, "Backup did not complete. Check folder permissions.", "Backup failed");
        return false;
    }

    char message[AHC_STATUS_CAPACITY];
    snprintf(message, sizeof(message), "Backed up %s to AttuneHelperBackup/%s.", label, backup_child);
    set_action_status(state, message, "Backup complete");
    return true;
}

static bool addon_backup_root(const CompanionState *state, char *out, size_t out_capacity)
{
    return backup_child_root(state, "Addons", out, out_capacity);
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

static void make_backup_path_for_folder(const CompanionState *state, const char *folder, const char *reason, char *out, size_t out_capacity)
{
    char root[AHC_PATH_CAPACITY];
    if (!addon_backup_root(state, root, sizeof(root))) {
        out[0] = '\0';
        return;
    }

    char name[256];
    snprintf(name, sizeof(name), "%s-%s-%lld", folder, reason, (long long)time(NULL));
    path_join(out, out_capacity, root, name);
}

static bool move_addon_folder_to_backup(CompanionState *state, const char *folder_path, const char *folder, const char *reason)
{
    if (!DirectoryExists(folder_path)) {
        return true;
    }

    char backup_path[AHC_PATH_CAPACITY];
    make_backup_path_for_folder(state, folder, reason, backup_path, sizeof(backup_path));
    if (!backup_path[0]) {
        set_status(state, "Could not create addon backup folder.");
        return false;
    }
    if (rename(folder_path, backup_path) != 0) {
        set_status(state, "Could not move existing addon to backup folder.");
        return false;
    }
    return true;
}

static bool extract_zip_to_path(const char *zip_path, const char *destination)
{
    if (!ensure_directory(destination)) {
        return false;
    }
    if (!ahc_preflight_zip_safe(zip_path)) {
        return false;
    }

    char command[AHC_PATH_CAPACITY * 4];
#if defined(_WIN32)
    snprintf(command, sizeof(command), "powershell -NoProfile -ExecutionPolicy Bypass -Command \"Expand-Archive -LiteralPath '%s' -DestinationPath '%s' -Force\"", zip_path, destination);
#else
    snprintf(command, sizeof(command), "unzip -q -o \"%s\" -d \"%s\"", zip_path, destination);
#endif
    return ahc_run_command_hidden(command);
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

static bool addon_marker_matches(const AhcAddon *addon, const char *path)
{
    char marker_path[AHC_PATH_CAPACITY];
    path_join(marker_path, sizeof(marker_path), path, ".attune-helper-companion");
    FILE *file = fopen(marker_path, "rb");
    if (!file) {
        return false;
    }

    char line[AHC_PATH_CAPACITY];
    bool matches = false;
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "repo=", 5) == 0) {
            char *value = line + 5;
            value[strcspn(value, "\r\n")] = '\0';
            matches = strcmp(value, addon->repo) == 0;
            break;
        }
    }
    fclose(file);
    return matches;
}

static bool find_managed_addon_path_in_root(const AhcAddon *addon, const char *addons_path, char *out, size_t out_capacity)
{
    if (!DirectoryExists(addons_path)) {
        return false;
    }

    FilePathList entries = LoadDirectoryFiles(addons_path);
    bool found = false;
    for (unsigned int i = 0; i < entries.count; i++) {
        const char *entry = entries.paths[i];
        if (DirectoryExists(entry) && addon_marker_matches(addon, entry)) {
            snprintf(out, out_capacity, "%s", entry);
            found = true;
            break;
        }
    }
    UnloadDirectoryFiles(entries);
    return found;
}

static bool addon_current_install_path(const CompanionState *state, const AhcAddon *addon, char *out, size_t out_capacity)
{
    if (addon_install_path(state, addon, out, out_capacity) && DirectoryExists(out)) {
        return true;
    }

    char addons_path[AHC_PATH_CAPACITY];
    if (!resolve_addons_path(state, addons_path, sizeof(addons_path))) {
        return false;
    }
    return find_managed_addon_path_in_root(addon, addons_path, out, out_capacity);
}

typedef struct AddonTocFolderList {
    char paths[32][AHC_PATH_CAPACITY];
    char names[32][128];
    size_t count;
} AddonTocFolderList;

static void toc_stem_from_file(const char *filename, char *out, size_t out_cap)
{
    const char *base = GetFileName(filename);
    const char *dot = strrchr(base, '.');
    if (dot) {
        size_t len = (size_t)(dot - base);
        if (len >= out_cap) {
            len = out_cap - 1u;
        }
        memcpy(out, base, len);
        out[len] = '\0';
    } else {
        out[0] = '\0';
    }
}

static bool is_embedded_dependency_folder_name(const char *name)
{
    if (AHC_STRICMP(name, "libs") == 0) {
        return true;
    }
    if (AHC_STRICMP(name, "lib") == 0) {
        return true;
    }
    if (AHC_STRICMP(name, "libraries") == 0) {
        return true;
    }
    if (AHC_STRICMP(name, "externals") == 0) {
        return true;
    }
    if (AHC_STRICMP(name, "external") == 0) {
        return true;
    }
    return false;
}

static bool pick_addon_basename_for_toc_folder(const char *path, char *basename, size_t basename_cap)
{
    if (!path || !basename || basename_cap == 0u) {
        return false;
    }

    const char *dname = GetFileName(path);
    char dname_toc_path[AHC_PATH_CAPACITY];
    path_join(dname_toc_path, sizeof(dname_toc_path), path, dname);
    {
        size_t m = strlen(dname_toc_path);
        if (m + 5u < sizeof(dname_toc_path)) {
            snprintf(dname_toc_path + m, sizeof(dname_toc_path) - m, "%s", ".toc");
        }
    }
    if (FileExists(dname_toc_path)) {
        snprintf(basename, basename_cap, "%s", dname);
        return true;
    }

    FilePathList entries = LoadDirectoryFiles(path);
    char stems[32][128];
    size_t stem_count = 0u;
    for (unsigned int i = 0; i < entries.count; i++) {
        const char *entry = entries.paths[i];
        if (DirectoryExists(entry)) {
            continue;
        }
        const char *name = GetFileName(entry);
        const char *dot = strrchr(name, '.');
        if (!dot || AHC_STRICMP(dot, ".toc") != 0) {
            continue;
        }
        if (stem_count < 32u) {
            toc_stem_from_file(name, stems[stem_count], sizeof(stems[stem_count]));
            stem_count++;
        }
    }
    UnloadDirectoryFiles(entries);

    if (stem_count == 0u) {
        return false;
    }
    if (stem_count == 1u) {
        snprintf(basename, basename_cap, "%s", stems[0]);
        return true;
    }

    for (size_t s = 0; s < stem_count; s++) {
        if (AHC_STRICMP(stems[s], dname) == 0) {
            snprintf(basename, basename_cap, "%s", stems[s]);
            return true;
        }
    }

    snprintf(basename, basename_cap, "%s", stems[0]);
    return true;
}

static void find_toc_folders_recursive(const char *root, AddonTocFolderList *folders, int depth)
{
    if (!root || !DirectoryExists(root) || !folders || folders->count >= 32u || depth > 8) {
        return;
    }

    char base_name[128];
    if (pick_addon_basename_for_toc_folder(root, base_name, sizeof(base_name))) {
        snprintf(folders->paths[folders->count], sizeof(folders->paths[folders->count]), "%s", root);
        snprintf(folders->names[folders->count], sizeof(folders->names[folders->count]), "%s", base_name[0] ? base_name : GetFileName(root));
        folders->count++;

        FilePathList sub = LoadDirectoryFiles(root);
        for (unsigned int j = 0; j < sub.count && folders->count < 32u; j++) {
            const char *entry = sub.paths[j];
            const char *name = GetFileName(entry);
            if (!DirectoryExists(entry) || AHC_STRICMP(name, ".git") == 0 || AHC_STRICMP(name, ".github") == 0) {
                continue;
            }
            if (is_embedded_dependency_folder_name(name)) {
                continue;
            }
            find_toc_folders_recursive(entry, folders, depth + 1);
        }
        UnloadDirectoryFiles(sub);
        return;
    }

    FilePathList entries = LoadDirectoryFiles(root);
    for (unsigned int i = 0; i < entries.count && folders->count < 32u; i++) {
        const char *entry = entries.paths[i];
        const char *name = GetFileName(entry);
        if (!DirectoryExists(entry) || AHC_STRICMP(name, ".git") == 0 || AHC_STRICMP(name, ".github") == 0) {
            continue;
        }
        find_toc_folders_recursive(entry, folders, depth + 1);
    }
    UnloadDirectoryFiles(entries);
}

static bool stage_addon_source(CompanionState *state, const AhcAddon *addon, char *package_root, size_t package_root_capacity)
{
    char staging_root[AHC_PATH_CAPACITY];
    char stage_name[256];
    if (!addon_staging_root(state, staging_root, sizeof(staging_root))) {
        return false;
    }

    snprintf(stage_name, sizeof(stage_name), "%s-source-%lld", addon->folder, (long long)time(NULL));
    path_join(package_root, package_root_capacity, staging_root, stage_name);
    remove_directory_tree(package_root);

    if (addon_repo_is_zip_url(addon)) {
        set_addon_job_progress(state, "Downloading addon ZIP");
        char zip_path[AHC_PATH_CAPACITY];
        char extract_path[AHC_PATH_CAPACITY];
        char zip_name[300];
        snprintf(zip_name, sizeof(zip_name), "%s.zip", stage_name);
        path_join(zip_path, sizeof(zip_path), staging_root, zip_name);
        path_join(extract_path, sizeof(extract_path), staging_root, stage_name);
        remove(zip_path);
        remove_directory_tree(extract_path);
        if (!ahc_curl_download_file(addon->repo, zip_path)) {
            remove(zip_path);
            remove_directory_tree(extract_path);
            return false;
        }
        set_addon_job_progress(state, "Extracting addon ZIP");
        if (!extract_zip_to_path(zip_path, extract_path)) {
            remove(zip_path);
            remove_directory_tree(extract_path);
            return false;
        }
        remove(zip_path);
        snprintf(package_root, package_root_capacity, "%s", extract_path);
        return true;
    }

    set_addon_job_progress(state, "Cloning addon repository");
    if (!ahc_git_shallow_clone(addon->repo, package_root)) {
        remove_directory_tree(package_root);
        return false;
    }
    return true;
}

static bool promote_staged_addon_folders(CompanionState *state, const AhcAddon *addon, const char *package_root)
{
    char addons_path[AHC_PATH_CAPACITY];
    char scan_root[AHC_PATH_CAPACITY];
    if (!resolve_addons_path(state, addons_path, sizeof(addons_path))) {
        return false;
    }

    snprintf(scan_root, sizeof(scan_root), "%s", package_root);
    if (addon_has_source_subdir(addon)) {
        char source_path[AHC_PATH_CAPACITY];
        path_join(source_path, sizeof(source_path), package_root, addon->source_subdir);
        if (DirectoryExists(source_path)) {
            snprintf(scan_root, sizeof(scan_root), "%s", source_path);
        }
    }

    set_addon_job_progress(state, "Finding addon folders");
    AddonTocFolderList folders = { 0 };
    find_toc_folders_recursive(scan_root, &folders, 0);
    if (folders.count == 0u) {
        set_status(state, "Download did not contain an addon folder with a .toc file.");
        return false;
    }

    char legacy_path[AHC_PATH_CAPACITY];
    addon_install_path(state, addon, legacy_path, sizeof(legacy_path));
    for (size_t i = 0; i < folders.count; i++) {
        char destination[AHC_PATH_CAPACITY];
        path_join(destination, sizeof(destination), addons_path, folders.names[i]);
        set_addon_job_progress(state, "Backing up existing addon folder");
        if (!move_addon_folder_to_backup(state, destination, folders.names[i], "update")) {
            return false;
        }
        if (AHC_STRICMP(folders.names[i], addon->folder) != 0 && DirectoryExists(legacy_path)) {
            if (!move_addon_folder_to_backup(state, legacy_path, addon->folder, "legacy-backup")) {
                return false;
            }
        }
        set_addon_job_progress(state, "Installing addon folder");
        if (rename(folders.paths[i], destination) != 0) {
            if (!copy_directory_tree(folders.paths[i], destination)) {
                set_status(state, "Could not promote staged addon folder.");
                return false;
            }
            remove_directory_tree(folders.paths[i]);
        }
        write_addon_managed_marker(addon, destination);
    }

    return true;
}

static bool download_addon_to_addons(CompanionState *state, const AhcAddon *addon)
{
    char package_root[AHC_PATH_CAPACITY];
    if (!stage_addon_source(state, addon, package_root, sizeof(package_root))) {
        return false;
    }

    bool promoted = promote_staged_addon_folders(state, addon, package_root);
    set_addon_job_progress(state, "Cleaning staging files");
    remove_directory_tree(package_root);
    return promoted;
}

static void install_or_update_addon_sync(CompanionState *state, const AhcAddon *addon)
{
    char path[AHC_PATH_CAPACITY];

    if (!addon_install_path(state, addon, path, sizeof(path))) {
        set_status(state, "Set a valid Synastria folder before managing addons.");
        return;
    }

    bool was_installed = addon_current_install_path(state, addon, path, sizeof(path));
    set_addon_job_progress(state, was_installed ? "Updating addon" : "Installing addon");
    bool result = download_addon_to_addons(state, addon);
    char message[AHC_STATUS_CAPACITY];
    if (result) {
        snprintf(message, sizeof(message), "%s %s.", was_installed ? "Updated" : "Installed", addon->name);
    } else {
        snprintf(message, sizeof(message), "Could not install %s from source.", addon->name);
    }
    set_status(state, message);
    addon_status_cache_invalidate(state);
}

typedef struct AddonInstallJobArgs {
    CompanionState *state;
    const AhcAddon *addon;
} AddonInstallJobArgs;

static void addon_install_job_run(AddonInstallJobArgs *args)
{
    CompanionState *state = args->state;
    const AhcAddon *addon = args->addon;
    snprintf(state->addon_job_progress, sizeof(state->addon_job_progress), "Starting %s", addon->name);
    install_or_update_addon_sync(state, addon);
    snprintf(state->addon_job_result, sizeof(state->addon_job_result), "%s", state->status);
    state->addon_job_success = strstr(state->status, "Could not") == NULL;
    state->addon_job_done = 1;
    free(args);
}

#if defined(_WIN32)
static unsigned __stdcall addon_install_job_thread(void *opaque)
{
    addon_install_job_run((AddonInstallJobArgs *)opaque);
    return 0u;
}
#else
static void *addon_install_job_thread(void *opaque)
{
    addon_install_job_run((AddonInstallJobArgs *)opaque);
    return NULL;
}
#endif

static void begin_install_or_update_addon(CompanionState *state, const AhcAddon *addon)
{
    if (state->addon_job_running) {
        set_action_status(state, "Another addon install is already running.", "Addon install busy");
        return;
    }

    AddonInstallJobArgs *args = (AddonInstallJobArgs *)calloc(1, sizeof(*args));
    if (!args) {
        set_action_status(state, "Could not start addon install job.", "Install start failed");
        return;
    }
    args->state = state;
    args->addon = addon;
    state->addon_job_running = 1;
    state->addon_job_done = 0;
    state->addon_job_success = 0;
    snprintf(state->addon_job_progress, sizeof(state->addon_job_progress), "Queued %s", addon->name);
    snprintf(state->addon_job_result, sizeof(state->addon_job_result), "");
    set_process_action(state, state->addon_job_progress);

#if defined(_WIN32)
    state->addon_job_thread = _beginthreadex(NULL, 0, addon_install_job_thread, args, 0, NULL);
    if (state->addon_job_thread == 0u) {
        state->addon_job_running = 0;
        free(args);
        set_action_status(state, "Could not start addon install thread.", "Install start failed");
    }
#else
    if (pthread_create(&state->addon_job_thread, NULL, addon_install_job_thread, args) != 0) {
        state->addon_job_running = 0;
        free(args);
        set_action_status(state, "Could not start addon install thread.", "Install start failed");
    } else {
        pthread_detach(state->addon_job_thread);
    }
#endif
}

static void poll_addon_install_job(CompanionState *state)
{
    if (!state->addon_job_running) {
        return;
    }
    snprintf(state->process_action, sizeof(state->process_action), "%s", state->addon_job_progress);
    state->process_started_at = app_time();
    state->process_until = state->process_started_at + 1.0;
    if (!state->addon_job_done) {
        return;
    }
#if defined(_WIN32)
    if (state->addon_job_thread != 0u) {
        CloseHandle((void *)state->addon_job_thread);
        state->addon_job_thread = 0u;
    }
#endif
    state->addon_job_running = 0;
    set_action_status(state, state->addon_job_result[0] ? state->addon_job_result : "Addon install finished.", state->addon_job_success ? "Addon install complete" : "Addon install failed");
    addon_status_cache_invalidate(state);
}

static void uninstall_addon(CompanionState *state, const AhcAddon *addon)
{
    char path[AHC_PATH_CAPACITY];
    char backup_path[AHC_PATH_CAPACITY];

    if (!addon_current_install_path(state, addon, path, sizeof(path))) {
        set_status(state, "Addon is not installed.");
        return;
    }

    if (!move_installed_addon_to_backup(state, addon, path, "uninstalled", backup_path, sizeof(backup_path))) {
        return;
    }

    char message[AHC_STATUS_CAPACITY];
    snprintf(message, sizeof(message), "Uninstalled %s. Backup saved.", addon->name);
    set_status(state, message);
    addon_status_cache_invalidate(state);
}

static void set_status(CompanionState *state, const char *message)
{
    snprintf(state->status, sizeof(state->status), "%s", message);
}

static double app_time(void)
{
    return IsWindowReady() ? GetTime() : 0.0;
}

static void set_process_action(CompanionState *state, const char *message)
{
    snprintf(state->process_action, sizeof(state->process_action), "%s", message);
    state->process_started_at = app_time();
    state->process_until = state->process_started_at + AHC_PROCESS_ACTIVE_SECONDS;
}

static void set_action_status(CompanionState *state, const char *status, const char *action)
{
    set_status(state, status);
    set_process_action(state, action);
}

static void set_addon_job_progress(CompanionState *state, const char *message)
{
    snprintf(state->addon_job_progress, sizeof(state->addon_job_progress), "%s", message);
    snprintf(state->addon_job_action, sizeof(state->addon_job_action), "%s", message);
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

static bool parse_iso_date(const char *date, int *year, int *month, int *day)
{
    int parsed_year = 0;
    int parsed_month = 0;
    int parsed_day = 0;
    if (!date || sscanf(date, "%d-%d-%d", &parsed_year, &parsed_month, &parsed_day) != 3) {
        return false;
    }
    if (parsed_month < 1 || parsed_month > 12 || parsed_day < 1 || parsed_day > 31) {
        return false;
    }

    *year = parsed_year;
    *month = parsed_month;
    *day = parsed_day;
    return true;
}

static int days_from_civil(int year, int month, int day)
{
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = (unsigned)(year - era * 400);
    const unsigned doy = (153u * (unsigned)(month + (month > 2 ? -3 : 9)) + 2u) / 5u + (unsigned)day - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097 + (int)doe - 719468;
}

static void civil_from_days(int days, int *year, int *month, int *day)
{
    days += 719468;
    const int era = (days >= 0 ? days : days - 146096) / 146097;
    const unsigned doe = (unsigned)(days - era * 146097);
    const unsigned yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
    int y = (int)yoe + era * 400;
    const unsigned doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
    const unsigned mp = (5u * doy + 2u) / 153u;
    const unsigned d = doy - (153u * mp + 2u) / 5u + 1u;
    const int m = (int)mp + ((int)mp < 10 ? 3 : -9);
    y += m <= 2;

    *year = y;
    *month = m;
    *day = (int)d;
}

static bool date_to_ordinal(const char *date, int *ordinal)
{
    int year = 0;
    int month = 0;
    int day = 0;
    if (!parse_iso_date(date, &year, &month, &day)) {
        return false;
    }

    *ordinal = days_from_civil(year, month, day);
    return true;
}

static void ordinal_to_date(int ordinal, char *out, size_t out_capacity)
{
    int year = 0;
    int month = 0;
    int day = 0;
    civil_from_days(ordinal, &year, &month, &day);
    snprintf(out, out_capacity, "%04d-%02d-%02d", year, month, day);
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
        state->autologin_password_stored = ahc_credential_wow_password_exists(state->config_dir);
        return;
    }

    char line[AHC_PATH_CAPACITY + 32];
    while (fgets(line, sizeof(line), file)) {
        trim_line(line);
        if (strncmp(line, "synastria_path=", 15) == 0) {
            snprintf(state->synastria_path, sizeof(state->synastria_path), "%s", line + 15);
        } else if (strncmp(line, "launch_parameters=", 18) == 0) {
            snprintf(state->launch_parameters, sizeof(state->launch_parameters), "%s", line + 18);
        } else if (strncmp(line, "ui_scale=", 9) == 0) {
            state->ui_scale_index = atoi(line + 9);
            if (state->ui_scale_index < 0 || state->ui_scale_index > 3) {
                state->ui_scale_index = 0;
            }
        } else if (strncmp(line, "awesome_wotlk_autologin=", 25) == 0) {
            state->awesome_wotlk_autologin = atoi(line + 25) != 0;
        } else if (strncmp(line, "autologin_user=", 16) == 0) {
            snprintf(state->autologin_user, sizeof(state->autologin_user), "%s", line + 16);
        } else if (strncmp(line, "autologin_realm=", 17) == 0) {
            snprintf(state->autologin_realm, sizeof(state->autologin_realm), "%s", line + 17);
        }
    }

    fclose(file);
    state->autologin_password_stored = ahc_credential_wow_password_exists(state->config_dir);
}

static bool ahc_line_contains_ci(const char *hay, const char *needle)
{
    if (!hay || !needle) {
        return false;
    }
    for (const char *h = hay; *h; h++) {
        const char *a = h;
        const char *b = needle;
        while (*a && *b) {
            unsigned char ca = (unsigned char)*a;
            unsigned char cb = (unsigned char)*b;
            if (ca >= (unsigned char)'A' && ca <= (unsigned char)'Z') {
                ca = (unsigned char)(ca - (unsigned char)'A' + (unsigned char)'a');
            }
            if (cb >= (unsigned char)'A' && cb <= (unsigned char)'Z') {
                cb = (unsigned char)(cb - (unsigned char)'A' + (unsigned char)'a');
            }
            if (ca != cb) {
                break;
            }
            a++;
            b++;
        }
        if (!*b) {
            return true;
        }
    }
    return false;
}

static bool save_settings(CompanionState *state)
{
    AHC_MKDIR(state->config_dir);
    if (state->awesome_wotlk_autologin && state->launch_parameters[0]) {
        if (ahc_line_contains_ci(state->launch_parameters, "-password")
            || ahc_line_contains_ci(state->launch_parameters, "-login")) {
            state->launch_parameters[0] = '\0';
            set_action_status(
                state,
                "Cleared launch parameters: do not use -password or -login here when autologin is on.",
                "Launch parameters adjusted");
        }
    }

    FILE *file = fopen(state->config_path, "wb");
    if (!file) {
        set_status(state, "Could not save settings.");
        return false;
    }

    fprintf(file, "synastria_path=%s\n", state->synastria_path);
    fprintf(file, "launch_parameters=%s\n", state->launch_parameters);
    fprintf(file, "ui_scale=%d\n", state->ui_scale_index);
    fprintf(file, "awesome_wotlk_autologin=%d\n", state->awesome_wotlk_autologin ? 1 : 0);
    fprintf(file, "autologin_user=%s\n", state->autologin_user);
    fprintf(file, "autologin_realm=%s\n", state->autologin_realm);
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

#if defined(__linux__)
static bool ahc_linux_exe_directory(char *out, size_t out_size)
{
    char linkpath[AHC_PATH_CAPACITY];
    ssize_t n = readlink("/proc/self/exe", linkpath, sizeof(linkpath) - 1);
    if (n < 0) {
        return false;
    }
    linkpath[n] = '\0';
    const char *slash = strrchr(linkpath, '/');
    if (!slash) {
        return false;
    }
    size_t len = (size_t)(slash - linkpath);
    if (len + 1 > out_size) {
        return false;
    }
    memcpy(out, linkpath, len);
    out[len] = '\0';
    return true;
}
#endif

#if defined(_WIN32)
static bool ahc_windows_exe_directory(char *out, size_t out_size)
{
    char path[AHC_PATH_CAPACITY];
    unsigned long n = GetModuleFileNameA(NULL, path, (unsigned long)sizeof(path));
    if (n == 0u || n >= (unsigned long)sizeof(path)) {
        return false;
    }
    const char *slash = strrchr(path, '\\');
    if (slash == NULL) {
        slash = strrchr(path, '/');
    }
    if (slash == NULL) {
        return false;
    }
    size_t len = (size_t)(slash - path);
    if (len + 1u > out_size) {
        return false;
    }
    memcpy(out, path, len);
    out[len] = '\0';
    return true;
}
#endif

static void ahc_init_exe_dir_once(void)
{
    static bool done;
    if (done) {
        return;
    }
    done = true;
#if defined(_WIN32)
    s_have_exe_dir = ahc_windows_exe_directory(s_exe_dir, sizeof(s_exe_dir));
#elif defined(__linux__)
    s_have_exe_dir = ahc_linux_exe_directory(s_exe_dir, sizeof(s_exe_dir));
#else
    s_have_exe_dir = false;
#endif
}

static void load_addon_catalog(CompanionState *state)
{
    ahc_init_exe_dir_once();
    state->addons = ahc_addon_catalog_items();
    state->addon_count = ahc_addon_catalog_count();
    state->addon_manifest_loaded = false;
    snprintf(state->addon_catalog_source, sizeof(state->addon_catalog_source), "baked manifest catalog");

    if (s_have_exe_dir) {
        char exe_manifest[AHC_PATH_CAPACITY];
        path_join(exe_manifest, sizeof(exe_manifest), s_exe_dir, "manifest/addons.json");
        if (try_load_addon_manifest(state, exe_manifest)) {
            return;
        }
    }

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
    return strcmp(a->date, b->date);
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
        if (strcmp(existing->date, snapshot->date) == 0) {
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
        { true, "2026-03-29", 9860, 118, 44, 380 },
        { true, "2026-03-30", 9888, 125, 46, 404 },
        { true, "2026-03-31", 9916, 131, 48, 431 },
        { true, "2026-04-01", 9963, 146, 52, 468 },
        { true, "2026-04-03", 10127, 189, 61, 580 },
        { true, "2026-04-04", 10127, 189, 61, 580 },
        { true, "2026-04-06", 10184, 201, 64, 622 },
        { true, "2026-04-07", 10220, 212, 67, 644 },
        { true, "2026-04-10", 10310, 240, 72, 701 },
        { true, "2026-04-11", 10310, 240, 72, 701 },
        { true, "2026-04-12", 10372, 260, 77, 738 }
    };

    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
        upsert_history_snapshot(state, &samples[i]);
    }

    state->snapshot = samples[(sizeof(samples) / sizeof(samples[0])) - 1];
    set_action_status(state, "Loaded old-days test history with gaps and account gains.", "Seeded attune history");
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

#if !defined(_WIN32)
#define AHC_PATH_ENV_STRLEN 8192

static bool ahc_posix_is_executable(const char *p)
{
    return p && p[0] && FileExists(p) && access(p, X_OK) == 0;
}

static bool ahc_posix_find_in_path(const char *name, char *out, size_t out_size)
{
    if (!name || !name[0] || out_size < 1) {
        return false;
    }
    out[0] = '\0';
    const char *path_env = getenv("PATH");
    if (!path_env) {
        return false;
    }
    size_t len = strlen(path_env);
    if (len >= AHC_PATH_ENV_STRLEN) {
        return false;
    }
    char path_buf[AHC_PATH_ENV_STRLEN];
    memcpy(path_buf, path_env, len + 1);
    for (char *at = path_buf; at && *at; ) {
        char *next = strchr(at, ':');
        if (next) {
            *next = '\0';
        }
        if (at[0]) {
            path_join(out, out_size, at, name);
            if (ahc_posix_is_executable(out)) {
                return true;
            }
        }
        at = next ? next + 1 : NULL;
    }
    out[0] = '\0';
    return false;
}

static bool ahc_posix_find_proton_under_steam_common(const char *common_root, char *out, size_t out_size)
{
    if (!common_root[0] || !DirectoryExists(common_root)) {
        return false;
    }
    const char *priority[] = { "Proton - Experimental", "Proton Hotfix", NULL };
    for (int i = 0; priority[i]; i++) {
        char sub[AHC_PATH_CAPACITY];
        path_join(sub, sizeof(sub), common_root, priority[i]);
        if (!DirectoryExists(sub)) {
            continue;
        }
        path_join(out, out_size, sub, "proton");
        if (ahc_posix_is_executable(out)) {
            return true;
        }
    }

    FilePathList entries = LoadDirectoryFiles(common_root);
    bool found = false;
    for (unsigned int i = 0; i < entries.count && !found; i++) {
        const char *sub = entries.paths[i];
        if (!DirectoryExists(sub)) {
            continue;
        }
        const char *folder = GetFileName(sub);
        if (strncmp(folder, "Proton", 6) != 0) {
            continue;
        }
        path_join(out, out_size, sub, "proton");
        if (ahc_posix_is_executable(out)) {
            found = true;
        }
    }
    UnloadDirectoryFiles(entries);
    if (!found) {
        out[0] = '\0';
    }
    return found;
}

static bool ahc_posix_find_steam_proton(char *out, size_t out_size)
{
    const char *home = getenv("HOME");
    if (!home || !home[0]) {
        return false;
    }
    const char *suffixes[] = {
        ".local/share/Steam/steamapps/common",
        ".steam/steam/steamapps/common",
        ".var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/common",
        NULL
    };
    for (int i = 0; suffixes[i]; i++) {
        char base[AHC_PATH_CAPACITY];
        path_join(base, sizeof(base), home, suffixes[i]);
        if (ahc_posix_find_proton_under_steam_common(base, out, out_size)) {
            return true;
        }
    }
    return false;
}

static bool ahc_posix_env_executable_path(const char *e, char *out, size_t out_size)
{
    if (!e || !e[0]) {
        return false;
    }
    if (e[0] == '/' || (e[0] == '.' && e[1] == '/')) {
        if (ahc_posix_is_executable(e)) {
            snprintf(out, out_size, "%s", e);
            return true;
        }
        return false;
    }
    return ahc_posix_find_in_path(e, out, out_size);
}

static bool ahc_posix_resolve_wine_proton(char *runner, size_t runner_size, int *use_proton_run)
{
    char buffer[AHC_PATH_CAPACITY];
    const char *proton_a = getenv("AHC_PROTON");
    const char *proton_b = getenv("PROTON_PATH");
    if (proton_a && ahc_posix_env_executable_path(proton_a, buffer, sizeof(buffer))) {
        snprintf(runner, runner_size, "%s", buffer);
        *use_proton_run = 1;
        return true;
    }
    if (proton_b && ahc_posix_env_executable_path(proton_b, buffer, sizeof(buffer))) {
        snprintf(runner, runner_size, "%s", buffer);
        *use_proton_run = 1;
        return true;
    }
    const char *w = getenv("WINE");
    if (w && w[0] && ahc_posix_env_executable_path(w, buffer, sizeof(buffer))) {
        snprintf(runner, runner_size, "%s", buffer);
        *use_proton_run = 0;
        return true;
    }
    if (ahc_posix_find_in_path("wine", buffer, sizeof(buffer))) {
        snprintf(runner, runner_size, "%s", buffer);
        *use_proton_run = 0;
        return true;
    }
    if (ahc_posix_find_steam_proton(buffer, sizeof(buffer))) {
        snprintf(runner, runner_size, "%s", buffer);
        *use_proton_run = 1;
        return true;
    }
    if (ahc_posix_find_in_path("proton", buffer, sizeof(buffer))) {
        snprintf(runner, runner_size, "%s", buffer);
        *use_proton_run = 1;
        return true;
    }
    *use_proton_run = 0;
    runner[0] = '\0';
    return false;
}

static bool ahc_posix_wine_or_proton_ready(void)
{
    char runner[AHC_PATH_CAPACITY];
    int proton;
    return ahc_posix_resolve_wine_proton(runner, sizeof(runner), &proton);
}

static bool ahc_posix_build_game_launch(
    const char *workdir,
    const char *exe_path,
    const char *args,
    char *cmd,
    size_t cmd_size
)
{
    int proton_mode = 0;
    char runner[AHC_PATH_CAPACITY];
    if (!ahc_posix_resolve_wine_proton(runner, sizeof(runner), &proton_mode)) {
        return false;
    }
    if (proton_mode) {
        if (args[0]) {
            return (size_t)snprintf(
                       cmd,
                       cmd_size,
                       "cd \"%s\" && nohup \"%s\" run \"%s\" %s >/dev/null 2>&1 &",
                       workdir,
                       runner,
                       exe_path,
                       args) < cmd_size;
        }
        return (size_t)snprintf(
                   cmd,
                   cmd_size,
                   "cd \"%s\" && nohup \"%s\" run \"%s\" >/dev/null 2>&1 &",
                   workdir,
                   runner,
                   exe_path) < cmd_size;
    }
    if (args[0]) {
        return (size_t)snprintf(
                   cmd,
                   cmd_size,
                   "cd \"%s\" && nohup \"%s\" \"%s\" %s >/dev/null 2>&1 &",
                   workdir,
                   runner,
                   exe_path,
                   args) < cmd_size;
    }
    return (size_t)snprintf(
               cmd,
               cmd_size,
               "cd \"%s\" && nohup \"%s\" \"%s\" >/dev/null 2>&1 &",
               workdir,
               runner,
               exe_path) < cmd_size;
}
#endif

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
        set_action_status(state, "Set a valid Synastria folder first.", "Snapshot scan blocked");
        return false;
    }

    set_process_action(state, "Scanning AttuneHelper snapshot");

    char account_root[AHC_PATH_CAPACITY];
    path_join(account_root, sizeof(account_root), state->synastria_path, "WTF/Account");

    if (!DirectoryExists(account_root)) {
        set_action_status(state, "No WTF/Account folder found.", "Snapshot scan failed");
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
        state->snapshot_mod_time = GetFileModTime(state->snapshot_path);
        state->next_snapshot_poll_time = app_time() + AHC_SNAPSHOT_POLL_SECONDS;
        char message[AHC_STATUS_CAPACITY];
        snprintf(message, sizeof(message), "Loaded AttuneHelper snapshot for %s.", state->snapshot.date);
        set_action_status(state, message, "Snapshot loaded");
        return true;
    }

    set_action_status(state, "AttuneHelper.lua was not found under WTF/Account.", "Snapshot scan failed");
    return false;
}

static void poll_attunehelper_snapshot(CompanionState *state)
{
    double now = app_time();
    if (now < state->next_snapshot_poll_time) {
        return;
    }

    state->next_snapshot_poll_time = now + AHC_SNAPSHOT_POLL_SECONDS;
    if (!validate_synastria_path(state->synastria_path)) {
        return;
    }

    if (!state->snapshot_path[0] || !FileExists(state->snapshot_path)) {
        scan_attunehelper_snapshot(state);
        return;
    }

    long mod_time = GetFileModTime(state->snapshot_path);
    if (mod_time <= 0 || mod_time == state->snapshot_mod_time) {
        return;
    }

    AhcDailyAttuneSnapshot snapshot;
    if (!ahc_parse_daily_attune_snapshot_file(state->snapshot_path, &snapshot)) {
        set_action_status(state, "AttuneHelper.lua changed but could not be parsed.", "Snapshot refresh failed");
        state->snapshot_mod_time = mod_time;
        return;
    }

    state->snapshot = snapshot;
    state->snapshot_mod_time = mod_time;
    upsert_history_snapshot(state, &state->snapshot);

    char message[AHC_STATUS_CAPACITY];
    snprintf(message, sizeof(message), "Updated chart from AttuneHelper snapshot for %s.", state->snapshot.date);
    set_action_status(state, message, "Snapshot refreshed");
}

static void reset_wow_config_fields(CompanionState *state)
{
    snprintf(state->wow_width, sizeof(state->wow_width), "1920");
    snprintf(state->wow_height, sizeof(state->wow_height), "1080");
    snprintf(state->wow_refresh, sizeof(state->wow_refresh), "60");
    snprintf(state->wow_vsync, sizeof(state->wow_vsync), "0");
    snprintf(state->wow_multisampling, sizeof(state->wow_multisampling), "1");
    snprintf(state->wow_texture_resolution, sizeof(state->wow_texture_resolution), "0");
    snprintf(state->wow_environment_detail, sizeof(state->wow_environment_detail), "1");
    snprintf(state->wow_ground_effect_density, sizeof(state->wow_ground_effect_density), "16");
    state->wow_windowed = true;
    state->wow_borderless = true;
    state->wow_config_loaded = false;
    apply_monitor_defaults(state);
}

static void apply_monitor_defaults(CompanionState *state)
{
    if (!IsWindowReady()) {
        return;
    }

    int monitor = GetCurrentMonitor();
    int monitor_count = GetMonitorCount();
    if (monitor < 0 || monitor >= monitor_count) {
        monitor = 0;
    }
    if (monitor < 0 || monitor_count <= 0) {
        return;
    }

    int width = GetMonitorWidth(monitor);
    int height = GetMonitorHeight(monitor);
    int refresh = GetMonitorRefreshRate(monitor);
    const char *name = GetMonitorName(monitor);

    if (width > 0 && height > 0) {
        snprintf(state->wow_width, sizeof(state->wow_width), "%d", width);
        snprintf(state->wow_height, sizeof(state->wow_height), "%d", height);
    }
    if (refresh > 0) {
        snprintf(state->wow_refresh, sizeof(state->wow_refresh), "%d", refresh);
    }
    snprintf(state->monitor_name, sizeof(state->monitor_name), "%s", name && name[0] ? name : "Current monitor");
    state->wow_windowed = true;
    state->wow_borderless = true;
}

static bool parse_wow_config_set(const char *line, char *key, size_t key_capacity, char *value, size_t value_capacity)
{
    char parsed_key[64] = { 0 };
    char parsed_value[128] = { 0 };
    if (sscanf(line, " SET %63s \"%127[^\"]\"", parsed_key, parsed_value) != 2) {
        return false;
    }

    snprintf(key, key_capacity, "%s", parsed_key);
    snprintf(value, value_capacity, "%s", parsed_value);
    return true;
}

static void apply_wow_config_value(CompanionState *state, const char *key, const char *value)
{
    if (strcmp(key, "gxResolution") == 0) {
        char width[16] = { 0 };
        char height[16] = { 0 };
        if (sscanf(value, "%15[^x]x%15s", width, height) == 2) {
            snprintf(state->wow_width, sizeof(state->wow_width), "%s", width);
            snprintf(state->wow_height, sizeof(state->wow_height), "%s", height);
        }
    } else if (strcmp(key, "gxRefresh") == 0) {
        snprintf(state->wow_refresh, sizeof(state->wow_refresh), "%s", value);
    } else if (strcmp(key, "gxWindow") == 0) {
        state->wow_windowed = strcmp(value, "0") != 0;
    } else if (strcmp(key, "gxMaximize") == 0) {
        state->wow_borderless = strcmp(value, "0") != 0;
    } else if (strcmp(key, "gxVSync") == 0) {
        snprintf(state->wow_vsync, sizeof(state->wow_vsync), "%s", value);
    } else if (strcmp(key, "gxMultisample") == 0) {
        snprintf(state->wow_multisampling, sizeof(state->wow_multisampling), "%s", value);
    } else if (strcmp(key, "terrainMipLevel") == 0) {
        snprintf(state->wow_texture_resolution, sizeof(state->wow_texture_resolution), "%s", value);
    } else if (strcmp(key, "environmentDetail") == 0) {
        snprintf(state->wow_environment_detail, sizeof(state->wow_environment_detail), "%s", value);
    } else if (strcmp(key, "groundEffectDensity") == 0) {
        snprintf(state->wow_ground_effect_density, sizeof(state->wow_ground_effect_density), "%s", value);
    }
}

static bool load_wow_config(CompanionState *state)
{
    char config_path[AHC_PATH_CAPACITY];
    reset_wow_config_fields(state);

    if (!resolve_wow_config_path(state, config_path, sizeof(config_path)) || !FileExists(config_path)) {
        set_action_status(state, "Config.wtf was not found. Defaults are ready to save.", "Loaded config defaults");
        return false;
    }

    FILE *file = fopen(config_path, "rb");
    if (!file) {
        set_action_status(state, "Could not open Config.wtf.", "Config load failed");
        return false;
    }

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        char key[64];
        char value[128];
        if (parse_wow_config_set(line, key, sizeof(key), value, sizeof(value))) {
            apply_wow_config_value(state, key, value);
        }
    }

    fclose(file);
    state->wow_config_loaded = true;
    set_action_status(state, "Loaded Config.wtf display settings.", "Config loaded");
    return true;
}

static bool wow_config_key_index(const char *key, int *index)
{
    const char *keys[] = {
        "gxResolution",
        "gxRefresh",
        "gxWindow",
        "gxMaximize",
        "gxVSync",
        "gxMultisample",
        "terrainMipLevel",
        "environmentDetail",
        "groundEffectDensity"
    };

    for (int i = 0; i < (int)(sizeof(keys) / sizeof(keys[0])); i++) {
        if (strcmp(key, keys[i]) == 0) {
            *index = i;
            return true;
        }
    }

    return false;
}

static void wow_config_value_for_index(const CompanionState *state, int index, char *out, size_t out_capacity)
{
    switch (index) {
        case 0:
            snprintf(out, out_capacity, "%sx%s", state->wow_width, state->wow_height);
            break;
        case 1:
            snprintf(out, out_capacity, "%s", state->wow_refresh);
            break;
        case 2:
            snprintf(out, out_capacity, "%s", state->wow_windowed ? "1" : "0");
            break;
        case 3:
            snprintf(out, out_capacity, "%s", state->wow_borderless ? "1" : "0");
            break;
        case 4:
            snprintf(out, out_capacity, "%s", state->wow_vsync);
            break;
        case 5:
            snprintf(out, out_capacity, "%s", state->wow_multisampling);
            break;
        case 6:
            snprintf(out, out_capacity, "%s", state->wow_texture_resolution);
            break;
        case 7:
            snprintf(out, out_capacity, "%s", state->wow_environment_detail);
            break;
        case 8:
            snprintf(out, out_capacity, "%s", state->wow_ground_effect_density);
            break;
        default:
            out[0] = '\0';
            break;
    }
}

static void write_wow_config_line(FILE *file, int index, const char *value)
{
    const char *keys[] = {
        "gxResolution",
        "gxRefresh",
        "gxWindow",
        "gxMaximize",
        "gxVSync",
        "gxMultisample",
        "terrainMipLevel",
        "environmentDetail",
        "groundEffectDensity"
    };

    fprintf(file, "SET %s \"%s\"\n", keys[index], value);
}

static bool backup_wow_config_file(CompanionState *state, const char *config_path)
{
    if (!FileExists(config_path)) {
        return true;
    }

    char root[AHC_PATH_CAPACITY];
    if (!backup_child_root(state, "WTF", root, sizeof(root))) {
        return false;
    }

    char name[128];
    char destination[AHC_PATH_CAPACITY];
    snprintf(name, sizeof(name), "Config-%lld.wtf", (long long)time(NULL));
    path_join(destination, sizeof(destination), root, name);
    return copy_file_contents(config_path, destination);
}

static bool save_wow_config(CompanionState *state)
{
    if (!validate_synastria_path(state->synastria_path)) {
        set_action_status(state, "Set a valid Synastria folder before saving WoW settings.", "Config save blocked");
        return false;
    }

    char wtf_path[AHC_PATH_CAPACITY];
    char config_path[AHC_PATH_CAPACITY];
    char temp_path[AHC_PATH_CAPACITY];
    path_join(wtf_path, sizeof(wtf_path), state->synastria_path, "WTF");
    if (!ensure_directory(wtf_path)) {
        set_action_status(state, "Could not create WTF folder.", "Config save failed");
        return false;
    }
    path_join(config_path, sizeof(config_path), wtf_path, "Config.wtf");
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", config_path);

    if (!backup_wow_config_file(state, config_path)) {
        set_action_status(state, "Could not back up Config.wtf before saving.", "Config backup failed");
        return false;
    }

    FILE *input = fopen(config_path, "rb");
    FILE *output = fopen(temp_path, "wb");
    if (!output) {
        if (input) {
            fclose(input);
        }
        set_action_status(state, "Could not write Config.wtf.", "Config save failed");
        return false;
    }

    bool written[9] = { false };
    char line[512];
    if (input) {
        while (fgets(line, sizeof(line), input)) {
            char key[64];
            char value[128];
            int index = 0;
            if (parse_wow_config_set(line, key, sizeof(key), value, sizeof(value)) && wow_config_key_index(key, &index)) {
                char replacement[128];
                wow_config_value_for_index(state, index, replacement, sizeof(replacement));
                write_wow_config_line(output, index, replacement);
                written[index] = true;
            } else {
                fputs(line, output);
            }
        }
        fclose(input);
    }

    for (int i = 0; i < 9; i++) {
        if (!written[i]) {
            char value[128];
            wow_config_value_for_index(state, i, value, sizeof(value));
            write_wow_config_line(output, i, value);
        }
    }

    if (fclose(output) != 0) {
        remove(temp_path);
        set_action_status(state, "Could not finish writing Config.wtf.", "Config save failed");
        return false;
    }

    remove(config_path);
    if (rename(temp_path, config_path) != 0) {
        remove(temp_path);
        set_action_status(state, "Could not replace Config.wtf.", "Config save failed");
        return false;
    }

    state->wow_config_loaded = true;
    set_action_status(state, "Saved WoW display and quality settings.", "Config saved");
    return true;
}

static bool resolve_wowext_path(const CompanionState *state, char *out, size_t out_capacity)
{
    if (!validate_synastria_path(state->synastria_path)) {
        return false;
    }

    path_join(out, out_capacity, state->synastria_path, "WoWExt.exe");
    return FileExists(out);
}

static bool resolve_wow_exe_path(const CompanionState *state, char *out, size_t out_capacity)
{
    if (!validate_synastria_path(state->synastria_path)) {
        return false;
    }
    path_join(out, out_capacity, state->synastria_path, "Wow.exe");
    return FileExists(out);
}

static bool persist_autologin_settings(CompanionState *state)
{
    AHC_MKDIR(state->config_dir);
    if (!state->awesome_wotlk_autologin) {
        ahc_credential_clear_wow_password(state->config_dir);
        state->autologin_password_stored = false;
        state->autologin_password_edit[0] = '\0';
    } else {
        char u[AHC_WOW_TRUST_STR_MAX + 1], rlm[AHC_WOW_TRUST_STR_MAX + 1], pwv[AHC_CREDENTIAL_PASSWORD_MAX];
        const char *r_src = state->autologin_realm[0] ? state->autologin_realm : "AzerothCore";
        if (!ahc_wow_trust_sanitize_user(state->autologin_user, u, sizeof u) || !ahc_wow_trust_sanitize_realm(r_src, rlm, sizeof rlm)) {
            set_action_status(
                state,
                "Autologin: use a non-empty account and realm (alphanumeric, email, realm spaces allowed).",
                "Save failed");
            return false;
        }
        snprintf(state->autologin_user, sizeof state->autologin_user, "%s", u);
        snprintf(state->autologin_realm, sizeof state->autologin_realm, "%s", rlm);
        if (state->autologin_password_edit[0]) {
            if (!ahc_wow_trust_password(state->autologin_password_edit, pwv, sizeof pwv)) {
                set_action_status(state, "Autologin: password has disallowed control characters or is out of range.", "Save failed");
                return false;
            }
            if (!ahc_credential_store_wow_password(state->config_dir, pwv)) {
                memset(pwv, 0, sizeof pwv);
                set_action_status(state, "Could not store the password. Check the config folder is writable.", "Save failed");
                return false;
            }
            memset(pwv, 0, sizeof pwv);
            state->autologin_password_edit[0] = '\0';
            state->autologin_password_stored = true;
        }
    }
    if (!save_settings(state)) {
        return false;
    }
    if (state->awesome_wotlk_autologin) {
        state->autologin_password_stored = ahc_credential_wow_password_exists(state->config_dir);
    }
    set_action_status(
        state,
        "Autologin settings saved. The password is never written to settings.ini; Windows uses DPAPI, Linux uses a 0600 file.",
        "Autologin saved");
    return true;
}

static bool launch_game(CompanionState *state)
{
    if (!validate_synastria_path(state->synastria_path)) {
        set_action_status(state, "Set a valid Synastria folder before launching.", "Launch blocked");
        return false;
    }

    if (state->awesome_wotlk_autologin) {
        if (state->launch_parameters[0]
            && (ahc_line_contains_ci(state->launch_parameters, "-password")
                || ahc_line_contains_ci(state->launch_parameters, "-login"))) {
            set_action_status(
                state,
                "Remove -password and -login from launch parameters when autologin is on.",
                "Launch blocked");
            return false;
        }
    }

    save_settings(state);
    if (!save_wow_config(state)) {
        return false;
    }

    set_process_action(state, "Launching game");

    if (state->awesome_wotlk_autologin) {
        char wow_exe[AHC_PATH_CAPACITY];
        if (!resolve_wow_exe_path(state, wow_exe, sizeof(wow_exe))) {
            set_action_status(state, "Autologin requires Wow.exe in the Synastria folder.", "Launch blocked");
            return false;
        }
        char pass[AHC_CREDENTIAL_PASSWORD_MAX];
        if (!ahc_credential_load_wow_password(state->config_dir, pass, sizeof(pass))) {
            set_action_status(
                state,
                "Autologin: set account, realm, and password in Settings, then Save autologin.",
                "Launch blocked");
            return false;
        }
        char u[AHC_WOW_TRUST_STR_MAX + 1], rlm[AHC_WOW_TRUST_STR_MAX + 1], xtra[256] = { 0 };
        const char *r_src = state->autologin_realm[0] ? state->autologin_realm : "AzerothCore";
        if (!ahc_wow_trust_sanitize_user(state->autologin_user, u, sizeof u) || !ahc_wow_trust_sanitize_realm(r_src, rlm, sizeof rlm)
            || !ahc_wow_trust_sanitize_extra_arg(state->launch_parameters, xtra, sizeof xtra)) {
            memset(pass, 0, sizeof pass);
            set_action_status(
                state,
                "Autologin: check account, realm, and extra launch text (no -login/-password in that field).",
                "Launch failed");
            return false;
        }
#if defined(_WIN32)
        {
            wchar_t wcmd[32768];
            if (ahc_wow_win_build_cmdline(
                    wow_exe,
                    u,
                    pass,
                    rlm,
                    xtra[0] != '\0' ? xtra : NULL,
                    wcmd,
                    (int)(sizeof wcmd / sizeof wcmd[0]))
                != 0) {
                memset(pass, 0, sizeof pass);
                set_action_status(
                    state,
                    "Autologin: could not build command line (size or character rules).",
                    "Launch failed");
                return false;
            }
            memset(pass, 0, sizeof pass);
            if (!ahc_win_launch_wcmdline_in_dir(wcmd, state->synastria_path)) {
                set_action_status(state, "CreateProcessW failed to start Wow.exe (autologin).", "Launch failed");
                return false;
            }
        }
#else
        {
            char final_args[4096];
            if (!ahc_wow_posix_build_args(
                    u, pass, rlm, (xtra[0] != '\0') ? xtra : NULL, final_args, sizeof final_args)) {
                memset(pass, 0, sizeof pass);
                set_action_status(
                    state,
                    "Autologin: could not build launch arguments (too long or bad characters).",
                    "Launch failed");
                return false;
            }
            memset(pass, 0, sizeof pass);
            char command[AHC_SH_CMD_BYTES];
            if (!ahc_posix_build_game_launch(state->synastria_path, wow_exe, final_args, command, sizeof command)) {
                set_action_status(
                    state,
                    "Install Wine, set WINE, or set AHC_PROTON or PROTON_PATH to a Proton script from Steam.",
                    "Launch failed");
                return false;
            }
            if (!ahc_posix_run_detached_shell(command)) {
                set_action_status(state, "Could not launch Wow.exe via Wine/Proton (autologin).", "Launch failed");
                return false;
            }
        }
#endif
        set_action_status(state, "Launched Wow.exe with autologin (legacy client args).", "Game launched");
        return true;
    }

    char wowext_path[AHC_PATH_CAPACITY];
    if (!resolve_wowext_path(state, wowext_path, sizeof(wowext_path))) {
        set_action_status(
            state,
            "Set a valid Synastria folder (missing WoWExt.exe) or enable autologin if you only use Wow.exe.",
            "Launch blocked");
        return false;
    }

#if defined(_WIN32)
    if (!ahc_launch_file_hidden(wowext_path, state->launch_parameters, state->synastria_path)) {
        set_action_status(state, "Could not launch WoWExt.exe.", "Launch failed");
        return false;
    }
#else
    {
        char final_args[4096];
        int na = snprintf(final_args, sizeof final_args, "%s", state->launch_parameters);
        if (na < 0 || (size_t)na >= sizeof final_args) {
            set_action_status(state, "Launch parameters are too long.", "Launch failed");
            return false;
        }
        char command[AHC_SH_CMD_BYTES];
        if (!ahc_posix_build_game_launch(state->synastria_path, wowext_path, final_args, command, sizeof command)) {
            set_action_status(
                state,
                "Install Wine, set WINE, or set AHC_PROTON or PROTON_PATH to a Proton script from Steam.",
                "Launch failed");
            return false;
        }
        if (!ahc_posix_run_detached_shell(command)) {
            set_action_status(state, "Could not launch WoWExt.exe via Wine/Proton.", "Launch failed");
            return false;
        }
    }
#endif

    set_action_status(state, "Launched Synastria (WoWExt).", "Game launched");
    return true;
}

static void clear_companion_data(CompanionState *state)
{
    remove(state->config_path);
    remove(state->history_path);
    ahc_credential_clear_wow_password(state->config_dir);
    state->synastria_path[0] = '\0';
    state->snapshot_path[0] = '\0';
    state->launch_parameters[0] = '\0';
    state->awesome_wotlk_autologin = false;
    state->autologin_user[0] = '\0';
    state->autologin_realm[0] = '\0';
    state->autologin_password_edit[0] = '\0';
    state->autologin_password_stored = false;
    state->history_count = 0;
    state->snapshot_mod_time = 0;
    state->next_snapshot_poll_time = 0.0;
    memset(&state->snapshot, 0, sizeof(state->snapshot));
    reset_wow_config_fields(state);
    state->clear_data_confirming = false;
    companion_dispose_phone_qr(state);
    set_action_status(state, "Cleared local companion settings and attune history.", "Companion data cleared");
}

static bool companion_can_launch(const CompanionState *state)
{
    if (!validate_synastria_path(state->synastria_path)) {
        return false;
    }
#if !defined(_WIN32)
    if (!ahc_posix_wine_or_proton_ready()) {
        return false;
    }
#endif
    if (state->awesome_wotlk_autologin) {
        char wow_exe[AHC_PATH_CAPACITY];
        if (!resolve_wow_exe_path(state, wow_exe, sizeof(wow_exe))) {
            return false;
        }
        if (!ahc_credential_wow_password_exists(state->config_dir)) {
            return false;
        }
        char u[AHC_WOW_TRUST_STR_MAX + 1], rlm[AHC_WOW_TRUST_STR_MAX + 1];
        if (!ahc_wow_trust_sanitize_user(state->autologin_user, u, sizeof u)) {
            return false;
        }
        const char *r_src = state->autologin_realm[0] ? state->autologin_realm : "AzerothCore";
        if (!ahc_wow_trust_sanitize_realm(r_src, rlm, sizeof rlm)) {
            return false;
        }
        if (state->launch_parameters[0]
            && (ahc_line_contains_ci(state->launch_parameters, "-password")
                || ahc_line_contains_ci(state->launch_parameters, "-login"))) {
            return false;
        }
        return true;
    }
    char wowext_path[AHC_PATH_CAPACITY];
    return resolve_wowext_path(state, wowext_path, sizeof(wowext_path));
}

static void draw_card(Rectangle bounds, const char *title)
{
    DrawRectangleRounded((Rectangle){ bounds.x + 5.0f, bounds.y + 7.0f, bounds.width, bounds.height }, 0.05f, 10, (Color){ 0, 0, 0, 125 });
    DrawRectangleRounded(bounds, 0.035f, 8, AHC_PANEL);
    DrawRectangleRounded((Rectangle){ bounds.x + 2.0f, bounds.y + 2.0f, bounds.width - 4.0f, 42.0f }, 0.035f, 8, (Color){ 30, 45, 68, 255 });
    DrawRectangle((int)bounds.x + 12, (int)bounds.y + 43, (int)bounds.width - 24, 1, (Color){ 88, 124, 166, 190 });
    DrawRectangleRoundedLines(bounds, 0.035f, 8, AHC_BORDER);
    DrawRectangleRoundedLines((Rectangle){ bounds.x + 3.0f, bounds.y + 3.0f, bounds.width - 6.0f, bounds.height - 6.0f }, 0.03f, 8, (Color){ 91, 124, 161, 120 });
    draw_text(title, (int)bounds.x + 18, (int)bounds.y + 13, 20, (Color){ 198, 224, 255, 255 });
}

static bool draw_tab_button(const char *label, Rectangle bounds, bool active)
{
    return draw_wow_button(label, bounds, active, si(15));
}

static void draw_window_chrome(CompanionState *state)
{
    (void)state;
    int sw = GetScreenWidth();
    int h = AHC_CHROME_H;
    Vector2 mouse = GetMousePosition();
    DrawRectangle(0, 0, sw, h, (Color){ 12, 18, 32, 255 });
    DrawRectangle(0, h - 1, sw, 1, AHC_BORDER);
    draw_text("Attune Helper Companion", 12, 8, 16, AHC_MUTED);

    Rectangle rmin = { (float)sw - 148.0f, 4.0f, 40.0f, 28.0f };
    Rectangle rfull = { (float)sw - 100.0f, 4.0f, 40.0f, 28.0f };
    Rectangle rclose = { (float)sw - 52.0f, 4.0f, 40.0f, 28.0f };
    Vector2 chrome_mouse = GetMousePosition();
    if (CheckCollisionPointRec(chrome_mouse, rmin)) {
        DrawRectangleRoundedLinesEx(rmin, 0.2f, 6, 2.0f, (Color){ 150, 200, 255, 200 });
    }
    if (CheckCollisionPointRec(chrome_mouse, rfull)) {
        DrawRectangleRoundedLinesEx(rfull, 0.2f, 6, 2.0f, (Color){ 150, 200, 255, 200 });
    }
    if (CheckCollisionPointRec(chrome_mouse, rclose)) {
        DrawRectangleRoundedLinesEx(rclose, 0.2f, 6, 2.0f, (Color){ 150, 200, 255, 200 });
    }
    bool press_min = draw_wow_button("_", rmin, false, 16);
    bool press_full = draw_wow_button(IsWindowFullscreen() ? "[]-" : "[]", rfull, IsWindowFullscreen(), 15);
    bool press_close = draw_wow_button("X", rclose, false, 16);
    if (press_min) {
        MinimizeWindow();
    } else if (press_full) {
        ToggleFullscreen();
    } else if (press_close) {
        ahc_tray_request_background();
    } else {
        Rectangle rdrag = { 0, 0, (float)(sw - 156), (float)h };
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, rdrag)
            && !CheckCollisionPointRec(mouse, rmin) && !CheckCollisionPointRec(mouse, rfull) && !CheckCollisionPointRec(mouse, rclose)) {
            g_chrome_drag = true;
            g_chrome_grab = mouse;
        }
    }
    if (g_chrome_drag) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsWindowFullscreen()) {
            Vector2 w = GetWindowPosition();
            Vector2 m = GetMousePosition();
            float sx = w.x + m.x;
            float sy = w.y + m.y;
            SetWindowPosition((int)(sx - g_chrome_grab.x), (int)(sy - g_chrome_grab.y));
        } else {
            g_chrome_drag = false;
        }
    }
}

static void draw_header(CompanionState *state)
{
    int c = AHC_CHROME_H;
    DrawRectangleGradientH(0, c, GetScreenWidth(), 72, (Color){ 18, 31, 51, 255 }, (Color){ 8, 16, 31, 255 });
    DrawCircle(GetScreenWidth() - 124, 28 + c, 72, (Color){ 56, 118, 184, 36 });
    DrawRectangle(0, 70 + c, GetScreenWidth(), 2, (Color){ 88, 124, 166, 190 });

    draw_text("Addon installs, updates, and attune history", 26, 12 + c, 22, AHC_TEXT);

    if (state->synastria_path[0]) {
        char path_line[AHC_PATH_CAPACITY + 32];
        snprintf(path_line, sizeof(path_line), "Synastria: %s", state->synastria_path);
        draw_text(path_line, 28, 42 + c, 15, validate_synastria_path(state->synastria_path) ? (Color){ 180, 230, 205, 255 } : AHC_STATUS_WARNING);
    } else {
        draw_text("Setup needed: open Settings and paste your Synastria folder path.", 28, 42 + c, 15, AHC_STATUS_WARNING);
    }

    Rectangle launch_button = { (float)GetScreenWidth() - 188.0f, 14.0f + (float)c, 158.0f, 42.0f };
    bool can_launch = companion_can_launch(state);
    if (draw_wow_button("Play Game", launch_button, can_launch, 20)) {
        launch_game(state);
    }
}

static void draw_tabs(CompanionState *state)
{
    const float y = (float)AHC_CHROME_H + sf(8.0f);
    const float x = sf(380.0f);
    const float width = sf(128.0f);
    const float height = sf(34.0f);
    const float gap = sf(10.0f);
    if (draw_tab_button("Attune chart", (Rectangle){ x, y, width, height }, state->tab == COMPANION_TAB_ATTUNES)) {
        state->tab = COMPANION_TAB_ATTUNES;
    }
    if (draw_tab_button("Addons", (Rectangle){ x + width + gap, y, width, height }, state->tab == COMPANION_TAB_ADDONS)) {
        state->tab = COMPANION_TAB_ADDONS;
    }
    if (draw_tab_button("Settings", (Rectangle){ x + (width + gap) * 2.0f, y, width, height }, state->tab == COMPANION_TAB_SETTINGS)) {
        state->tab = COMPANION_TAB_SETTINGS;
    }
}

static size_t build_display_history(const CompanionState *state, DisplaySnapshot *out, size_t out_capacity);
static int daily_metric_value(const DisplaySnapshot *series, size_t count, size_t index, GraphMetric metric);

static void format_count_with_delta(char *out, size_t out_capacity, const char *label, int value, int delta)
{
    char value_text[32];
    char delta_text[32];
    format_int_with_commas(value, value_text, sizeof(value_text));
    format_int_with_commas(delta, delta_text, sizeof(delta_text));
    if (delta > 0) {
        snprintf(out, out_capacity, "%s  %s (+%s)", label, value_text, delta_text);
        return;
    }

    snprintf(out, out_capacity, "%s  %s", label, value_text);
}

static void format_tooltip_count_with_delta(char *out, size_t out_capacity, const char *label, int value, int delta)
{
    char value_text[32];
    char delta_text[32];
    format_int_with_commas(value, value_text, sizeof(value_text));
    format_int_with_commas(delta, delta_text, sizeof(delta_text));
    if (delta > 0) {
        snprintf(out, out_capacity, "%s: %s (+%s)", label, value_text, delta_text);
        return;
    }

    snprintf(out, out_capacity, "%s: %s", label, value_text);
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

    char account_text[32];
    format_int_with_commas(state->snapshot.account, account_text, sizeof(account_text));
    snprintf(line, sizeof(line), "Account %s", account_text);
    draw_text(line, (int)bounds.x + 20, (int)bounds.y + 98, 18, AHC_MUTED);

    DisplaySnapshot display_history[AHC_DISPLAY_HISTORY_CAPACITY];
    size_t display_count = build_display_history(state, display_history, AHC_DISPLAY_HISTORY_CAPACITY);
    size_t snapshot_index = display_count;
    for (size_t i = 0; i < display_count; i++) {
        if (!display_history[i].synthetic && strcmp(display_history[i].snapshot.date, state->snapshot.date) == 0) {
            snapshot_index = i;
        }
    }

    int tf_delta = snapshot_index < display_count ? daily_metric_value(display_history, display_count, snapshot_index, GRAPH_METRIC_TITANFORGED) : 0;
    int wf_delta = snapshot_index < display_count ? daily_metric_value(display_history, display_count, snapshot_index, GRAPH_METRIC_WARFORGED) : 0;
    int lf_delta = snapshot_index < display_count ? daily_metric_value(display_history, display_count, snapshot_index, GRAPH_METRIC_LIGHTFORGED) : 0;

    format_count_with_delta(line, sizeof(line), "TF", state->snapshot.titanforged, tf_delta);
    draw_text(line, (int)bounds.x + 20, (int)bounds.y + 146, 24, AHC_TF);

    format_count_with_delta(line, sizeof(line), "WF", state->snapshot.warforged, wf_delta);
    draw_text(line, (int)bounds.x + 20, (int)bounds.y + 182, 24, AHC_WF);

    format_count_with_delta(line, sizeof(line), "LF", state->snapshot.lightforged, lf_delta);
    draw_text(line, (int)bounds.x + 20, (int)bounds.y + 218, 24, AHC_LF);
}

static int snapshot_metric_value(const AhcDailyAttuneSnapshot *snapshot, GraphMetric metric)
{
    switch (metric) {
        case GRAPH_METRIC_ACCOUNT:
            return snapshot->account;
        case GRAPH_METRIC_WARFORGED:
            return snapshot->warforged;
        case GRAPH_METRIC_LIGHTFORGED:
            return snapshot->lightforged;
        case GRAPH_METRIC_TITANFORGED:
            return snapshot->titanforged;
        default:
            return snapshot->account;
    }
}

static const char *graph_metric_label(GraphMetric metric)
{
    switch (metric) {
        case GRAPH_METRIC_ACCOUNT:
            return "Account / day";
        case GRAPH_METRIC_WARFORGED:
            return "WF / day";
        case GRAPH_METRIC_LIGHTFORGED:
            return "LF / day";
        case GRAPH_METRIC_TITANFORGED:
            return "TF / day";
        default:
            return "Account / day";
    }
}

static const char *graph_metric_gain_label(GraphMetric metric)
{
    switch (metric) {
        case GRAPH_METRIC_ACCOUNT:
            return "New account attunes";
        case GRAPH_METRIC_WARFORGED:
            return "New WF";
        case GRAPH_METRIC_LIGHTFORGED:
            return "New LF";
        case GRAPH_METRIC_TITANFORGED:
            return "New TF";
        default:
            return "New account attunes";
    }
}

static Color graph_metric_color(GraphMetric metric)
{
    switch (metric) {
        case GRAPH_METRIC_ACCOUNT:
            return AHC_ACCENT;
        case GRAPH_METRIC_WARFORGED:
            return AHC_WF;
        case GRAPH_METRIC_LIGHTFORGED:
            return AHC_LF;
        case GRAPH_METRIC_TITANFORGED:
            return AHC_TF;
        default:
            return AHC_ACCENT;
    }
}

static size_t build_display_history(const CompanionState *state, DisplaySnapshot *out, size_t out_capacity)
{
    size_t out_count = 0;
    for (size_t i = 0; i < state->history_count && out_count < out_capacity; i++) {
        if (out_count == 0) {
            out[out_count++] = (DisplaySnapshot){ state->history[i], false };
            continue;
        }

        int previous_ordinal = 0;
        int current_ordinal = 0;
        if (date_to_ordinal(out[out_count - 1].snapshot.date, &previous_ordinal) && date_to_ordinal(state->history[i].date, &current_ordinal)) {
            for (int day = previous_ordinal + 1; day < current_ordinal && out_count < out_capacity; day++) {
                DisplaySnapshot synthetic = { out[out_count - 1].snapshot, true };
                ordinal_to_date(day, synthetic.snapshot.date, sizeof(synthetic.snapshot.date));
                out[out_count++] = synthetic;
            }
        }

        if (out_count < out_capacity) {
            out[out_count++] = (DisplaySnapshot){ state->history[i], false };
        }
    }

    return out_count;
}

static int daily_metric_value(const DisplaySnapshot *series, size_t count, size_t index, GraphMetric metric)
{
    if (index == 0 || index >= count) {
        return 0;
    }

    const AhcDailyAttuneSnapshot *previous_snapshot = &series[index - 1].snapshot;
    const AhcDailyAttuneSnapshot *current_snapshot = &series[index].snapshot;
    int previous = snapshot_metric_value(previous_snapshot, metric);
    int current = snapshot_metric_value(current_snapshot, metric);
    return current > previous ? current - previous : 0;
}

static double account_attunes_per_tracked_day(const DisplaySnapshot *series, size_t count)
{
    if (count < 2) {
        return 0.0;
    }

    int first = snapshot_metric_value(&series[0].snapshot, GRAPH_METRIC_ACCOUNT);
    int last = snapshot_metric_value(&series[count - 1].snapshot, GRAPH_METRIC_ACCOUNT);
    int first_ordinal = 0;
    int last_ordinal = 0;
    if (last <= first || !date_to_ordinal(series[0].snapshot.date, &first_ordinal) || !date_to_ordinal(series[count - 1].snapshot.date, &last_ordinal)) {
        return 0.0;
    }

    int tracked_days = last_ordinal - first_ordinal;
    if (tracked_days < 1) {
        tracked_days = 1;
    }

    return (double)(last - first) / (double)tracked_days;
}

static bool draw_metric_chip(const char *label, Rectangle bounds, bool active)
{
    return draw_wow_button(label, bounds, active, 15);
}

static void draw_graph_card(CompanionState *state, Rectangle bounds)
{
    draw_card(bounds, "Attune chart");

    const float chip_gap = 8.0f;
    const float total_chip_width = 92.0f + 52.0f + 52.0f + 52.0f + chip_gap * 3.0f;
    float chip_x = bounds.x + bounds.width - total_chip_width - 18.0f;
    float chip_y = bounds.y + 8.0f;
    if (chip_x < bounds.x + 250.0f) {
        chip_x = bounds.x + 18.0f;
        chip_y = bounds.y + 52.0f;
    }

    if (draw_metric_chip("Account", (Rectangle){ chip_x, chip_y, 92.0f, 32.0f }, state->graph_metric == GRAPH_METRIC_ACCOUNT)) {
        state->graph_metric = GRAPH_METRIC_ACCOUNT;
    }
    chip_x += 92.0f + chip_gap;
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

    DisplaySnapshot display_history[AHC_DISPLAY_HISTORY_CAPACITY];
    size_t display_count = build_display_history(state, display_history, AHC_DISPLAY_HISTORY_CAPACITY);

    if (display_count == 0) {
        draw_text("Scan AttuneHelper to save daily points here.", plot_left + 12, plot_top + 42, 18, AHC_MUTED);
        draw_text("Hover a spike to see TF / WF / LF for that date.", plot_left + 12, plot_top + 72, 16, AHC_MUTED);
        return;
    }

    int max_delta = 1;
    for (size_t i = 0; i < display_count; i++) {
        int delta = daily_metric_value(display_history, display_count, i, state->graph_metric);
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
        display_history[0].snapshot.date,
        display_history[display_count - 1].snapshot.date);
    draw_text(axis_label, plot_left, plot_top - 24, 14, AHC_MUTED);

    for (int grid = 1; grid <= 3; grid++) {
        int y = base_y - (plot_height * grid) / 4;
        DrawLine(plot_left + 1, y, plot_right, y, (Color){ 122, 152, 190, 70 });
    }

    float slot_width = (float)(plot_right - plot_left) / (float)display_count;
    float bar_width = slot_width * 0.46f;
    if (bar_width < 6.0f) {
        bar_width = 6.0f;
    }
    if (bar_width > 30.0f) {
        bar_width = 30.0f;
    }

    int label_step = (int)(display_count / 6) + 1;
    Vector2 previous_point = { 0.0f, 0.0f };
    bool has_previous_point = false;
    for (size_t i = 0; i < display_count; i++) {
        float x = (float)plot_left + slot_width * (float)i + slot_width * 0.5f;
        int delta = daily_metric_value(display_history, display_count, i, state->graph_metric);
        float bar_height = delta > 0 ? ((float)delta / (float)max_delta) * (float)(plot_height - 12) : 3.0f;
        float point_y = (float)base_y - ((float)delta / (float)max_delta) * (float)(plot_height - 12);
        Vector2 point = { x, point_y };
        Rectangle hit = { x - slot_width * 0.5f, (float)plot_top, slot_width, (float)plot_height + 10.0f };
        Rectangle bar = { x - bar_width * 0.5f, (float)base_y - bar_height, bar_width, bar_height };
        bool hovered = CheckCollisionPointRec(mouse, hit);

        if (hovered) {
            hovered_index = (int)i;
            DrawRectangleRounded((Rectangle){ hit.x + 2.0f, hit.y, hit.width - 4.0f, hit.height }, 0.12f, 6, (Color){ 83, 167, 239, 34 });
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
        if ((int)i % label_step == 0 || i + 1 == display_count) {
            draw_text(display_history[i].snapshot.date + 5, (int)(x - 16.0f), base_y + 12, 12, AHC_MUTED);
        }
    }

    char average_line[160];
    snprintf(average_line, sizeof(average_line), "Account attunes/day since tracking: %.1f", account_attunes_per_tracked_day(display_history, display_count));
    draw_text(average_line, plot_left, (int)(bounds.y + bounds.height) - 30, 16, AHC_MUTED);

    if (hovered_index >= 0) {
        const DisplaySnapshot *hovered_display = &display_history[hovered_index];
        const AhcDailyAttuneSnapshot *hovered = &hovered_display->snapshot;
        char line[128];
        int daily_value = daily_metric_value(display_history, display_count, (size_t)hovered_index, state->graph_metric);
        int width = 252;
        int x = (int)mouse.x + 14;
        int y = (int)mouse.y - 190;
        if (x + width > GetScreenWidth() - 16) {
            x = GetScreenWidth() - width - 16;
        }
        if (y < 16) {
            y = 16;
        }

        DrawRectangleRounded((Rectangle){ (float)x + 4.0f, (float)y + 5.0f, (float)width, 176.0f }, 0.10f, 8, (Color){ 0, 0, 0, 120 });
        DrawRectangleRounded((Rectangle){ (float)x, (float)y, (float)width, 176.0f }, 0.10f, 8, (Color){ 16, 27, 42, 246 });
        DrawRectangleRoundedLines((Rectangle){ (float)x, (float)y, (float)width, 176.0f }, 0.10f, 8, AHC_BORDER_BRIGHT);
        snprintf(line, sizeof(line), "%s%s", hovered->date, hovered_display->synthetic ? "  |  filled day" : "");
        draw_text(line, x + 14, y + 12, 17, AHC_TEXT);
        char value_text[32];
        format_int_with_commas(daily_value, value_text, sizeof(value_text));
        snprintf(line, sizeof(line), "%s: %s", graph_metric_gain_label(state->graph_metric), value_text);
        draw_text(line, x + 14, y + 42, 16, metric_color);
        char hovered_account[32];
        format_int_with_commas(hovered->account, hovered_account, sizeof(hovered_account));
        snprintf(line, sizeof(line), "Account: %s", hovered_account);
        draw_text(line, x + 14, y + 68, 16, AHC_ACCENT);
        format_tooltip_count_with_delta(line, sizeof(line), "TF", hovered->titanforged, daily_metric_value(display_history, display_count, (size_t)hovered_index, GRAPH_METRIC_TITANFORGED));
        draw_text(line, x + 14, y + 96, 17, AHC_TF);
        format_tooltip_count_with_delta(line, sizeof(line), "WF", hovered->warforged, daily_metric_value(display_history, display_count, (size_t)hovered_index, GRAPH_METRIC_WARFORGED));
        draw_text(line, x + 14, y + 122, 17, AHC_WF);
        format_tooltip_count_with_delta(line, sizeof(line), "LF", hovered->lightforged, daily_metric_value(display_history, display_count, (size_t)hovered_index, GRAPH_METRIC_LIGHTFORGED));
        draw_text(line, x + 14, y + 148, 17, AHC_LF);
    }
}

static void companion_dispose_phone_qr(CompanionState *state)
{
    if (state->phone_qr_valid) {
        UnloadTexture(state->phone_qr_tx);
        state->phone_qr_valid = false;
    }
}

static void companion_refresh_phone_qr(CompanionState *state)
{
    if (state->history_count == 0) {
        set_action_status(state, "No attune history yet. Scan a snapshot first.", "No history");
        return;
    }
    const AhcDailyAttuneSnapshot *last = &state->history[state->history_count - 1U];
    char text[256];
    if (ahc_sync_encode_one_day_qr(last, text, sizeof text) < 0) {
        set_action_status(state, "Could not build QR payload.", "QR build failed");
        return;
    }
    size_t bmax = (size_t)qrcodegen_BUFFER_LEN_MAX;
    uint8_t *tempb = (uint8_t *)malloc(bmax);
    uint8_t *qrcodeb = (uint8_t *)malloc(bmax);
    if (tempb == NULL || qrcodeb == NULL) {
        free(tempb);
        free(qrcodeb);
        set_action_status(state, "Out of memory for QR.", "QR failed");
        return;
    }
    if (!qrcodegen_encodeText(
            text,
            tempb,
            qrcodeb,
            qrcodegen_Ecc_MEDIUM,
            qrcodegen_VERSION_MIN,
            qrcodegen_VERSION_MAX,
            qrcodegen_Mask_AUTO,
            true)) {
        free(tempb);
        free(qrcodeb);
        set_action_status(state, "QR is too large for the encoder (unexpected).", "QR failed");
        return;
    }
    int qsize = qrcodegen_getSize(qrcodeb);
    if (qsize <= 0) {
        free(tempb);
        free(qrcodeb);
        return;
    }
    int scale = 5;
    int px = qsize * scale;
    Image im = GenImageColor(px, px, (Color){ 255, 255, 255, 255 });
    for (int y = 0; y < qsize; y++) {
        for (int x = 0; x < qsize; x++) {
            Color c = qrcodegen_getModule(qrcodeb, x, y) ? (Color){ 0, 0, 0, 255 } : (Color){ 255, 255, 255, 255 };
            for (int dy = 0; dy < scale; dy++) {
                for (int dx = 0; dx < scale; dx++) {
                    ImageDrawPixel(&im, x * scale + dx, y * scale + dy, c);
                }
            }
        }
    }
    free(tempb);
    free(qrcodeb);
    companion_dispose_phone_qr(state);
    state->phone_qr_tx = LoadTextureFromImage(im);
    UnloadImage(im);
    if (!IsTextureValid(state->phone_qr_tx)) {
        set_action_status(state, "Could not create QR image.", "QR failed");
        return;
    }
    state->phone_qr_valid = true;
    set_action_status(
        state,
        "Scan with Attune Helper on your phone. QR is latest day only; use Copy for full AHC1 history.",
        "QR ready");
}

static void companion_copy_full_sync(CompanionState *state)
{
    if (state->history_count == 0) {
        set_action_status(state, "No attune history to export.", "Nothing to copy");
        return;
    }
    size_t cap = 2U * 1024U * 1024U + 64U;
    char *buf = (char *)malloc(cap);
    if (buf == NULL) {
        set_action_status(state, "Out of memory for full sync string.", "Copy failed");
        return;
    }
    int n = ahc_sync_encode_full_history(state->history, state->history_count, buf, cap);
    if (n < 0) {
        free(buf);
        set_action_status(
            state,
            "Full sync string is too large. Reduce how much history the companion keeps, then retry.",
            "Copy failed");
        return;
    }
    buf[n] = '\0';
    SetClipboardText(buf);
    free(buf);
    set_action_status(
        state,
        "Full sync (AHC1) copied. Paste in Android or another device’s companion to merge.",
        "Copied AHC1");
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
    if (draw_wow_button("Scan now", (Rectangle){ content.x, button_y, 150.0f, button_height }, false, 18)) {
        scan_attunehelper_snapshot(state);
    }
    if (draw_wow_button("Show QR (phone)", (Rectangle){ content.x + 168.0f, button_y, 196.0f, button_height }, false, 16)) {
        companion_refresh_phone_qr(state);
    }
    if (draw_wow_button("Copy AHC1", (Rectangle){ content.x + 380.0f, button_y, 150.0f, button_height }, false, 16)) {
        companion_copy_full_sync(state);
    }
    if (draw_wow_button("Seed test data", (Rectangle){ content.x + 546.0f, button_y, 180.0f, button_height }, false, 18)) {
        seed_test_history(state);
    }
    if (state->phone_qr_valid && IsTextureValid(state->phone_qr_tx)) {
        float qy = button_y + button_height + 12.0f;
        float qscale = 1.5f;
        DrawTextureEx(state->phone_qr_tx, (Vector2){ content.x, qy }, 0.0f, qscale, WHITE);
    }
}

static bool addon_matches_selected_category(const CompanionState *state, const AhcAddon *addon)
{
    if (!state->selected_addon_category[0]) {
        return true;
    }
    if (addon->categories && addon->category_count > 0u) {
        for (size_t i = 0; i < addon->category_count; i++) {
            if (addon->categories[i] && strcmp(addon->categories[i], state->selected_addon_category) == 0) {
                return true;
            }
        }
        return false;
    }
    return addon->category && strcmp(addon->category, state->selected_addon_category) == 0;
}

static size_t addon_filtered_count(CompanionState *state, const AhcAddon *addons, size_t count)
{
    if (state->addon_filter_valid
        && state->addon_filter_cache_items == addons
        && state->addon_filter_cache_count == count
        && strcmp(state->addon_filter_cache_category, state->selected_addon_category) == 0) {
        return state->addon_filter_count;
    }

    size_t filtered = 0u;
    for (size_t i = 0; i < count; i++) {
        if (addon_matches_selected_category(state, &addons[i])) {
            filtered++;
        }
    }
    state->addon_filter_cache_items = addons;
    state->addon_filter_cache_count = count;
    snprintf(state->addon_filter_cache_category, sizeof(state->addon_filter_cache_category), "%s", state->selected_addon_category);
    state->addon_filter_count = filtered;
    state->addon_filter_valid = true;
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

static size_t addon_category_count(const AhcAddon *addon)
{
    if (addon->categories && addon->category_count > 0u) {
        return addon->category_count;
    }
    return addon->category && addon->category[0] ? 1u : 0u;
}

static const char *addon_category_at(const AhcAddon *addon, size_t index)
{
    if (addon->categories && addon->category_count > 0u) {
        return index < addon->category_count ? addon->categories[index] : NULL;
    }
    return index == 0u ? addon->category : NULL;
}

static void addon_category_line(const AhcAddon *addon, char *out, size_t out_capacity)
{
    size_t written = 0u;
    if (!out_capacity) {
        return;
    }
    out[0] = '\0';

    size_t count = addon_category_count(addon);
    for (size_t i = 0; i < count; i++) {
        const char *category = addon_category_at(addon, i);
        if (!category || !category[0]) {
            continue;
        }
        int n = snprintf(out + written, out_capacity - written, "%s%s", written ? ", " : "", category);
        if (n < 0) {
            break;
        }
        if ((size_t)n >= out_capacity - written) {
            out[out_capacity - 1u] = '\0';
            break;
        }
        written += (size_t)n;
    }

    if (!out[0]) {
        snprintf(out, out_capacity, "Uncategorized");
    }
}

static void addon_category_cache_prepare(CompanionState *state, const AhcAddon *addons, size_t count)
{
    if (state->addon_categories_valid
        && state->addon_category_cache_items == addons
        && state->addon_category_cache_count == count) {
        return;
    }

    state->addon_category_cache_items = addons;
    state->addon_category_cache_count = count;
    state->addon_category_count = 0u;
    for (size_t i = 0; i < AHC_CATEGORY_CAPACITY; i++) {
        state->addon_categories[i] = NULL;
    }
    for (size_t i = 0; i < count && state->addon_category_count < AHC_CATEGORY_CAPACITY; i++) {
        size_t addon_categories = addon_category_count(&addons[i]);
        for (size_t j = 0; j < addon_categories && state->addon_category_count < AHC_CATEGORY_CAPACITY; j++) {
            const char *category = addon_category_at(&addons[i], j);
            if (category && category[0] && !category_seen(state->addon_categories, state->addon_category_count, category)) {
                state->addon_categories[state->addon_category_count++] = category;
            }
        }
    }
    state->addon_categories_valid = true;
}

static int draw_category_filters(CompanionState *state, const AhcAddon *addons, size_t count, float x, float y, float right)
{
    addon_category_cache_prepare(state, addons, count);

    float cursor_x = x;
    float cursor_y = y;
    int row = 0;

    const char *labels[AHC_CATEGORY_CAPACITY + 1];
    labels[0] = "All";
    for (size_t i = 0; i < state->addon_category_count; i++) {
        labels[i + 1] = state->addon_categories[i];
    }

    for (size_t i = 0; i < state->addon_category_count + 1; i++) {
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

static void draw_tooltip_box(const char *text, Rectangle anchor)
{
    if (!text || !text[0] || !CheckCollisionPointRec(GetMousePosition(), anchor)) {
        return;
    }
    int width = measure_text_width(text, 13) + 22;
    if (width < 120) {
        width = 120;
    }
    Rectangle tip = { anchor.x, anchor.y - 34.0f, (float)width, 26.0f };
    if (tip.y < 42.0f) {
        tip.y = anchor.y + anchor.height + 8.0f;
    }
    DrawRectangleRounded(tip, 0.12f, 8, (Color){ 6, 12, 20, 245 });
    DrawRectangleRoundedLines(tip, 0.12f, 8, AHC_BORDER_BRIGHT);
    draw_text(text, (int)tip.x + 11, (int)tip.y + 7, 13, AHC_TEXT);
}

static void draw_status_icon(Rectangle bounds, bool installed, bool managed)
{
    bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    if (hovered) {
        float cx = bounds.x + bounds.width * 0.5f;
        float cy = bounds.y + bounds.height * 0.5f;
        DrawCircleLines((int)cx, (int)cy, bounds.width * 0.5f, (Color){ 200, 224, 255, 200 });
    }
    Color color = !installed ? (Color){ 210, 72, 78, 255 } : (managed ? (Color){ 92, 214, 143, 255 } : (Color){ 232, 194, 83, 255 });
    if (hovered) {
        int rr = (int)color.r + 28;
        int gg = (int)color.g + 28;
        int bb = (int)color.b + 22;
        color = (Color){ (unsigned char)(rr > 255 ? 255 : rr), (unsigned char)(gg > 255 ? 255 : gg), (unsigned char)(bb > 255 ? 255 : bb), 255 };
    }
    const char *label = !installed ? "X" : (managed ? "OK" : "!");
    DrawCircle((int)(bounds.x + bounds.width * 0.5f), (int)(bounds.y + bounds.height * 0.5f), bounds.width * 0.42f, color);
    draw_centered_text(label, bounds, 12, (Color){ 10, 18, 28, 255 });
    draw_tooltip_box(!installed ? "Not installed" : (managed ? "Managed install" : "Manual install detected"), bounds);
}

static void draw_addon_card(CompanionState *state, const AhcAddon *addons, size_t count, size_t index, Rectangle bounds)
{
    const AhcAddon *addon = &addons[index];
    AddonInstallStatus install_status = addon_install_status_for_index(state, addons, count, index);
    bool installed = install_status.installed;
    bool git_checkout = install_status.git_checkout;
    bool managed = install_status.managed;
    bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    Rectangle badge = { bounds.x + bounds.width - 178.0f, bounds.y + 8.0f, 166.0f, 46.0f };
    DrawRectangleRounded(bounds, 0.035f, 8, hovered ? AHC_PANEL_HOVER : AHC_PANEL);
    DrawRectangleRoundedLines(bounds, 0.035f, 8, hovered ? AHC_BORDER_BRIGHT : AHC_BORDER);
    draw_source_badge(state, badge, addon);
    draw_text(addon->name, (int)bounds.x + 16, (int)bounds.y + 10, 21, AHC_TEXT);

    {
        int mx = (int)bounds.x + 48;
        int my = (int)bounds.y + 39;
        int ms = 15;
        const char *sep0 = "  |  By ";
        const char *sep1 = "  |  Folder: ";
        char aurlb[AHC_PATH_CAPACITY];
        char surlb[AHC_PATH_CAPACITY];
        bool a_link = addon_url_for_author_link(addon, aurlb, sizeof(aurlb));
        const char *src = addon_source_label(addon);
        if (addon_url_for_source_link(addon, surlb, sizeof(surlb))) {
            if (draw_interactive_text_link(mx, my, ms, src, surlb)) {
                try_open_url_with_feedback(state, surlb, "Addon page");
            }
        } else {
            draw_text(src, mx, my, ms, AHC_MUTED);
        }
        mx += measure_text_width(src, ms);
        draw_text(sep0, mx, my, ms, AHC_MUTED);
        mx += measure_text_width(sep0, ms);
        if (a_link) {
            if (draw_interactive_text_link(mx, my, ms, addon->author, aurlb)) {
                try_open_url_with_feedback(state, aurlb, "Author link");
            }
        } else {
            draw_text(addon->author, mx, my, ms, AHC_MUTED);
        }
        mx += measure_text_width(addon->author, ms);
        draw_text(sep1, mx, my, ms, AHC_MUTED);
        mx += measure_text_width(sep1, ms);
        draw_text(addon->folder, mx, my, ms, AHC_MUTED);
    }
    char category_line[128];
    addon_category_line(addon, category_line, sizeof(category_line));
    Rectangle category_icon = { bounds.x + 16.0f, bounds.y + 36.0f, 24.0f, 24.0f };
    DrawRectangleRounded(category_icon, 0.24f, 8, (Color){ 34, 66, 103, 255 });
    draw_centered_text("C", category_icon, 13, AHC_TEXT);
    draw_tooltip_box(category_line, category_icon);

    char description_line[220];
    snprintf(description_line, sizeof(description_line), "%s", addon->description);
    draw_text(description_line, (int)bounds.x + 16, (int)bounds.y + 66, 17, AHC_TEXT);

    const char *version_text = (addon->version && addon->version[0] && strcmp(addon->version, "unknown") != 0)
        ? addon->version
        : "No tagged version";
    char version_line[96];
    snprintf(version_line, sizeof(version_line), "Version %s", version_text);
    draw_text(version_line, (int)bounds.x + 16, (int)bounds.y + 84, 13, AHC_MUTED);

    bool installable = addon_has_install_source(addon);
    bool downloadable = addon_repo_is_zip_url(addon);
    Rectangle status_icon = { bounds.x + bounds.width - 226.0f, bounds.y + 75.0f, 24.0f, 24.0f };
    draw_status_icon(status_icon, installed, managed);

    if (installable) {
        const char *primary_label = installed ? (managed || git_checkout ? "Update" : "Replace") : "Install";
        float primary_x = installed ? bounds.x + bounds.width - 184.0f : bounds.x + bounds.width - 88.0f;
        if (draw_wow_button(primary_label, (Rectangle){ primary_x, bounds.y + 58.0f, 74.0f, 30.0f }, state->addon_job_running != 0, 15)) {
            begin_install_or_update_addon(state, addon);
        }
        if (installed && draw_wow_button("Uninstall", (Rectangle){ bounds.x + bounds.width - 96.0f, bounds.y + 58.0f, 82.0f, 30.0f }, state->addon_job_running != 0, 15) && !state->addon_job_running) {
            uninstall_addon(state, addon);
        }
    } else if (draw_wow_button(downloadable ? "Download" : "Open Page", (Rectangle){ bounds.x + bounds.width - 108.0f, bounds.y + 58.0f, 96.0f, 30.0f }, false, 14)) {
        if (open_external_url(addon->repo)) {
            char message[AHC_STATUS_CAPACITY];
            snprintf(message, sizeof(message), downloadable ? "Started download for %s." : "Opened %s page in browser.", addon->name);
            set_action_status(state, message, downloadable ? "Download started" : "Opened addon page");
        } else {
            set_action_status(state, downloadable ? "Could not open download URL." : "Could not open addon page in browser.", downloadable ? "Download failed" : "Open page failed");
        }
    }
}

static const char *community_favorite_description(const AhcAddon *addon)
{
    if (!addon || !addon->folder) {
        return NULL;
    }
    if (strcmp(addon->folder, "AttuneHelper") == 0) {
        return "Automate your Attunement swaps and manage your gear quickly with this powerful addon.";
    }
    if (strcmp(addon->folder, "ScootsStats") == 0) {
        return "Adds a stat panel beside your character panel with extra stats.";
    }
    if (strcmp(addon->folder, "ScootsConfirmationSkip") == 0) {
        return "Skips confirmation dialogs for bind-on-equip gear and bind-on-pickup chest loot.";
    }
    if (strcmp(addon->folder, "ScootsCraft") == 0) {
        return "Overhauls professions and includes a modified Acki's Recipe List.";
    }
    if (strcmp(addon->folder, "ScootsRares") == 0) {
        return "Creates a chat link to run .findnpc when you are near a rare.";
    }
    return NULL;
}

static void draw_community_favorite_card(CompanionState *state, const AhcAddon *addons, size_t count, size_t index, const char *description, Rectangle bounds)
{
    const AhcAddon *addon = &addons[index];
    AddonInstallStatus install_status = addon_install_status_for_index(state, addons, count, index);
    DrawRectangleRounded(bounds, 0.08f, 8, AHC_PANEL);
    DrawRectangleRoundedLines(bounds, 0.08f, 8, AHC_BORDER);
    draw_wrapped_text(addon->name, (int)bounds.x + 12, (int)bounds.y + 10, 17, (int)bounds.width - 86, 1, AHC_TEXT);
    draw_status_icon((Rectangle){ bounds.x + bounds.width - 32.0f, bounds.y + 10.0f, 22.0f, 22.0f }, install_status.installed, install_status.managed);
    int byx = (int)bounds.x + 12;
    int byy = (int)bounds.y + 34;
    const char *byp = "By ";
    draw_text(byp, byx, byy, 13, AHC_MUTED);
    byx += measure_text_width(byp, 13);
    char ahref[AHC_PATH_CAPACITY];
    if (addon_url_for_author_link(addon, ahref, sizeof(ahref))) {
        if (draw_interactive_text_link(byx, byy, 13, addon->author, ahref)) {
            try_open_url_with_feedback(state, ahref, "Author link");
        }
    } else {
        draw_text(addon->author, byx, byy, 13, AHC_MUTED);
    }
    draw_wrapped_text(description, (int)bounds.x + 12, (int)bounds.y + 56, 13, (int)bounds.width - 24, 2, AHC_ACCENT);
    const char *label = install_status.installed ? (install_status.managed ? "Update" : "Replace") : "Install";
    if (draw_wow_button(label, (Rectangle){ bounds.x + bounds.width - 82.0f, bounds.y + bounds.height - 34.0f, 70.0f, 26.0f }, state->addon_job_running != 0, 13)) {
        begin_install_or_update_addon(state, addon);
    }
}

static float draw_community_favorites_section(CompanionState *state, const AhcAddon *addons, size_t count, float x, float y, float right)
{
    size_t favorites[5] = { 0 };
    const char *descriptions[5] = { 0 };
    size_t favorite_count = 0u;
    for (size_t i = 0; i < count && favorite_count < 5u; i++) {
        const char *description = community_favorite_description(&addons[i]);
        if (description) {
            favorites[favorite_count] = i;
            descriptions[favorite_count] = description;
            favorite_count++;
        }
    }

    if (favorite_count == 0u) {
        return 0.0f;
    }

    draw_text("Community Favorites", (int)x, (int)y, 18, AHC_TEXT);
    draw_text("Highly recommended addons for new players.", (int)x, (int)y + 24, 14, AHC_MUTED);

    const float gap = 12.0f;
    float available = right - x;
    int columns = available >= 1180.0f ? 5 : (available >= 920.0f ? 4 : (available >= 680.0f ? 3 : 2));
    if (columns < 1) {
        columns = 1;
    }
    const float card_width = (available - (float)(columns - 1) * gap) / (float)columns;
    const float card_height = 126.0f;
    float cursor_x = x;
    float cursor_y = y + 48.0f;
    float bottom = cursor_y + card_height;

    for (size_t i = 0; i < favorite_count; i++) {
        if (cursor_x + card_width > right && cursor_x > x) {
            cursor_x = x;
            cursor_y += card_height + gap;
            bottom = cursor_y + card_height;
        }
        draw_community_favorite_card(state, addons, count, favorites[i], descriptions[i], (Rectangle){ cursor_x, cursor_y, card_width, card_height });
        cursor_x += card_width + gap;
    }

    return bottom - y + 18.0f;
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

    float controls_y = content.y + 50.0f;
    controls_y += draw_community_favorites_section(state, addons, count, content.x + 20.0f, controls_y, content.x + content.width - 20.0f);
    int category_rows = draw_category_filters(state, addons, count, content.x + 20.0f, controls_y, content.x + content.width - 20.0f);
    float list_y = controls_y + (float)(category_rows * 34) + 8.0f;
    float list_height = content.y + content.height - list_y - 14.0f;
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
    const float row_stride = 120.0f;
    const float addon_card_height = 108.0f;
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
            draw_addon_card(state, addons, count, i, (Rectangle){ x, y, column_width - 8.0f, addon_card_height });
            slot++;
        }
        EndScissorMode();
    }
}

static void draw_labeled_textbox(const char *label, Rectangle bounds, char *value, int value_capacity, bool *editing)
{
    draw_text(label, (int)bounds.x, (int)bounds.y - 22, 14, AHC_MUTED);
    if (GuiTextBox(bounds, value, value_capacity, *editing)) {
        *editing = !*editing;
    }
}

static void draw_settings_tab(CompanionState *state)
{
    Rectangle content = content_rect();
    float gap = 18.0f;
    float left_width = content.width * 0.48f;
    if (left_width < 480.0f) {
        left_width = content.width;
    }
    float right_x = content.x + left_width + gap;
    float right_width = content.width - left_width - gap;
    bool two_columns = right_width >= 420.0f;
    if (!two_columns) {
        right_x = content.x;
        right_width = content.width;
    }

    Rectangle setup_card = { content.x, content.y, left_width, 600.0f };
    Rectangle data_card = { content.x, content.y + 618.0f, left_width, 174.0f };
    Rectangle wow_card = { two_columns ? right_x : content.x, two_columns ? content.y : content.y + 810.0f, right_width, 456.0f };

    draw_card(setup_card, "Setup");
    draw_text("Synastria folder", (int)setup_card.x + 20, (int)setup_card.y + 50, 22, AHC_TEXT);
    draw_text("Paste the folder that contains WoWExt.exe, or let the app detect it.", (int)setup_card.x + 20, (int)setup_card.y + 80, 16, AHC_MUTED);

    bool paste_requested = state->synastria_path_editing
        && (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER))
        && IsKeyPressed(KEY_V);

    if (paste_requested) {
        const char *clipboard = GetClipboardText();
        if (clipboard && clipboard[0]) {
            snprintf(state->synastria_path, sizeof(state->synastria_path), "%s", clipboard);
            trim_line(state->synastria_path);
            set_action_status(state, "Pasted Synastria path from clipboard.", "Path pasted");
        }
    }

    Rectangle textbox = { setup_card.x + 20.0f, setup_card.y + 112.0f, setup_card.width - 146.0f, 38.0f };
    if (textbox.width < 300.0f) {
        textbox.width = 300.0f;
    }
    if (GuiTextBox(textbox, state->synastria_path, (int)sizeof(state->synastria_path), state->synastria_path_editing)) {
        state->synastria_path_editing = !state->synastria_path_editing;
    }

    if (draw_wow_button("Save", (Rectangle){ textbox.x + textbox.width + 16.0f, textbox.y, 100.0f, 38.0f }, false, 17)) {
        if (validate_synastria_path(state->synastria_path)) {
            save_settings(state);
            scan_attunehelper_snapshot(state);
            load_wow_config(state);
        } else {
            set_action_status(state, "That folder does not look like a Synastria install.", "Path validation failed");
        }
    }

    if (draw_wow_button("Scan snapshots", (Rectangle){ setup_card.x + 20.0f, setup_card.y + 162.0f, 160.0f, 34.0f }, false, 15)) {
        scan_attunehelper_snapshot(state);
    }
    if (draw_wow_button("Auto-detect", (Rectangle){ setup_card.x + 194.0f, setup_card.y + 162.0f, 142.0f, 34.0f }, false, 15)) {
        if (try_auto_detect_synastria(state, true)) {
            scan_attunehelper_snapshot(state);
            load_wow_config(state);
        }
    }
    draw_text("UI scale", (int)setup_card.x + 20, (int)setup_card.y + 205, 15, AHC_MUTED);
    const char *scale_labels[] = { "Auto", "100%", "125%", "150%" };
    for (int i = 0; i < 4; i++) {
        Rectangle scale_button = { setup_card.x + 92.0f + (float)i * 72.0f, setup_card.y + 199.0f, 64.0f, 30.0f };
        if (draw_wow_button(scale_labels[i], scale_button, state->ui_scale_index == i, 13)) {
            state->ui_scale_index = i;
            update_ui_scale(state);
            save_settings(state);
        }
    }
    {
        bool path_ok = validate_synastria_path(state->synastria_path);
#if !defined(_WIN32)
        bool run_ok = path_ok && ahc_posix_wine_or_proton_ready();
        const char *ready_msg = !path_ok    ? "Choose the folder with WoWExt.exe."
            : !run_ok ? "WoWExt found: install Wine or set AHC_PROTON / PROTON_PATH for Proton."
                      : "Ready: WoWExt.exe found; Wine/Proton available.";
#else
        bool run_ok = path_ok;
        const char *ready_msg = path_ok ? "Ready: WoWExt.exe found." : "Choose the folder with WoWExt.exe.";
#endif
        draw_text(ready_msg, (int)setup_card.x + 352, (int)setup_card.y + 171, 15, run_ok ? (Color){ 140, 224, 186, 255 } : AHC_STATUS_WARNING);
    }

    if (state->awesome_wotlk_autologin) {
        state->launch_parameters_editing = false;
    }
    draw_text("Launch parameters", (int)setup_card.x + 20, (int)setup_card.y + 244, 16, AHC_MUTED);
    if (state->awesome_wotlk_autologin) {
        draw_text(
            "Autologin on: only extra client flags here (no -login / -password).",
            (int)setup_card.x + 20,
            (int)setup_card.y + 221,
            14,
            AHC_STATUS_WARNING);
    }
    Rectangle launch_box = { setup_card.x + 20.0f, setup_card.y + 266.0f, setup_card.width - 166.0f, 34.0f };
    if (launch_box.width < 300.0f) {
        launch_box.width = 300.0f;
    }
    {
        bool launch_editing = state->launch_parameters_editing && !state->awesome_wotlk_autologin;
        if (GuiTextBox(launch_box, state->launch_parameters, (int)sizeof(state->launch_parameters), launch_editing)) {
            state->launch_parameters_editing = !state->launch_parameters_editing;
        }
    }
    draw_text("Use Play Game in the header for quick launch.", (int)setup_card.x + 352, (int)setup_card.y + 274, 14, AHC_MUTED);

    draw_text(
        "AwesomeWotLK autologin: launches Wow.exe with -login, -password, -realmname; optional extras in Launch parameters.",
        (int)setup_card.x + 20,
        (int)setup_card.y + 300,
        15,
        AHC_MUTED);
    (void)GuiCheckBox(
        (Rectangle){ setup_card.x + 20.0f, setup_card.y + 328.0f, 420.0f, 28.0f },
        "Enable autologin (password: Windows DPAPI, Linux 0600 file, Android not yet wired).",
        &state->awesome_wotlk_autologin);
    {
        const char *pwst = state->autologin_password_stored ? "On-disk password: stored" : "On-disk password: not stored";
        draw_text(
            pwst,
            (int)setup_card.x + 20,
            (int)setup_card.y + 364,
            14,
            state->autologin_password_stored ? (Color){ 140, 224, 186, 255 } : AHC_STATUS_WARNING);
    }
    draw_labeled_textbox("Account (login name)", (Rectangle){ setup_card.x + 20.0f, setup_card.y + 402.0f, setup_card.width - 200.0f, 32.0f },
        state->autologin_user, (int)sizeof(state->autologin_user), &state->autologin_user_editing);
    draw_labeled_textbox("Realm", (Rectangle){ setup_card.x + 20.0f, setup_card.y + 474.0f, setup_card.width - 200.0f, 32.0f },
        state->autologin_realm, (int)sizeof(state->autologin_realm), &state->autologin_realm_editing);
    draw_labeled_textbox("Password (type to set/change; cleared on save from memory only)", (Rectangle){ setup_card.x + 20.0f, setup_card.y + 544.0f, setup_card.width - 200.0f, 32.0f },
        state->autologin_password_edit, (int)sizeof(state->autologin_password_edit), &state->autologin_password_editing);
    if (draw_wow_button("Save autologin", (Rectangle){ setup_card.x + setup_card.width - 192.0f, setup_card.y + 542.0f, 180.0f, 40.0f }, false, 16)) {
        (void)persist_autologin_settings(state);
    }

    draw_card(data_card, "Data and backups");
    draw_text("Backups are copied under Synastria/AttuneHelperBackup.", (int)data_card.x + 20, (int)data_card.y + 52, 16, AHC_MUTED);
    if (draw_wow_button("Backup WTF", (Rectangle){ data_card.x + 20.0f, data_card.y + 80.0f, 142.0f, 34.0f }, false, 15)) {
        char wtf_path[AHC_PATH_CAPACITY];
        if (resolve_wtf_path(state, wtf_path, sizeof(wtf_path))) {
            backup_directory_snapshot(state, wtf_path, "WTF", "WTF");
        } else {
            set_action_status(state, "WTF folder was not found.", "WTF backup blocked");
        }
    }
    if (draw_wow_button("Backup AddOns", (Rectangle){ data_card.x + 176.0f, data_card.y + 80.0f, 160.0f, 34.0f }, false, 15)) {
        char addons_path[AHC_PATH_CAPACITY];
        if (resolve_addons_path(state, addons_path, sizeof(addons_path))) {
            backup_directory_snapshot(state, addons_path, "Addons", "AddOns");
        } else {
            set_action_status(state, "AddOns folder was not found.", "AddOns backup blocked");
        }
    }

    const char *clear_label = state->clear_data_confirming ? "Confirm clear" : "Clear companion data";
    if (draw_wow_button(clear_label, (Rectangle){ data_card.x + 20.0f, data_card.y + 126.0f, 190.0f, 34.0f }, state->clear_data_confirming, 15)) {
        if (state->clear_data_confirming) {
            clear_companion_data(state);
        } else {
            state->clear_data_confirming = true;
            set_action_status(state, "Click Confirm clear to remove local companion settings/history.", "Clear confirmation armed");
        }
    }
    draw_text("Clear does not touch Synastria, WTF, AddOns, or backups.", (int)data_card.x + 226, (int)data_card.y + 134, 14, AHC_MUTED);

    draw_card(wow_card, "WoW 3.3.5a settings");
    draw_text("Edit display and safe quality fields before launch. Existing unknown Config.wtf lines are preserved.", (int)wow_card.x + 20, (int)wow_card.y + 52, 15, AHC_MUTED);
    if (draw_wow_button("Load Config", (Rectangle){ wow_card.x + 20.0f, wow_card.y + 78.0f, 132.0f, 34.0f }, false, 15)) {
        load_wow_config(state);
    }
    if (draw_wow_button("Save Config", (Rectangle){ wow_card.x + 164.0f, wow_card.y + 78.0f, 132.0f, 34.0f }, false, 15)) {
        save_wow_config(state);
    }
    if (draw_wow_button("Use Monitor", (Rectangle){ wow_card.x + 308.0f, wow_card.y + 78.0f, 132.0f, 34.0f }, false, 15)) {
        apply_monitor_defaults(state);
        set_action_status(state, "Applied current monitor resolution and refresh.", "Monitor defaults applied");
    }
    char monitor_line[220];
    snprintf(monitor_line, sizeof(monitor_line), "%s%s", state->monitor_name[0] ? state->monitor_name : "Monitor auto-detect ready", state->wow_config_loaded ? "  |  Config loaded" : "");
    draw_text(monitor_line, (int)wow_card.x + 454, (int)wow_card.y + 86, 14, state->monitor_name[0] ? (Color){ 140, 224, 186, 255 } : AHC_STATUS_WARNING);

    float field_y = wow_card.y + 146.0f;
    draw_labeled_textbox("Width", (Rectangle){ wow_card.x + 20.0f, field_y, 84.0f, 32.0f }, state->wow_width, (int)sizeof(state->wow_width), &state->wow_width_editing);
    draw_labeled_textbox("Height", (Rectangle){ wow_card.x + 116.0f, field_y, 84.0f, 32.0f }, state->wow_height, (int)sizeof(state->wow_height), &state->wow_height_editing);
    draw_labeled_textbox("Refresh", (Rectangle){ wow_card.x + 212.0f, field_y, 84.0f, 32.0f }, state->wow_refresh, (int)sizeof(state->wow_refresh), &state->wow_refresh_editing);

    if (draw_wow_button(state->wow_windowed ? "Windowed: On" : "Windowed: Off", (Rectangle){ wow_card.x + 320.0f, field_y, 128.0f, 32.0f }, state->wow_windowed, 14)) {
        state->wow_windowed = !state->wow_windowed;
    }
    if (draw_wow_button(state->wow_borderless ? "Borderless: On" : "Borderless: Off", (Rectangle){ wow_card.x + 460.0f, field_y, 142.0f, 32.0f }, state->wow_borderless, 14)) {
        state->wow_borderless = !state->wow_borderless;
        if (state->wow_borderless) {
            state->wow_windowed = true;
        }
    }

    field_y += 78.0f;
    draw_labeled_textbox("VSync", (Rectangle){ wow_card.x + 20.0f, field_y, 84.0f, 32.0f }, state->wow_vsync, (int)sizeof(state->wow_vsync), &state->wow_vsync_editing);
    draw_labeled_textbox("Multisample", (Rectangle){ wow_card.x + 116.0f, field_y, 110.0f, 32.0f }, state->wow_multisampling, (int)sizeof(state->wow_multisampling), &state->wow_multisampling_editing);
    draw_labeled_textbox("Texture mip", (Rectangle){ wow_card.x + 238.0f, field_y, 110.0f, 32.0f }, state->wow_texture_resolution, (int)sizeof(state->wow_texture_resolution), &state->wow_texture_resolution_editing);
    draw_labeled_textbox("Env detail", (Rectangle){ wow_card.x + 360.0f, field_y, 110.0f, 32.0f }, state->wow_environment_detail, (int)sizeof(state->wow_environment_detail), &state->wow_environment_detail_editing);
    draw_labeled_textbox("Ground FX", (Rectangle){ wow_card.x + 482.0f, field_y, 110.0f, 32.0f }, state->wow_ground_effect_density, (int)sizeof(state->wow_ground_effect_density), &state->wow_ground_effect_density_editing);

    draw_text("Common 21:9 setup: set native width/height, refresh rate, Windowed On, Borderless On.", (int)wow_card.x + 20, (int)(wow_card.y + wow_card.height - 42.0f), 14, AHC_MUTED);
}

static void draw_process_footer(const CompanionState *state)
{
    Rectangle footer = { 0.0f, (float)GetScreenHeight() - 46.0f, (float)GetScreenWidth(), 46.0f };
    DrawRectangleRec(footer, (Color){ 7, 14, 23, 244 });
    DrawRectangle(0, GetScreenHeight() - 46, GetScreenWidth(), 1, AHC_BORDER);

    double now = app_time();
    bool active = state->process_action[0] && now <= state->process_until;
    const char *action = active ? state->process_action : "Idle";

    if (active) {
        float progress = (float)((now - state->process_started_at) / AHC_PROCESS_ACTIVE_SECONDS);
        if (progress < 0.0f) {
            progress = 0.0f;
        }
        if (progress > 1.0f) {
            progress = 1.0f;
        }
        int sweep_width = GetScreenWidth() / 4;
        int sweep_x = (int)((float)(GetScreenWidth() + sweep_width) * progress) - sweep_width;
        DrawRectangle(sweep_x, GetScreenHeight() - 45, sweep_width, 3, AHC_ACCENT);
    }

    char dots[4] = "";
    if (active) {
        int dot_count = ((int)(now * 3.0)) % 4;
        for (int i = 0; i < dot_count; i++) {
            dots[i] = '.';
        }
        dots[dot_count] = '\0';
    }

    char line[AHC_STATUS_CAPACITY * 2];
    snprintf(line, sizeof(line), "%s%s  |  %s", action, dots, state->status);
    draw_text(line, 26, GetScreenHeight() - 32, 17, active ? AHC_TEXT : AHC_MUTED);

    if (active) {
        float pulse_phase = (float)(now * 2.0 - (double)((int)(now * 2.0)));
        float pulse = pulse_phase < 0.5f ? pulse_phase * 2.0f : (1.0f - pulse_phase) * 2.0f;
        DrawCircle(GetScreenWidth() - 32, GetScreenHeight() - 23, 6.0f + pulse * 3.0f, (Color){ 83, 167, 239, 90 });
        DrawCircle(GetScreenWidth() - 32, GetScreenHeight() - 23, 4.0f, AHC_ACCENT);
    }
}

static void init_state(CompanionState *state)
{
    memset(state, 0, sizeof(*state));
    state->tab = COMPANION_TAB_ATTUNES;
    state->graph_metric = GRAPH_METRIC_ACCOUNT;
    state->graph_display = GRAPH_DISPLAY_PLOT;
    reset_wow_config_fields(state);
    load_addon_catalog(state);
    resolve_config_paths(state);
    load_settings(state);
    if (!state->autologin_realm[0]) {
        snprintf(state->autologin_realm, sizeof state->autologin_realm, "AzerothCore");
    }
    state->autologin_password_stored = ahc_credential_wow_password_exists(state->config_dir);
    load_history(state);
    set_status(state, "Ready.");

    if (!state->synastria_path[0]) {
        try_auto_detect_synastria(state, true);
    }

    if (state->synastria_path[0]) {
        scan_attunehelper_snapshot(state);
    }
}

static int ahc_display_fps_cap(const CompanionState *state)
{
    if (state->tab == COMPANION_TAB_ADDONS) {
        return AHC_ADDON_STATIC_FPS;
    }
    int m = GetCurrentMonitor();
    int rr = GetMonitorRefreshRate(m);
    return (rr > 0) ? rr : 60;
}

static void apply_window_defaults(void)
{
    int monitor = GetCurrentMonitor();
    int monitor_width = GetMonitorWidth(monitor);
    int monitor_height = GetMonitorHeight(monitor);
    int target_width = 1500;
    int target_height = 900;

    if (monitor_width > 0 && monitor_height > 0) {
        target_width = (int)((float)monitor_width * 0.84f);
        target_height = (int)((float)monitor_height * 0.84f);
        if (target_width < 1280) {
            target_width = 1280;
        }
        if (target_height < 820) {
            target_height = 820;
        }
        if (target_width > monitor_width - 80) {
            target_width = monitor_width - 80;
        }
        if (target_height > monitor_height - 90) {
            target_height = monitor_height - 90;
        }
    }

    if (target_width < 1120) {
        target_width = 1120;
    }
    if (target_height < 720) {
        target_height = 720;
    }

    SetWindowMinSize(1120, 720);
    SetWindowSize(target_width, target_height);
    if (monitor_width > target_width && monitor_height > target_height) {
        SetWindowPosition((monitor_width - target_width) / 2, (monitor_height - target_height) / 2);
    }
}

int main(void)
{
    CompanionState state;
    init_state(&state);

    SetConfigFlags(FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(1500, 900, "Attune Helper Companion");
    SetExitKey(0);
    apply_window_defaults();
    apply_monitor_defaults(&state);
    load_ui_font();
    load_ui_images();
    {
        void *wh = GetWindowHandle();
        if (wh) {
            ahc_tray_install(wh);
        }
    }
    GuiSetStyle(DEFAULT, TEXT_SIZE, 18);
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, color_to_gui_hex(AHC_PANEL_DARK));
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, color_to_gui_hex(AHC_BORDER));
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, color_to_gui_hex(AHC_TEXT));
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, color_to_gui_hex(AHC_PANEL_HOVER));
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, color_to_gui_hex(AHC_BORDER_BRIGHT));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, color_to_gui_hex(AHC_TEXT));
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, color_to_gui_hex((Color){ 13, 21, 34, 255 }));
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, color_to_gui_hex(AHC_ACCENT));
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, color_to_gui_hex(AHC_TEXT));
    GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, color_to_gui_hex(AHC_PANEL_HOVER));
    GuiSetStyle(BUTTON, BORDER_COLOR_FOCUSED, color_to_gui_hex(AHC_BORDER_BRIGHT));
    GuiSetStyle(BUTTON, BASE_COLOR_PRESSED, color_to_gui_hex((Color){ 18, 29, 44, 255 }));
    GuiSetStyle(BUTTON, BORDER_COLOR_PRESSED, color_to_gui_hex(AHC_ACCENT));
    GuiSetStyle(BUTTON, TEXT_COLOR_PRESSED, color_to_gui_hex(AHC_TEXT));

    {
        int last_fps_target = -1;
        while (!g_quit) {
        uint32_t acts = ahc_tray_consume_actions();
        if (acts & AHC_TRAYA_EXIT) {
            g_quit = true;
        }
        if (acts & AHC_TRAYA_BACKGROUND) {
            SetWindowState(FLAG_WINDOW_HIDDEN);
        }
        if (acts & AHC_TRAYA_SHOW) {
            ClearWindowState(FLAG_WINDOW_HIDDEN);
            RestoreWindow();
            SetWindowFocused();
        }
        if (g_quit) {
            break;
        }

        if (IsKeyPressed(KEY_F11) || ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && IsKeyPressed(KEY_ENTER))) {
            ToggleFullscreen();
        }

        update_ui_scale(&state);
        poll_addon_install_job(&state);
        poll_attunehelper_snapshot(&state);

        bool throttled = IsWindowHidden() || IsWindowMinimized();
        if (!throttled) {
            ahc_process_avatar_deferred_loads();
        }
        {
            int want = throttled ? AHC_HIDDEN_FPS : ahc_display_fps_cap(&state);
            if (want != last_fps_target) {
                SetTargetFPS(want);
                last_fps_target = want;
            }
        }
        if (throttled) {
            PollInputEvents();
            WaitTime(0.20f);
        }

        if (IsWindowHidden() || IsWindowMinimized()) {
            continue;
        }

        BeginDrawing();
        ClearBackground(AHC_BACKGROUND);
        DrawRectangleGradientV(
            0,
            72 + AHC_CHROME_H,
            GetScreenWidth(),
            GetScreenHeight() - (72 + AHC_CHROME_H),
            AHC_BACKGROUND_TOP,
            AHC_BACKGROUND_BOTTOM);
        DrawCircle(GetScreenWidth() - 120, 132 + AHC_CHROME_H, 220, (Color){ 55, 112, 175, 42 });
        DrawCircle(120, GetScreenHeight() - 86, 180, (Color){ 25, 56, 93, 38 });

        draw_window_chrome(&state);
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

        draw_process_footer(&state);

        EndDrawing();
        }
    }

    {
        void *wh = GetWindowHandle();
        if (wh) {
            ahc_tray_shutdown(wh);
        }
    }
    CloseWindow();
    companion_dispose_phone_qr(&state);
    ahc_addon_manifest_free(&state.addon_manifest);
    if (g_has_ui_font) {
        UnloadFont(g_ui_font);
    }
    for (size_t i = 0; i < g_avatar_cache_count; i++) {
        if (g_avatar_cache[i].loaded) {
            UnloadTexture(g_avatar_cache[i].texture);
        }
    }
    if (g_avatar_placeholder_valid) {
        UnloadTexture(g_avatar_placeholder);
        g_avatar_placeholder_valid = false;
    }
    return 0;
}
