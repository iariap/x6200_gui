#ifndef SSTV_PREVIEW_H
#define SSTV_PREVIEW_H

/*
 * Progressive display: decides which picture row can be drawn next, which
 * slice of the capture it needs, and draws it with the decoder's row
 * renderer (sstv_render_row()).  Pure C, no LVGL, no locking: the caller
 * copies the slice out of the capture buffer under its own mutex, then
 * renders outside of it.  The final picture still comes from the full
 * decoder; the preview is only shown while receiving.
 * From 1KO125's x6100_gui 1.0.2 (Hany El Imam).
 *
 * Positions are sample indices of the capture buffer.  Row timing:
 *  - before the line-sync tracker locks: row 0 from the VIS + nominal period;
 *  - afterwards: phase and period of the received syncs (slant follows).
 */

#include <stdbool.h>
#include <stdint.h>
#include "decoder.h"
#include "linesync.h"

/* Measured at 11025 Hz (tests/test_sstv.c): start_s + SSTV_VIS_LEN_S of
 * sstv/vis.c lies this many samples before the true row-0 sync, the
 * tracker's sync start this many samples before the true sync. */
#define SSTV_PREVIEW_VIS_BIAS   35.0
#define SSTV_PREVIEW_SYNC_BIAS  6.0

typedef struct {
    sstv_mode_t mode;
    int         width, height;
    double      row0;          /* anchor of row 0 (bias corrected), <0 unknown */
    double      nominal;       /* nominal line period, samples               */
    int         next_row;      /* first row not drawn yet                    */
    double      pend_anchor;   /* anchor of next_row, from the last need()   */
    double      pend_scale;
    long        pend_a, pend_b;
} sstv_preview_t;

/* row0_anchor: sample index of the anchor of row 0 as estimated from the
 * VIS (vis_end + sstv_row0_anchor_after_vis(mode) * SR), WITHOUT bias
 * correction.  Pass < 0 if unknown (Manual): row 0 is then the first sync
 * of the tracker's locking chain. */
void sstv_preview_init(sstv_preview_t *p, sstv_mode_t mode, double row0_anchor);

/* Is the next row ready?  written: samples available in the capture
 * buffer; ls: tracker status (may be NULL).  On true, [*a, *b) is the
 * slice to copy and pass to sstv_preview_render(). */
bool sstv_preview_need(sstv_preview_t *p, long written, const sstv_ls_status_t *ls, long *a, long *b);

/* Draw the row prepared by sstv_preview_need() from slice = capture[a..b).
 * rgb_row: sstv_row_width(mode) * 3 bytes.  Returns the row index drawn,
 * or -1 if it could not be drawn (it is skipped). */
int sstv_preview_render(sstv_preview_t *p, const float *slice, uint8_t *rgb_row);

static inline bool sstv_preview_done(const sstv_preview_t *p) { return p->next_row >= p->height; }

#endif
