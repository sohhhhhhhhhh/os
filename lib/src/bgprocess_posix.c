#include "bgprocess_internal.h"

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
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

    bg_process *p = (bg_process *)calloc(1, sizeof(*p));
    if (!p) return BG_ERR_NOMEM;

    // pipe чтобы узнать упал ли exec у ребёнка
    // cloexec- exec ок то pipe сам закрылся, читаем 0 байт
    // exec упал то ребёнок пишет туда errno
    int fds[2];
    if (pipe(fds) != 0) {
        bg_set_os_error(errno);
        free(p);
        return BG_ERR_START;
    }
    fcntl(fds[0], F_SETFD, FD_CLOEXEC);
    fcntl(fds[1], F_SETFD, FD_CLOEXEC);

    pid_t pid = fork();
    if (pid < 0) {
        int err = errno;
        close(fds[0]);
        close(fds[1]);
        free(p);
        bg_set_os_error(err);
        return BG_ERR_START;
    }

    if (pid == 0) {
        // ребёнок
        close(fds[0]);
        execvp(argv[0], (char *const *)argv);
        int err = errno;
        ssize_t w = write(fds[1], &err, sizeof(err));
        (void)w;
        _exit(127);
    }

    // родитель
    close(fds[1]);
    int child_errno = 0;
    ssize_t n;
    do {
        n = read(fds[0], &child_errno, sizeof(child_errno));
    } while (n < 0 && errno == EINTR);
    close(fds[0]);

    if (n > 0) {
        waitpid(pid, NULL, 0); // забрать
        free(p);
        bg_set_os_error(child_errno);
        return BG_ERR_START;
    }

    p->pid = pid;
    *out = p;
    return BG_OK;
}

static int wait_impl(bg_process *p, int *exit_code, int options)
{
    if (!p) return BG_ERR_ARGS;

    if (!p->finished) {
        int status = 0;
        pid_t r;
        do {
            r = waitpid(p->pid, &status, options);
        } while (r < 0 && errno == EINTR);

        if (r == 0) return BG_RUNNING;
        if (r < 0) {
            bg_set_os_error(errno);
            return BG_ERR_WAIT;
        }

        if (WIFEXITED(status))
            p->exit_code = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
            p->exit_code = 128 + WTERMSIG(status); //как в шелле
        else
            p->exit_code = -1;
        p->finished = 1;
    }
    if (exit_code) *exit_code = p->exit_code;
    return BG_OK;
}

int bg_wait(bg_process *p, int *exit_code)     { return wait_impl(p, exit_code, 0); }
int bg_try_wait(bg_process *p, int *exit_code) { return wait_impl(p, exit_code, WNOHANG); }
long bg_pid(const bg_process *p)               { return p ? (long)p->pid : -1; }
void bg_free(bg_process *p)                    { free(p); }
