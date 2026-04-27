#ifndef AHC_POSIX_ARGLINE_H
#define AHC_POSIX_ARGLINE_H

#include <stddef.h>

/**
 * Whitespace-splits `arg_line` (like typical CLI words; no shell quoting) into
 * `out_ptrs[]`, with content stored in `buf` (each token null-terminated).
 * Returns the number of tokens, 0 for empty, or -1 on overflow/invalid args.
 * Does not set out_ptrs[k] to NULL; caller may set out_ptrs[count] = NULL.
 */
int ahc_posix_split_arg_line_to_buf(
    const char *arg_line,
    char *buf, size_t buf_size,
    char *out_ptrs[], int max_ptrs
);

#endif
