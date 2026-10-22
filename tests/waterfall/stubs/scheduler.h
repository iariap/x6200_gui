/* Stub de scheduler.h para el test determinista del waterfall (OPT-2, #148):
 * una cola sincrona que el test drena con scheduler_work(). */
#pragma once

#include <stdbool.h>

typedef void (*scheduler_fn_t)(void *);

bool scheduler_put_noargs(scheduler_fn_t fn);
void scheduler_work(void);
