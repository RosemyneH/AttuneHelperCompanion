#ifndef AHC_FIND_LITERAL_H
#define AHC_FIND_LITERAL_H

#include <stddef.h>

const char *ahc_find_literal_scalar(const char *text, const char *literal);
const char *ahc_find_literal(
    const char *text, const char *text_end, const char *literal, size_t literal_len
);

const char *ahc_find_char_range(const void *p, int ch, const void *end);

#endif
