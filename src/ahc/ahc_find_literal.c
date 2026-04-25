#include "ahc/ahc_find_literal.h"
#include "ahc/ahc_memchr_masm.h"

#include <string.h>

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
#include <emmintrin.h>
#define AHC_USE_SSE2_STR 1
#endif

const char *ahc_find_literal_scalar(const char *text, const char *literal)
{
    if (!text || !literal) {
        return NULL;
    }
    return strstr(text, literal);
}

const char *ahc_find_char_range(const void *p, int ch, const void *end)
{
    const unsigned char *s = (const unsigned char *)p;
    const unsigned char *e = (const unsigned char *)end;
    if (!s || !e || s >= e) {
        return NULL;
    }
    return (const char *)ahc_memchr_masm64(s, ch, (size_t)(e - s));
}

#if defined(AHC_USE_SSE2_STR)
static int prefix_match_16(const char *p, const char *literal)
{
    const __m128i a = _mm_loadu_si128((const __m128i *)(const void *)p);
    const __m128i b = _mm_loadu_si128((const __m128i *)(const void *)literal);
    const int m = _mm_movemask_epi8(_mm_cmpeq_epi8(a, b));
    return m == 0xFFFF;
}
#endif

const char *ahc_find_literal(
    const char *text, const char *text_end, const char *literal, size_t literal_len
)
{
    if (!text || !literal || literal_len == 0) {
        return NULL;
    }
    if (text_end == NULL) {
        return strstr(text, literal);
    }
    if (literal_len > (size_t)(text_end - text)) {
        return NULL;
    }
    const char *p = text;
    const char *end = text_end - literal_len + 1;
#if defined(AHC_USE_SSE2_STR)
    if (literal_len >= 16) {
        const unsigned char first = (unsigned char)literal[0];
        while (p < end) {
            const char *c = ahc_find_char_range(p, (int)first, text_end);
            if (c == NULL) {
                return NULL;
            }
            if (c >= end) {
                return NULL;
            }
            if (c + literal_len > text_end) {
                return NULL;
            }
            if (prefix_match_16(c, literal) && memcmp(c + 16, literal + 16, literal_len - 16) == 0) {
                return c;
            }
            p = c + 1u;
        }
        return NULL;
    }
#endif
    for (const char *c = p; c < end; c++) {
        if (memcmp(c, literal, literal_len) == 0) {
            return c;
        }
    }
    return NULL;
}
