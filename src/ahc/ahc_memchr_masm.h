#ifndef AHC_MEMCHR_MASM_H
#define AHC_MEMCHR_MASM_H

#include <stddef.h>

const unsigned char *ahc_memchr_masm64(const unsigned char *p, int c, size_t n);

#endif
