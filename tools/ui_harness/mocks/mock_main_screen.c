/*
 *  Explicit host mocks for the parts of the main screen that are not rendered
 *  by the harness: the app dialogs (they open over the main screen), the DX
 *  cluster / spot marks, the second encoder, and the recording/radio command
 *  surface. Every answer is documented and fixed; nothing reaches a device.
 *
 *  These are the "rest of the GUI" for `main_screen.c`: the harness renders the
 *  screen itself (spectrum, waterfall, buttons, panels, meters), but not the
 *  dialogs the buttons can open, so their constructors are deliberate no-ops.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdint.h>
#include <string.h>

#include "lvgl/lvgl.h"

#include "cluster_column.h"
#include "dialog.h"
#include "helpers.h"
#include "keyboard.h"
#include "main.h"
#include "radio.h"
#include "recorder.h"
#include "rtty.h"
#include "scheduler.h"
#include "screen_rec.h"
#include "screenshot.h"
#include "spot_marks.h"

#include "mock_radio.h"

/* --- the second encoder and the volume encoder are absent on the host ------- */

rotary_t  *vol;
encoder_t *mfk;
rotary_t  *mfk_inner;

/* --- app dialogs -----------------------------------------------------------
 * `main_screen.c` references one descriptor per app. The harness compiles the
 * real shared dialog machinery (src/dialog.c) and the real Ajustes window
 * (src/dialog_settings.c, which owns `dialog_settings`); the rest of the apps
 * still point at one inert descriptor whose constructor does nothing, so
 * opening one of them from a fixture is a no-op instead of a NULL call.
 * `dialog_settings` is NOT declared here: its own source defines it. */

static void mock_construct_cb(lv_obj_t *parent) { (void) parent; }

static dialog_t mock_dialog = { .construct_cb = mock_construct_cb };

dialog_t *dialog_bands      = &mock_dialog;
dialog_t *dialog_bt         = &mock_dialog;
dialog_t *dialog_callsign   = &mock_dialog;
dialog_t *dialog_cluster    = &mock_dialog;
dialog_t *dialog_cw_kbd     = &mock_dialog;
dialog_t *dialog_cw_trainer = &mock_dialog;
dialog_t *dialog_eq         = &mock_dialog;
dialog_t *dialog_field      = &mock_dialog;
dialog_t *dialog_freq       = &mock_dialog;
dialog_t *dialog_ft8        = &mock_dialog;
dialog_t *dialog_gps        = &mock_dialog;
dialog_t *dialog_js8        = &mock_dialog;
dialog_t *dialog_msg_cw     = &mock_dialog;
dialog_t *dialog_msg_voice  = &mock_dialog;
dialog_t *dialog_navtex     = &mock_dialog;
dialog_t *dialog_qth        = &mock_dialog;
dialog_t *dialog_recorder   = &mock_dialog;
dialog_t *dialog_sstv       = &mock_dialog;
dialog_t *dialog_status     = &mock_dialog;
dialog_t *dialog_swrscan    = &mock_dialog;
dialog_t *dialog_wefax      = &mock_dialog;
dialog_t *dialog_wifi       = &mock_dialog;
dialog_t *dialog_wspr       = &mock_dialog;

bool dialog_cluster_tune_spot(void) { return false; }

/* --- keyboard group ---------------------------------------------------------
 * The firmware's keyboard.c creates the group that focus travels through;
 * keyboard_init() is a no-op here, so the group starts NULL. A fixture that
 * exercises the focus chain asks for it explicitly. */

void mock_keyboard_group_up(void)
{
    if (!keyboard_group) {
        keyboard_group = lv_group_create();
    }
}

/* --- DX cluster column and spot marks over the waterfall ------------------- */

void cluster_column_init(lv_obj_t *parent, lv_coord_t y) { (void) parent; (void) y; }
void cluster_column_layout(lv_coord_t y) { (void) y; }
void cluster_column_refresh(void) {}
void cluster_column_set_active(bool on) { (void) on; }
void cluster_column_hide(bool hide) { (void) hide; }
const char *cluster_column_move(int dir) { (void) dir; return NULL; }
void cluster_column_tune(void) {}
bool cluster_column_area(lv_area_t *a) { (void) a; return false; }

void spot_marks_init(lv_obj_t *parent, lv_coord_t line_y, lv_coord_t tag_y)
{
    (void) parent;
    (void) line_y;
    (void) tag_y;
}
void spot_marks_layout(lv_coord_t line_y, lv_coord_t tag_y) { (void) line_y; (void) tag_y; }
void spot_marks_update(int32_t center, int32_t half_width) { (void) center; (void) half_width; }
bool spot_marks_crossed(int32_t from, int32_t to, int32_t *spot_hz) { (void) from; (void) to; (void) spot_hz; return false; }

/* --- keyboard group --------------------------------------------------------- */

lv_group_t *keyboard_group;

void keyboard_init(void) {}
bool keyboard_ready(void) { return false; }

/* --- scheduler (deferred work) ---------------------------------------------
 * The firmware runs these on its worker thread. The harness has no worker, so
 * they are queued and run by scheduler_work(), which virtual_time_advance()
 * calls after every timer slice. Running them outside the render pass keeps
 * LVGL from seeing a dirty area change mid-draw, and the FIFO order keeps the
 * sequence deterministic. */

#define SCHED_QUEUE 256

static scheduler_fn_t sched_fn[SCHED_QUEUE];
static void         *sched_arg[SCHED_QUEUE];
static int           sched_head;
static int           sched_tail;
static int           sched_count;

static void sched_push(scheduler_fn_t fn, void *arg)
{
    if (!fn || sched_count >= SCHED_QUEUE) {
        return;
    }
    sched_fn[sched_tail] = fn;
    sched_arg[sched_tail] = arg;
    sched_tail = (sched_tail + 1) % SCHED_QUEUE;
    sched_count++;
}

void scheduler_put(scheduler_fn_t fn, void *arg, size_t arg_size)
{
    (void) arg_size;
    sched_push(fn, arg);
}

void scheduler_put_noargs(scheduler_fn_t fn)
{
    sched_push(fn, NULL);
}

void scheduler_work(void)
{
    /* Bounded so a callback that schedules more work cannot loop forever. */
    int budget = SCHED_QUEUE;

    while (sched_count > 0 && budget-- > 0) {
        scheduler_fn_t fn = sched_fn[sched_head];
        void *arg = sched_arg[sched_head];

        sched_head = (sched_head + 1) % SCHED_QUEUE;
        sched_count--;
        fn(arg);
    }
}

/* --- recorder / RTTY / screen recording / screenshot (no device) ----------- */

char *recorder_path;

void recorder_set_on(bool on) { (void) on; }
bool recorder_is_on(void) { return false; }
void recorder_put_audio_samples(size_t nsamples, int16_t *samples) { (void) nsamples; (void) samples; }

void rtty_init(void) {}
void rtty_put_audio_samples(unsigned int n, cfloat *samples) { (void) n; (void) samples; }
void rtty_set_state(rtty_state_t state) { (void) state; }
rtty_state_t rtty_get_state(void) { return RTTY_OFF; }
float rtty_change_rate(int16_t df) { (void) df; return 0.0f; }
uint16_t rtty_change_shift(int16_t df) { (void) df; return 0; }
uint16_t rtty_change_center(int16_t df) { (void) df; return 0; }
bool rtty_change_reverse(int16_t df) { (void) df; return false; }

void (*screen_rec_out)(lv_disp_drv_t *, const lv_area_t *, lv_color_t *);

void screen_rec_toggle(void) {}
bool screen_rec_is_on(void) { return false; }
void screen_rec_shutdown(void) {}
void screen_rec_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    (void) drv;
    (void) area;
    (void) color_p;
}
void screen_rec_put_audio_samples(size_t nsamples, int16_t *samples) { (void) nsamples; (void) samples; }

void screenshot_take(void) {}
