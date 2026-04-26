#include "ahc/ahc_wow_autologin.h"

#include <stdio.h>
#include <string.h>

static int fail_count;

static void check_true(const char *name, bool cond)
{
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", name);
        fail_count++;
    }
}

static void check_streq(const char *name, const char *a, const char *b)
{
    if (!a || !b || strcmp(a, b) != 0) {
        fprintf(stderr, "FAIL: %s (got '%s' expected '%s')\n", name, a ? a : "(null)", b ? b : "(null)");
        fail_count++;
    }
}

int main(void)
{
    char out[128];

    check_true("reject bad user (space)", !ahc_wow_trust_sanitize_user("bad name", out, sizeof out));
    check_true("accept user", ahc_wow_trust_sanitize_user("player@mail.com", out, sizeof out));
    check_streq("user stripped", out, "player@mail.com");

    check_true("reject bad realm", !ahc_wow_trust_sanitize_realm("bad;drop", out, sizeof out));
    check_true("accept realm", ahc_wow_trust_sanitize_realm("AzerothCore 1", out, sizeof out));
    check_streq("realm", out, "AzerothCore 1");

    check_true("password control", !ahc_wow_trust_password("a\nb", out, sizeof out));
    check_true("password ok", ahc_wow_trust_password("secr3t!", out, sizeof out));
    check_streq("password", out, "secr3t!");

    check_true("extra rejects -password", !ahc_wow_trust_sanitize_extra_arg("x -password y", out, sizeof out));
    check_true("extra ok empty", ahc_wow_trust_sanitize_extra_arg("", out, sizeof out) && out[0] == '\0');
    check_true("extra ok", ahc_wow_trust_sanitize_extra_arg("  -console", out, sizeof out));
    check_streq("extra trim", out, "-console");

    char argline[512];
    check_true("posix args", ahc_wow_posix_build_args("u1", "p1", "Realm X", "-console", argline, sizeof argline));
    check_streq("posix golden", argline, "-login 'u1' -password 'p1' -realmname 'Realm X' -console");
    check_true("posix has login", strstr(argline, "-login") != NULL);
    check_true("posix has password", strstr(argline, "-password") != NULL);
    check_true("posix has realm", strstr(argline, "-realmname") != NULL);
    check_true("posix has extra", strstr(argline, "-console") != NULL);

    return fail_count > 0 ? 1 : 0;
}
