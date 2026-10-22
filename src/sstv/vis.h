#ifndef SSTV_VIS_H
#define SSTV_VIS_H

/*
 * SSTV VIS header detector (SSTV_SAMPLE_RATE mono float).
 * From 1KO125's x6100_gui 1.0.2 (Hany El Imam), re-derived for 11025 Hz.
 *
 * x is a block of n samples (normally 1 s, scanned every 50 ms of audio) and
 * the VIS must lie entirely inside it.  A VIS starting in the first ~55 ms of
 * the block is found, so consecutive blocks must not be more than 50 ms apart.
 * n may be at most 2 s.  Reentrant: may be called from several threads.
 *
 * Returns true and fills *out for a mode our decoder handles.  For a valid
 * header of a known but unsupported mode it returns false and stores the VIS
 * code in res->unsupported_vis (otherwise -1).
 */

#include <stdbool.h>
#include "decoder.h"

typedef struct {
    int   unsupported_vis;  /* known VIS code we do not decode, or -1 */
    float leader_q;         /* leader tone energy fraction (gate)      */
    float offset_hz;        /* measured tuning offset (leader - 1900)  */
    float score;            /* frame score 0..1, -1 if no frame        */
    float start_s;          /* VIS start inside the block, seconds     */
    float bit_ms;           /* measured bit period                     */
    float slip_s;           /* gap found inside the data bits, seconds */
} sstv_vis_result_t;

bool sstv_vis_detect(const float *x, int n, sstv_info_t *out, sstv_vis_result_t *res);

/* Seconds from the VIS start (start_s) to the end of its stop bit. */
#define SSTV_VIS_LEN_S 0.910

#endif
