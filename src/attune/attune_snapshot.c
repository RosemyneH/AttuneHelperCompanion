#include "attune/attune_snapshot.h"
#include "ahc/ahc_find_literal.h"

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AHC_SNAPSHOT_READ_MAX (8u * 1024u * 1024u)

static const char AHC_KEY_DAILY[] = "[\"DailyAttuneSnapshot\"]";
#define AHC_KEY_DAILY_LEN (sizeof(AHC_KEY_DAILY) - 1u)

static const char *skip_ws(const char *cursor)
{
    while (*cursor && isspace((unsigned char)*cursor)) {
        cursor++;
    }
    return cursor;
}

static const char *read_quoted_unescaped(
    const char *p, char *out, size_t out_cap
)
{
    if (*p != '"') {
        return NULL;
    }
    p++;
    size_t w = 0;
    while (*p && *p != '"' && w + 1 < out_cap) {
        out[w++] = *p;
        p++;
    }
    if (*p != '"') {
        return NULL;
    }
    out[w] = '\0';
    return p + 1u;
}

static const char *read_lua_bracket_quoted_key(
    const char *p, char *buf, size_t buf_cap
)
{
    p = skip_ws(p);
    if (*p != '[') {
        return NULL;
    }
    p++;
    p = skip_ws(p);
    if (*p != '"') {
        return NULL;
    }
    p++;
    size_t n = 0;
    while (*p && n + 1 < buf_cap) {
        if (*p == '"') {
            break;
        }
        buf[n++] = (char)*p++;
    }
    if (*p != '"') {
        return NULL;
    }
    buf[n] = '\0';
    p++;
    p = skip_ws(p);
    if (*p != ']') {
        return NULL;
    }
    p++;
    p = skip_ws(p);
    if (*p != '=') {
        return NULL;
    }
    p++;
    return skip_ws(p);
}

static const char *end_of_quoted(const char *p)
{
    if (*p != '"') {
        return NULL;
    }
    p++;
    while (*p && *p != '"') {
        p++;
    }
    if (*p != '"') {
        return NULL;
    }
    return p + 1u;
}

static const char *end_of_scalar(const char *p)
{
    p = skip_ws(p);
    if (*p == '\0' || *p == ',' || *p == '}') {
        return p;
    }
    while (*p && *p != ',' && *p != '}') {
        p++;
    }
    return p;
}

static void apply_key_value(
    const char *key, const char *p, AhcDailyAttuneSnapshot *out
)
{
    p = skip_ws(p);
    if (strcmp(key, "date") == 0) {
        (void)read_quoted_unescaped(p, out->date, sizeof(out->date));
    } else if (strcmp(key, "account") == 0) {
        char *e = NULL;
        long v = strtol(p, &e, 10);
        (void)e;
        if (v >= INT_MIN && v <= INT_MAX) {
            out->account = (int)v;
        }
    } else if (strcmp(key, "warforged") == 0) {
        char *e = NULL;
        long v = strtol(p, &e, 10);
        if (v >= INT_MIN && v <= INT_MAX) {
            out->warforged = (int)v;
        }
    } else if (strcmp(key, "lightforged") == 0) {
        char *e = NULL;
        long v = strtol(p, &e, 10);
        if (v >= INT_MIN && v <= INT_MAX) {
            out->lightforged = (int)v;
        }
    } else if (strcmp(key, "titanforged") == 0) {
        char *e = NULL;
        long v = strtol(p, &e, 10);
        if (v >= INT_MIN && v <= INT_MAX) {
            out->titanforged = (int)v;
        }
    }
}

static const char *end_of_value(const char *p)
{
    p = skip_ws(p);
    if (*p == '"') {
        return end_of_quoted(p);
    }
    return end_of_scalar(p);
}

static const char *find_table_brace(
    const char *text, const char *text_end
)
{
    const char *k = ahc_find_literal(
        text, text_end, AHC_KEY_DAILY, AHC_KEY_DAILY_LEN
    );
    if (k == NULL) {
        return NULL;
    }
    if (text_end) {
        const void *e = (const void *)text_end;
        if (e <= (const void *)k) {
            return NULL;
        }
        return (const char *)memchr((const void *)k, (int)'{', (size_t)(text_end - k));
    }
    return strchr(k, '{');
}

static bool parse_snapshot_object(
    const char *table, AhcDailyAttuneSnapshot *out
)
{
    if (table[0] != '{') {
        return false;
    }
    const char *p = table + 1u;
    char keybuf[64];

    for (;;) {
        p = skip_ws(p);
        if (*p == '\0') {
            break;
        }
        if (*p == '}') {
            out->found = out->date[0] != '\0' || out->account != 0 || out->warforged != 0
                || out->lightforged != 0 || out->titanforged != 0;
            return out->found;
        }
        if (*p == ',') {
            p++;
            continue;
        }
        const char *vstart = read_lua_bracket_quoted_key(p, keybuf, sizeof(keybuf));
        if (vstart == NULL) {
            return false;
        }
        p = vstart;
        const char *vend = end_of_value(p);
        if (vend == NULL) {
            return false;
        }
        apply_key_value(keybuf, p, out);
        p = skip_ws(vend);
        if (*p == ',') {
            p++;
        }
    }

    out->found = out->date[0] != '\0' || out->account != 0 || out->warforged != 0
        || out->lightforged != 0 || out->titanforged != 0;
    return out->found;
}

bool ahc_parse_daily_attune_snapshot(
    const char *text, AhcDailyAttuneSnapshot *out
)
{
    if (!text || !out) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    const char *z = strchr(text, '\0');
    const char *open = find_table_brace(text, z);
    if (open == NULL) {
        return false;
    }
    return parse_snapshot_object(open, out);
}

bool ahc_parse_daily_attune_snapshot_file(
    const char *path, AhcDailyAttuneSnapshot *out
)
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
    if ((unsigned long)length > (unsigned long)AHC_SNAPSHOT_READ_MAX) {
        fclose(file);
        return false;
    }
    rewind(file);

    char *buffer = (char *)malloc((size_t)length + 1u);
    if (!buffer) {
        fclose(file);
        return false;
    }
    size_t n = fread(buffer, 1, (size_t)length, file);
    fclose(file);
    buffer[n] = '\0';
    bool ok = ahc_parse_daily_attune_snapshot(buffer, out);
    free(buffer);
    return ok;
}
