/* Driver de la traza "despues" con el motor nuevo (issue #186). Mismos eventos
 * con timestamps reales en microsegundos; sin align_int(), todo int64. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "adaptive_tuning.h"

static int32_t freq;
static int32_t base_step;
static adaptive_accel_t profile;
static uint8_t threshold;

static void run(const int32_t *diffs, const uint64_t *ts_us, size_t n, const char *name) {
    adaptive_tuning_state_t st;
    int32_t    seg_diffs[64];
    uint64_t   seg_ts[64];
    rotary_event_enhanced_t segs[64];
    uint8_t    mults[64];
    int64_t    weighted = 0;

    adaptive_tuning_init(&st);
    memcpy(seg_diffs, diffs, n * sizeof(int32_t));
    memcpy(seg_ts, ts_us, n * sizeof(uint64_t));

    size_t nseg = adaptive_tuning_segments_ex(&st, seg_diffs, seg_ts, n, profile, threshold,
                                              segs, 64, mults, 64);

    printf("%s step=%d %s start=%d\n", name, base_step,
           profile == ADAPTIVE_ACCEL_LITE ? "Lite" : (profile == ADAPTIVE_ACCEL_STRONG ? "Strong" : "None"),
           freq);

    size_t k = 0;

    for (size_t s = 0; s < nseg; s++) {
        weighted += segs[s].weighted_detents;
        printf("  tramo raw=%+d weighted=%+lld mult_final=%u\n",
               segs[s].raw_diff, (long long) segs[s].weighted_detents, segs[s].last_multiplier);
        (void) k;
    }

    int32_t target;

    if (adaptive_tuning_target_freq(freq, weighted, base_step, &target)) {
        freq = target;
    }

    printf("  weighted_total=%+lld  final=%d\n", (long long) weighted, freq);
}

static void reset(int start, int step, adaptive_accel_t prof, uint8_t t) {
    freq = start;
    base_step = step;
    profile = prof;
    threshold = t;
}

int main(void) {
    reset(14200000, 10, ADAPTIVE_ACCEL_LITE, 7);
    { int32_t d[] = {1}; uint64_t t[] = {1000000}; run(d, t, 1, "lento"); }

    reset(14200000, 100, ADAPTIVE_ACCEL_STRONG, 7);
    { int32_t d[10]; uint64_t t[10];
      for (int i = 0; i < 10; i++) { d[i] = 1; t[i] = (uint64_t)(i + 1) * 14000; }
      run(d, t, 10, "rapido 10x14ms"); }

    reset(14200000, 100, ADAPTIVE_ACCEL_STRONG, 7);
    { int32_t d[9]; uint64_t t[9];
      for (int i = 0; i < 8; i++) { d[i] = 1; t[i] = (uint64_t)(i + 1) * 14000; }
      d[8] = 1; t[8] = t[7] + 180000;
      run(d, t, 9, "freno 8x14ms+1x180ms"); }

    reset(14200000, 100, ADAPTIVE_ACCEL_STRONG, 7);
    { int32_t d[9]; uint64_t t[9];
      for (int i = 0; i < 8; i++) { d[i] = 1; t[i] = (uint64_t)(i + 1) * 14000; }
      d[8] = -1; t[8] = t[7] + 14000;
      run(d, t, 9, "reversa 8x14ms+1x14ms(-)"); }

    reset(14200000, 100, ADAPTIVE_ACCEL_STRONG, 7);
    { int32_t d[9]; uint64_t t[9];
      for (int i = 0; i < 5; i++) { d[i] = 1; t[i] = (uint64_t)(i + 1) * 14000; }
      d[5] = 1; t[5] = t[4] + 134000;
      for (int i = 6; i < 9; i++) { d[i] = 1; t[i] = t[i - 1] + 14000; }
      run(d, t, 9, "gui-lag 5x14ms+134ms+3x14ms"); }

    return 0;
}
