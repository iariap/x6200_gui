#pragma once
#include <stdint.h>

#include "font_families.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The families, their FONT_* values, their ids and the compiled symbols are
 * generated from tools/fonts/candidates.json into font_families.h (see
 * tools/fonts/README.md). FONT_STOCK is the ROM's own slot (src/fonts/sony_*.c),
 * generated from the libre family the migration of issue #9 / decision 1 chose;
 * every other value is a candidate family compiled in from src/fonts/. */

/* The family a theme JSON names in its `font` key, or -1 when the id is unknown
 * (the Ajustes choice then decides). */
int font_family_by_id(const char *id);

/* The Ajustes dropdown's item list, in FONT_* order (LVGL's " a \n b" format),
 * built from font_names[] so the row a user picks IS the FONT_* value. */
const char *font_dropdown_items(void);

/* The family the digit panels draw with (the frequency row's own labels and the
 * clock): FONT_DIGIT_NONE is the default family, every other value is a
 * segmented (seven-segment LCD) family, which is digit-only and therefore never a
 * whole-family choice (the buttons and the labels are letters). `font_digit()`
 * returns the family for one stock slot, or the slot itself when nothing is
 * selected or the family does not carry that slot. */
void fonts_set_digit_family(uint8_t digit_family);
const struct _lv_font_t *font_digit(const struct _lv_font_t *stock_slot);

/* The digit family a theme's `font_digits` key names, or -1 when the id is
 * unknown (no segmented digits). */
int font_digit_by_id(const char *id);

/* Call once at boot, before any style or widget uses the fonts. FONT_STOCK (the
 * default) copies nothing: the ROM already draws with its own family. */
void fonts_apply(uint8_t family);
void fonts_scale(int steps);
const struct _lv_font_t *font_down(const struct _lv_font_t *f, int steps);

#ifdef __cplusplus
}
#endif
