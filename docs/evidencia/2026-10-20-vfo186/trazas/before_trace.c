/* Driver de la traza "antes" con el motor RETIRADO de la base 60b9f5b2.
 * Emula el consumidor base: productor que mide ms por lectura, consumer que
 * desempaca el lote con timestamps sinteticos `now - dt*(ticks-1-k)` y aplica
 * align_int(). Uso solo para la tabla antes/despues de la evidencia. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "adaptive_tuning.h"

static uint32_t g_tick;
uint32_t lv_tick_get(void) { return g_tick; }

static int32_t align_int(int32_t x, uint16_t step) {
    return step == 0 ? x : x - (x % step);
}

/* Una lectura del fd: diff total y ms medidos por el productor. */
typedef struct { int diff; int dt_ms; } read_t;

static int32_t freq;
static int32_t base_step;
static adaptive_accel_t profile;

static void frame(const read_t *reads, int nreads, int now_ms) {
    int diff = 0, n = 0, ms = 0;

    for (int i = 0; i < nreads; i++) {
        diff += reads[i].diff;
        n += abs(reads[i].diff);
        ms += reads[i].dt_ms * abs(reads[i].diff);
    }

    if (!diff) return;

    int dt = ms / n;
    if (dt < 1) dt = 1;

    g_tick = (uint32_t) now_ms;

    int16_t step = diff > 0 ? 1 : -1;
    int16_t ticks = (int16_t) abs(diff);

    for (int16_t k = 0; k < ticks; k++) {
        adaptive_tuning_add_event(&g_adaptive_tuning, step,
                                  (uint32_t) now_ms - (uint32_t) dt * (uint32_t) (ticks - 1 - k));
    }

    adaptive_tuning_update(&g_adaptive_tuning);

    float mult = adaptive_multiplier_gate(g_adaptive_tuning.current_multiplier, profile);
    int32_t adaptive_step = (int32_t) (base_step * mult);
    if (adaptive_step < 1) adaptive_step = 1;

    int32_t df = diff * adaptive_step;
    uint16_t grid = adaptive_step <= 65535 ? (uint16_t) adaptive_step : (uint16_t) base_step;
    freq = align_int(freq + df, grid);

    printf("  frame now=%d diff=%+d dt=%d ms  mult=%.2f  step=%d  df=%+d  freq=%d\n",
           now_ms, diff, dt, mult, adaptive_step, df, freq);
}

static void reset(int start_freq, int step, adaptive_accel_t prof) {
    freq = start_freq;
    base_step = step;
    profile = prof;
    memset(&g_adaptive_tuning, 0, sizeof(g_adaptive_tuning));
    adaptive_tuning_init(&g_adaptive_tuning);
}

int main(void) {
    /* 1. lento: un detent a 400 ms, step 10, Lite (antes: x0,1 -> 1 Hz). */
    printf("lento step=10 Lite start=14200000\n");
    reset(14200000, 10, ADAPTIVE_ACCEL_LITE);
    { read_t r[] = {{1, 400}}; frame(r, 1, 1000); }
    printf("  final=%d\n", freq);

    /* 2. rapido: 10 detents a 14 ms, step 100, Strong (antes: tope 100x). */
    printf("rapido 10x14ms step=100 Strong start=14200000\n");
    reset(14200000, 100, ADAPTIVE_ACCEL_STRONG);
    { int t = 0; for (int i = 0; i < 10; i++) { read_t r[] = {{1, 14}}; t += 14; frame(r, 1, t); } }
    printf("  final=%d\n", freq);

    /* 3. freno: 8 detents a 14 ms + uno a 180 ms, step 100, Strong. */
    printf("freno 8x14ms + 1x180ms step=100 Strong start=14200000\n");
    reset(14200000, 100, ADAPTIVE_ACCEL_STRONG);
    { int t = 0; for (int i = 0; i < 8; i++) { read_t r[] = {{1, 14}}; t += 14; frame(r, 1, t); }
      t += 180; { read_t r[] = {{1, 180}}; frame(r, 1, t); } }
    printf("  final=%d\n", freq);

    /* 4. reversa: 8 detents +14 ms y uno -14 ms, step 100, Strong. */
    printf("reversa 8x14ms(+) + 1x14ms(-) step=100 Strong start=14200000\n");
    reset(14200000, 100, ADAPTIVE_ACCEL_STRONG);
    { int t = 0; for (int i = 0; i < 8; i++) { read_t r[] = {{1, 14}}; t += 14; frame(r, 1, t); }
      t += 14; { read_t r[] = {{-1, 14}}; frame(r, 1, t); } }
    printf("  final=%d\n", freq);

    /* 5. GUI con lag: misma traza rapida que 2, pero el productor mide el
     * stall (120 ms) en la lectura siguiente y el GUI drena todo junto. */
    printf("gui-lag 5x14ms + stall 120ms + 4x14ms, drenado junto, step=100 Strong\n");
    reset(14200000, 100, ADAPTIVE_ACCEL_STRONG);
    { read_t reads[9]; int t = 0, j = 0;
      for (int i = 0; i < 5; i++) { reads[j++] = (read_t){1, 14}; t += 14; }
      t += 120; reads[j++] = (read_t){1, 120};            /* el stall entra en la lectura siguiente */
      for (int i = 0; i < 3; i++) { reads[j++] = (read_t){1, 14}; t += 14; }
      frame(reads, j, t); }
    printf("  final=%d\n", freq);

    return 0;
}
