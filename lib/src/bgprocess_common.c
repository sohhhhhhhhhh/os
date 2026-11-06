#include "bgprocess_internal.h"

static int g_last_os_error = 0;

void bg_set_os_error(int err) { g_last_os_error = err; }

int bg_last_os_error(void) { return g_last_os_error; }

const char *bg_strerror(int code)
{
    switch (code) {
    case BG_OK:        return "ok";
    case BG_RUNNING:   return "still running";
    case BG_ERR_ARGS:  return "invalid arguments";
    case BG_ERR_START: return "failed to start process";
    case BG_ERR_WAIT:  return "failed to wait for process";
    case BG_ERR_NOMEM: return "out of memory";
    default:           return "unknown error";
    }
}
