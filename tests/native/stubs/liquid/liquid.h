/*
 * Minimal liquid-dsp stub for the unit tests: pass-through notch filter.
 * Enough to exercise audio_enh_notch_add/remove/clear_all and apply_notch_filters.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

typedef struct firfilt_rrrf_s * firfilt_rrrf;

firfilt_rrrf firfilt_rrrf_create_notch(unsigned int m, float As, float f0);
void         firfilt_rrrf_destroy(firfilt_rrrf q);
void         firfilt_rrrf_push(firfilt_rrrf q, float x);
void         firfilt_rrrf_execute(firfilt_rrrf q, float *y);
