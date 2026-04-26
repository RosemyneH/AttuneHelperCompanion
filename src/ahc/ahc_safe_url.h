#ifndef AHC_AHC_SAFE_URL_H
#define AHC_AHC_SAFE_URL_H

#include <stdbool.h>
#include <stddef.h>

/* https:// only; rejects quotes, newlines, control characters; max length. */
bool ahc_url_safe_for_download(const char *url);

/* Local path for -o and clone dest: no quotes or control bytes. */
bool ahc_path_safe_for_arg(const char *path);

/* One line from "tar -tf" / zip listing. */
bool ahc_zip_list_line_looks_dangerous(const char *line);

/* Full listing buffer (UTF-8); rejects if any line is dangerous. */
bool ahc_zip_listing_looks_dangerous(const char *data, size_t n);

#endif
