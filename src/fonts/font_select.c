/* Runtime font choice.
 *
 * The ROM's code keeps using `&sony_N;` everywhere. `sony_N` is the stock family:
 * since issue #9 / decision 1 it is generated from a libre font (tools/fonts/,
 * docs/THIRD_PARTY.md) instead of the proprietary Sony one, so the default needs
 * no switch at all. Choosing another family copies its struct into the stock slot
 * before the UI is built, and the alternate families are ASCII-only, so the icon
 * glyphs keep coming from the stock copy through the fallback.
 *
 * Which families exist, what they are called and which one is at which index all
 * come from src/fonts/font_families.h, generated from tools/fonts/candidates.json:
 * the dropdown row, the `params.font` value, a theme's `font` id and the compiled
 * symbols are one list, so they cannot drift apart (the off-by-one that made
 * "Roboto" apply DejaVu, src/main.c, was exactly that).
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "lvgl/lvgl.h"
#include "font_select.h"

#define N_FONTS FONT_SIZES_N

static lv_font_t orig[N_FONTS];
static bool applied;

/* The dropdown's item string: " A \n B \n C" in FONT_* order. Built once from
 * font_names[] (the generated list) because the row index is the FONT_* value. */
#define FONT_ITEMS_LEN (FONT_COUNT * 24 + 1)
static char font_items[FONT_ITEMS_LEN];

const char *font_dropdown_items(void) {
    if (font_items[0] == '\0') {
        size_t used = 0;

        for (int family = 0; family < FONT_COUNT; family++) {
            int written = snprintf(font_items + used, sizeof(font_items) - used, "%s %s ",
                                   family ? "\n" : "", font_names[family]);
            if (written < 0 || (size_t) written >= sizeof(font_items) - used) {
                /* The buffer is sized for the generated list; a longer one is a
                 * build-time fact (FONT_COUNT), so this cannot be silent. */
                font_items[used] = '\0';
                break;
            }
            used += (size_t) written;
        }
    }
    return font_items;
}

int font_family_by_id(const char *id) {
    for (int family = 0; family < FONT_COUNT; family++) {
        if (!strcasecmp(id, font_ids[family])) return family;
    }
    return -1;
}

int buttons_h = 54;  /* = BUTTONS_H_MIN */

/* The segmented-digit family in use, or FONT_DIGIT_NONE. */
static uint8_t digit_family = FONT_DIGIT_NONE;

int font_digit_by_id(const char *id) {
    for (int family = 0; family < FONT_DIGIT_COUNT; family++) {
        if (!strcasecmp(id, font_digit_ids[family])) return family;
    }
    return -1;
}

void fonts_set_digit_family(uint8_t family) {
    digit_family = family < FONT_DIGIT_COUNT ? family : (uint8_t) FONT_DIGIT_NONE;
}

/* The family one stock slot draws with. The digit panels ask for the slot they
 * would use (`font_digit(&sony_38)`), and get the segmented family when one is
 * selected; a slot the segmented family does not carry (or FONT_DIGIT_NONE)
 * returns the slot itself, so this is never a change of behaviour on its own. */
const lv_font_t *font_digit(const lv_font_t *stock_slot) {
    if (digit_family == FONT_DIGIT_NONE) return stock_slot;

    for (unsigned i = 0; i < N_FONTS; i++) {
        if (font_stock[i] == stock_slot) return font_digit_families[digit_family][i];
    }
    /* Not one of the alternating slots (sony_8/10/12): the segmented families do
     * not carry them, so the panel keeps the default family. */
    return stock_slot;
}

void fonts_apply(uint8_t family) {
    if (applied || family == FONT_STOCK || family >= FONT_COUNT) return;
    applied = true;

    for (unsigned i = 0; i < N_FONTS; i++) {
        orig[i] = *font_stock[i];
        *font_stock[i] = *font_families[family][i];
        font_stock[i]->fallback = &orig[i];
    }
}

/* The stock family `steps` sizes smaller (sony_60 stays): for screens that VISION's
 * scale makes too big */
const lv_font_t *font_down(const lv_font_t *f, int steps) {
    for (int i = 0; i + 1 < (int) N_FONTS; i++)
        if (font_stock[i] == f) return font_stock[i - steps > 0 ? i - steps : 0];
    return f;
}

/* VISION: every stock size drawn `steps` sizes bigger (sony_20 as sony_26 with 3), up to sony_44;
 * sony_60 stays. Call once, after fonts_apply() and before the UI is built */
void fonts_scale(int steps) {
    static lv_font_t was[N_FONTS];

    for (unsigned i = 0; i < N_FONTS; i++) was[i] = *font_stock[i];
    for (unsigned i = 0; i + 1 < N_FONTS; i++) {
        unsigned to = i + steps < N_FONTS - 1 ? i + steps : N_FONTS - 2;
        *font_stock[i]    = was[to];
        /* The scaled struct borrows the coverage of the size it replaced: only
         * sony_20/sony_30 carry the icon ranges, so without this fallback the
         * clock (sony_30) and the top-bar icons (sony_20) draw as empty boxes
         * under VISION (UI-011). was[i] keeps the icon cmap of the original slot
         * (and, with an alternate font, its own stock fallback). */
        font_stock[i]->fallback = &was[i];
    }
}
