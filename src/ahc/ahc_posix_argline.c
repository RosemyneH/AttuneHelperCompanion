#include "ahc/ahc_posix_argline.h"

#include <ctype.h>
#include <string.h>

int ahc_posix_split_arg_line_to_buf(
    const char *arg_line, char *buf, size_t buf_size, char *out_ptrs[], int max_ptrs
)
{
    if (!out_ptrs || max_ptrs < 1) {
        return -1;
    }
    if (!arg_line) {
        return 0;
    }
    if (!buf || buf_size < 1u) {
        return -1;
    }
    int argc = 0;
    size_t used = 0u;
    for (const char *p = arg_line;;) {
        while (isspace((unsigned char)*p) && *p) {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (argc >= max_ptrs) {
            return -1;
        }
        const char *t = p;
        while (*p != '\0' && !isspace((unsigned char)*p)) {
            p++;
        }
        size_t len = (size_t)(p - t);
        if (used + len + 1u > buf_size) {
            return -1;
        }
        memcpy(buf + used, t, len);
        buf[used + len] = '\0';
        out_ptrs[argc] = buf + used;
        argc++;
        used += len + 1u;
    }
    return argc;
}
