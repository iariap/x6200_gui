/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Fine-tuning assistant UI implementation
 *
 *  Copyright (c) 2024 LU2DSX
 */

#include "autotune_ui.h"

#include "autotune.h"
#include "params/params.h"
#include "meter.h"
#include "msg.h"
#include "util.h"

#include "lvgl/lvgl.h"

/* One tick every 50 ms: the S-meter is refreshed at the waterfall cadence
 * (src/dsp.cpp), so a sample per tick is a fresh reading and the dial
 * visibly steps probe by probe. */
#define AUTOTUNE_UI_TICK_MS      50
/* Ticks to let the AGC and the S-meter settle after retuning, before sampling:
 * the first readings after a retune are stale and would be rejected anyway,
 * but waiting keeps the score honest instead of median-filtering them out. */
#define AUTOTUNE_UI_SETTLE_TICKS 2

static autotune_t  at;
static lv_timer_t *timer = NULL;
static bool        active = false;
static int         settle_ticks = 0;

static void dial_set(int32_t freq) {
    subject_set_int(cfg_cur.fg_freq, freq);
}

static void say_freq(const char *prefix, int32_t freq) {
    uint16_t mhz, khz, hz;

    split_freq(freq, &mhz, &khz, &hz);
    msg_update_text_fmt("%s %i.%03i.%03i", prefix, mhz, khz, hz);
}

/* The level of the best probe over the start, in dB (0 when nothing measured
 * or the start is the best). */
static int best_gain_db(const autotune_t *a) {
    int best = 0;

    if (a->completed <= 0) {
        return 0;
    }
    for (int i = 1; i < a->completed && i < a->count; i++) {
        if (a->scores[i] > a->scores[best]) {
            best = i;
        }
    }
    return (int) a->scores[best] - (int) a->scores[0];
}

static void stop(void) {
    if (timer != NULL) {
        lv_timer_del(timer);
        timer = NULL;
    }
    active = false;
    settle_ticks = 0;
}

static void finish(void) {
    int32_t best = autotune_result(&at, AUTOTUNE_MIN_IMPROVE_DB);

    dial_set(best);

    if (best == at.start_freq) {
        say_freq("Auto tune: no better spot, restored", best);
    } else {
        uint16_t mhz, khz, hz;

        split_freq(best, &mhz, &khz, &hz);
        msg_update_text_fmt("Auto tune: best %i.%03i.%03i (+%i dB)",
                            mhz, khz, hz, best_gain_db(&at));
    }
    stop();
}

static void tick_cb(lv_timer_t *t) {
    (void) t;

    if (!active) {
        return;
    }
    if (settle_ticks > 0) {
        settle_ticks--;
        return;
    }

    if (autotune_feed(&at, meter_get_raw_db())) {
        if (autotune_next(&at)) {
            dial_set(autotune_probe_freq(&at));
            settle_ticks = AUTOTUNE_UI_SETTLE_TICKS;
        } else {
            finish();
        }
    }
}

void autotune_ui_start(void) {
    if (active) {
        return;
    }

    autotune_plan(&at, subject_get_int(cfg_cur.fg_freq),
                  AUTOTUNE_STEP_HZ, AUTOTUNE_WINDOW_HZ);

    active = true;
    /* Probe 0 is the start frequency: anchor the dial there and measure it. */
    dial_set(autotune_probe_freq(&at));
    settle_ticks = AUTOTUNE_UI_SETTLE_TICKS;

    timer = lv_timer_create(tick_cb, AUTOTUNE_UI_TICK_MS, NULL);

    msg_update_text_fmt("Auto tune: scanning +/-%i Hz", AUTOTUNE_WINDOW_HZ);
}

void autotune_ui_cancel(void) {
    if (!active) {
        return;
    }

    dial_set(at.start_freq);
    say_freq("Auto tune: cancelled, restored", at.start_freq);
    stop();
}

void autotune_ui_toggle(void) {
    if (active) {
        autotune_ui_cancel();
    } else {
        autotune_ui_start();
    }
}

bool autotune_ui_running(void) {
    return active;
}
