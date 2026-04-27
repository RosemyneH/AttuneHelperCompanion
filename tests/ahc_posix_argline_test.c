#include "ahc/ahc_posix_argline.h"

#undef NDEBUG
#include <assert.h>
#include <string.h>

int main(void)
{
    char buf[256];
    char *p[16];

    assert(ahc_posix_split_arg_line_to_buf("a b c", buf, sizeof buf, p, 16) == 3);
    assert(strcmp(p[0], "a") == 0);
    assert(strcmp(p[1], "b") == 0);
    assert(strcmp(p[2], "c") == 0);

    assert(ahc_posix_split_arg_line_to_buf("  -windowed  ", buf, sizeof buf, p, 16) == 1);
    assert(strcmp(p[0], "-windowed") == 0);

    assert(ahc_posix_split_arg_line_to_buf("$(id)", buf, sizeof buf, p, 16) == 1);
    assert(strcmp(p[0], "$(id)") == 0);

    assert(ahc_posix_split_arg_line_to_buf("", buf, sizeof buf, p, 16) == 0);

    char tiny[2];
    assert(ahc_posix_split_arg_line_to_buf("ab", tiny, sizeof tiny, p, 16) == -1);

    return 0;
}
