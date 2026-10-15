/*
 *  Wakeup profile for the host UI harness (issue #13, UI-07 remainder).
 *
 *  Why it exists: the ROM's GUI thread is src/main.c's own loop
 *
 *      loop_start_time = get_time();
 *      observer_delayed_notify_all(); event_obj_check(); scheduler_work();
 *      next_loop_time = lv_timer_handler() + loop_start_time;
 *      sleep_time = next_loop_time - get_time();
 *      if (sleep_time > 0) usleep(sleep_time * 1000);
 *
 *  and the other wakeup source is its tick thread (src/main.c):
 *
 *      while (1) { usleep(5 * 1000); lv_tick_inc(5); }
 *
 *  Which of the two costs more wakeups is a number, not an opinion: the tick
 *  thread wakes every TICK_MS no matter what the loop does, while the loop only
 *  wakes when LVGL tells it a timer is due. This mode reproduces the loop over
 *  the virtual clock on the *firmware's own timer set* (the real components are
 *  the ones placing and arming the timers) and reports both counts for the same
 *  window. No wall clock is read, so two runs write the same bytes.
 *
 *  What it does NOT measure: the *cost* of a wakeup. This is an x86 host: the
 *  Cortex-A7's time in a wakeup, its idle-state behaviour and the battery are
 *  not observable here (the same limitation `benchmark`/`metrics` declare). The
 *  report is a count of wakeups, which is what the tick's 5 ms can be judged by.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "wakeup_bench.h"

#include <stdio.h>

#include "lvgl/lvgl.h"

#include "natural_bench.h"
#include "render_metrics.h"
#include "virtual_time.h"

#include "events.h"
#include "scheduler.h"

#include "cfg/subjects.h"

/* El periodo de refresco que la ROM configura (lv_conf.h:
 * LV_DISP_DEF_REFR_PERIOD). El arnes es un binario de host con su propio
 * lv_conf.h, que no define ese valor, asi que LVGL le queda en su default
 * (30 ms) y el lazo de la ROM dormiria contra el periodo equivocado. El modo
 * wakeups le pone al refr_timer el periodo de la ROM antes de medir:
 * tests/check_harness_wakeups.sh lee el numero de lv_conf.h y comprueba que sea
 * este mismo, asi que un cambio de config no puede mover la medicion en
 * silencio. */
#define WAKEUP_REFR_PERIOD_MS 50

/* La cadencia del hilo del tick (src/main.c, tick_thread()). La misma vara: el
 * gate la lee de src/main.c. Con LV_TICK_CUSTOM (lv_conf.h de la ROM) el hilo no
 * existe: el modo lo declara HARNESS_ROM_TICK_CUSTOM, que el CMake deriva del
 * lv_conf.h de la ROM, y el perfil reporta 0 wakeups de tick. */
#define WAKEUP_TICK_THREAD_MS 5

/* El CMake del arnes define HARNESS_ROM_TICK_CUSTOM a partir del lv_conf.h de la
 * ROM; sin esa definicion (build suelto) el modo por defecto es el hilo. */
#ifndef HARNESS_ROM_TICK_CUSTOM
#define HARNESS_ROM_TICK_CUSTOM 0
#endif

/* Paso del reloj virtual durante el sueno del lazo (el mismo del banco natural:
 * la resolucion a la que el banco inyecta datos y tick). */
#define WAKEUP_SLICE_MS NATURAL_BENCH_SLICE_MS

/* Ventana por defecto y techo del modo. */
#define WAKEUP_WINDOW_DEFAULT_MS 1000
#define WAKEUP_WINDOW_MAX_MS 60000

/* Un pase del lazo de la ROM sobre la ventana. */
typedef struct {
    uint32_t wakeups;            /* iteraciones del lazo */
    uint32_t with_flush;         /* de esas, cuantas dibujaron algo */
    uint32_t immediate;          /* lv_timer_handler() dijo 0 ms: el lazo no durmio */
    uint32_t no_timer;           /* lv_timer_handler() dijo "ningun timer listo" */
    uint32_t sleep_min_ms;
    uint32_t sleep_max_ms;
    uint64_t sleep_total_ms;     /* la suma de los suenos (<= window_ms) */
    uint64_t flushed_px;         /* px que el driver flusheo durante el pase */
} wakeup_pass_t;

static bool         enabled;
static uint32_t     window_ms;
static wakeup_pass_t with_drive;
static wakeup_pass_t static_pass;

static void pass_reset(wakeup_pass_t *pass)
{
    pass->wakeups = 0;
    pass->with_flush = 0;
    pass->immediate = 0;
    pass->no_timer = 0;
    pass->sleep_min_ms = UINT32_MAX;
    pass->sleep_max_ms = 0;
    pass->sleep_total_ms = 0;
    pass->flushed_px = 0;
}

/* Una iteracion del lazo (src/main.c), en su orden, y despues el sueno hasta el
 * proximo timer que LVGL diga. El sueno avanza el reloj sin correr el handler
 * (virtual_time_advance_idle): durante el sueno solo corre el hilo del tick y
 * el DSP, que es lo que el firmware realmente hace. */
static void loop_iteration(wakeup_pass_t *pass)
{
    uint64_t loop_start = virtual_time_now_ms();
    uint32_t flushes_before = render_metrics_flush_count();
    uint64_t px_before = render_metrics_flushed_pixels();

    observer_delayed_notify_all();
    event_obj_check();
    scheduler_work();

    uint32_t next = lv_timer_handler();

    pass->wakeups++;
    if (render_metrics_flush_count() != flushes_before) {
        pass->with_flush++;
    }
    pass->flushed_px += render_metrics_flushed_pixels() - px_before;

    uint64_t now = virtual_time_now_ms();
    uint64_t sleep_ms = 0;

    if (next == 0) {
        /* El lazo no duerme: hay algo listo ya. La ROM entraria de inmediato. */
        pass->immediate++;
        sleep_ms = 1;
    } else if (next == LV_NO_TIMER_READY) {
        /* Ningun timer armado. En la radio next_loop_time queda a 49 dias y el
         * usleep() del lazo falla (fuera de rango) devolviendo de inmediato, asi
         * que el lazo gira: se cuenta la vuelta y se avanza 1 ms para que el
         * pase termine (un giro real no consume reloj virtual). */
        pass->no_timer++;
        sleep_ms = 1;
    } else if (loop_start + next > now) {
        sleep_ms = loop_start + next - now;
    } else {
        sleep_ms = 1;
    }

    if (sleep_ms < pass->sleep_min_ms) {
        pass->sleep_min_ms = sleep_ms;
    }
    if (sleep_ms > pass->sleep_max_ms) {
        pass->sleep_max_ms = sleep_ms;
    }
    pass->sleep_total_ms += sleep_ms;

    virtual_time_advance_idle(sleep_ms, WAKEUP_SLICE_MS, natural_bench_drive_slice, NULL);
}

static void run_pass(wakeup_pass_t *pass, const scenario_t *scenario, bool drive)
{
    uint64_t end;

    pass_reset(pass);
    natural_bench_drive_reset(scenario->seed);
    natural_bench_set_drive(drive);
    end = virtual_time_now_ms() + window_ms;

    while (virtual_time_now_ms() < end) {
        loop_iteration(pass);
    }

    if (pass->wakeups == 0) {
        pass->sleep_min_ms = 0;
    }
}

void wakeup_bench_enable(bool on)
{
    enabled = on;
}

bool wakeup_bench_enabled(void)
{
    return enabled;
}

void wakeup_bench_run(const scenario_t *scenario, uint32_t ms)
{
    if (ms == 0) {
        ms = WAKEUP_WINDOW_DEFAULT_MS;
    }
    if (ms > WAKEUP_WINDOW_MAX_MS) {
        ms = WAKEUP_WINDOW_MAX_MS;
    }

    window_ms = ms;
    enabled = true;

    /* La ROM corre con su periodo de refresco configurado. */
    lv_disp_t *disp = lv_disp_get_default();

    if (disp != NULL && disp->refr_timer != NULL) {
        lv_timer_set_period(disp->refr_timer, WAKEUP_REFR_PERIOD_MS);
    }

    /* Pase 1: el estado de la radio, con el DSP alimentando espectro y cascada.
     * Pase 2 (la delimitacion): sin drive solo quedan los timers del firmware,
     * asi que la diferencia entre los dos es lo que el camino de datos agrega. */
    run_pass(&with_drive, scenario, true);
    run_pass(&static_pass, scenario, false);
}

uint32_t wakeup_bench_refresh_period_ms(void)
{
    return WAKEUP_REFR_PERIOD_MS;
}

/* El hilo que declara el tick de LVGL (src/main.c): con LV_TICK_CUSTOM en la ROM
 * el hilo no existe, asi que no hay cadencia que reportar (0) ni wakeups que
 * contar. */
uint32_t wakeup_bench_tick_thread_ms(void)
{
#if HARNESS_ROM_TICK_CUSTOM
    return 0;
#else
    return WAKEUP_TICK_THREAD_MS;
#endif
}

bool wakeup_bench_tick_custom(void)
{
#if HARNESS_ROM_TICK_CUSTOM
    return true;
#else
    return false;
#endif
}

/* Wakeups del hilo del tick en la ventana: 0 con LV_TICK_CUSTOM (no hay hilo),
 * window_ms / tick_ms con el hilo. */
static uint32_t tick_thread_wakeups(void)
{
    uint32_t tick_ms = wakeup_bench_tick_thread_ms();

    if (tick_ms == 0) {
        return 0;
    }

    return window_ms / tick_ms;
}

static void write_pass(FILE *f, const char *name, const wakeup_pass_t *pass, bool more)
{
    fprintf(f, "    \"%s\": {\n", name);
    fprintf(f, "      \"wakeups\": %u,\n", pass->wakeups);
    fprintf(f, "      \"with_flush\": %u,\n", pass->with_flush);
    fprintf(f, "      \"immediate\": %u,\n", pass->immediate);
    fprintf(f, "      \"no_timer\": %u,\n", pass->no_timer);
    fprintf(f, "      \"sleep_ms_min\": %u,\n", pass->sleep_min_ms);
    fprintf(f, "      \"sleep_ms_max\": %u,\n", pass->sleep_max_ms);
    fprintf(f, "      \"sleep_ms_total\": %llu,\n", (unsigned long long) pass->sleep_total_ms);
    fprintf(f, "      \"flushed_px\": %llu\n", (unsigned long long) pass->flushed_px);
    fprintf(f, "    }%s\n", more ? "," : "");
}

void wakeup_bench_write_json(FILE *f)
{
    fprintf(f, "  \"wakeups\": {\n");
    fprintf(f, "    \"enabled\": true,\n");
    fprintf(f, "    \"clock\": \"virtual-time\",\n");
    fprintf(f, "    \"environment\": \"x86 host: the wakeup cost (and the Cortex-A7 energy) "
                "is not measured\",\n");
    fprintf(f, "    \"window_ms\": %u,\n", window_ms);
    fprintf(f, "    \"refresh_period_ms\": %u,\n", wakeup_bench_refresh_period_ms());
    fprintf(f, "    \"slice_ms\": %d,\n", WAKEUP_SLICE_MS);
    fprintf(f, "    \"tick_custom\": %s,\n", wakeup_bench_tick_custom() ? "true" : "false");
    fprintf(f, "    \"tick_thread_ms\": %u,\n", wakeup_bench_tick_thread_ms());
    fprintf(f, "    \"tick_thread_wakeups\": %u,\n", tick_thread_wakeups());
    fprintf(f, "    \"spectrum_fps_ms\": %d,\n", NATURAL_SPECTRUM_FPS_MS);
    fprintf(f, "    \"waterfall_fps_ms\": %d,\n", NATURAL_WATERFALL_FPS_MS);
    write_pass(f, "with_drive", &with_drive, true);
    write_pass(f, "static", &static_pass, false);
    fprintf(f, "  }\n");
}

uint32_t wakeup_bench_window_ms(void)
{
    return window_ms;
}
