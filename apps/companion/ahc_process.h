#ifndef AHC_PROCESS_H
#define AHC_PROCESS_H

#include <stdbool.h>
#include <stddef.h>

#if defined(_WIN32)
#include <wchar.h>
#endif

bool ahc_run_command_hidden(const char *command);
bool ahc_open_url_hidden(const char *url);
bool ahc_launch_file_hidden(const char *file, const char *parameters, const char *working_dir);
unsigned int ahc_count_other_instances(void);
unsigned int ahc_terminate_other_instances(void);

#if defined(_WIN32)
/**
 * CreateProcessW with a mutable wide command line and UTF-8 working directory.
 */
bool ahc_win_launch_wcmdline_in_dir(const wchar_t *cmdline, const char *workdir_utf8);
#else
#include "ahc/ahc_posix_argline.h"

/**
 * Fork, chdir to workdir, ignore SIGHUP, stdio to /dev/null, execv(program_path, argv).
 * argv[0] must equal program_path. No shell.
 */
bool ahc_posix_spawn_detached_in_workdir(const char *workdir, const char *program_path, char *const *argv);
bool ahc_posix_spawn_shell_detached_in_workdir(const char *workdir, const char *command);

/**
 * Run unzip with argv (no shell). Requires ahc_path_safe_for_arg on both paths; caller may preflight first.
 */
bool ahc_posix_unzip_to_directory(const char *zip_path, const char *dest_dir);
#endif

bool ahc_curl_download_file(const char *url, const char *file_path);
bool ahc_http_download_file(const char *url, const char *file_path, size_t max_bytes);
bool ahc_git_shallow_clone(const char *url, const char *dest_path);
bool ahc_git_submodule_update_init_shallow(const char *repo_root, const char *submodule_path);
bool ahc_preflight_zip_safe(const char *zip_path);

#endif
