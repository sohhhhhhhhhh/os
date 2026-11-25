#include "plat.h"
#include "bgprocess.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_PATH "log.txt"

static int        *g_counter;  // в общей памяти
static atomic_int  g_stop;     // 1 = потокам пора заканчивать
static const char *g_self;     // argv[0], чтобы запускать свои копии

// [время] PID=.. сообщение
// вызывать только под plat_lock, тогда строки не перемешиваются
static void log_msg(const char *msg)
{
    char t[32];
    plat_now(t, sizeof(t));
    FILE *f = fopen(LOG_PATH, "a");
    if (!f) return;
    fprintf(f, "[%s] PID=%ld %s\n", t, plat_pid(), msg);
    fclose(f);
}

static void log_counter(const char *text)
{
    char msg[64];
    snprintf(msg, sizeof(msg), "%s%sCounter: %d", text, text[0] ? " " : "", *g_counter);
    log_msg(msg);
}

// ---------- копии ----------

static int run_copy1(void)
{
    plat_lock();
    log_msg("[COPY1] start");
    *g_counter += 10;
    log_counter("[COPY1] +10.");
    log_msg("[COPY1] exit");
    plat_unlock();
    return 0;
}

static int run_copy2(void)
{
    plat_lock();
    log_msg("[COPY2] start");
    *g_counter *= 2;
    log_counter("[COPY2] *2.");
    plat_unlock();

    plat_sleep_ms(2000);

    plat_lock();
    *g_counter /= 2;
    log_counter("[COPY2] /2.");
    log_msg("[COPY2] exit");
    plat_unlock();
    return 0;
}

// запускает копию, если прошлая уже закончилась (пункт 5c)
static void spawn_copy(bg_process **slot, const char *arg, const char *name)
{
    char msg[64];

    if (*slot) {
        if (bg_try_wait(*slot, NULL) == BG_RUNNING) {
            snprintf(msg, sizeof(msg), "%s still running, skip", name);
            plat_lock();
            log_msg(msg);
            plat_unlock();
            return;
        }
        bg_free(*slot);
        *slot = NULL;
    }

    const char *argv[] = { g_self, arg, NULL };
    if (bg_start(argv, slot) != BG_OK) {
        snprintf(msg, sizeof(msg), "%s start failed", name);
        plat_lock();
        log_msg(msg);
        plat_unlock();
    }
}

// ---------- потоки ----------

// у всех: раз в 300 мс +1
static void timer_thread(void)
{
    while (!atomic_load(&g_stop)) {
        plat_sleep_ms(300);
        plat_lock();
        (*g_counter)++;
        plat_unlock();
    }
}

// раз в 1 с лог, раз в 3 с копии, но только у главного
// если главный закрылся, его место займёт кто-то из остальных
static void logger_thread(void)
{
    bg_process *copy1 = NULL, *copy2 = NULL;
    int master = plat_try_master();
    int sec = 0;
    if (master) printf("я главный\n");

    while (!atomic_load(&g_stop)) {
        plat_sleep_ms(1000);

        if (!master) {
            master = plat_try_master();
            if (!master) continue;
            printf("теперь я главный\n");
        }

        plat_lock();
        log_counter("");
        plat_unlock();

        if (++sec % 3 == 0) {
            spawn_copy(&copy1, "--copy1", "COPY1");
            spawn_copy(&copy2, "--copy2", "COPY2");
        }
    }

    // дождаться копий, чтобы не оставлять зомби
    if (copy1) { bg_wait(copy1, NULL); bg_free(copy1); }
    if (copy2) { bg_wait(copy2, NULL); bg_free(copy2); }
}

// ---------- main ----------

int main(int argc, char **argv)
{
    g_self = argv[0];

    if (argc > 1 && strcmp(argv[1], "--reset") == 0) {
        plat_reset();
        printf("общая память и замок удалены\n");
        return 0;
    }

    if (plat_init() != 0) {
        printf("не получилось открыть общую память\n");
        return 1;
    }
    g_counter = plat_counter();

    if (argc > 1 && strcmp(argv[1], "--copy1") == 0) return run_copy1();
    if (argc > 1 && strcmp(argv[1], "--copy2") == 0) return run_copy2();

    plat_lock();
    log_msg("Process started");
    plat_unlock();
    printf("PID %ld\n", plat_pid());

    plat_thread *timer  = plat_thread_start(timer_thread);
    plat_thread *logger = plat_thread_start(logger_thread);
    if (!timer || !logger) {
        printf("не получилось запустить потоки\n");
        return 1;
    }

    printf("введи число чтобы поменять счётчик, q чтобы выйти\n");
    char line[64];
    while (fgets(line, sizeof(line), stdin)) {
        if (line[0] == 'q') break;
        if (line[0] == '\n') continue;

        char *end;
        long v = strtol(line, &end, 10);
        if (end == line) {
            printf("это не число\n");
            continue;
        }

        plat_lock();
        *g_counter = (int)v;
        log_counter("Changed by user.");
        plat_unlock();
        printf("ок\n");
    }

    atomic_store(&g_stop, 1);
    plat_thread_join(timer);
    plat_thread_join(logger);

    plat_lock();
    log_msg("Process exit");
    plat_unlock();
    return 0;
}
