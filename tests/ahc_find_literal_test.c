#include "ahc/ahc_find_literal.h"

#include <stdio.h>
#include <string.h>

static int expect_ptr(const char *name, const void *actual, const void *expected)
{
    if (actual == expected) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %p, got %p\n",
        name,
        (const void *)expected,
        (const void *)actual
    );
    return 1;
}

int main(void)
{
    int failures = 0;

    failures += expect_ptr("find_literal_scalar null text", ahc_find_literal_scalar(NULL, "a"), NULL);
    failures += expect_ptr("find_literal_scalar null lit", ahc_find_literal_scalar("a", NULL), NULL);

    const char *hay = "abbbc";
    const char *f = ahc_find_literal_scalar(hay, "bb");
    if (!f || f != hay + 1) {
        fprintf(
            stderr,
            "expected bb at off 1, got %s\n",
            f ? f : "NULL"
        );
        failures += 1;
    }

    failures += expect_ptr(
        "find_literal len0",
        ahc_find_literal("hi", "hi" + 2, "x", 0u),
        NULL
    );
    {
        const char t[] = "abc";
        const char *e = t + 3u;
        failures += expect_ptr(
            "no room for literal",
            ahc_find_literal(t, e, "abcd", 4u),
            NULL
        );
    }
    {
        const char t[] = "hello";
        const char *e = t + 5u;
        const char *h = ahc_find_literal(t, e, "ll", 2u);
        if (!h || h != t + 2) {
            fprintf(
                stderr,
                "expected ll in hello, got %s\n",
                h ? h : "NULL"
            );
            failures += 1;
        }
    }
    {
        const char *g = ahc_find_literal("hello", NULL, "e", 1u);
        if (g == NULL) {
            fprintf(
                stderr,
                "ahc_find_literal with no end should find\n"
            );
            failures += 1;
        } else {
            if (*g != (char)0x65) {
                fprintf(
                    stderr,
                    "expected e\n"
                );
                failures += 1;
            }
        }
    }
    {
        const unsigned char buf[] = { 0x0a, 0x0b, 0x0a };
        const char *r = ahc_find_char_range(buf, 0x0a, buf + 3u);
        if (r == NULL) {
            fprintf(
                stderr,
                "ahc_find_char_range should find first 0x0a\n"
            );
            failures += 1;
        } else {
            if (r != (const char *)buf) {
                fprintf(
                    stderr,
                    "expected first 0x0a\n"
                );
                failures += 1;
            }
        }
    }
    failures += expect_ptr("find_char s>=e", ahc_find_char_range("x", 0, "x" + 0u), NULL);

    return failures == 0 ? 0 : 1;
}
