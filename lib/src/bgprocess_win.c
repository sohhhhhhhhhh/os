#include "bgprocess_internal.h"

#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct bg_process {
    HANDLE process;
    DWORD  pid;
    int    finished;
    int    exit_code;
};

// винд берёт аргументы одной строкой надо самой расставить кавычки и экранировать \" и слэши перед кавычками
// dst == NULL считаем длину
static size_t quote_arg(const char *arg, char *dst)
{
    size_t n = 0;
    int need_quotes = (*arg == '\0') || strpbrk(arg, " \t\n\v\"") != NULL;

    if (!need_quotes) {
        size_t len = strlen(arg);
        if (dst) memcpy(dst, arg, len);
        return len;
    }

    if (dst) dst[n] = '"';
    n++;
    for (const char *p = arg;; p++) {
        size_t bs = 0;
        while (*p == '\\') { bs++; p++; }

        if (*p == '\0') {
            for (size_t i = 0; i < bs * 2; i++) { if (dst) dst[n] = '\\'; n++; }
            break;
        } else if (*p == '"') {
            for (size_t i = 0; i < bs * 2 + 1; i++) { if (dst) dst[n] = '\\'; n++; }
            if (dst) dst[n] = '"';
            n++;
        } else {
            for (size_t i = 0; i < bs; i++) { if (dst) dst[n] = '\\'; n++; }
            if (dst) dst[n] = *p;
            n++;
        }
    }
    if (dst) dst[n] = '"';
    n++;
    return n;
}

static char *build_command_line(const char *const argv[])
{
    size_t total = 0;
    for (int i = 0; argv[i]; i++)
        total += quote_arg(argv[i], NULL) + 1;

    char *cmd = (char *)malloc(total + 1);
    if (!cmd) return NULL;

    size_t pos = 0;
    for (int i = 0; argv[i]; i++) {
        if (i > 0) cmd[pos++] = ' ';
        pos += quote_arg(argv[i], cmd + pos);
    }
    cmd[pos] = '\0';
    return cmd;
}

int bg_start(const char *const argv[], bg_process **out)
{
    if (!argv || !argv[0] || !out) return BG_ERR_ARGS;
    *out = NULL;

    char *cmd = build_command_line(argv);
    if (!cmd) return BG_ERR_NOMEM;

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // NULL первым значит виндовсс сам ищет программу в PATH и дописывает .exe
    BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    DWORD err = ok ? 0 : GetLastError(); // до free, а то может затереться
    free(cmd);

    if (!ok) {
        bg_set_os_error((int)err);
        return BG_ERR_START;
    }

    CloseHandle(pi.hThread);

    bg_process *p = (bg_process *)calloc(1, sizeof(*p));
    if (!p) {
        CloseHandle(pi.hProcess);
        return BG_ERR_NOMEM;
    }
    p->process = pi.hProcess;
    p->pid = pi.dwProcessId;
    *out = p;
    return BG_OK;
}

static int wait_impl(bg_process *p, int *exit_code, DWORD timeout)
{
    if (!p) return BG_ERR_ARGS;

    if (!p->finished) {
        DWORD r = WaitForSingleObject(p->process, timeout);
        if (r == WAIT_TIMEOUT) return BG_RUNNING;
        if (r != WAIT_OBJECT_0) {
            bg_set_os_error((int)GetLastError());
            return BG_ERR_WAIT;
        }

        DWORD code;
        if (!GetExitCodeProcess(p->process, &code)) {
            bg_set_os_error((int)GetLastError());
            return BG_ERR_WAIT;
        }
        p->exit_code = (int)code;
        p->finished = 1;
    }
    if (exit_code) *exit_code = p->exit_code;
    return BG_OK;
}

int bg_wait(bg_process *p, int *exit_code)     { return wait_impl(p, exit_code, INFINITE); }
int bg_try_wait(bg_process *p, int *exit_code) { return wait_impl(p, exit_code, 0); }
long bg_pid(const bg_process *p)               { return p ? (long)p->pid : -1; }

void bg_free(bg_process *p)
{
    if (!p) return;
    CloseHandle(p->process);
    free(p);
}
