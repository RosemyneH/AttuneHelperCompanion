#include "ahc/ahc_wow_autologin.h"

#include <string.h>
#include <ctype.h>

static bool is_safe_user_char(int c)
{
    if (c >= (unsigned char)'A' && c <= (unsigned char)'Z') {
        return true;
    }
    if (c >= (unsigned char)'a' && c <= (unsigned char)'z') {
        return true;
    }
    if (c >= (unsigned char)'0' && c <= (unsigned char)'9') {
        return true;
    }
    return c == (unsigned char)'@' || c == (unsigned char)'.' || c == (unsigned char)'_'
        || c == (unsigned char)'-';
}

static bool is_safe_realm_char(int c)
{
    return c == (unsigned char)' ' || is_safe_user_char(c);
}

static const char *skip_ws(const char *p)
{
    while (*p == (unsigned char)' ' || *p == (unsigned char)'\t') {
        p++;
    }
    return p;
}

bool ahc_wow_trust_sanitize_user(const char *in, char *out, size_t out_size)
{
    if (!in || !out || out_size == 0) {
        return false;
    }
    in = skip_ws(in);
    size_t j = 0;
    for (const char *p = in; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (!is_safe_user_char(c)) {
            return false;
        }
        if (j + 1 >= out_size) {
            return false;
        }
        out[j++] = (char)c;
    }
    out[j] = '\0';
    return j > 0;
}

bool ahc_wow_trust_password(const char *in, char *out, size_t out_size)
{
    if (!in || !out || out_size < 2u) {
        return false;
    }
    size_t j = 0;
    for (const char *p = in; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x20u) {
            return false;
        }
        if (j + 1u >= out_size) {
            return false;
        }
        out[j++] = (char)c;
    }
    if (j == 0) {
        return false;
    }
    out[j] = '\0';
    return true;
}

bool ahc_wow_trust_sanitize_realm(const char *in, char *out, size_t out_size)
{
    if (!in || !out || out_size == 0) {
        return false;
    }
    in = skip_ws(in);
    size_t j = 0;
    for (const char *p = in; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (!is_safe_realm_char(c)) {
            return false;
        }
        if (j + 1 >= out_size) {
            return false;
        }
        out[j++] = (char)c;
    }
    out[j] = '\0';
    return j > 0;
}

static int lower_c(unsigned char c)
{
    if (c >= (unsigned char)'A' && c <= (unsigned char)'Z') {
        return c - (unsigned char)'A' + (unsigned char)'a';
    }
    return c;
}

static const char *strcasestr_local(const char *h, const char *n)
{
    if (!h || !n || !n[0]) {
        return NULL;
    }
    for (; *h; h++) {
        const char *a = h;
        const char *b = n;
        while (*a && *b && lower_c((unsigned char)*a) == lower_c((unsigned char)*b)) {
            a++;
            b++;
        }
        if (!*b) {
            return h;
        }
    }
    return NULL;
}

bool ahc_wow_trust_sanitize_extra_arg(const char *in, char *out, size_t out_size)
{
    if (!in || !out || out_size == 0) {
        return false;
    }
    if (strcasestr_local(in, "-password") != NULL) {
        return false;
    }
    if (strcasestr_local(in, "-login") != NULL) {
        return false;
    }
    in = skip_ws(in);
    size_t j = 0;
    for (const char *p = in; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x20u) {
            return false;
        }
        if (c == (unsigned char)'"' || c == (unsigned char)'`') {
            return false;
        }
        if (j + 1 >= out_size) {
            return false;
        }
        out[j++] = (char)c;
    }
    out[j] = '\0';
    return true;
}

static size_t sh_append_quoted(const char *s, char *out, size_t out_size, size_t at)
{
    if (at + 1 >= out_size) {
        return out_size;
    }
    out[at++] = '\'';
    for (const char *p = s; *p; p++) {
        if ((unsigned char)*p == (unsigned char)'\'') {
            const char *esc = "'\\''";
            for (const char *e = esc; *e; e++) {
                if (at + 1 >= out_size) {
                    return out_size;
                }
                out[at++] = *e;
            }
        } else {
            if (at + 1 >= out_size) {
                return out_size;
            }
            out[at++] = *p;
        }
    }
    if (at + 1 >= out_size) {
        return out_size;
    }
    out[at++] = '\'';
    if (at < out_size) {
        out[at] = '\0';
    }
    return at;
}

static size_t copy_literal(const char *s, char *out, size_t out_size, size_t at)
{
    for (const char *p = s; *p; p++) {
        if (at + 1 >= out_size) {
            return out_size;
        }
        out[at++] = *p;
    }
    if (at < out_size) {
        out[at] = '\0';
    }
    return at;
}

bool ahc_wow_posix_build_args(
    const char *login,
    const char *password,
    const char *realm,
    const char *extra,
    char *out,
    size_t out_size
)
{
    if (!login || !password || !realm || !out || out_size < 8u) {
        return false;
    }
    if (!login[0] || !password[0] || !realm[0]) {
        return false;
    }
    size_t at = 0;
    at = copy_literal("-login ", out, out_size, at);
    if (at >= out_size) {
        return false;
    }
    at = sh_append_quoted(login, out, out_size, at);
    if (at >= out_size) {
        return false;
    }
    if (at + 10u >= out_size) {
        return false;
    }
    at = copy_literal(" -password ", out, out_size, at);
    if (at >= out_size) {
        return false;
    }
    at = sh_append_quoted(password, out, out_size, at);
    if (at >= out_size) {
        return false;
    }
    if (at + 12u >= out_size) {
        return false;
    }
    at = copy_literal(" -realmname ", out, out_size, at);
    if (at >= out_size) {
        return false;
    }
    at = sh_append_quoted(realm, out, out_size, at);
    if (at >= out_size) {
        return false;
    }
    extra = extra ? skip_ws(extra) : NULL;
    if (extra && extra[0]) {
        if (at + 1u >= out_size) {
            return false;
        }
        out[at++] = (char)' ';
        for (const char *e = extra; *e; e++) {
            if (at + 1u >= out_size) {
                return false;
            }
            out[at++] = *e;
        }
    }
    if (at < out_size) {
        out[at] = '\0';
    }
    return at < out_size;
}
