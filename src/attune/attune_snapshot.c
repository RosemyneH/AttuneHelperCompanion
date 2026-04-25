#include "attune/attune_snapshot.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *skip_space(const char *cursor)
{
    while (*cursor && isspace((unsigned char)*cursor)) {
        cursor++;
    }

    return cursor;
}

static const char *find_snapshot_table(const char *text)
{
    const char *key = strstr(text, "[\"DailyAttuneSnapshot\"]");
    if (!key) {
        return NULL;
    }

    const char *open = strchr(key, '{');
    if (!open) {
        return NULL;
    }

    return open + 1;
}

static bool read_lua_string_field(const char *table, const char *name, char *out, size_t out_capacity)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "[\"%s\"]", name);

    const char *field = strstr(table, pattern);
    if (!field) {
        return false;
    }

    const char *equals = strchr(field, '=');
    if (!equals) {
        return false;
    }

    const char *value = skip_space(equals + 1);
    if (*value != '"') {
        return false;
    }

    value++;

    size_t written = 0;
    while (value[written] && value[written] != '"' && written + 1 < out_capacity) {
        out[written] = value[written];
        written++;
    }

    out[written] = '\0';
    return value[written] == '"';
}

static bool read_lua_int_field(const char *table, const char *name, int *out)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "[\"%s\"]", name);

    const char *field = strstr(table, pattern);
    if (!field) {
        return false;
    }

    const char *equals = strchr(field, '=');
    if (!equals) {
        return false;
    }

    const char *value = skip_space(equals + 1);
    char *end = NULL;
    long parsed = strtol(value, &end, 10);

    if (end == value) {
        return false;
    }

    *out = (int)parsed;
    return true;
}

bool ahc_parse_daily_attune_snapshot(const char *text, AhcDailyAttuneSnapshot *out)
{
    if (!text || !out) {
        return false;
    }

    memset(out, 0, sizeof(*out));

    const char *table = find_snapshot_table(text);
    if (!table) {
        return false;
    }

    read_lua_string_field(table, "date", out->date, sizeof(out->date));
    read_lua_int_field(table, "account", &out->account);
    read_lua_int_field(table, "warforged", &out->warforged);
    read_lua_int_field(table, "lightforged", &out->lightforged);
    read_lua_int_field(table, "titanforged", &out->titanforged);

    out->found = out->date[0] != '\0'
        || out->account != 0
        || out->warforged != 0
        || out->lightforged != 0
        || out->titanforged != 0;

    return out->found;
}

bool ahc_parse_daily_attune_snapshot_file(const char *path, AhcDailyAttuneSnapshot *out)
{
    if (!path || !out) {
        return false;
    }

    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }

    long length = ftell(file);
    if (length < 0) {
        fclose(file);
        return false;
    }

    rewind(file);

    char *buffer = (char *)malloc((size_t)length + 1);
    if (!buffer) {
        fclose(file);
        return false;
    }

    size_t read_count = fread(buffer, 1, (size_t)length, file);
    fclose(file);

    buffer[read_count] = '\0';
    bool parsed = ahc_parse_daily_attune_snapshot(buffer, out);

    free(buffer);
    return parsed;
}
