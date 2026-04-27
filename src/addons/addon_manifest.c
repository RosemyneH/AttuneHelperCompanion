#include "addons/addon_manifest.h"
#include "ahc/ahc_arena.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct AhcManifestStorage {
    AhcArena strings;
    char *file_text;
} AhcManifestStorage;

typedef struct AddonFields {
    char *id;
    char *name;
    char *author;
    char *source;
    char *category;
    const char **categories;
    size_t category_count;
    char *folder;
    char *repo;
    char *description;
    char *source_subdir;
    char *avatar_url;
    char *version;
    char *page_url;
    char *install_url;
} AddonFields;

static const char *json_skip_ws(const char *cursor)
{
    while (*cursor && isspace((unsigned char)*cursor)) {
        cursor++;
    }
    return cursor;
}

static char *read_file_malloc(const char *path, size_t *out_len)
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long length = ftell(file);
    if (length < 0) {
        fclose(file);
        return NULL;
    }
    rewind(file);
    char *text = (char *)malloc((size_t)length + 1u);
    if (!text) {
        fclose(file);
        return NULL;
    }
    size_t read_count = fread(text, 1, (size_t)length, file);
    fclose(file);
    text[read_count] = '\0';
    if (out_len) {
        *out_len = read_count;
    }
    return text;
}

static void rebase_string_ptr(
    const char **field,
    const uint8_t *old_base,
    uint8_t *new_base,
    size_t region_len
)
{
    if (!field || !*field) {
        return;
    }
    const uint8_t *p = (const uint8_t *)*field;
    if (p < old_base || p >= old_base + region_len) {
        return;
    }
    *field = (const char *)(new_base + (size_t)(p - old_base));
}

static void rebase_ptr(
    const void **field,
    const uint8_t *old_base,
    uint8_t *new_base,
    size_t region_len
)
{
    if (!field || !*field) {
        return;
    }
    const uint8_t *p = (const uint8_t *)*field;
    if (p < old_base || p >= old_base + region_len) {
        return;
    }
    *field = (const void *)(new_base + (size_t)(p - old_base));
}

static void ahc_compact_manifest_string_arena(
    AhcManifestStorage *st, AhcAddon *items, size_t n
)
{
    AhcArena *a = &st->strings;
    if (!a->data || a->used == 0u) {
        return;
    }
    if (a->cap <= a->used) {
        return;
    }
    if (a->cap - a->used < 16u * 1024u) {
        return;
    }

    uint8_t *new_block = (uint8_t *)malloc(a->used);
    if (new_block == NULL) {
        return;
    }
    memcpy(new_block, a->data, a->used);
    const uint8_t *ob = a->data;
    const size_t u = a->used;

    for (size_t i = 0; i < n; i++) {
        AhcAddon *x = &items[i];
        rebase_string_ptr((const char **)&x->id, ob, new_block, u);
        rebase_string_ptr(&x->name, ob, new_block, u);
        rebase_string_ptr(&x->author, ob, new_block, u);
        rebase_string_ptr(&x->category, ob, new_block, u);
        rebase_string_ptr(&x->folder, ob, new_block, u);
        rebase_string_ptr(&x->repo, ob, new_block, u);
        rebase_string_ptr(&x->description, ob, new_block, u);
        rebase_string_ptr(&x->source_subdir, ob, new_block, u);
        rebase_string_ptr(&x->avatar_url, ob, new_block, u);
        rebase_string_ptr(&x->version, ob, new_block, u);
        rebase_string_ptr(&x->source, ob, new_block, u);
        rebase_string_ptr(&x->page_url, ob, new_block, u);
        rebase_ptr((const void **)&x->categories, ob, new_block, u);
        for (size_t j = 0; j < x->category_count; j++) {
            rebase_string_ptr(&x->categories[j], ob, new_block, u);
        }
    }

    free(a->data);
    a->data = new_block;
    a->cap = a->used;
}

static void set_field_arena(AhcArena *arena, char **field, char *value)
{
    if (!value) {
        return;
    }
    char *p = ahc_arena_strcpy(arena, value);
    free(value);
    if (p) {
        *field = p;
    }
}

static bool parse_json_string_grow(char **decoded, size_t *capacity, size_t length, size_t need)
{
    if (length + need < *capacity) {
        return true;
    }
    size_t new_cap = *capacity;
    while (length + need >= new_cap) {
        if (new_cap < 64u) {
            new_cap = 64u;
            continue;
        }
        size_t doubled = new_cap * 2u;
        if (doubled < new_cap) {
            return false;
        }
        new_cap = doubled;
    }
    char *next = (char *)realloc(*decoded, new_cap);
    if (!next) {
        return false;
    }
    *decoded = next;
    *capacity = new_cap;
    return true;
}

static bool parse_json_string(const char **cursor, char **out)
{
    const char *current = json_skip_ws(*cursor);
    if (*current != '"') {
        return false;
    }
    current++;

    size_t capacity = 64;
    char *decoded = (char *)malloc(capacity);
    if (!decoded) {
        return false;
    }

    size_t length = 0;
    while (*current) {
        unsigned char ch = (unsigned char)*current++;
        if (ch == '"') {
            if (!parse_json_string_grow(&decoded, &capacity, length, 1)) {
                free(decoded);
                return false;
            }
            decoded[length] = '\0';
            *cursor = current;
            *out = decoded;
            return true;
        }

        if (ch == '\\') {
            ch = (unsigned char)*current++;
            switch (ch) {
                case '"':
                case '\\':
                case '/':
                    if (!parse_json_string_grow(&decoded, &capacity, length, 1)) {
                        free(decoded);
                        return false;
                    }
                    decoded[length++] = (char)ch;
                    break;
                case 'b':
                    if (!parse_json_string_grow(&decoded, &capacity, length, 1)) {
                        free(decoded);
                        return false;
                    }
                    decoded[length++] = '\b';
                    break;
                case 'f':
                    if (!parse_json_string_grow(&decoded, &capacity, length, 1)) {
                        free(decoded);
                        return false;
                    }
                    decoded[length++] = '\f';
                    break;
                case 'n':
                    if (!parse_json_string_grow(&decoded, &capacity, length, 1)) {
                        free(decoded);
                        return false;
                    }
                    decoded[length++] = '\n';
                    break;
                case 'r':
                    if (!parse_json_string_grow(&decoded, &capacity, length, 1)) {
                        free(decoded);
                        return false;
                    }
                    decoded[length++] = '\r';
                    break;
                case 't':
                    if (!parse_json_string_grow(&decoded, &capacity, length, 1)) {
                        free(decoded);
                        return false;
                    }
                    decoded[length++] = '\t';
                    break;
                case 'u':
                    if (strlen(current) < 4) {
                        free(decoded);
                        return false;
                    }
                    current += 4;
                    if (!parse_json_string_grow(&decoded, &capacity, length, 1)) {
                        free(decoded);
                        return false;
                    }
                    decoded[length++] = '?';
                    break;
                default:
                    free(decoded);
                    return false;
            }
        } else {
            if (!parse_json_string_grow(&decoded, &capacity, length, 1)) {
                free(decoded);
                return false;
            }
            decoded[length++] = (char)ch;
        }
    }

    free(decoded);
    return false;
}

static const char *parse_json_string_array(
    AhcArena *arena, const char *cursor, const char ***out_items, size_t *out_count
)
{
    cursor = json_skip_ws(cursor);
    if (*cursor != '[') {
        return NULL;
    }
    cursor++;

    const char **items = NULL;
    size_t count = 0;
    size_t capacity = 0;

    for (;;) {
        cursor = json_skip_ws(cursor);
        if (*cursor == ']') {
            cursor++;
            break;
        }

        char *value = NULL;
        if (!parse_json_string(&cursor, &value)) {
            free(items);
            return NULL;
        }

        char *stored = ahc_arena_strcpy(arena, value);
        free(value);
        if (!stored) {
            free(items);
            return NULL;
        }

        if (count == capacity) {
            size_t new_capacity = capacity ? capacity * 2u : 4u;
            const char **next = (const char **)realloc(items, new_capacity * sizeof(items[0]));
            if (!next) {
                free(items);
                return NULL;
            }
            items = next;
            capacity = new_capacity;
        }
        items[count++] = stored;

        cursor = json_skip_ws(cursor);
        if (*cursor == ',') {
            cursor++;
            continue;
        }
        if (*cursor == ']') {
            cursor++;
            break;
        }

        free(items);
        return NULL;
    }

    if (count == 0) {
        free(items);
        return NULL;
    }

    const char **stored_items = (const char **)ahc_arena_alloc(arena, count * sizeof(stored_items[0]), sizeof(void *));
    if (!stored_items) {
        free(items);
        return NULL;
    }
    memcpy(stored_items, items, count * sizeof(stored_items[0]));
    free(items);

    *out_items = stored_items;
    *out_count = count;
    return cursor;
}

static const char *json_skip_quoted_string(const char *cursor)
{
    if (!cursor || *cursor != '"') {
        return NULL;
    }
    cursor++;
    while (*cursor) {
        if (*cursor == '"') {
            return cursor + 1;
        }
        if (*cursor == '\\') {
            cursor++;
            if (!*cursor) {
                return NULL;
            }
            if (*cursor == 'u') {
                cursor++;
                for (int i = 0; i < 4; i++) {
                    if (!*cursor) {
                        return NULL;
                    }
                    cursor++;
                }
            } else {
                cursor++;
            }
            continue;
        }
        cursor++;
    }
    return NULL;
}

static const char *json_skip_value(const char *cursor);

static const char *json_skip_object(const char *cursor)
{
    cursor = json_skip_ws(cursor);
    if (*cursor != '{') {
        return NULL;
    }
    cursor++;

    for (;;) {
        cursor = json_skip_ws(cursor);
        if (*cursor == '}') {
            return cursor + 1;
        }
        if (*cursor != '"') {
            return NULL;
        }
        cursor = json_skip_quoted_string(cursor);
        if (!cursor) {
            return NULL;
        }

        cursor = json_skip_ws(cursor);
        if (*cursor != ':') {
            return NULL;
        }
        cursor++;

        cursor = json_skip_value(cursor);
        if (!cursor) {
            return NULL;
        }

        cursor = json_skip_ws(cursor);
        if (*cursor == ',') {
            cursor++;
            continue;
        }
        if (*cursor == '}') {
            return cursor + 1;
        }
        return NULL;
    }
}

static const char *json_skip_array(const char *cursor)
{
    cursor = json_skip_ws(cursor);
    if (*cursor != '[') {
        return NULL;
    }
    cursor++;

    for (;;) {
        cursor = json_skip_ws(cursor);
        if (*cursor == ']') {
            return cursor + 1;
        }

        cursor = json_skip_value(cursor);
        if (!cursor) {
            return NULL;
        }

        cursor = json_skip_ws(cursor);
        if (*cursor == ',') {
            cursor++;
            continue;
        }
        if (*cursor == ']') {
            return cursor + 1;
        }
        return NULL;
    }
}

static const char *json_skip_scalar(const char *cursor)
{
    cursor = json_skip_ws(cursor);
    while (*cursor && *cursor != ',' && *cursor != '}' && *cursor != ']') {
        cursor++;
    }
    return cursor;
}

static const char *json_skip_value(const char *cursor)
{
    cursor = json_skip_ws(cursor);
    if (*cursor == '"') {
        return json_skip_quoted_string(cursor);
    }
    if (*cursor == '{') {
        return json_skip_object(cursor);
    }
    if (*cursor == '[') {
        return json_skip_array(cursor);
    }
    if (*cursor) {
        return json_skip_scalar(cursor);
    }
    return NULL;
}

static void clear_addon_fields(AddonFields *fields)
{
    memset(fields, 0, sizeof(*fields));
}

static const char *parse_install_object(
    AhcArena *arena, const char *cursor, AddonFields *fields
)
{
    cursor = json_skip_ws(cursor);
    if (*cursor != '{') {
        return json_skip_value(cursor);
    }
    cursor++;

    for (;;) {
        char *key = NULL;
        cursor = json_skip_ws(cursor);
        if (*cursor == '}') {
            return cursor + 1;
        }
        if (!parse_json_string(&cursor, &key)) {
            return NULL;
        }

        cursor = json_skip_ws(cursor);
        if (*cursor != ':') {
            free(key);
            return NULL;
        }
        cursor++;

        if (strcmp(key, "url") == 0) {
            char *value = NULL;
            if (!parse_json_string(&cursor, &value)) {
                free(key);
                return NULL;
            }
            set_field_arena(arena, (char **)&fields->install_url, value);
        } else {
            cursor = json_skip_value(cursor);
            if (!cursor) {
                free(key);
                return NULL;
            }
        }

        free(key);
        cursor = json_skip_ws(cursor);
        if (*cursor == ',') {
            cursor++;
            continue;
        }
        if (*cursor == '}') {
            return cursor + 1;
        }
        return NULL;
    }
}

static bool append_addon(AhcAddonManifest *manifest, const AhcAddon *addon)
{
    size_t new_count = manifest->count + 1;
    AhcAddon *items = (AhcAddon *)realloc(manifest->items, new_count * sizeof(manifest->items[0]));
    if (!items) {
        return false;
    }

    manifest->items = items;
    manifest->items[manifest->count] = *addon;
    manifest->count = new_count;
    return true;
}

static const char *parse_addon_object(
    AhcArena *arena, const char *cursor, AhcAddonManifest *manifest
)
{
    AddonFields fields;
    memset(&fields, 0, sizeof(fields));

    cursor = json_skip_ws(cursor);
    if (*cursor != '{') {
        return NULL;
    }
    cursor++;

    for (;;) {
        char *key = NULL;
        cursor = json_skip_ws(cursor);
        if (*cursor == '}') {
            cursor++;
            break;
        }

        if (!parse_json_string(&cursor, &key)) {
            clear_addon_fields(&fields);
            return NULL;
        }

        cursor = json_skip_ws(cursor);
        if (*cursor != ':') {
            free(key);
            clear_addon_fields(&fields);
            return NULL;
        }
        cursor++;

        if (strcmp(key, "id") == 0) {
            char *value = NULL;
            if (!parse_json_string(&cursor, &value)) {
                free(key);
                clear_addon_fields(&fields);
                return NULL;
            }
            set_field_arena(arena, &fields.id, value);
        } else if (strcmp(key, "name") == 0) {
            char *value = NULL;
            if (!parse_json_string(&cursor, &value)) {
                free(key);
                clear_addon_fields(&fields);
                return NULL;
            }
            set_field_arena(arena, &fields.name, value);
        } else if (strcmp(key, "author") == 0) {
            char *value = NULL;
            if (!parse_json_string(&cursor, &value)) {
                free(key);
                clear_addon_fields(&fields);
                return NULL;
            }
            set_field_arena(arena, &fields.author, value);
        } else if (strcmp(key, "source") == 0) {
            char *value = NULL;
            if (!parse_json_string(&cursor, &value)) {
                free(key);
                clear_addon_fields(&fields);
                return NULL;
            }
            set_field_arena(arena, &fields.source, value);
        } else if (strcmp(key, "category") == 0) {
            char *value = NULL;
            if (!parse_json_string(&cursor, &value)) {
                free(key);
                clear_addon_fields(&fields);
                return NULL;
            }
            set_field_arena(arena, &fields.category, value);
        } else if (strcmp(key, "categories") == 0) {
            cursor = parse_json_string_array(arena, cursor, &fields.categories, &fields.category_count);
            if (!cursor) {
                free(key);
                clear_addon_fields(&fields);
                return NULL;
            }
        } else if (strcmp(key, "folder") == 0) {
            char *value = NULL;
            if (!parse_json_string(&cursor, &value)) {
                free(key);
                clear_addon_fields(&fields);
                return NULL;
            }
            set_field_arena(arena, &fields.folder, value);
        } else if (strcmp(key, "repo") == 0) {
            char *value = NULL;
            if (!parse_json_string(&cursor, &value)) {
                free(key);
                clear_addon_fields(&fields);
                return NULL;
            }
            set_field_arena(arena, &fields.repo, value);
        } else if (strcmp(key, "description") == 0) {
            char *value = NULL;
            if (!parse_json_string(&cursor, &value)) {
                free(key);
                clear_addon_fields(&fields);
                return NULL;
            }
            set_field_arena(arena, &fields.description, value);
        } else if (strcmp(key, "source_subdir") == 0) {
            char *value = NULL;
            if (!parse_json_string(&cursor, &value)) {
                free(key);
                clear_addon_fields(&fields);
                return NULL;
            }
            set_field_arena(arena, &fields.source_subdir, value);
        } else if (strcmp(key, "avatar_url") == 0) {
            char *value = NULL;
            if (!parse_json_string(&cursor, &value)) {
                free(key);
                clear_addon_fields(&fields);
                return NULL;
            }
            set_field_arena(arena, &fields.avatar_url, value);
        } else if (strcmp(key, "version") == 0) {
            char *value = NULL;
            if (!parse_json_string(&cursor, &value)) {
                free(key);
                clear_addon_fields(&fields);
                return NULL;
            }
            set_field_arena(arena, &fields.version, value);
        } else if (strcmp(key, "page_url") == 0) {
            char *value = NULL;
            if (!parse_json_string(&cursor, &value)) {
                free(key);
                clear_addon_fields(&fields);
                return NULL;
            }
            set_field_arena(arena, &fields.page_url, value);
        } else if (strcmp(key, "install") == 0) {
            cursor = parse_install_object(arena, cursor, &fields);
            if (!cursor) {
                free(key);
                clear_addon_fields(&fields);
                return NULL;
            }
        } else {
            cursor = json_skip_value(cursor);
            if (!cursor) {
                free(key);
                clear_addon_fields(&fields);
                return NULL;
            }
        }

        free(key);
        cursor = json_skip_ws(cursor);
        if (*cursor == ',') {
            cursor++;
            continue;
        }
        if (*cursor == '}') {
            cursor++;
            break;
        }

        clear_addon_fields(&fields);
        return NULL;
    }

    if (fields.install_url) {
        fields.repo = fields.install_url;
        fields.install_url = NULL;
    }

    if (!fields.categories && fields.category) {
        const char **categories = (const char **)ahc_arena_alloc(arena, sizeof(categories[0]), sizeof(void *));
        if (!categories) {
            clear_addon_fields(&fields);
            return NULL;
        }
        categories[0] = fields.category;
        fields.categories = categories;
        fields.category_count = 1u;
    } else if (!fields.category && fields.categories && fields.category_count > 0u) {
        fields.category = (char *)fields.categories[0];
    }

    if (!fields.id || !fields.name || !fields.author || !fields.category || !fields.folder || !fields.repo || !fields.description) {
        clear_addon_fields(&fields);
        return NULL;
    }

    AhcAddon addon;
    addon.id = fields.id;
    addon.name = fields.name;
    addon.author = fields.author;
    if (fields.source) {
        addon.source = fields.source;
    } else {
        char *d = ahc_arena_strcpy(arena, "GitHub");
        if (!d) {
            clear_addon_fields(&fields);
            return NULL;
        }
        addon.source = d;
    }
    addon.category = fields.category;
    addon.folder = fields.folder;
    addon.repo = fields.repo;
    addon.description = fields.description;
    addon.source_subdir = fields.source_subdir;
    addon.avatar_url = fields.avatar_url;
    addon.version = fields.version;
    addon.page_url = fields.page_url;
    addon.categories = fields.categories;
    addon.category_count = fields.category_count;
    fields.source = NULL;
    memset(&fields, 0, sizeof(fields));

    if (!append_addon(manifest, &addon)) {
        return NULL;
    }

    return cursor;
}

bool ahc_addon_manifest_load_file(const char *path, AhcAddonManifest *manifest)
{
    if (!manifest) {
        return false;
    }

    AhcManifestStorage *st = (AhcManifestStorage *)calloc(1, sizeof(*st));
    if (!st) {
        return false;
    }

    AhcAddonManifest loaded;
    memset(&loaded, 0, sizeof(loaded));
    loaded.storage = st;

    st->file_text = read_file_malloc(path, NULL);
    if (!st->file_text) {
        free(st);
        return false;
    }
    {
        const size_t slen = strlen(st->file_text);
        /* Decoded string bytes plus alignment: stay below this to avoid arena realloc. */
        size_t pool = slen + (slen >> 2) + 256u * 1024u;
        if (pool < 32u * 1024u) {
            pool = 32u * 1024u;
        }
        if (!ahc_arena_init(&st->strings, pool + 1u)) {
            free(st->file_text);
            free(st);
            return false;
        }
    }

    char *text = st->file_text;

    const char *addons_key = strstr(text, "\"addons\"");
    if (!addons_key) {
        ahc_addon_manifest_free(&loaded);
        return false;
    }

    const char *cursor = strchr(addons_key, '[');
    if (!cursor) {
        ahc_addon_manifest_free(&loaded);
        return false;
    }
    cursor++;

    for (;;) {
        cursor = json_skip_ws(cursor);
        if (*cursor == ']') {
            break;
        }
        if (*cursor == ',') {
            cursor++;
            continue;
        }

        cursor = parse_addon_object(&st->strings, cursor, &loaded);
        if (!cursor) {
            ahc_addon_manifest_free(&loaded);
            return false;
        }
    }

    if (loaded.count == 0) {
        ahc_addon_manifest_free(&loaded);
        return false;
    }

    ahc_compact_manifest_string_arena(st, loaded.items, loaded.count);

    free(st->file_text);
    st->file_text = NULL;

    *manifest = loaded;
    return true;
}

void ahc_addon_manifest_free(AhcAddonManifest *manifest)
{
    if (!manifest) {
        return;
    }

    free(manifest->items);
    manifest->items = NULL;
    manifest->count = 0;

    AhcManifestStorage *st = (AhcManifestStorage *)manifest->storage;
    manifest->storage = NULL;
    if (st) {
        free(st->file_text);
        st->file_text = NULL;
        ahc_arena_free(&st->strings);
        free(st);
    }
}

size_t ahc_addon_manifest_storage_bytes(const AhcAddonManifest *manifest)
{
    if (!manifest || !manifest->storage) {
        return 0u;
    }
    AhcManifestStorage *st = (AhcManifestStorage *)manifest->storage;
    return ahc_arena_used(&st->strings);
}
