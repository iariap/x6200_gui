/*
 * Link stubs for the audio enhancement chain (src/audio_enhancement.c) in the
 * host suite.
 *
 * PR #232 added audio_enh_apply_mode_preset(), which reads the stored preset
 * (params.audio_enh_preset), the live mode (subject_get_int(cfg_cur.mode)) and
 * otherwise leaves the processing to the functions the test drives. None of
 * those symbols is provided by the modules compiled into this stage (the real
 * params/cfg_cur/subjects live in the radio build), so the linker needs them
 * here. `subject_get_int()` is controllable from the test (test_audio_enh_mode_set) so
 * the CW/CWR rule of audio_enh_apply_mode_preset() can be exercised without touching
 * params.audio_enh_preset; its default is x6200_mode_lsb (0), the value the previous
 * fixed stub returned.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdint.h>

#include "cfg/cfg.h"
#include "params/params.h"

params_t  params;
cfg_cur_t cfg_cur;

/* Modo visible para audio_enh_apply_mode_preset(). Solo audio_enhancement.c llama a
 * subject_get_int() en este stage, asi que un unico entero controlable alcanza: el test
 * pone CW/CWR para probar la regla del PR #232 (perfil CW, NR off) sin tocar el preset
 * guardado. El default 0 es x6200_mode_lsb, el mismo valor que devolvia el stub fijo. */
static int32_t stub_mode = 0;

void test_audio_enh_mode_set(int32_t mode) {
    stub_mode = mode;
}

/* El test no incluye params.h (traeria el SDK de la radio a un stage de host): toca el
 * preset guardado por estos accesores, que son el mismo campo que lee el modulo. */
void test_audio_enh_stored_preset_set(uint8_t preset) {
    params.audio_enh_preset.x = preset;
}

uint8_t test_audio_enh_stored_preset_get(void) {
    return params.audio_enh_preset.x;
}

int32_t subject_get_int(Subject *subj) {
    (void) subj;

    return stub_mode;
}
