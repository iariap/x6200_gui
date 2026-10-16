/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Fine-tuning assistant (autotune) implementation
 *
 *  Copyright (c) 2024 LU2DSX
 */

#include "autotune.h"

#include <stddef.h>

int16_t autotune_score(const int16_t *samples, int n) {
    int16_t buf[AUTOTUNE_MAX_SAMPLES];
    int     i, j;

    if (samples == NULL || n <= 0) {
        return INT16_MIN;
    }
    if (n > AUTOTUNE_MAX_SAMPLES) {
        n = AUTOTUNE_MAX_SAMPLES;
    }

    /* Copy into the local buffer first (n is tiny and fixed), then insertion
     * sort it and take the median: one bad sample (a spike) does not move the
     * middle. The copy is what lets the sort read buf[i - 1] knowing every slot
     * below the cursor was already written. */
    for (i = 0; i < n; i++) {
        buf[i] = samples[i];
    }
    for (i = 1; i < n; i++) {
        int16_t v = buf[i];

        for (j = i; j > 0 && buf[j - 1] > v; j--) {
            buf[j] = buf[j - 1];
        }
        buf[j] = v;
    }

    if (n % 2 == 1) {
        return buf[n / 2];
    }
    return (int16_t) (((int32_t) buf[n / 2 - 1] + (int32_t) buf[n / 2]) / 2);
}

int autotune_plan(autotune_t *at, int32_t start_freq, int32_t step_hz, int32_t window_hz) {
    if (at == NULL) {
        return 0;
    }

    at->start_freq = start_freq;
    at->step_hz = (step_hz > 0) ? step_hz : AUTOTUNE_STEP_HZ;
    at->window_hz = (window_hz >= 0) ? window_hz : AUTOTUNE_WINDOW_HZ;
    at->index = 0;
    at->collected = 0;
    at->completed = 0;
    at->count = 0;

    /* The start is probe 0: it is measured too, so the decision compares
     * against the real signal at the operator's frequency, not against an
     * assumption. */
    at->probes[at->count++] = start_freq;

    /* Zigzag outwards: +step, -step, +2*step, -2*step, ... while inside the
     * window. The pairs are added one probe at a time so the buffer fills
     * exactly up to AUTOTUNE_MAX_PROBES, never past it. */
    for (int64_t k = 1; at->count < AUTOTUNE_MAX_PROBES; k++) {
        int64_t off = k * (int64_t) at->step_hz;

        if (off > (int64_t) at->window_hz) {
            break;
        }
        at->probes[at->count++] = start_freq + (int32_t) off;
        if (at->count < AUTOTUNE_MAX_PROBES) {
            at->probes[at->count++] = start_freq - (int32_t) off;
        }
    }

    return at->count;
}

int32_t autotune_probe_freq(const autotune_t *at) {
    if (at == NULL || at->count <= 0) {
        return 0;
    }
    if (at->index >= at->count) {
        return at->probes[at->count - 1];
    }
    return at->probes[at->index];
}

bool autotune_feed(autotune_t *at, int16_t rssi) {
    if (at == NULL || !autotune_running(at)) {
        return false;
    }

    if (at->collected < AUTOTUNE_SAMPLES) {
        at->samples[at->collected++] = rssi;
    }

    if (at->collected >= AUTOTUNE_SAMPLES) {
        at->scores[at->index] = autotune_score(at->samples, at->collected);
        at->completed = at->index + 1;
        at->collected = 0;
        return true;
    }
    return false;
}

bool autotune_next(autotune_t *at) {
    if (at == NULL) {
        return false;
    }
    if (at->index < at->count) {
        at->index++;
    }
    return at->index < at->count;
}

bool autotune_running(const autotune_t *at) {
    return at != NULL && at->index < at->count;
}

int32_t autotune_result(const autotune_t *at, int16_t margin_db) {
    int best = 0;

    if (at == NULL) {
        return 0;
    }
    if (at->completed <= 0) {
        return at->start_freq;
    }

    for (int i = 1; i < at->completed && i < at->count; i++) {
        if (at->scores[i] > at->scores[best]) {
            best = i;
        }
    }

    /* The start is probe 0. Only move when a measured probe beats it by the
     * margin; otherwise the operator's dial is restored untouched. */
    if (best == 0) {
        return at->start_freq;
    }
    if ((int32_t) at->scores[best] > (int32_t) at->scores[0] + (int32_t) margin_db) {
        return at->probes[best];
    }
    return at->start_freq;
}
