/*
 *  Host stubs for the NON-UI services the three app dialogs of the
 *  `ui_harness_dialogs` target call, so those REAL windows can be built and
 *  rendered on a PC:
 *
 *    - the CW keyer       (src/cw_encoder.c): nothing is ever keyed here;
 *    - the textarea window (src/textarea_window.c): the Rename/Edit editor;
 *    - the CW message store (src/params/params.c): params_msg_cw_*;
 *    - the audio playback surface (src/audio.c): audio_play*().
 *
 *  This file is compiled ONLY into `ui_harness_dialogs` (CMakeLists.txt), which
 *  links the REAL src/dialog_freq.c, src/dialog_msg_cw.c and
 *  src/dialog_msg_voice.c over the same LVGL and the same firmware components as
 *  `ui_harness`. Every answer is a documented no-op: rendering a window never
 *  calls these, so the capture measures the dialog's own layout, not a service.
 *  The coverage table of the dialog screens declares them SYNTHETIC.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdbool.h>
#include <stdint.h>

#include "lvgl/lvgl.h"

#include "audio.h"
#include "cw_encoder.h"
#include "params/params.h"
#include "textarea_window.h"

/* --- CW keyer (src/cw_encoder.c): no keying on the host --------------------- */

static cw_encoder_state_t cw_encoder_cur = CW_ENCODER_IDLE;

void cw_encoder_send(const char *text, bool beacon) { (void) text; (void) beacon; }
void cw_encoder_stop(void) {}
cw_encoder_state_t cw_encoder_state(void) { return cw_encoder_cur; }
int cw_encoder_sent(void) { return 0; }
void cw_encoder_require_train(bool on) { (void) on; }

/* --- textarea editing window (src/textarea_window.c) ----------------------- */

lv_obj_t *textarea_window_open(textarea_window_cb_t ok_cb, textarea_window_cb_t cancel_cb)
{
    (void) ok_cb;
    (void) cancel_cb;

    return NULL;
}

lv_obj_t *textarea_window_open_w_label(textarea_window_cb_t ok_cb, textarea_window_cb_t cancel_cb,
                                       const char *text)
{
    (void) ok_cb;
    (void) cancel_cb;
    (void) text;

    return NULL;
}

void textarea_window_close(void) {}
const char *textarea_window_get(void) { return ""; }
void textarea_window_set(const char *text) { (void) text; }
lv_obj_t *textarea_window_text(void) { return NULL; }

/* --- CW message store (src/params/params.c) -------------------------------- */

void params_msg_cw_load(void) {}
void params_msg_cw_new(const char *val) { (void) val; }
void params_msg_cw_edit(uint32_t id, const char *val) { (void) id; (void) val; }
void params_msg_cw_delete(uint32_t id) { (void) id; }
int params_msg_cw_list(char (*out)[PARAMS_MSG_CW_LEN], int max)
{
    (void) out;
    (void) max;

    return 0;
}

/* --- audio playback (src/audio.c) ------------------------------------------ */

int audio_play(int16_t *buf, size_t samples) { (void) buf; (void) samples; return 0; }
void audio_play_wait(void) {}
void audio_play_en(bool on) { (void) on; }
