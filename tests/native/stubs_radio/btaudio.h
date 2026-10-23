/* Minimal btaudio.h stub for the radio_thread test (issues #219, #244): src/radio.c mueve el
 * nivel de los auriculares A2DP y consulta si el altavoz esta muteado por BT (radio_vol_apply).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdbool.h>

void btaudio_headphone_volume(int pct);
bool btaudio_speaker_muted(void);
