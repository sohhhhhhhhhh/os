#ifndef BGPROCESS_H
#define BGPROCESS_H

#include "bgprocess_export.h"

#ifdef __cplusplus
extern "C" {
#endif

// внутри pid_t или хендл, снаружи не видно
typedef struct bg_process bg_process;

enum {
    BG_OK        = 0,
    BG_RUNNING   = 1,
    BG_ERR_ARGS  = -1,
    BG_ERR_START = -2,
    BG_ERR_WAIT  = -3,
    BG_ERR_NOMEM = -4
};

//argv как в main, в конце нал
BGPROCESS_EXPORT int  bg_start(const char *const argv[], bg_process **out);
BGPROCESS_EXPORT int  bg_wait(bg_process *p, int *exit_code);
// не блокирует, если ещё работает
BGPROCESS_EXPORT int  bg_try_wait(bg_process *p, int *exit_code);
BGPROCESS_EXPORT long bg_pid(const bg_process *p);
// процесс не убивает, только освобождает память
BGPROCESS_EXPORT void bg_free(bg_process *p);
BGPROCESS_EXPORT const char *bg_strerror(int code);
// errno на unix, GetLastError на виндовсе
BGPROCESS_EXPORT int  bg_last_os_error(void);

#ifdef __cplusplus
}
#endif

#endif
