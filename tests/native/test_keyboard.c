/*
 * Unit tests: keyboard rollover (kbd_rollover.c).
 *
 * The USB and Bluetooth keyboards share one LVGL input device, and a new key
 * can go down before the previous one is released. The rollover helper turns
 * that into the press/release pairs LVGL expects. It is pure logic over a
 * source callback, so it can be driven from a fake source on the host.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "kbd_rollover.h"
#include "test_framework.h"

/* A fake /dev/input stream: a list of (code, key, value) events */
typedef struct {
    uint16_t code;
    uint32_t key;
    int      value;
} ev_t;

typedef struct {
    const ev_t *evs;
    int         n;
    int         i;
} fake_t;

static bool fake_next(void *ctx, uint16_t *scancode, uint32_t *key, int *value) {
    fake_t *f = ctx;

    if (f->i >= f->n) {
        return false;
    }

    *scancode = f->evs[f->i].code;
    *key      = f->evs[f->i].key;
    *value    = f->evs[f->i].value;
    f->i++;

    return true;
}

/* One press and release of a single key */
static void test_single_key(void) {
    static const ev_t evs[] = {
        { 16, 'q', 1 },     /* press Q */
        { 16, 'q', 0 },     /* release Q */
    };
    fake_t        f  = { evs, 2, 0 };
    kbd_rollover_t k = {0};
    lv_indev_data_t d = {0};

    kbd_rollover_read(&k, fake_next, &f, &d);
    CHECK(d.key == 'q');
    CHECK(d.state == LV_INDEV_STATE_PRESSED);

    /* The release is already in the stream, so it comes on the next read */
    kbd_rollover_read(&k, fake_next, &f, &d);
    CHECK(d.key == 'q');
    CHECK(d.state == LV_INDEV_STATE_RELEASED);

    /* With nothing new the last state is repeated, without asking for more */
    kbd_rollover_read(&k, fake_next, &f, &d);
    CHECK(d.key == 'q');
    CHECK(d.state == LV_INDEV_STATE_RELEASED);
    CHECK(!d.continue_reading);
}

/* The second key goes down before the first is released: the old key is
 * released now and the new one pressed on the next call */
static void test_rollover(void) {
    static const ev_t evs[] = {
        { 16, 'a', 1 },     /* press A */
        { 17, 'b', 1 },     /* B goes down while A is still held */
        { 17, 'b', 0 },     /* release B */
    };
    fake_t         f  = { evs, 3, 0 };
    kbd_rollover_t k  = {0};
    lv_indev_data_t d = {0};

    kbd_rollover_read(&k, fake_next, &f, &d);
    CHECK(d.key == 'a');
    CHECK(d.state == LV_INDEV_STATE_PRESSED);

    /* A is released first (the radio would otherwise see "ab" as one key) */
    kbd_rollover_read(&k, fake_next, &f, &d);
    CHECK(d.key == 'a');
    CHECK(d.state == LV_INDEV_STATE_RELEASED);

    /* The pending press comes now */
    kbd_rollover_read(&k, fake_next, &f, &d);
    CHECK(d.key == 'b');
    CHECK(d.state == LV_INDEV_STATE_PRESSED);

    /* And its release, once the stream reaches it */
    kbd_rollover_read(&k, fake_next, &f, &d);
    CHECK(d.key == 'b');
    CHECK(d.state == LV_INDEV_STATE_RELEASED);
}

/* Auto-repeat is LVGL's job: the same key held must not be reported twice */
static void test_auto_repeat(void) {
    static const ev_t evs[] = {
        { 16, 'x', 1 },     /* press */
        { 16, 'x', 1 },     /* kernel auto-repeat */
        { 16, 'x', 1 },
        { 16, 'x', 0 },     /* release */
    };
    fake_t         f  = { evs, 4, 0 };
    kbd_rollover_t k  = {0};
    lv_indev_data_t d = {0};

    kbd_rollover_read(&k, fake_next, &f, &d);
    CHECK(d.key == 'x');
    CHECK(d.state == LV_INDEV_STATE_PRESSED);

    /* The repeats are skipped, so the next report is the release */
    kbd_rollover_read(&k, fake_next, &f, &d);
    CHECK(d.key == 'x');
    CHECK(d.state == LV_INDEV_STATE_RELEASED);
}

/* Keys with no character (Shift, Ctrl...) are skipped */
static void test_modifiers_skipped(void) {
    static const ev_t evs[] = {
        { 42, 0, 1 },       /* left shift: key 0 */
        { 16, 'q', 1 },
        { 16, 'q', 0 },
    };
    fake_t         f  = { evs, 3, 0 };
    kbd_rollover_t k  = {0};
    lv_indev_data_t d = {0};

    kbd_rollover_read(&k, fake_next, &f, &d);
    CHECK(d.key == 'q');
    CHECK(d.state == LV_INDEV_STATE_PRESSED);
}

/* A release of a key that is not the held one is ignored */
static void test_stray_release(void) {
    static const ev_t evs[] = {
        { 16, 'a', 1 },
        { 30, 'z', 0 },     /* Z was never pressed */
        { 16, 'a', 0 },
    };
    fake_t         f  = { evs, 3, 0 };
    kbd_rollover_t k  = {0};
    lv_indev_data_t d = {0};

    kbd_rollover_read(&k, fake_next, &f, &d);
    CHECK(d.key == 'a');
    CHECK(d.state == LV_INDEV_STATE_PRESSED);

    kbd_rollover_read(&k, fake_next, &f, &d);
    CHECK(d.key == 'a');
    CHECK(d.state == LV_INDEV_STATE_RELEASED);   /* not 'z' */
}

void test_keyboard(void) {
    test_single_key();
    test_rollover();
    test_auto_repeat();
    test_modifiers_skipped();
    test_stray_release();
}
