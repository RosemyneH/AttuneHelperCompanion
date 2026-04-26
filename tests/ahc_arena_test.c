#include "ahc/ahc_arena.h"

#include <stdio.h>
#include <string.h>

static int expect_size(const char *name, size_t actual, size_t expected)
{
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %zu, got %zu\n", name, expected, actual);
    return 1;
}

int main(void)
{
    int failures = 0;

    if (ahc_arena_init(NULL, 64) != 0) {
        fprintf(stderr, "ahc_arena_init should reject NULL\n");
        failures += 1;
    }

    AhcArena a = { 0 };
    if (!ahc_arena_init(&a, 0)) {
        fprintf(stderr, "ahc_arena_init(&a,0) should succeed\n");
        failures += 1;
    } else if (a.data != NULL) {
        fprintf(stderr, "zero cap should leave data NULL\n");
        failures += 1;
    }
    ahc_arena_free(&a);

    if (!ahc_arena_init(&a, 256) || a.data == NULL) {
        fprintf(stderr, "ahc_arena_init with non-zero cap failed\n");
        return 1;
    }
    if (ahc_arena_strcpy(&a, NULL) != NULL) {
        fprintf(stderr, "strcpy of NULL should fail\n");
        failures += 1;
    }
    {
        const char *s1 = ahc_arena_strcpy(&a, "x");
        const char *s2 = ahc_arena_strcpy(&a, "y");
        if (!s1 || !s2 || strcmp(s1, "x") != 0 || strcmp(s2, "y") != 0) {
            fprintf(stderr, "strcpy or allocation failed\n");
            failures += 1;
        }
    }
    char *pb = NULL;
    const char in[] = { 0x00, 0x01, 0x02 };
    if (!ahc_arena_push_bytes(&a, in, 3, &pb) || memcmp(pb, in, 3) != 0) {
        fprintf(stderr, "push_bytes failed or wrong data\n");
        failures += 1;
    }
    {
        AhcArena *null_arena = NULL;
        if (ahc_arena_push_bytes(null_arena, in, 0, &pb) != 0) {
            fprintf(stderr, "push_bytes should reject null arena with len 0 and out\n");
            failures += 1;
        }
    }
    failures += expect_size("used", ahc_arena_used(&a), 35u);
    ahc_arena_free(&a);
    failures += expect_size("used_after_free", ahc_arena_used(&a), 0u);
    if (a.data != NULL) {
        fprintf(stderr, "free should null data\n");
        failures += 1;
    }

    return failures == 0 ? 0 : 1;
}
