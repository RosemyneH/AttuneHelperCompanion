#include "ahc_process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>

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

bool ahc_win_launch_wcmdline_in_dir(const wchar_t *cmdline, const char *workdir_utf8)
{
    if (!cmdline || !*cmdline || !workdir_utf8) {
        return false;
    }
    size_t wdc = 0u;
    int wdneed = MultiByteToWideChar(CP_UTF8, 0, workdir_utf8, -1, NULL, 0);
    if (wdneed <= 0) {
        return false;
    }
    wchar_t *wd = (wchar_t *)malloc((size_t)wdneed * sizeof(wchar_t));
    if (!wd) {
        return false;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, workdir_utf8, -1, wd, wdneed) <= 0) {
        free(wd);
        return false;
    }
    wdc = (size_t)wdneed;
    (void)wdc;
    size_t clen = wcslen(cmdline);
    size_t cbytes = (clen + 1u) * sizeof(wchar_t);
    wchar_t *m = (wchar_t *)malloc(cbytes);
    if (!m) {
        free(wd);
        return false;
    }
    memcpy(m, cmdline, cbytes);

    STARTUPINFOW su = { 0 };
    PROCESS_INFORMATION pi;
    su.cb = sizeof(su);
    if (!CreateProcessW(
            NULL,
            m,
            NULL,
            NULL,
            FALSE,
            0,
            NULL,
            wd,
            &su,
            &pi
        )) {
        free(m);
        free(wd);
        return false;
    }
    free(m);
    free(wd);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
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

bool ahc_posix_run_detached_shell(const char *command)
{
    if (!command || !command[0]) {
        return false;
    }
    return system(command) == 0;
}

#endif
