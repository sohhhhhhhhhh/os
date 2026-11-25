#include "bgprocess_internal.h"

#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

struct bg_process {
    pid_t pid;
    int   finished;
    int   exit_code;
};

int bg_start(const char *const argv[], bg_process **out)
{
    if (!argv || !argv[0] || !out) return BG_ERR_ARGS;
    *out = NULL;

    bg_process *p = calloc(1, sizeof(*p));
    if (!p) return BG_ERR_NOMEM;

    pid_t pid = fork();
    if (pid < 0) {
        bg_set_os_error(errno);
        free(p);
        return BG_ERR_START;
    }

    if (pid == 0) {
        // ребёнок
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }

    // родитель: запомнили номер ребёнка и сразу вернулись
    p->pid = pid;
    *out = p;
    return BG_OK;
}

static int wait_impl(bg_process *p, int *exit_code, int options)
{
    if (!p) return BG_ERR_ARGS;

    if (!p->finished) {
        int status;
        pid_t r = waitpid(p->pid, &status, options);
        if (r == 0) return BG_RUNNING;
        if (r < 0) {
            bg_set_os_error(errno);
            return BG_ERR_WAIT;
        }

        if (WIFEXITED(status))
            p->exit_code = WEXITSTATUS(status);   // сам завершился
        else
            p->exit_code = 128 + WTERMSIG(status); // убили сигналом
        p->finished = 1;
    }
    if (exit_code) *exit_code = p->exit_code;
    return BG_OK;
}

int bg_wait(bg_process *p, int *exit_code)     { return wait_impl(p, exit_code, 0); }
int bg_try_wait(bg_process *p, int *exit_code) { return wait_impl(p, exit_code, WNOHANG); }
long bg_pid(const bg_process *p)               { return p ? (long)p->pid : -1; }
void bg_free(bg_process *p)                    { free(p); }