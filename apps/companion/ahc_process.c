#if defined(__linux__) && !defined(_DEFAULT_SOURCE) && !defined(_GNU_SOURCE)
#define _DEFAULT_SOURCE
#endif

#include "ahc_process.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <psapi.h>

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

static bool ahc_windows_current_exe(char *out, size_t out_size)
{
    DWORD n = GetModuleFileNameA(NULL, out, (DWORD)out_size);
    return n > 0u && n < (DWORD)out_size;
}

static bool ahc_windows_process_image(DWORD pid, char *out, size_t out_size, DWORD access)
{
    HANDLE process = OpenProcess(access, FALSE, pid);
    if (!process) {
        return false;
    }
    DWORD size = (DWORD)out_size;
    BOOL ok = QueryFullProcessImageNameA(process, 0, out, &size);
    CloseHandle(process);
    if (!ok || size == 0u || size >= (DWORD)out_size) {
        return false;
    }
    out[size] = '\0';
    return true;
}

static unsigned int ahc_windows_other_instances(bool terminate)
{
    char self_path[MAX_PATH];
    if (!ahc_windows_current_exe(self_path, sizeof(self_path))) {
        return 0u;
    }

    DWORD pids[4096];
    DWORD bytes = 0u;
    if (!EnumProcesses(pids, sizeof(pids), &bytes)) {
        return 0u;
    }

    unsigned int matched = 0u;
    DWORD self_pid = GetCurrentProcessId();
    size_t count = (size_t)(bytes / sizeof(pids[0]));
    for (size_t i = 0u; i < count; i++) {
        DWORD pid = pids[i];
        if (pid == 0u || pid == self_pid) {
            continue;
        }

        char process_path[MAX_PATH];
        DWORD access = PROCESS_QUERY_LIMITED_INFORMATION | (terminate ? PROCESS_TERMINATE : 0u);
        if (!ahc_windows_process_image(pid, process_path, sizeof(process_path), access)) {
            continue;
        }
        if (_stricmp(process_path, self_path) != 0) {
            continue;
        }

        matched++;
        if (terminate) {
            HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
            if (process) {
                (void)TerminateProcess(process, 0u);
                CloseHandle(process);
            }
        }
    }

    return matched;
}

unsigned int ahc_count_other_instances(void)
{
    return ahc_windows_other_instances(false);
}

unsigned int ahc_terminate_other_instances(void)
{
    return ahc_windows_other_instances(true);
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

#include <dirent.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

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

static bool ahc_posix_current_exe(char *out, size_t out_size)
{
#if defined(__linux__)
    ssize_t n = readlink("/proc/self/exe", out, out_size - 1u);
    if (n < 0 || (size_t)n >= out_size) {
        return false;
    }
    out[n] = '\0';
    return true;
#else
    (void)out;
    (void)out_size;
    return false;
#endif
}

static bool ahc_is_pid_name(const char *name)
{
    if (!name || !name[0]) {
        return false;
    }
    for (const char *p = name; *p; p++) {
        if (!isdigit((unsigned char)*p)) {
            return false;
        }
    }
    return true;
}

static unsigned int ahc_posix_other_instances(bool terminate)
{
#if defined(__linux__)
    char self_path[4096];
    if (!ahc_posix_current_exe(self_path, sizeof(self_path))) {
        return 0u;
    }

    DIR *proc = opendir("/proc");
    if (!proc) {
        return 0u;
    }

    unsigned int matched = 0u;
    pid_t self_pid = getpid();
    struct dirent *entry = NULL;
    while ((entry = readdir(proc)) != NULL) {
        if (!ahc_is_pid_name(entry->d_name)) {
            continue;
        }
        pid_t pid = (pid_t)strtol(entry->d_name, NULL, 10);
        if (pid <= 0 || pid == self_pid) {
            continue;
        }

        char exe_link[128];
        snprintf(exe_link, sizeof(exe_link), "/proc/%ld/exe", (long)pid);
        char process_path[4096];
        ssize_t n = readlink(exe_link, process_path, sizeof(process_path) - 1u);
        if (n < 0) {
            continue;
        }
        process_path[n] = '\0';
        if (strcmp(process_path, self_path) != 0) {
            continue;
        }

        matched++;
        if (terminate) {
            (void)kill(pid, SIGTERM);
        }
    }

    closedir(proc);
    return matched;
#else
    (void)terminate;
    return 0u;
#endif
}

unsigned int ahc_count_other_instances(void)
{
    return ahc_posix_other_instances(false);
}

unsigned int ahc_terminate_other_instances(void)
{
    return ahc_posix_other_instances(true);
}

#endif
