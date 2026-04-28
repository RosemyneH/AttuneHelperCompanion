#include "ahc_process.h"

#include "ahc/ahc_safe_url.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

static bool ahc_posix_execvp_wait(const char *const *argv)
{
#if defined(_WIN32)
    (void)argv;
    return false;
#else
    pid_t p = fork();
    if (p < 0) {
        return false;
    }
    if (p == 0) {
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    int st = 0;
    if (waitpid(p, &st, 0) < 0) {
        return false;
    }
    return WIFEXITED(st) && WEXITSTATUS(st) == 0;
#endif
}

#if defined(_WIN32)
static bool ahc_append_quoted_arg(char *out, size_t out_cap, const char *arg)
{
    size_t used = strlen(out);
    if (used + 3u >= out_cap) {
        return false;
    }
    if (used > 0u) {
        out[used++] = ' ';
        out[used] = '\0';
    }
    out[used++] = '"';
    out[used] = '\0';
    for (const char *p = arg; p && *p; p++) {
        if (used + 3u >= out_cap) {
            return false;
        }
        if (*p == '"' || *p == '\\') {
            out[used++] = '\\';
        }
        out[used++] = *p;
        out[used] = '\0';
    }
    if (used + 2u >= out_cap) {
        return false;
    }
    out[used++] = '"';
    out[used] = '\0';
    return true;
}
#endif

static bool ahc_win_spawnv_wait(const char *exe_basename, const char *const *argv)
{
#if !defined(_WIN32)
    (void)exe_basename;
    (void)argv;
    return false;
#else
    char full[1024];
    char *filepart = NULL;
    if (SearchPathA(NULL, exe_basename, ".exe", sizeof(full), full, &filepart) == 0) {
        return false;
    }
    char command_line[8192] = { 0 };
    if (!ahc_append_quoted_arg(command_line, sizeof(command_line), full)) {
        return false;
    }
    for (size_t i = 1u; argv[i]; i++) {
        if (!ahc_append_quoted_arg(command_line, sizeof(command_line), argv[i])) {
            return false;
        }
    }

    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    ZeroMemory(&startup, sizeof(startup));
    ZeroMemory(&process, sizeof(process));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;

    BOOL ok = CreateProcessA(
        full,
        command_line,
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &startup,
        &process);
    if (!ok) {
        return false;
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 1u;
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return exit_code == 0u;
#endif
}

static bool ahc_append_https_origin_referer(const char *url, char *out, size_t cap)
{
    out[0] = '\0';
    if (!url || strncmp(url, "https://", 8) != 0) {
        return false;
    }
    const char *hoststart = url + 8;
    const char *slash = strchr(hoststart, '/');
    if (slash == NULL) {
        if (strlen(url) + 2u >= cap) {
            return false;
        }
        snprintf(out, cap, "%s/", url);
        return true;
    }
    size_t hostlen = (size_t)(slash - hoststart);
    if (8u + hostlen + 2u >= cap) {
        return false;
    }
    memcpy(out, "https://", 8u);
    memcpy(out + 8, hoststart, hostlen);
    out[8u + hostlen] = '/';
    out[8u + hostlen + 1u] = '\0';
    return true;
}

bool ahc_curl_download_file(const char *url, const char *file_path)
{
    if (!ahc_url_safe_for_download(url) || !ahc_path_safe_for_arg(file_path)) {
        return false;
    }
    char referer[256];
    ahc_append_https_origin_referer(url, referer, sizeof(referer));
    static const char *const user_agent
        = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36";
    const char *argv[16];
    int argc = 0;
    argv[argc++] = "curl";
    argv[argc++] = "-L";
    argv[argc++] = "--fail";
    argv[argc++] = "--silent";
    argv[argc++] = "--show-error";
    if (referer[0]) {
        argv[argc++] = "-e";
        argv[argc++] = referer;
    }
    argv[argc++] = "-A";
    argv[argc++] = user_agent;
    argv[argc++] = "-o";
    argv[argc++] = file_path;
    argv[argc++] = url;
    argv[argc] = NULL;
#if defined(_WIN32)
    return ahc_win_spawnv_wait("curl", argv);
#else
    return ahc_posix_execvp_wait(argv);
#endif
}

bool ahc_git_shallow_clone(const char *url, const char *dest_path)
{
    if (!ahc_url_safe_for_download(url) || !ahc_path_safe_for_arg(dest_path)) {
        return false;
    }
    const char *argv[] = {
        "git",
        "clone",
        "--depth",
        "1",
        url,
        dest_path,
        NULL,
    };
#if defined(_WIN32)
    return ahc_win_spawnv_wait("git", argv);
#else
    return ahc_posix_execvp_wait(argv);
#endif
}

bool ahc_git_submodule_update_init_shallow(const char *repo_root, const char *submodule_path)
{
    if (!ahc_path_safe_for_arg(repo_root) || !ahc_path_safe_for_arg(submodule_path)) {
        return false;
    }
    const char *argv[] = {
        "git",
        "-C",
        repo_root,
        "submodule",
        "update",
        "--init",
        "--depth",
        "1",
        "--",
        submodule_path,
        NULL,
    };
#if defined(_WIN32)
    return ahc_win_spawnv_wait("git", argv);
#else
    return ahc_posix_execvp_wait(argv);
#endif
}

#if defined(_WIN32)
static bool read_tar_list_win(const char *zip_path, char *out, size_t out_cap, size_t *nout)
{
    char tar_exe[1024];
    char *fp = NULL;
    if (SearchPathA(NULL, "tar", ".exe", sizeof(tar_exe), tar_exe, &fp) == 0) {
        return false;
    }
    char cmd[8192];
    int n = snprintf(cmd, sizeof cmd, "\"%s\" -tf \"%s\"", tar_exe, zip_path);
    if (n < 0 || (size_t)n >= sizeof cmd) {
        return false;
    }
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    HANDLE rh = NULL;
    HANDLE wh = NULL;
    if (!CreatePipe(&rh, &wh, &sa, 0u)) {
        return false;
    }
    if (!SetHandleInformation(rh, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(rh);
        CloseHandle(wh);
        return false;
    }
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = wh;
    si.hStdError = wh;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    size_t cmdline_len = (size_t)n + 1u;
    char *mutable_cmd = (char *)malloc(cmdline_len);
    if (mutable_cmd == NULL) {
        CloseHandle(rh);
        CloseHandle(wh);
        return false;
    }
    memcpy(mutable_cmd, cmd, cmdline_len);
    BOOL ok = CreateProcessA(
        NULL,
        mutable_cmd,
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    );
    free(mutable_cmd);
    CloseHandle(wh);
    if (!ok) {
        CloseHandle(rh);
        return false;
    }
    size_t total = 0;
    for (;;) {
        if (total >= out_cap - 1u) {
            (void)TerminateProcess(pi.hProcess, 1u);
            break;
        }
        DWORD br = 0u;
        BOOL r_ok = ReadFile(
            rh,
            out + total,
            (DWORD)(out_cap - 1u - total),
            &br,
            NULL
        );
        if (!r_ok || br == 0u) {
            break;
        }
        total += (size_t)br;
    }
    CloseHandle(rh);
    (void)WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD ex = 1u;
    (void)GetExitCodeProcess(pi.hProcess, &ex);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (ex != 0u) {
        return false;
    }
    if (total >= out_cap - 1u) {
        return false;
    }
    out[total] = '\0';
    *nout = total;
    return true;
}
#endif

#if !defined(_WIN32)
/* GNU tar on typical Linux does not list zip; Windows tar (libarchive) does. */
static bool read_zip_list_posix(const char *zip_path, char *out, size_t out_cap, size_t *nout)
{
    int pfd[2];
    if (pipe(pfd) != 0) {
        return false;
    }
    pid_t ch = fork();
    if (ch < 0) {
        close(pfd[0]);
        close(pfd[1]);
        return false;
    }
    if (ch == 0) {
        close(pfd[0]);
        if (dup2(pfd[1], STDOUT_FILENO) < 0) {
            _exit(127);
        }
        close(pfd[1]);
        const char *argv[] = { "unzip", "-Z", "-1", zip_path, NULL };
        execvp("unzip", (char *const *)argv);
        const char *argv2[] = { "zipinfo", "-1", zip_path, NULL };
        execvp("zipinfo", (char *const *)argv2);
        _exit(127);
    }
    close(pfd[1]);
    size_t total = 0;
    for (;;) {
        if (total >= out_cap - 1u) {
            (void)kill(ch, SIGTERM);
            (void)close(pfd[0]);
            (void)waitpid(ch, NULL, 0);
            return false;
        }
        ssize_t n = read(pfd[0], out + total, out_cap - 1u - total);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (n == 0) {
            break;
        }
        total += (size_t)n;
    }
    close(pfd[0]);
    int st = 0;
    waitpid(ch, &st, 0);
    out[total] = '\0';
    *nout = total;
    return WIFEXITED(st) && WEXITSTATUS(st) == 0;
}
#endif

#if !defined(_WIN32)
bool ahc_posix_spawn_detached_in_workdir(const char *workdir, const char *program_path, char *const *argv)
{
    if (!workdir || !program_path || !argv || !argv[0]) {
        return false;
    }
    if (!ahc_path_safe_for_arg(workdir) || !ahc_path_safe_for_arg(program_path)) {
        return false;
    }
    if (strcmp(argv[0], program_path) != 0) {
        return false;
    }
    pid_t p = fork();
    if (p < 0) {
        return false;
    }
    if (p > 0) {
        return true;
    }
    (void)signal(SIGHUP, SIG_IGN);
    if (chdir(workdir) != 0) {
        _exit(127);
    }
    (void)setsid();
    int d = open("/dev/null", O_RDWR);
    if (d >= 0) {
        (void)dup2(d, STDIN_FILENO);
        (void)dup2(d, STDOUT_FILENO);
        (void)dup2(d, STDERR_FILENO);
        if (d > 2) {
            (void)close(d);
        }
    }
    execv(program_path, argv);
    _exit(127);
}

bool ahc_posix_unzip_to_directory(const char *zip_path, const char *dest_dir)
{
    if (!ahc_path_safe_for_arg(zip_path) || !ahc_path_safe_for_arg(dest_dir)) {
        return false;
    }
    const char *const argv[] = { "unzip", "-q", "-o", zip_path, "-d", dest_dir, NULL };
    return ahc_posix_execvp_wait(argv);
}
#endif

bool ahc_preflight_zip_safe(const char *zip_path)
{
    if (!ahc_path_safe_for_arg(zip_path)) {
        return false;
    }
    char buf[256u * 1024u];
    size_t n = 0;
#if defined(_WIN32)
    if (!read_tar_list_win(zip_path, buf, sizeof buf, &n)) {
        return false;
    }
#else
    if (!read_zip_list_posix(zip_path, buf, sizeof buf, &n)) {
        return false;
    }
#endif
    if (n == 0) {
        return true;
    }
    if (ahc_zip_listing_looks_dangerous(buf, n)) {
        return false;
    }
    return true;
}
