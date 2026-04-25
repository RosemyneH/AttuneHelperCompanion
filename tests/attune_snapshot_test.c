#include "attune/attune_snapshot.h"

#include <stdio.h>
#include <string.h>

static int expect_int(const char *name, int actual, int expected)
{
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: expected %d, got %d\n", name, expected, actual);
    return 1;
}

static int expect_string(const char *name, const char *actual, const char *expected)
{
    if (strcmp(actual, expected) == 0) {
        return 0;
    }

    fprintf(stderr, "%s: expected %s, got %s\n", name, expected, actual);
    return 1;
}

int main(void)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/AttuneHelper.lua", AHC_TEST_FIXTURE_DIR);

    AhcDailyAttuneSnapshot snapshot;
    if (!ahc_parse_daily_attune_snapshot_file(path, &snapshot)) {
        fprintf(stderr, "failed to parse fixture: %s\n", path);
        return 1;
    }

    int failures = 0;
    failures += snapshot.found ? 0 : 1;
    failures += expect_string("date", snapshot.date, "2026-04-25");
    failures += expect_int("account", snapshot.account, 10126);
    failures += expect_int("warforged", snapshot.warforged, 362);
    failures += expect_int("lightforged", snapshot.lightforged, 81);
    failures += expect_int("titanforged", snapshot.titanforged, 2009);

    return failures == 0 ? 0 : 1;
}
