#ifndef AHC_PROCESS_H
#define AHC_PROCESS_H

#include <stdbool.h>

#if defined(_WIN32)
#include <wchar.h>
#endif

bool ahc_run_command_hidden(const char *command);
bool ahc_open_url_hidden(const char *url);
bool ahc_launch_file_hidden(const char *file, const char *parameters, const char *working_dir);

#if defined(_WIN32)
/**
 * CreateProcessW with a mutable wide command line and UTF-8 working directory.
 */
bool ahc_win_launch_wcmdline_in_dir(const wchar_t *cmdline, const char *workdir_utf8);
#else
/**
 * Runs a full shell line (e.g. nohup wine … &) for detached game launch.
 */
bool ahc_posix_run_detached_shell(const char *command);
#endif

bool ahc_curl_download_file(const char *url, const char *file_path);
bool ahc_git_shallow_clone(const char *url, const char *dest_path);
bool ahc_preflight_zip_safe(const char *zip_path);

#endif
