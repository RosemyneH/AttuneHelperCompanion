#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "attune/attune_sync_codec.h"

#include "miniz.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define AHC_SYNC_JSON_MAX (1024U * 1024U)
#define AHC_SYNC_MULTI_RAW_MAX 1024U
#define AHC_SYNC_MAX_BULK_B64_CHARS 2000000U
#define AHC_SYNC_MAX_BULK_GZIP_BYTES (1U << 20)
#define AHC_SYNC_MAX_BULK_JSON_UNCOMP (1024U * 1024U)

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

static int ahc_b64url_decode(const char *in, size_t in_len, unsigned char *out, size_t out_cap, size_t *out_len)
{
    size_t o = 0;
    size_t i = 0;
    while (i < in_len) {
        int rem = (int)(in_len - i);
        if (rem >= 4) {
            int a = ahc_b64url_value((unsigned char)in[i]);
            int b2 = ahc_b64url_value((unsigned char)in[i + 1U]);
            int c2 = ahc_b64url_value((unsigned char)in[i + 2U]);
            int d2 = ahc_b64url_value((unsigned char)in[i + 3U]);
            if (a < 0 || b2 < 0 || c2 < 0 || d2 < 0) {
                return -1;
            }
            if (o + 3U > out_cap) {
                return -1;
            }
            out[o++] = (unsigned char)((a << 2) | (b2 >> 4));
            out[o++] = (unsigned char)(((b2 & 0x0f) << 4) | (c2 >> 2));
            out[o++] = (unsigned char)(((c2 & 0x03) << 6) | d2);
            i += 4U;
        } else if (rem == 3) {
            int a = ahc_b64url_value((unsigned char)in[i]);
            int b2 = ahc_b64url_value((unsigned char)in[i + 1U]);
            int c2 = ahc_b64url_value((unsigned char)in[i + 2U]);
            if (a < 0 || b2 < 0 || c2 < 0) {
                return -1;
            }
            if (o + 2U > out_cap) {
                return -1;
            }
            out[o++] = (unsigned char)((a << 2) | (b2 >> 4));
            out[o++] = (unsigned char)(((b2 & 0x0f) << 4) | (c2 >> 2));
            i += 3U;
        } else if (rem == 2) {
            int a = ahc_b64url_value((unsigned char)in[i]);
            int b2 = ahc_b64url_value((unsigned char)in[i + 1U]);
            if (a < 0 || b2 < 0) {
                return -1;
            }
            if (o + 1U > out_cap) {
                return -1;
            }
            out[o++] = (unsigned char)((a << 2) | (b2 >> 4));
            i += 2U;
        } else {
            return -1;
        }
    }
    *out_len = o;
    return 0;
}

static int ahc_sync_parse_one_snapshot_object(const char *obj, size_t obj_len, AhcDailyAttuneSnapshot *out)
{
    if (out == NULL || obj_len < 8U || obj_len + 1U > 1024U) {
        return -1;
    }
    char buf[1024];
    if (obj_len >= sizeof buf) {
        return -1;
    }
    memcpy(buf, obj, obj_len);
    buf[obj_len] = '\0';

    const char *d = strstr(buf, "\"date\":\"");
    if (d == NULL) {
        return -1;
    }
    d += 8;
    const char *de = strchr(d, '"');
    if (de == NULL) {
        return -1;
    }
    size_t dl = (size_t)(de - d);
    if (dl == 0U || dl >= AHC_DATE_CAPACITY) {
        return -1;
    }
    memcpy(out->date, d, dl);
    out->date[dl] = '\0';

    const char *k_account = strstr(buf, "\"account\":");
    const char *k_wf = strstr(buf, "\"warforged\":");
    const char *k_lf = strstr(buf, "\"lightforged\":");
    const char *k_tf = strstr(buf, "\"titanforged\":");
    if (k_account == NULL || k_wf == NULL || k_lf == NULL || k_tf == NULL) {
        return -1;
    }
    {
        const char *p = k_account + strlen("\"account\":");
        out->account = (int)strtol(p, NULL, 10);
    }
    {
        const char *p = k_wf + strlen("\"warforged\":");
        out->warforged = (int)strtol(p, NULL, 10);
    }
    {
        const char *p = k_lf + strlen("\"lightforged\":");
        out->lightforged = (int)strtol(p, NULL, 10);
    }
    {
        const char *p = k_tf + strlen("\"titanforged\":");
        out->titanforged = (int)strtol(p, NULL, 10);
    }
    return 0;
}

static bool ahc_parse_iso_date(const char *date, int *year, int *month, int *day)
{
    int parsed_year = 0;
    int parsed_month = 0;
    int parsed_day = 0;
    if (date == NULL || sscanf(date, "%d-%d-%d", &parsed_year, &parsed_month, &parsed_day) != 3) {
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

static int ahc_days_from_civil(int year, int month, int day)
{
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = (unsigned)(year - era * 400);
    const unsigned doy = (153u * (unsigned)(month + (month > 2 ? -3 : 9)) + 2u) / 5u + (unsigned)day - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097 + (int)doe - 719468;
}

static bool ahc_date_to_ordinal(const char *date, int *ordinal)
{
    int year = 0;
    int month = 0;
    int day = 0;
    if (!ahc_parse_iso_date(date, &year, &month, &day)) {
        return false;
    }

    *ordinal = ahc_days_from_civil(year, month, day);
    return true;
}

static int ahc_put_u8(unsigned char *buf, size_t cap, size_t *len, unsigned int value)
{
    if (*len >= cap || value > 0xffU) {
        return -1;
    }
    buf[(*len)++] = (unsigned char)value;
    return 0;
}

static int ahc_put_i32_be(unsigned char *buf, size_t cap, size_t *len, int value)
{
    if (*len + 4U > cap) {
        return -1;
    }
    uint32_t u = (uint32_t)value;
    buf[(*len)++] = (unsigned char)((u >> 24) & 0xffU);
    buf[(*len)++] = (unsigned char)((u >> 16) & 0xffU);
    buf[(*len)++] = (unsigned char)((u >> 8) & 0xffU);
    buf[(*len)++] = (unsigned char)(u & 0xffU);
    return 0;
}

static int ahc_put_uvarint(unsigned char *buf, size_t cap, size_t *len, uint32_t value)
{
    do {
        unsigned char b = (unsigned char)(value & 0x7fU);
        value >>= 7;
        if (value != 0U) {
            b |= 0x80U;
        }
        if (ahc_put_u8(buf, cap, len, b) != 0) {
            return -1;
        }
    } while (value != 0U);
    return 0;
}

static bool ahc_snapshot_is_qr2_eligible(const AhcDailyAttuneSnapshot *s)
{
    int ordinal = 0;
    return s != NULL
        && s->found
        && s->date[0] != '\0'
        && s->account >= 0
        && s->warforged >= 0
        && s->lightforged >= 0
        && s->titanforged >= 0
        && ahc_date_to_ordinal(s->date, &ordinal);
}

static int ahc_append_json_char(char *buf, size_t cap, size_t *len, char c)
{
    if (*len + 1 >= cap) {
        return -1;
    }
    buf[*len] = c;
    (*len)++;
    return 0;
}

static int ahc_append_json_str(char *buf, size_t cap, size_t *len, const char *s)
{
    size_t sl = strlen(s);
    if (*len + sl >= cap) {
        return -1;
    }
    memcpy(buf + *len, s, sl);
    *len += sl;
    return 0;
}

static int ahc_append_json_int(char *buf, size_t cap, size_t *len, int v)
{
    char tmp[32];
    int n = snprintf(tmp, sizeof tmp, "%d", v);
    if (n < 0 || (size_t)n >= sizeof tmp) {
        return -1;
    }
    if (*len + (size_t)n >= cap) {
        return -1;
    }
    memcpy(buf + *len, tmp, (size_t)n);
    *len += (size_t)n;
    return 0;
}

static int ahc_gzip_raw_deflate_to_heap(const unsigned char *data, size_t data_len, unsigned char **out_gz, size_t *out_gz_len)
{
    size_t defl_len = 0;
    void *deflated = tdefl_compress_mem_to_heap(data, data_len, &defl_len, 0);
    if (deflated == NULL || defl_len == 0) {
        return -1;
    }
    size_t need = 10U + defl_len + 8U;
    unsigned char *o = (unsigned char *)malloc(need);
    if (o == NULL) {
        mz_free(deflated);
        return -1;
    }
    mz_ulong crc = mz_crc32(0, data, data_len);
    o[0] = 0x1f;
    o[1] = 0x8b;
    o[2] = 0x08;
    o[3] = 0x00;
    o[4] = 0x00;
    o[5] = 0x00;
    o[6] = 0x00;
    o[7] = 0x00;
    o[8] = 0x00;
    o[9] = 0xff;
    memcpy(o + 10, deflated, defl_len);
    mz_free(deflated);
    size_t p = 10U + defl_len;
    o[p++] = (unsigned char)(crc & 0xffU);
    o[p++] = (unsigned char)((crc >> 8) & 0xffU);
    o[p++] = (unsigned char)((crc >> 16) & 0xffU);
    o[p++] = (unsigned char)((crc >> 24) & 0xffU);
    {
        unsigned int isize = (unsigned int)(data_len & 0xffffffffU);
        o[p++] = (unsigned char)(isize & 0xffU);
        o[p++] = (unsigned char)((isize >> 8) & 0xffU);
        o[p++] = (unsigned char)((isize >> 16) & 0xffU);
        o[p++] = (unsigned char)((isize >> 24) & 0xffU);
    }
    *out_gz = o;
    *out_gz_len = p;
    return 0;
}

static int ahc_b64url_encode(const unsigned char *in, size_t in_len, char *out, size_t out_cap, size_t *o_len)
{
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    size_t o = 0;
    size_t i = 0;
    while (i + 2U < in_len) {
        uint32_t v
            = ((uint32_t)in[i] << 16) | ((uint32_t)in[i + 1U] << 8) | (uint32_t)in[i + 2U];
        if (o + 4U > out_cap) {
            return -1;
        }
        out[o++] = T[(v >> 18) & 63U];
        out[o++] = T[(v >> 12) & 63U];
        out[o++] = T[(v >> 6) & 63U];
        out[o++] = T[v & 63U];
        i += 3U;
    }
    if (i < in_len) {
        uint32_t v = (uint32_t)in[i] << 16;
        if (i + 1U < in_len) {
            v |= (uint32_t)in[i + 1U] << 8;
        }
        if (i + 1U < in_len) {
            if (o + 3U > out_cap) {
                return -1;
            }
            out[o++] = T[(v >> 18) & 63U];
            out[o++] = T[(v >> 12) & 63U];
            out[o++] = T[(v >> 6) & 63U];
        } else {
            if (o + 2U > out_cap) {
                return -1;
            }
            out[o++] = T[(v >> 18) & 63U];
            out[o++] = T[(v >> 12) & 63U];
        }
    }
    *o_len = o;
    return 0;
}

int ahc_sync_encode_one_day_qr(const AhcDailyAttuneSnapshot *s, char *out, size_t out_cap)
{
    if (s == NULL || out == NULL || out_cap == 0) {
        return -1;
    }
    if (s->date[0] == '\0') {
        return -1;
    }
    int n = snprintf(
        out,
        out_cap,
        AHC_SYNC_QR_PREFIX "%s|%d|%d|%d|%d",
        s->date,
        s->account,
        s->warforged,
        s->lightforged,
        s->titanforged
    );
    if (n < 0 || (size_t)n >= out_cap) {
        return -1;
    }
    return n;
}

int ahc_sync_encode_multi_day_qr(const AhcDailyAttuneSnapshot *snapshots, size_t count, char *out, size_t out_cap)
{
    if (out == NULL || snapshots == NULL || count == 0U) {
        return -1;
    }

    const AhcDailyAttuneSnapshot *selected[AHC_SYNC_MULTI_QR_MAX_DAYS];
    size_t selected_count = 0;
    for (size_t i = count; i > 0U && selected_count < AHC_SYNC_MULTI_QR_MAX_DAYS; i--) {
        const AhcDailyAttuneSnapshot *s = &snapshots[i - 1U];
        if (ahc_snapshot_is_qr2_eligible(s)) {
            selected[selected_count++] = s;
        }
    }
    if (selected_count == 0U || selected_count > 255U) {
        return -1;
    }

    unsigned char raw[AHC_SYNC_MULTI_RAW_MAX];
    size_t raw_len = 0;
    if (ahc_put_u8(raw, sizeof raw, &raw_len, 1U) != 0
        || ahc_put_u8(raw, sizeof raw, &raw_len, (unsigned int)selected_count) != 0) {
        return -1;
    }

    int previous_ordinal = 0;
    for (size_t reverse_i = selected_count; reverse_i > 0U; reverse_i--) {
        const AhcDailyAttuneSnapshot *s = selected[reverse_i - 1U];
        int ordinal = 0;
        if (!ahc_date_to_ordinal(s->date, &ordinal)) {
            return -1;
        }
        if (reverse_i == selected_count) {
            if (ahc_put_i32_be(raw, sizeof raw, &raw_len, ordinal) != 0
                || ahc_put_uvarint(raw, sizeof raw, &raw_len, 0U) != 0) {
                return -1;
            }
        } else {
            int delta_days = ordinal - previous_ordinal;
            if (delta_days < 0) {
                return -1;
            }
            if (ahc_put_uvarint(raw, sizeof raw, &raw_len, (uint32_t)delta_days) != 0) {
                return -1;
            }
        }
        previous_ordinal = ordinal;

        if (ahc_put_uvarint(raw, sizeof raw, &raw_len, (uint32_t)s->account) != 0
            || ahc_put_uvarint(raw, sizeof raw, &raw_len, (uint32_t)s->warforged) != 0
            || ahc_put_uvarint(raw, sizeof raw, &raw_len, (uint32_t)s->lightforged) != 0
            || ahc_put_uvarint(raw, sizeof raw, &raw_len, (uint32_t)s->titanforged) != 0) {
            return -1;
        }
    }

    size_t pre = sizeof(AHC_SYNC_MULTI_QR_PREFIX) - 1U;
    if (out_cap <= pre) {
        return -1;
    }
    memcpy(out, AHC_SYNC_MULTI_QR_PREFIX, pre);
    size_t b64o = 0;
    if (ahc_b64url_encode(raw, raw_len, out + pre, out_cap - pre, &b64o) != 0) {
        return -1;
    }
    out[pre + b64o] = '\0';
    return (int)(pre + b64o);
}

int ahc_sync_encode_full_history(const AhcDailyAttuneSnapshot *snapshots, size_t count, char *out, size_t out_cap)
{
    if (out == NULL || (count > 0U && snapshots == NULL)) {
        return -1;
    }
    char *json = (char *)malloc(AHC_SYNC_JSON_MAX);
    if (json == NULL) {
        return -1;
    }
    size_t jl = 0;
    if (ahc_append_json_str(json, AHC_SYNC_JSON_MAX, &jl, "{\"v\":1,\"snapshots\":[") != 0) {
        free(json);
        return -1;
    }
    bool first_obj = true;
    for (size_t i = 0; i < count; i++) {
        const AhcDailyAttuneSnapshot *s = &snapshots[i];
        if (s->date[0] == '\0') {
            continue;
        }
        if (!first_obj) {
            if (ahc_append_json_char(json, AHC_SYNC_JSON_MAX, &jl, ',') != 0) {
                free(json);
                return -1;
            }
        }
        first_obj = false;
        if (ahc_append_json_str(json, AHC_SYNC_JSON_MAX, &jl, "{\"date\":\"") != 0
            || ahc_append_json_str(json, AHC_SYNC_JSON_MAX, &jl, s->date) != 0
            || ahc_append_json_str(json, AHC_SYNC_JSON_MAX, &jl, "\",\"account\":") != 0) {
            free(json);
            return -1;
        }
        if (ahc_append_json_int(json, AHC_SYNC_JSON_MAX, &jl, s->account) != 0
            || ahc_append_json_str(json, AHC_SYNC_JSON_MAX, &jl, ",\"warforged\":") != 0) {
            free(json);
            return -1;
        }
        if (ahc_append_json_int(json, AHC_SYNC_JSON_MAX, &jl, s->warforged) != 0
            || ahc_append_json_str(json, AHC_SYNC_JSON_MAX, &jl, ",\"lightforged\":") != 0) {
            free(json);
            return -1;
        }
        if (ahc_append_json_int(json, AHC_SYNC_JSON_MAX, &jl, s->lightforged) != 0
            || ahc_append_json_str(json, AHC_SYNC_JSON_MAX, &jl, ",\"titanforged\":") != 0) {
            free(json);
            return -1;
        }
        if (ahc_append_json_int(json, AHC_SYNC_JSON_MAX, &jl, s->titanforged) != 0
            || ahc_append_json_str(json, AHC_SYNC_JSON_MAX, &jl, "}") != 0) {
            free(json);
            return -1;
        }
    }
    if (ahc_append_json_str(json, AHC_SYNC_JSON_MAX, &jl, "]}") != 0) {
        free(json);
        return -1;
    }
    if (jl >= AHC_SYNC_JSON_MAX) {
        free(json);
        return -1;
    }
    json[jl] = '\0';
    size_t jbytes = strlen(json);

    unsigned char *gz = NULL;
    size_t gzlen = 0;
    if (ahc_gzip_raw_deflate_to_heap((const unsigned char *)json, jbytes, &gz, &gzlen) != 0) {
        free(json);
        return -1;
    }
    free(json);

    size_t pre = sizeof(AHC_SYNC_BULK_PREFIX) - 1U;
    if (out_cap <= pre) {
        free(gz);
        return -1;
    }
    memcpy(out, AHC_SYNC_BULK_PREFIX, pre);
    size_t b64o = 0;
    if (ahc_b64url_encode(gz, gzlen, out + pre, out_cap - pre, &b64o) != 0) {
        free(gz);
        return -1;
    }
    free(gz);
    out[pre + b64o] = '\0';
    return (int)(pre + b64o);
}

int ahc_sync_decode_full_history(const char *token, AhcDailyAttuneSnapshot *out_snapshots, size_t out_cap, size_t *out_count)
{
    if (out_count == NULL) {
        return -1;
    }
    *out_count = 0U;
    if (token == NULL) {
        return -1;
    }
    if (out_cap == 0U && out_snapshots != NULL) {
        return -1;
    }
    if (out_cap > 0U && out_snapshots == NULL) {
        return -1;
    }

    while (token[0] != '\0' && isspace((unsigned char)token[0])) {
        token++;
    }
    {
        const char *p = token;
        if (p[0] == (char)0xefu && p[1] == (char)0xbbu && p[2] == (char)0xbfu) {
            p += 3;
        }
        if (strncmp(p, AHC_SYNC_BULK_PREFIX, sizeof AHC_SYNC_BULK_PREFIX - 1U) != 0) {
            return -1;
        }
        token = p + (sizeof AHC_SYNC_BULK_PREFIX - 1U);
    }

    char *b64_stripped = (char *)malloc(AHC_SYNC_MAX_BULK_B64_CHARS + 2U);
    if (b64_stripped == NULL) {
        return -1;
    }
    size_t w = 0U;
    for (const char *q = token; *q; q++) {
        if (isspace((unsigned char)*q)) {
            continue;
        }
        if (w >= AHC_SYNC_MAX_BULK_B64_CHARS) {
            free(b64_stripped);
            return -1;
        }
        b64_stripped[w++] = *q;
    }
    b64_stripped[w] = '\0';
    if (w == 0U) {
        free(b64_stripped);
        return -1;
    }

    unsigned char *gz = (unsigned char *)malloc(AHC_SYNC_MAX_BULK_GZIP_BYTES + 64U);
    if (gz == NULL) {
        free(b64_stripped);
        return -1;
    }
    size_t gzlen = 0U;
    if (ahc_b64url_decode(b64_stripped, w, gz, AHC_SYNC_MAX_BULK_GZIP_BYTES + 64U, &gzlen) != 0) {
        free(b64_stripped);
        free(gz);
        return -1;
    }
    free(b64_stripped);
    if (gzlen < 18U || gzlen > AHC_SYNC_MAX_BULK_GZIP_BYTES) {
        free(gz);
        return -1;
    }
    if (gz[0] != 0x1fu || gz[1] != 0x8bu) {
        free(gz);
        return -1;
    }
    if (gzlen < 10U + 8U) {
        free(gz);
        return -1;
    }
    const unsigned char *defl = gz + 10U;
    size_t defl_len = gzlen - 10U - 8U;
    void *json_heap = NULL;
    size_t json_len = 0U;
    json_heap = tinfl_decompress_mem_to_heap(defl, defl_len, &json_len, 0);
    free(gz);
    gz = NULL;
    if (json_heap == NULL) {
        return -1;
    }
    if (json_len == 0U || json_len > AHC_SYNC_MAX_BULK_JSON_UNCOMP) {
        free(json_heap);
        return -1;
    }
    char *json = (char *)json_heap;
    if (json_len < AHC_SYNC_MAX_BULK_JSON_UNCOMP) {
        json[json_len] = '\0';
    } else {
        free(json_heap);
        return -1;
    }
    if (strstr(json, "\"v\":1") == NULL) {
        free(json);
        return -1;
    }

    const char *array_key = strstr(json, "\"snapshots\"");
    if (array_key == NULL) {
        free(json);
        return -1;
    }
    const char *bracket = strchr(array_key, '[');
    if (bracket == NULL) {
        free(json);
        return -1;
    }
    const char *p = bracket + 1U;
    while (p[0] != '\0' && isspace((unsigned char)p[0])) {
        p++;
    }
    if (p[0] == ']') {
        *out_count = 0U;
        free(json);
        return 0;
    }
    for (;;) {
        const char *obj = strstr(p, "{\"date\"");
        if (obj == NULL) {
            /* tolerate trailing space before ] */
            const char *t = p;
            while (t[0] != '\0' && isspace((unsigned char)t[0])) {
                t++;
            }
            if (t[0] == ']' || t[0] == '\0') {
                break;
            }
            free(json);
            return -1;
        }
        const char *cend = strchr(obj + 1, '}');
        if (cend == NULL) {
            free(json);
            return -1;
        }
        if (*out_count >= out_cap) {
            free(json);
            return -1;
        }
        AhcDailyAttuneSnapshot s;
        memset(&s, 0, sizeof s);
        if (ahc_sync_parse_one_snapshot_object(obj, (size_t)(cend - obj) + 1U, &s) == 0) {
            s.found = true;
            out_snapshots[(*out_count)++] = s;
        } else {
            free(json);
            return -1;
        }
        p = cend + 1U;
        while (p[0] != '\0' && (p[0] == ',' || isspace((unsigned char)p[0]))) {
            p++;
        }
        if (p[0] == ']' || p[0] == '\0') {
            break;
        }
    }
    free(json);
    return 0;
}
