#ifndef AHC_PROCESS_H
#define AHC_PROCESS_H

#include <stdbool.h>

bool ahc_run_command_hidden(const char *command);
bool ahc_open_url_hidden(const char *url);
bool ahc_launch_file_hidden(const char *file, const char *parameters, const char *working_dir);

#endif
