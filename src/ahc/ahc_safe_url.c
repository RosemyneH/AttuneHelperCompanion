#include "ahc/ahc_safe_url.h"

#include <string.h>

#define AHC_URL_MAX 4095u
#define AHC_PATH_ARG_MAX 4095u
#define AHC_ZIP_LIST_LINE_MAX 2048u

static bool ahc_has_banned_chars_in_url(const char *url, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)url[i];
        if (c < 32) {
            return true;
        }
        if (url[i] == '"' || url[i] == '\'' || url[i] == '`' || url[i] == '\t') {
            return true;
        }
    }
    return false;
}

bool ahc_url_safe_for_download(const char *url)
{
    if (url == NULL) {
        return false;
    }
    size_t len = strlen(url);
    if (len < 8 || len > AHC_URL_MAX) {
        return false;
    }
    if (strncmp(url, "https://", 8) != 0) {
        return false;
    }
    for (const char *p = url; *p; p++) {
        if (*p == ' ') {
            return false;
        }
    }
    if (ahc_has_banned_chars_in_url(url, len)) {
        return false;
    }
    return true;
}

bool ahc_path_safe_for_arg(const char *path)
{
    if (path == NULL || !path[0]) {
        return false;
    }
    size_t len = strlen(path);
    if (len == 0 || len > AHC_PATH_ARG_MAX) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if (path[i] == '"' || path[i] == '\'' || (unsigned char)path[i] < 32) {
            return false;
        }
    }
    return true;
}

bool ahc_zip_list_line_looks_dangerous(const char *line)
{
    if (line == NULL) {
        return true;
    }
    size_t n = strcspn(line, "\r\n");
    if (n == 0) {
        return false;
    }
    if (n > AHC_ZIP_LIST_LINE_MAX) {
        return true;
    }
    char buf[2048 + 1u];
    if (n > sizeof buf - 1u) {
        return true;
    }
    memcpy(buf, line, n);
    buf[n] = '\0';
    for (char *c = buf; *c; c++) {
        if (*c == '\\') {
            *c = '/';
        }
    }
    if (buf[0] == '/' || (buf[0] && buf[1] == ':')) {
        return true;
    }
    for (const char *s = buf;;) {
        const char *slash = strchr(s, '/');
        size_t seg;
        if (slash == NULL) {
            seg = strlen(s);
        } else {
            seg = (size_t)(slash - s);
        }
        if (seg == 2u && s[0] == '.' && s[1] == '.') {
            return true;
        }
        for (size_t j = 0; j < seg; j++) {
            if (s[j] < 32) {
                return true;
            }
        }
        if (slash == NULL) {
            break;
        }
        s = slash + 1u;
    }
    return false;
}

bool ahc_zip_listing_looks_dangerous(const char *data, size_t n)
{
    if (data == NULL) {
        return n > 0u;
    }
    if (n == 0) {
        return false;
    }
    size_t i = 0;
    while (i < n) {
        size_t linestart = i;
        while (i < n && data[i] != '\n' && data[i] != '\r') {
            i++;
        }
        size_t linelen = i - linestart;
        if (linelen > 0) {
            char one[AHC_ZIP_LIST_LINE_MAX + 2u];
            if (linelen > AHC_ZIP_LIST_LINE_MAX) {
                return true;
            }
            memcpy(one, data + linestart, linelen);
            one[linelen] = '\0';
            if (ahc_zip_list_line_looks_dangerous(one)) {
                return true;
            }
        }
        while (i < n && (data[i] == '\n' || data[i] == '\r')) {
            i++;
        }
    }
    return false;
}
