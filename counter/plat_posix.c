#include "plat.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define SHM_NAME    "/counter_shm"
#define SEM_NAME    "/counter_lock"
#define MASTER_FILE "counter.lock"

static sem_t *g_lock;
static int   *g_shared;
static int    g_master_fd = -1;

int plat_init(void)
{
    // семафор с именем = замок, который видят все процессы
    g_lock = sem_open(SEM_NAME, O_CREAT, 0644, 1);
    if (g_lock == SEM_FAILED) return -1;

    // общая память: кусок памяти с именем, один на все процессы
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0644);
    if (fd < 0) return -1;

    // размер задаём только если память новая (на маке второй раз нельзя)
    struct stat st;
    if (fstat(fd, &st) == 0 && st.st_size < (off_t)sizeof(int)) {
        if (ftruncate(fd, sizeof(int)) != 0) {
            close(fd);
            return -1;
        }
    }

    void *mem = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (mem == MAP_FAILED) return -1;
    g_shared = (int *)mem;
    return 0;
}

// они живут после выхода программы, это чтобы начать с нуля
void plat_reset(void)
{
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_NAME);
}

long plat_pid(void) { return (long)getpid(); }

void plat_now(char *buf, size_t size)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);
    char base[24];
    strftime(base, sizeof(base), "%Y-%m-%d %H:%M:%S", &tm);
    snprintf(buf, size, "%s.%03ld", base, ts.tv_nsec / 1000000);
}

void plat_sleep_ms(int ms)
{
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

int *plat_counter(void) { return g_shared; }

void plat_lock(void)
{
    while (sem_wait(g_lock) != 0 && errno == EINTR) {}
}

void plat_unlock(void) { sem_post(g_lock); }

// flock держится пока процесс жив, умер = ядро само отпустит
int plat_try_master(void)
{
    if (g_master_fd < 0)
        g_master_fd = open(MASTER_FILE, O_CREAT | O_RDWR | O_CLOEXEC, 0644);
    if (g_master_fd < 0) return 0;
    return flock(g_master_fd, LOCK_EX | LOCK_NB) == 0;
}

// ---------- потоки ----------

struct plat_thread {
    pthread_t id;
    void (*fn)(void);
};

static void *thread_entry(void *arg)
{
    plat_thread *t = (plat_thread *)arg;
    t->fn();
    return NULL;
}

plat_thread *plat_thread_start(void (*fn)(void))
{
    plat_thread *t = malloc(sizeof(*t));
    if (!t) return NULL;
    t->fn = fn;
    if (pthread_create(&t->id, NULL, thread_entry, t) != 0) {
        free(t);
        return NULL;
    }
    return t;
}

void plat_thread_join(plat_thread *t)
{
    if (!t) return;
    pthread_join(t->id, NULL);
    free(t);
}
