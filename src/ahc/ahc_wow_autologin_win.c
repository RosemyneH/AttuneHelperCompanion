#include "ahc/ahc_wow_autologin.h"

#if defined(_WIN32)

#include <windows.h>
#include <stdlib.h>
#include <string.h>

static int append_quoted_w(const wchar_t *s, wchar_t *out, int cap, int pos)
{
    if (!*s) {
        if (pos + 2 >= cap) {
            return -1;
        }
        out[pos++] = L'\"';
        out[pos++] = L'\"';
        return pos;
    }
    int n = (wcschr(s, L' ') == NULL && wcschr(s, L'\t') == NULL) ? 1 : 0;
    if (n) {
        for (const wchar_t *p = s; *p; p++) {
            if (pos + 1 >= cap) {
                return -1;
            }
            out[pos++] = *p;
        }
        return pos;
    }
    if (pos + 1 >= cap) {
        return -1;
    }
    out[pos++] = L'\"';
    for (const wchar_t *p = s; *p; p++) {
        if (*p == (wchar_t)L'\"' || *p == (wchar_t)L'\\') {
            if (pos + 1 >= cap) {
                return -1;
            }
            out[pos++] = L'\\';
        }
        if (pos + 1 >= cap) {
            return -1;
        }
        out[pos++] = *p;
    }
    if (pos + 1 >= cap) {
        return -1;
    }
    out[pos++] = L'\"';
    return pos;
}

int ahc_wow_win_build_cmdline(
    const char *exe_path,
    const char *login,
    const char *password,
    const char *realm,
    const char *extra,
    wchar_t *out,
    int out_wchars
)
{
    if (!exe_path || !login || !password || !realm || !out || out_wchars < 8) {
        return -1;
    }
    int wexe = MultiByteToWideChar(CP_UTF8, 0, exe_path, -1, NULL, 0);
    int wlo = MultiByteToWideChar(CP_UTF8, 0, login, -1, NULL, 0);
    int wpa = MultiByteToWideChar(CP_UTF8, 0, password, -1, NULL, 0);
    int wre = MultiByteToWideChar(CP_UTF8, 0, realm, -1, NULL, 0);
    int wex = 0;
    if (extra && extra[0]) {
        wex = MultiByteToWideChar(CP_UTF8, 0, extra, -1, NULL, 0);
    }
    if (wexe <= 0 || wlo <= 0 || wpa <= 0 || wre <= 0) {
        return -1;
    }
    wchar_t *Wexe = (wchar_t *)malloc((size_t)wexe * sizeof(wchar_t));
    wchar_t *Wlo = (wchar_t *)malloc((size_t)wlo * sizeof(wchar_t));
    wchar_t *Wpa = (wchar_t *)malloc((size_t)wpa * sizeof(wchar_t));
    wchar_t *Wre = (wchar_t *)malloc((size_t)wre * sizeof(wchar_t));
    wchar_t *Wex = NULL;
    if (!Wexe || !Wlo || !Wpa || !Wre) {
        free(Wexe);
        free(Wlo);
        free(Wpa);
        free(Wre);
        return -1;
    }
    MultiByteToWideChar(CP_UTF8, 0, exe_path, -1, Wexe, wexe);
    MultiByteToWideChar(CP_UTF8, 0, login, -1, Wlo, wlo);
    MultiByteToWideChar(CP_UTF8, 0, password, -1, Wpa, wpa);
    MultiByteToWideChar(CP_UTF8, 0, realm, -1, Wre, wre);
    if (wex > 0) {
        Wex = (wchar_t *)malloc((size_t)wex * sizeof(wchar_t));
        if (!Wex) {
            free(Wexe);
            free(Wlo);
            free(Wpa);
            free(Wre);
            return -1;
        }
        MultiByteToWideChar(CP_UTF8, 0, extra, -1, Wex, wex);
    }

    int pos = 0;
    int r;
    int success = 0;
    r = append_quoted_w(Wexe, out, out_wchars, pos);
    if (r < 0) {
        goto err;
    }
    pos = r;
    {
        const wchar_t *a = L" -login ";
        for (const wchar_t *p = a; *p; p++) {
            if (pos + 1 >= out_wchars) {
                pos = -1;
                goto err;
            }
            out[pos++] = *p;
        }
    }
    r = append_quoted_w(Wlo, out, out_wchars, pos);
    if (r < 0) {
        goto err;
    }
    pos = r;
    {
        const wchar_t *a = L" -password ";
        for (const wchar_t *p = a; *p; p++) {
            if (pos + 1 >= out_wchars) {
                pos = -1;
                goto err;
            }
            out[pos++] = *p;
        }
    }
    r = append_quoted_w(Wpa, out, out_wchars, pos);
    if (r < 0) {
        goto err;
    }
    pos = r;
    {
        const wchar_t *a = L" -realmname ";
        for (const wchar_t *p = a; *p; p++) {
            if (pos + 1 >= out_wchars) {
                pos = -1;
                goto err;
            }
            out[pos++] = *p;
        }
    }
    r = append_quoted_w(Wre, out, out_wchars, pos);
    if (r < 0) {
        goto err;
    }
    pos = r;
    if (Wex && Wex[0]) {
        if (pos + 1 >= out_wchars) {
            pos = -1;
            goto err;
        }
        out[pos++] = (wchar_t)L' ';
        r = append_quoted_w(Wex, out, out_wchars, pos);
        if (r < 0) {
            pos = r;
            goto err;
        }
        pos = r;
    }
    if (pos + 1 >= out_wchars) {
        goto err;
    }
    out[pos] = L'\0';
    success = 1;
err:
    free(Wex);
    free(Wexe);
    free(Wlo);
    free(Wpa);
    free(Wre);
    return success ? 0 : -1;
}
#endif
