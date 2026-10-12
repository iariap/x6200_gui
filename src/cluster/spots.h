/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - band lookup for the app dialogs' frequency strip.
 *
 *  The X6100 takes the band note from its DX cluster spots module, which is not
 *  ported yet; this answers the same two questions with an IARU R1 band table.
 *  When the cluster lands it can take over and the callers stay the same.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Band index of a frequency, or -1 outside a ham band */
int cluster_band_of(int32_t freq_hz);

/* Name of the band ("20m"), or NULL outside a ham band */
const char *cluster_band_name(int band);

#ifdef __cplusplus
}
#endif
