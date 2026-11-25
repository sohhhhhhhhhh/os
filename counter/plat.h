#ifndef PLAT_H
#define PLAT_H

#include <stddef.h>

// всё что зависит от ос, реализация в plat_posix.c / plat_win.c

int   plat_init(void);       // общая память + замок, 0 если ок
void  plat_reset(void);      // удалить старую общую память и замок (только posix)

long  plat_pid(void);
void  plat_now(char *buf, size_t size);   // "2026-11-25 12:00:01.123"
void  plat_sleep_ms(int ms);

int  *plat_counter(void);    // счётчик в общей памяти
void  plat_lock(void);       // один замок на все процессы
void  plat_unlock(void);
int   plat_try_master(void); // 1 если я главный

typedef struct plat_thread plat_thread;
plat_thread *plat_thread_start(void (*fn)(void));
void         plat_thread_join(plat_thread *t);

#endif
