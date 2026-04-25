#include "ahc_process.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

bool ahc_run_command_hidden(const char *command)
{
    if (!command || !command[0]) {
        return false;
    }

    char command_line[4096];
    snprintf(command_line, sizeof(command_line), "%s", command);

    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    ZeroMemory(&startup, sizeof(startup));
    ZeroMemory(&process, sizeof(process));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;

    BOOL ok = CreateProcessA(
        NULL,
        command_line,
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &startup,
        &process
    );
    if (!ok) {
        return false;
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 1u;
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return exit_code == 0u;
}

bool ahc_open_url_hidden(const char *url)
{
    if (!url || !url[0]) {
        return false;
    }
    HINSTANCE result = ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
    return (UINT_PTR)result > 32u;
}

bool ahc_launch_file_hidden(const char *file, const char *parameters, const char *working_dir)
{
    if (!file || !file[0]) {
        return false;
    }
    HINSTANCE result = ShellExecuteA(NULL, "open", file, parameters, working_dir, SW_SHOWNORMAL);
    return (UINT_PTR)result > 32u;
}

#else

bool ahc_run_command_hidden(const char *command)
{
    return command && command[0] && system(command) == 0;
}

bool ahc_open_url_hidden(const char *url)
{
    if (!url || !url[0]) {
        return false;
    }
#if defined(__APPLE__)
    char command[4096];
    snprintf(command, sizeof(command), "open \"%s\" >/dev/null 2>&1", url);
    return system(command) == 0;
#else
    char command[4096];
    snprintf(command, sizeof(command), "xdg-open \"%s\" >/dev/null 2>&1", url);
    return system(command) == 0;
#endif
}

bool ahc_launch_file_hidden(const char *file, const char *parameters, const char *working_dir)
{
    (void)parameters;
    (void)working_dir;
    return file && file[0];
}

#endif
