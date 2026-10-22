/*
 * Native test for src/cfg/digital_modes.c (issue #63, "el boton de WSPR
 * reinicia la radio").
 *
 * The defect: cfg_digital_label_get() returned the label of the last
 * successful cfg_digital_load(), and that pointer is NULL until one happens.
 * The WSPR dialog hands it straight to LVGL's text formatting --
 * lv_label_set_text_fmt(frame.head_left, "%s   %.4f MHz", cfg_digital_label_get(),
 * ...) in its timer callback (src/dialog_wspr.c) -- and LVGL's formatter
 * dereferences the argument (_strnlen_s() in lvgl/src/misc/lv_printf.c). A
 * radio whose database has no rows of type 4 (WSPR) therefore crashed with
 * SIGSEGV as soon as the app was opened (reproduced on the bench:
 * CRASHLOG sig=11 fallo=(nil) pc=0x4160d6a8, right after
 * "cfg_digital_load: No results for load from digital_modes ... type=4").
 *
 * The invariant this test pins: cfg_digital_label_get() always returns a valid,
 * NUL-terminated string -- "" while no load has succeeded -- and still returns
 * the label once a load does succeed (the fix must not turn the getter into a
 * constant "").
 *
 * The label reads below go through label_now() and are deliberately unguarded:
 * the real consumer (LVGL) has no guard either, so on a defective revision this
 * test dies with the same SIGSEGV the radio did. The reason is printed to
 * stderr first, which is unbuffered and therefore survives the crash (the
 * framework's own FALLO goes to stdout, which a SIGSEGV discards).
 *
 * src/cfg/digital_modes.c is staged next to minimal stubs (tests/native/
 * stubs_digital/), the same pattern as test_clock.c / test_backlight.c: the
 * real module is compiled, and the fake sqlite3 lets the test choose whether
 * cfg_digital_load() finds a row.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "test_framework.h"

#include "digital_modes.h"
#include "digital_modes.private.h" /* cfg_digital_modes_init() */

/* Provided by tests/native/digital_modes_stubs.c. */
void digital_modes_stub_set_row(bool has_row, const char * label, int32_t freq, int32_t mode);
int32_t digital_modes_stub_fg_freq(void);
int32_t digital_modes_stub_mode(void);

static const char * label_now(void) {
    const char * label = cfg_digital_label_get();

    if (label == NULL) {
        fprintf(stderr, "FALLO %s:%d: cfg_digital_label_get() devolvio NULL y su consumidor "
                        "(LVGL) lo desreferencia\n",
                __FILE__, __LINE__);
    }

    return label;
}

void test_digital_modes(void) {
    cfg_digital_modes_init(NULL);

    /* Before any load, and for a database without rows of the requested type
     * (the WSPR case: type 4 missing), the getter must yield "". */
    digital_modes_stub_set_row(false, NULL, 0, 0);

    CHECK(label_now()[0] == '\0');

    CHECK(cfg_digital_load(0, CFG_DIG_TYPE_WSPR) == false);
    CHECK(label_now()[0] == '\0');

    CHECK(cfg_digital_load(0, CFG_DIG_TYPE_JS8) == false);
    CHECK(label_now()[0] == '\0');

    /* A successful load still publishes the label and moves the radio. */
    digital_modes_stub_set_row(true, "20m FT8", 14074000, 0);
    CHECK(cfg_digital_load(0, CFG_DIG_TYPE_FT8) == true);
    CHECK(strcmp(label_now(), "20m FT8") == 0);
    CHECK(cfg_digital_freq_get() == 14074000);
    CHECK(digital_modes_stub_fg_freq() == 14074000);
    CHECK(digital_modes_stub_mode() == 0);

    /* A later failed load keeps the last good label (still never NULL). */
    digital_modes_stub_set_row(false, NULL, 0, 0);
    CHECK(cfg_digital_load(0, CFG_DIG_TYPE_WSPR) == false);
    CHECK(strcmp(label_now(), "20m FT8") == 0);
}
