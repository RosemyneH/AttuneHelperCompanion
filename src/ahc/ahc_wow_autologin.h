#ifndef AHC_WOW_AUTOLOGIN_H
#define AHC_WOW_AUTOLOGIN_H

#include <stdbool.h>
#include <stddef.h>

#define AHC_WOW_TRUST_STR_MAX 64

/**
 * Copy `in` to `out` with a strict allowlist (alphanumeric, @ . _ -).
 * Returns false if empty after trim, too long, or disallowed character.
 */
bool ahc_wow_trust_sanitize_user(const char *in, char *out, size_t out_size);

/**
 * Realm: allow alnum, space, underscore, hyphen, dot (AzerothCore, My Realm-1).
 */
bool ahc_wow_trust_sanitize_realm(const char *in, char *out, size_t out_size);

/** Password: no control characters; max out_size-1. */
bool ahc_wow_trust_password(const char *in, char *out, size_t out_size);

/**
 * If `in` contains -password (case-insensitive) or newlines, mark invalid.
 * Otherwise copy to out with ahc_wow_trust_sanitize_user applied to the whole as extra flags
 *   (or allowlist space/comma for simple extra args - keep conservative: same as user).
 * For "extra" launch params when autologin is on, use single-token style or reject.
 * Returns true if out is a safe string (may be empty).
 */
bool ahc_wow_trust_sanitize_extra_arg(const char *in, char *out, size_t out_size);

/**
 * Build POSIX sh-style args for Wine/Proton: -login 'u' -password 'p' -realmname 'r' [ extra ].
 * Each value is single-quoted for the shell. `extra` may be NULL or "".
 * Returns false if any buffer would overflow.
 */
bool ahc_wow_posix_build_args(
    const char *login,
    const char *password,
    const char *realm,
    const char *extra,
    char *out,
    size_t out_size
);

#if defined(_WIN32)
#include <wchar.h>
/**
 * Build a full CreateProcessW command line. Returns 0 on success, -1 on error.
 */
int ahc_wow_win_build_cmdline(
    const char *exe_path,
    const char *login,
    const char *password,
    const char *realm,
    const char *extra,
    wchar_t *out,
    int out_wchars
);
#endif

#endif
