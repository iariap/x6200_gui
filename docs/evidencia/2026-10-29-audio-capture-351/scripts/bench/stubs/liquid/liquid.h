/*
 * Stub minimo de liquid-dsp para el banco: solo los tipos/firmas que usa
 * src/audio_enhancement.c. La implementacion (stubs_bench.c) aborta, de modo
 * que un caso con notch activo falla en vez de medir otra cosa.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

typedef struct firfilt_rrrf_s *firfilt_rrrf;

firfilt_rrrf firfilt_rrrf_create_notch(unsigned int m, float as, float f0);
void         firfilt_rrrf_destroy(firfilt_rrrf q);
void         firfilt_rrrf_push(firfilt_rrrf q, float x);
void         firfilt_rrrf_execute(firfilt_rrrf q, float *y);
