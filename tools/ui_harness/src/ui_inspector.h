/*
 *  Structural inspection of the live LVGL object tree.
 *
 *  The harness renders a screen and, with `--inspect`, walks the object tree of
 *  the active screen and serialises what the pixels alone cannot show: absolute
 *  bounding boxes (and the box actually visible after the parents' clipping),
 *  the effective visibility, the widget class, the label text with its real
 *  font metrics, the relevant style colours and the glyphs that fall back or
 *  are missing from every font in the chain.
 *
 *  It never changes a rendered pixel: the block is only written to the JSON and
 *  only when asked for, so the benchmark path and the plain capture stay
 *  untouched. Text sizes come from LVGL's own font metrics (lv_txt_get_size and
 *  lv_font_get_glyph_dsc), never from OCR.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdio.h>

#include "lvgl/lvgl.h"

/* Write the inspection object for `screen` (the active screen) to `f`.
 * Emits `{ "instrumented": true, "viewport": {...}, "nodes": [...] }`. */
void ui_inspector_write_json(FILE *f, const lv_obj_t *screen);
