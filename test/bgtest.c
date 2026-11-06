#include "bgprocess.h"
#include "sleep_ms.h"

#include <stdio.h>

// bgtest <программа> [аргументы...]
int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("использование: %s <программа> [аргументы...]\n", argv[0]);
        return 2;
    }

    bg_process *p;
    int rc = bg_start((const char *const *)&argv[1], &p);
    if (rc != BG_OK) {
        printf("не удалось запустить %s: %s (системный код %d)\n",
               argv[1], bg_strerror(rc), bg_last_os_error());
        return 1;
    }
    printf("запущен %s, PID %ld\n", argv[1], bg_pid(p));

    // фоновый режим: ребёнок работает, а родитель занят своим
    int code = -1;
    for (int i = 1; i <= 3 && bg_try_wait(p, &code) == BG_RUNNING; i++) {
        printf("родитель занят своими делами.......%d\n", i);
        sleep_ms(300);
    }

    printf("теперь жду завершения\n");
    rc = bg_wait(p, &code);
    bg_free(p);

    if (rc != BG_OK) {
        printf("ошибка ожидания: %s\n", bg_strerror(rc));
        return 1;
    }
    printf("программа завершилась с кодом %d\n", code);
    return 0;
}
