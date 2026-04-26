#include "attune/attune_sync_codec.h"
#include "miniz.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int b64url_value(int c)
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

static int b64url_decode(const char *in, size_t in_len, unsigned char *out, size_t out_cap, size_t *out_len)
{
    size_t o = 0;
    size_t i = 0;
    while (i < in_len) {
        int rem = (int)(in_len - i);
        if (rem >= 4) {
            int a = b64url_value((unsigned char)in[i]);
            int b = b64url_value((unsigned char)in[i + 1U]);
            int c = b64url_value((unsigned char)in[i + 2U]);
            int d = b64url_value((unsigned char)in[i + 3U]);
            if (a < 0 || b < 0 || c < 0 || d < 0) {
                return -1;
            }
            if (o + 3U > out_cap) {
                return -1;
            }
            out[o++] = (unsigned char)((a << 2) | (b >> 4));
            out[o++] = (unsigned char)(((b & 0x0f) << 4) | (c >> 2));
            out[o++] = (unsigned char)(((c & 0x03) << 6) | d);
            i += 4U;
        } else if (rem == 3) {
            int a = b64url_value((unsigned char)in[i]);
            int b = b64url_value((unsigned char)in[i + 1U]);
            int c = b64url_value((unsigned char)in[i + 2U]);
            if (a < 0 || b < 0 || c < 0) {
                return -1;
            }
            if (o + 2U > out_cap) {
                return -1;
            }
            out[o++] = (unsigned char)((a << 2) | (b >> 4));
            out[o++] = (unsigned char)(((b & 0x0f) << 4) | (c >> 2));
            i += 3U;
        } else if (rem == 2) {
            int a = b64url_value((unsigned char)in[i]);
            int b = b64url_value((unsigned char)in[i + 1U]);
            if (a < 0 || b < 0) {
                return -1;
            }
            if (o + 1U > out_cap) {
                return -1;
            }
            out[o++] = (unsigned char)((a << 2) | (b >> 4));
            i += 2U;
        } else {
            return -1;
        }
    }
    *out_len = o;
    return 0;
}

static int read_uvarint(const unsigned char *in, size_t in_len, size_t *offset, unsigned int *out)
{
    unsigned int value = 0;
    unsigned int shift = 0;
    while (*offset < in_len && shift <= 28U) {
        unsigned char b = in[(*offset)++];
        value |= (unsigned int)(b & 0x7fU) << shift;
        if ((b & 0x80U) == 0U) {
            *out = value;
            return 0;
        }
        shift += 7U;
    }
    return -1;
}

int main(void)
{
    AhcDailyAttuneSnapshot s = { 0 };
    s.found = true;
    snprintf(s.date, sizeof s.date, "%s", "2020-12-25");
    s.account = 10;
    s.warforged = 1;
    s.lightforged = 2;
    s.titanforged = 3;

    char line[256];
    int n = ahc_sync_encode_one_day_qr(&s, line, sizeof line);
    if (n < 0) {
        fprintf(stderr, "encode one day failed\n");
        return 1;
    }
    const char *exp = AHC_SYNC_QR_PREFIX "2020-12-25|10|1|2|3";
    if (strcmp(line, exp) != 0) {
        fprintf(stderr, "one day line mismatch: %s\n", line);
        return 1;
    }

    AhcDailyAttuneSnapshot many[3] = { 0 };
    many[0] = s;
    many[1].found = true;
    snprintf(many[1].date, sizeof many[1].date, "%s", "2020-12-27");
    many[1].account = 25;
    many[1].warforged = 3;
    many[1].lightforged = 4;
    many[1].titanforged = 8;
    many[2].found = true;
    snprintf(many[2].date, sizeof many[2].date, "%s", "2020-12-28");
    many[2].account = 28;
    many[2].warforged = 4;
    many[2].lightforged = 4;
    many[2].titanforged = 9;
    char multi_line[512];
    int q2 = ahc_sync_encode_multi_day_qr(many, 3, multi_line, sizeof multi_line);
    if (q2 < 0) {
        fprintf(stderr, "encode multi-day failed\n");
        return 1;
    }
    if (strncmp(multi_line, AHC_SYNC_MULTI_QR_PREFIX, sizeof AHC_SYNC_MULTI_QR_PREFIX - 1U) != 0) {
        fprintf(stderr, "multi-day missing prefix: %s\n", multi_line);
        return 1;
    }
    unsigned char raw[512];
    size_t raw_len = 0;
    const char *multi_b64 = multi_line + (sizeof AHC_SYNC_MULTI_QR_PREFIX - 1U);
    if (b64url_decode(multi_b64, strlen(multi_b64), raw, sizeof raw, &raw_len) != 0 || raw_len < 8U) {
        fprintf(stderr, "multi-day b64 decode failed\n");
        return 1;
    }
    if (raw[0] != 1U || raw[1] != 3U) {
        fprintf(stderr, "multi-day header mismatch\n");
        return 1;
    }
    size_t off = 6U;
    const unsigned int expected[] = {
        0U, 10U, 1U, 2U, 3U,
        2U, 25U, 3U, 4U, 8U,
        1U, 28U, 4U, 4U, 9U,
    };
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
        unsigned int got = 0;
        if (read_uvarint(raw, raw_len, &off, &got) != 0 || got != expected[i]) {
            fprintf(stderr, "multi-day varint mismatch at %zu\n", i);
            return 1;
        }
    }
    if (off != raw_len) {
        fprintf(stderr, "multi-day trailing bytes\n");
        return 1;
    }

    AhcDailyAttuneSnapshot one[1] = { s };
    char buf[8192];
    int m = ahc_sync_encode_full_history(one, 1, buf, sizeof buf);
    if (m < 0) {
        fprintf(stderr, "encode full failed\n");
        return 1;
    }
    if (strncmp(buf, AHC_SYNC_BULK_PREFIX, sizeof AHC_SYNC_BULK_PREFIX - 1) != 0) {
        fprintf(stderr, "full missing prefix\n");
        return 1;
    }
    const char *b64 = buf + (sizeof AHC_SYNC_BULK_PREFIX - 1);
    unsigned char gz[4096];
    size_t gzlen = 0;
    if (b64url_decode(b64, strlen((const char *)b64), gz, sizeof gz, &gzlen) != 0) {
        fprintf(stderr, "b64 decode failed\n");
        return 1;
    }
    if (gzlen < 18) {
        fprintf(stderr, "gzip too short\n");
        return 1;
    }
    if (gz[0] != 0x1f || gz[1] != 0x8b) {
        fprintf(stderr, "not gzip\n");
        return 1;
    }
    const unsigned char *defl = gz + 10;
    size_t defl_len = gzlen - 10u - 8u;
    size_t json_len = 0;
    void *jsonb = tinfl_decompress_mem_to_heap(defl, defl_len, &json_len, 0);
    if (jsonb == NULL) {
        fprintf(stderr, "inflate failed\n");
        return 1;
    }
    const char *json_exp = "{\"v\":1,\"snapshots\":[{\"date\":\"2020-12-25\",\"account\":10,\"warforged\":1,\"lightforged\":2,\"titanforged\":3}]}";
    if (json_len != strlen(json_exp) || memcmp(jsonb, json_exp, json_len) != 0) {
        fprintf(stderr, "json mismatch\n");
        free(jsonb);
        return 1;
    }
    free(jsonb);

    char empty_buf[1024];
    int e = ahc_sync_encode_full_history(NULL, 0, empty_buf, sizeof empty_buf);
    if (e < 0) {
        fprintf(stderr, "empty encode failed\n");
        return 1;
    }
    {
        const char *eb64 = empty_buf + (sizeof AHC_SYNC_BULK_PREFIX - 1U);
        unsigned char gze[1024];
        size_t gzel = 0;
        if (b64url_decode(eb64, strlen(eb64), gze, sizeof gze, &gzel) != 0) {
            fprintf(stderr, "empty b64 failed\n");
            return 1;
        }
        size_t elen = 0;
        void *jempty = tinfl_decompress_mem_to_heap(gze + 10, gzel - 10u - 8u, &elen, 0);
        if (jempty == NULL) {
            fprintf(stderr, "empty inflate\n");
            return 1;
        }
        const char *exp_e = "{\"v\":1,\"snapshots\":[]}";
        if (elen != strlen(exp_e) || memcmp(jempty, exp_e, elen) != 0) {
            fprintf(stderr, "empty json bad\n");
            free(jempty);
            return 1;
        }
        free(jempty);
    }
    return 0;
}
