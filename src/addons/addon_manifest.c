#include "addons/addon_manifest.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct AddonFields {
    char *name;
    char *author;
    char *category;
    char *folder;
    char *repo;
    char *description;
    char *source_subdir;
    char *install_url;
} AddonFields;

static const char *json_skip_ws(const char *cursor)
{
    while (*cursor && isspace((unsigned char)*cursor)) {
        cursor++;
    }
    return cursor;
}

static char *read_text_file(const char *path)
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

    char *text = (char *)malloc((size_t)length + 1);
    if (!text) {
        fclose(file);
        return NULL;
    }

    size_t read_count = fread(text, 1, (size_t)length, file);
    fclose(file);
    text[read_count] = '\0';
    return text;
}


static void replace_owned_string(char **target, char *value)
{
    free(*target);
    *target = value;
}

static bool parse_json_string(const char **cursor, char **out)
{
    const char *current = json_skip_ws(*cursor);
    if (*current != '"') {
        return false;
    }
    current++;

    size_t capacity = strlen(current) + 1;
    char *decoded = (char *)malloc(capacity);
    if (!decoded) {
        return false;
    }

    size_t length = 0;
    while (*current) {
        unsigned char ch = (unsigned char)*current++;
        if (ch == '"') {
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
                    decoded[length++] = (char)ch;
                    break;
                case 'b':
                    decoded[length++] = '\b';
                    break;
                case 'f':
                    decoded[length++] = '\f';
                    break;
                case 'n':
                    decoded[length++] = '\n';
                    break;
                case 'r':
                    decoded[length++] = '\r';
                    break;
                case 't':
                    decoded[length++] = '\t';
                    break;
                case 'u':
                    if (strlen(current) < 4) {
                        free(decoded);
                        return false;
                    }
                    current += 4;
                    decoded[length++] = '?';
                    break;
                default:
                    free(decoded);
                    return false;
            }
        } else {
            decoded[length++] = (char)ch;
        }
    }

    free(decoded);
    return false;
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
        char *key = NULL;
        cursor = json_skip_ws(cursor);
        if (*cursor == '}') {
            return cursor + 1;
        }
        if (!parse_json_string(&cursor, &key)) {
            return NULL;
        }
        free(key);

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
        char *ignored = NULL;
        if (!parse_json_string(&cursor, &ignored)) {
            return NULL;
        }
        free(ignored);
        return cursor;
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

static void free_addon_fields(AddonFields *fields)
{
    free(fields->name);
    free(fields->author);
    free(fields->category);
    free(fields->folder);
    free(fields->repo);
    free(fields->description);
    free(fields->source_subdir);
    free(fields->install_url);
    memset(fields, 0, sizeof(*fields));
}

static const char *parse_install_object(const char *cursor, AddonFields *fields)
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
            replace_owned_string(&fields->install_url, value);
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

static const char *parse_addon_object(const char *cursor, AhcAddonManifest *manifest)
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
            free_addon_fields(&fields);
            return NULL;
        }

        cursor = json_skip_ws(cursor);
        if (*cursor != ':') {
            free(key);
            free_addon_fields(&fields);
            return NULL;
        }
        cursor++;

        if (strcmp(key, "name") == 0) {
            char *value = NULL;
            if (!parse_json_string(&cursor, &value)) {
                free(key);
                free_addon_fields(&fields);
                return NULL;
            }
            replace_owned_string(&fields.name, value);
        } else if (strcmp(key, "author") == 0) {
            char *value = NULL;
            if (!parse_json_string(&cursor, &value)) {
                free(key);
                free_addon_fields(&fields);
                return NULL;
            }
            replace_owned_string(&fields.author, value);
        } else if (strcmp(key, "category") == 0) {
            char *value = NULL;
            if (!parse_json_string(&cursor, &value)) {
                free(key);
                free_addon_fields(&fields);
                return NULL;
            }
            replace_owned_string(&fields.category, value);
        } else if (strcmp(key, "folder") == 0) {
            char *value = NULL;
            if (!parse_json_string(&cursor, &value)) {
                free(key);
                free_addon_fields(&fields);
                return NULL;
            }
            replace_owned_string(&fields.folder, value);
        } else if (strcmp(key, "repo") == 0) {
            char *value = NULL;
            if (!parse_json_string(&cursor, &value)) {
                free(key);
                free_addon_fields(&fields);
                return NULL;
            }
            replace_owned_string(&fields.repo, value);
        } else if (strcmp(key, "description") == 0) {
            char *value = NULL;
            if (!parse_json_string(&cursor, &value)) {
                free(key);
                free_addon_fields(&fields);
                return NULL;
            }
            replace_owned_string(&fields.description, value);
        } else if (strcmp(key, "source_subdir") == 0) {
            char *value = NULL;
            if (!parse_json_string(&cursor, &value)) {
                free(key);
                free_addon_fields(&fields);
                return NULL;
            }
            replace_owned_string(&fields.source_subdir, value);
        } else if (strcmp(key, "install") == 0) {
            cursor = parse_install_object(cursor, &fields);
            if (!cursor) {
                free(key);
                free_addon_fields(&fields);
                return NULL;
            }
        } else {
            cursor = json_skip_value(cursor);
            if (!cursor) {
                free(key);
                free_addon_fields(&fields);
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

        free_addon_fields(&fields);
        return NULL;
    }

    if (fields.install_url) {
        replace_owned_string(&fields.repo, fields.install_url);
        fields.install_url = NULL;
    }

    if (!fields.name || !fields.author || !fields.category || !fields.folder || !fields.repo || !fields.description) {
        free_addon_fields(&fields);
        return NULL;
    }

    AhcAddon addon;
    addon.name = fields.name;
    addon.author = fields.author;
    addon.category = fields.category;
    addon.folder = fields.folder;
    addon.repo = fields.repo;
    addon.description = fields.description;
    addon.source_subdir = fields.source_subdir;
    memset(&fields, 0, sizeof(fields));

    if (!append_addon(manifest, &addon)) {
        free((char *)addon.name);
        free((char *)addon.author);
        free((char *)addon.category);
        free((char *)addon.folder);
        free((char *)addon.repo);
        free((char *)addon.description);
        free((char *)addon.source_subdir);
        return NULL;
    }

    return cursor;
}

bool ahc_addon_manifest_load_file(const char *path, AhcAddonManifest *manifest)
{
    if (!manifest) {
        return false;
    }

    AhcAddonManifest loaded;
    memset(&loaded, 0, sizeof(loaded));

    char *text = read_text_file(path);
    if (!text) {
        return false;
    }

    const char *addons_key = strstr(text, "\"addons\"");
    if (!addons_key) {
        free(text);
        return false;
    }

    const char *cursor = strchr(addons_key, '[');
    if (!cursor) {
        free(text);
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

        cursor = parse_addon_object(cursor, &loaded);
        if (!cursor) {
            ahc_addon_manifest_free(&loaded);
            free(text);
            return false;
        }
    }

    free(text);
    if (loaded.count == 0) {
        ahc_addon_manifest_free(&loaded);
        return false;
    }

    *manifest = loaded;
    return true;
}

void ahc_addon_manifest_free(AhcAddonManifest *manifest)
{
    if (!manifest) {
        return;
    }

    for (size_t i = 0; i < manifest->count; i++) {
        free((char *)manifest->items[i].name);
        free((char *)manifest->items[i].author);
        free((char *)manifest->items[i].category);
        free((char *)manifest->items[i].folder);
        free((char *)manifest->items[i].repo);
        free((char *)manifest->items[i].description);
        free((char *)manifest->items[i].source_subdir);
    }

    free(manifest->items);
    manifest->items = NULL;
    manifest->count = 0;
}
