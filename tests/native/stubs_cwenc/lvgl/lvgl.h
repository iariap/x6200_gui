/*
 * Minimal LVGL stub for the native CW encoder test (issue #116).
 *
 * src/cw_encoder.c includes "lvgl/lvgl.h" but calls nothing from it: the keyer
 * only toggles the radio's morse key and sleeps. The header exists so the
 * include resolves against the stage, not the real LVGL tree.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once
