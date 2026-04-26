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

#define AHC_SYNC_JSON_MAX (1024U * 1024U)
#define AHC_SYNC_MULTI_RAW_MAX 1024U

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
