#include "plat.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define SHM_NAME    "counter_shm"
#define LOCK_NAME   "counter_lock"
#define MASTER_NAME "counter_master"

static HANDLE g_lock;    // мьютекс с именем = замок на все процессы
static HANDLE g_master;  // мьютекс главного
static int   *g_shared;

int plat_init(void)
{
    SetConsoleOutputCP(CP_UTF8); // чтобы русский не ломался

    g_lock = CreateMutexA(NULL, FALSE, LOCK_NAME);
    if (!g_lock) return -1;

    //общая память: если такая уже есть,то она прст откроется
    HANDLE map = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                    0, sizeof(int), SHM_NAME);
    if (!map) return -1;
    g_shared = (int *)MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(int));
    return g_shared ? 0 : -1;
}

// на виндоус всё само удаляется, когда закрылся последний процесс
void plat_reset(void) {}

long plat_pid(void) { return (long)GetCurrentProcessId(); }

void plat_now(char *buf, size_t size)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    snprintf(buf, size, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
             t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond,
             t.wMilliseconds);
}

void plat_sleep_ms(int ms) { Sleep((DWORD)ms); }

int *plat_counter(void) { return g_shared; }

// WAIT_ABANDONED тоже ок: тот кто держал замок умер, теперь он наш
void plat_lock(void)   { WaitForSingleObject(g_lock, INFINITE); }
void plat_unlock(void) { ReleaseMutex(g_lock); }

int plat_try_master(void)
{
    if (!g_master) g_master = CreateMutexA(NULL, FALSE, MASTER_NAME);
    if (!g_master) return 0;
    DWORD r = WaitForSingleObject(g_master, 0);
    return r == WAIT_OBJECT_0 || r == WAIT_ABANDONED;
}

//потоки

struct plat_thread {
    HANDLE h;
    void (*fn)(void);
};

static DWORD WINAPI thread_entry(LPVOID arg)
{
    plat_thread *t = (plat_thread *)arg;
    t->fn();
    return 0;
}

plat_thread *plat_thread_start(void (*fn)(void))
{
    plat_thread *t = malloc(sizeof(*t));
    if (!t) return NULL;
    t->fn = fn;
    t->h = CreateThread(NULL, 0, thread_entry, t, 0, NULL);
    if (!t->h) {
        free(t);
        return NULL;
    }
    return t;
}

void plat_thread_join(plat_thread *t)
{
    if (!t) return;
    WaitForSingleObject(t->h, INFINITE);
    CloseHandle(t->h);
    free(t);
}
