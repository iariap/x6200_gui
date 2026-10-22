/*
 * Link stubs for the audio enhancement chain (src/audio_enhancement.c) in the
 * host suite.
 *
 * PR #232 added audio_enh_apply_mode_preset(), which reads the stored preset
 * (params.audio_enh_preset), the live mode (subject_get_int(cfg_cur.mode)) and
 * otherwise leaves the processing to the functions the test drives. None of
 * those symbols is provided by the modules compiled into this stage (the real
 * params/cfg_cur/subjects live in the radio build), so the linker needs them
 * here. The test suite never calls audio_enh_apply_mode_preset(): these are
 * link-only definitions with the exact types of the real globals, kept
 * separate from stubs/stubs.c so the reason is explicit.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdint.h>

#include "cfg/cfg.h"
#include "params/params.h"

params_t  params;
cfg_cur_t cfg_cur;

int32_t subject_get_int(Subject *subj) {
    (void) subj;

    return 0;
}
