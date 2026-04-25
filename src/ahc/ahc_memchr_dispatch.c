#include "ahc/ahc_memchr_masm.h"

#include <string.h>

const unsigned char *ahc_memchr_masm64(const unsigned char *p, int c, size_t n)
{
    if (p == NULL) {
        return NULL;
    }
    return (const unsigned char *)memchr(p, c & 0xFF, n);
}
