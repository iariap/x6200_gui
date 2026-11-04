#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Hooks llamados desde src/audio.c y src/radio.c SÓLO con X6200_LINUX_DSP_ROUTE=ON.
 * DEFAULT OFF: no se compilan ni se llaman; el build normal queda BASE. */
void x6200_route_audio_submit(const int16_t *samples, size_t count, bool already_processed);
void x6200_route_audio_generated(bool active);
void x6200_route_radio_tx(bool tx);
