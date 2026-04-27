#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "addons/addon_profile_codec.h"

#include "miniz.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AHC_PROFILE_JSON_MAX (128U * 1024U)
#define AHC_PROFILE_GZIP_MAX (128U * 1024U)
#define AHC_PROFILE_B64_MAX (256U * 1024U)

static int ahc_json_append_char(char *buf, size_t cap, size_t *len, char c)
{
    if (*len + 1u >= cap) {
        return -1;
    }
    buf[(*len)++] = c;
    buf[*len] = '\0';
    return 0;
}

static int ahc_json_append_raw(char *buf, size_t cap, size_t *len, const char *text)
{
    size_t n = strlen(text);
    if (*len + n >= cap) {
        return -1;
    }
    memcpy(buf + *len, text, n);
    *len += n;
    buf[*len] = '\0';
    return 0;
}

static int ahc_json_append_string(char *buf, size_t cap, size_t *len, const char *text)
{
    if (ahc_json_append_char(buf, cap, len, '"') != 0) {
        return -1;
    }
    for (const unsigned char *p = (const unsigned char *)text; p && *p; p++) {
        if (*p == '"' || *p == '\\') {
            if (ahc_json_append_char(buf, cap, len, '\\') != 0 || ahc_json_append_char(buf, cap, len, (char)*p) != 0) {
                return -1;
            }
        } else if (*p == '\n') {
            if (ahc_json_append_raw(buf, cap, len, "\\n") != 0) {
                return -1;
            }
        } else if (*p == '\r') {
            if (ahc_json_append_raw(buf, cap, len, "\\r") != 0) {
                return -1;
            }
        } else if (*p == '\t') {
            if (ahc_json_append_raw(buf, cap, len, "\\t") != 0) {
                return -1;
            }
        } else if (*p < 0x20u) {
            return -1;
        } else if (ahc_json_append_char(buf, cap, len, (char)*p) != 0) {
            return -1;
        }
    }
    return ahc_json_append_char(buf, cap, len, '"');
}

static int ahc_gzip_to_heap(const unsigned char *data, size_t data_len, unsigned char **out_gz, size_t *out_gz_len)
{
    size_t defl_len = 0;
    void *deflated = tdefl_compress_mem_to_heap(data, data_len, &defl_len, 0);
    if (!deflated || defl_len == 0u) {
        return -1;
    }
    size_t need = 10u + defl_len + 8u;
    unsigned char *o = (unsigned char *)malloc(need);
    if (!o) {
        mz_free(deflated);
        return -1;
    }
    mz_ulong crc = mz_crc32(0, data, data_len);
    o[0] = 0x1f;
    o[1] = 0x8b;
    o[2] = 0x08;
    o[3] = 0x00;
    o[4] = o[5] = o[6] = o[7] = 0x00;
    o[8] = 0x00;
    o[9] = 0xff;
    memcpy(o + 10, deflated, defl_len);
    mz_free(deflated);
    size_t p = 10u + defl_len;
    o[p++] = (unsigned char)(crc & 0xffu);
    o[p++] = (unsigned char)((crc >> 8) & 0xffu);
    o[p++] = (unsigned char)((crc >> 16) & 0xffu);
    o[p++] = (unsigned char)((crc >> 24) & 0xffu);
    {
        unsigned int isize = (unsigned int)(data_len & 0xffffffffu);
        o[p++] = (unsigned char)(isize & 0xffu);
        o[p++] = (unsigned char)((isize >> 8) & 0xffu);
        o[p++] = (unsigned char)((isize >> 16) & 0xffu);
        o[p++] = (unsigned char)((isize >> 24) & 0xffu);
    }
    *out_gz = o;
    *out_gz_len = p;
    return 0;
}

static int ahc_b64url_value(int c)
{
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }
    if (c >= '0' && c <= '9') {
        return c - '0' + 52;
    }
    if (c == '-') {
        return 62;
    }
    if (c == '_') {
        return 63;
    }
    return -1;
}

static int ahc_b64url_encode(const unsigned char *in, size_t in_len, char *out, size_t out_cap, size_t *o_len)
{
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    size_t o = 0;
    size_t i = 0;
    while (i + 2u < in_len) {
        uint32_t v = ((uint32_t)in[i] << 16) | ((uint32_t)in[i + 1u] << 8) | (uint32_t)in[i + 2u];
        if (o + 4u > out_cap) {
            return -1;
        }
        out[o++] = T[(v >> 18) & 63u];
        out[o++] = T[(v >> 12) & 63u];
        out[o++] = T[(v >> 6) & 63u];
        out[o++] = T[v & 63u];
        i += 3u;
    }
    if (i < in_len) {
        uint32_t v = (uint32_t)in[i] << 16;
        if (i + 1u < in_len) {
            v |= (uint32_t)in[i + 1u] << 8;
        }
        if (i + 1u < in_len) {
            if (o + 3u > out_cap) {
                return -1;
            }
            out[o++] = T[(v >> 18) & 63u];
            out[o++] = T[(v >> 12) & 63u];
            out[o++] = T[(v >> 6) & 63u];
        } else {
            if (o + 2u > out_cap) {
                return -1;
            }
            out[o++] = T[(v >> 18) & 63u];
            out[o++] = T[(v >> 12) & 63u];
        }
    }
    *o_len = o;
    return 0;
}

static int ahc_b64url_decode(const char *in, size_t in_len, unsigned char *out, size_t out_cap, size_t *out_len)
{
    size_t o = 0;
    size_t i = 0;
    while (i < in_len) {
        int rem = (int)(in_len - i);
        if (rem >= 4) {
            int a = ahc_b64url_value((unsigned char)in[i]);
            int b = ahc_b64url_value((unsigned char)in[i + 1u]);
            int c = ahc_b64url_value((unsigned char)in[i + 2u]);
            int d = ahc_b64url_value((unsigned char)in[i + 3u]);
            if (a < 0 || b < 0 || c < 0 || d < 0 || o + 3u > out_cap) {
                return -1;
            }
            out[o++] = (unsigned char)((a << 2) | (b >> 4));
            out[o++] = (unsigned char)(((b & 0x0f) << 4) | (c >> 2));
            out[o++] = (unsigned char)(((c & 0x03) << 6) | d);
            i += 4u;
        } else if (rem == 3) {
            int a = ahc_b64url_value((unsigned char)in[i]);
            int b = ahc_b64url_value((unsigned char)in[i + 1u]);
            int c = ahc_b64url_value((unsigned char)in[i + 2u]);
            if (a < 0 || b < 0 || c < 0 || o + 2u > out_cap) {
                return -1;
            }
            out[o++] = (unsigned char)((a << 2) | (b >> 4));
            out[o++] = (unsigned char)(((b & 0x0f) << 4) | (c >> 2));
            i += 3u;
        } else if (rem == 2) {
            int a = ahc_b64url_value((unsigned char)in[i]);
            int b = ahc_b64url_value((unsigned char)in[i + 1u]);
            if (a < 0 || b < 0 || o + 1u > out_cap) {
                return -1;
            }
            out[o++] = (unsigned char)((a << 2) | (b >> 4));
            i += 2u;
        } else {
            return -1;
        }
    }
    *out_len = o;
    return 0;
}

static char *ahc_gunzip_to_heap(const unsigned char *gz, size_t gz_len, size_t *out_len)
{
    if (gz_len < 18u || gz[0] != 0x1f || gz[1] != 0x8b || gz[2] != 0x08) {
        return NULL;
    }
    size_t defl_len = gz_len - 10u - 8u;
    size_t json_len = 0;
    void *json = tinfl_decompress_mem_to_heap(gz + 10u, defl_len, &json_len, 0);
    if (!json || json_len > AHC_PROFILE_JSON_MAX) {
        if (json) {
            mz_free(json);
        }
        return NULL;
    }
    char *text = (char *)malloc(json_len + 1u);
    if (!text) {
        mz_free(json);
        return NULL;
    }
    memcpy(text, json, json_len);
    text[json_len] = '\0';
    mz_free(json);
    *out_len = json_len;
    return text;
}

static const char *ahc_json_find_key(const char *json, const char *key)
{
    char needle[80];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(json, needle);
    if (!p) {
        return NULL;
    }
    p += strlen(needle);
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
        p++;
    }
    return *p == ':' ? p + 1 : NULL;
}

static int ahc_parse_json_string_value(const char **cursor, char *out, size_t out_cap)
{
    const char *p = *cursor;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
        p++;
    }
    if (*p != '"') {
        return -1;
    }
    p++;
    size_t n = 0;
    while (*p && *p != '"') {
        char c = *p++;
        if (c == '\\') {
            c = *p++;
            if (c == 'n') {
                c = '\n';
            } else if (c == 'r') {
                c = '\r';
            } else if (c == 't') {
                c = '\t';
            }
        }
        if (n + 1u >= out_cap) {
            return -1;
        }
        out[n++] = c;
    }
    if (*p != '"') {
        return -1;
    }
    out[n] = '\0';
    *cursor = p + 1;
    return 0;
}

int ahc_profile_encode(const AhcAddonProfile *profile, char *out, size_t out_cap)
{
    if (!profile || !out || out_cap == 0u || profile->addon_count > AHC_PROFILE_MAX_ADDONS) {
        return -1;
    }
    char json[AHC_PROFILE_JSON_MAX];
    size_t len = 0;
    json[0] = '\0';
    if (ahc_json_append_raw(json, sizeof(json), &len, "{\"v\":1,\"name\":") != 0
        || ahc_json_append_string(json, sizeof(json), &len, profile->name[0] ? profile->name : "Add-on Profile") != 0
        || ahc_json_append_raw(json, sizeof(json), &len, ",\"addon_ids\":[") != 0) {
        return -1;
    }
    for (size_t i = 0; i < profile->addon_count; i++) {
        if (i > 0u && ahc_json_append_char(json, sizeof(json), &len, ',') != 0) {
            return -1;
        }
        if (!profile->addon_ids[i][0] || ahc_json_append_string(json, sizeof(json), &len, profile->addon_ids[i]) != 0) {
            return -1;
        }
    }
    if (ahc_json_append_raw(json, sizeof(json), &len, "]}") != 0) {
        return -1;
    }

    unsigned char *gz = NULL;
    size_t gz_len = 0;
    if (ahc_gzip_to_heap((const unsigned char *)json, len, &gz, &gz_len) != 0) {
        return -1;
    }
    size_t prefix_len = strlen(AHC_PROFILE_CODE_PREFIX);
    if (prefix_len >= out_cap) {
        free(gz);
        return -1;
    }
    memcpy(out, AHC_PROFILE_CODE_PREFIX, prefix_len);
    size_t b64_len = 0;
    int rc = ahc_b64url_encode(gz, gz_len, out + prefix_len, out_cap - prefix_len - 1u, &b64_len);
    free(gz);
    if (rc != 0) {
        return -1;
    }
    out[prefix_len + b64_len] = '\0';
    return (int)(prefix_len + b64_len);
}

int ahc_profile_decode(const char *code, AhcAddonProfile *out)
{
    if (!code || !out) {
        return -1;
    }
    while (*code == ' ' || *code == '\t' || *code == '\r' || *code == '\n') {
        code++;
    }
    size_t prefix_len = strlen(AHC_PROFILE_CODE_PREFIX);
    if (strncmp(code, AHC_PROFILE_CODE_PREFIX, prefix_len) != 0) {
        return -1;
    }
    const char *b64 = code + prefix_len;
    unsigned char gz[AHC_PROFILE_GZIP_MAX];
    size_t gz_len = 0;
    if (strlen(b64) > AHC_PROFILE_B64_MAX || ahc_b64url_decode(b64, strlen(b64), gz, sizeof(gz), &gz_len) != 0) {
        return -1;
    }
    size_t json_len = 0;
    char *json = ahc_gunzip_to_heap(gz, gz_len, &json_len);
    (void)json_len;
    if (!json) {
        return -1;
    }
    AhcAddonProfile profile;
    memset(&profile, 0, sizeof(profile));
    const char *name_cursor = ahc_json_find_key(json, "name");
    if (name_cursor) {
        (void)ahc_parse_json_string_value(&name_cursor, profile.name, sizeof(profile.name));
    }
    if (!profile.name[0]) {
        snprintf(profile.name, sizeof(profile.name), "%s", "Imported Profile");
    }
    const char *ids = ahc_json_find_key(json, "addon_ids");
    if (!ids) {
        free(json);
        return -1;
    }
    while (*ids == ' ' || *ids == '\t' || *ids == '\r' || *ids == '\n') {
        ids++;
    }
    if (*ids != '[') {
        free(json);
        return -1;
    }
    ids++;
    for (;;) {
        while (*ids == ' ' || *ids == '\t' || *ids == '\r' || *ids == '\n') {
            ids++;
        }
        if (*ids == ']') {
            break;
        }
        if (profile.addon_count >= AHC_PROFILE_MAX_ADDONS) {
            free(json);
            return -1;
        }
        if (ahc_parse_json_string_value(&ids, profile.addon_ids[profile.addon_count], sizeof(profile.addon_ids[profile.addon_count])) != 0) {
            free(json);
            return -1;
        }
        profile.addon_count++;
        while (*ids == ' ' || *ids == '\t' || *ids == '\r' || *ids == '\n') {
            ids++;
        }
        if (*ids == ',') {
            ids++;
            continue;
        }
        if (*ids == ']') {
            break;
        }
        free(json);
        return -1;
    }
    free(json);
    if (profile.addon_count == 0u) {
        return -1;
    }
    *out = profile;
    return 0;
}
