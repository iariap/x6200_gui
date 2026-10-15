/*
 * Native test for the CLOCK_POWER branch of src/clock.c (UI-009).
 *
 * The same binary also pins the "8 fantasma" layer of the clock (issue #9,
 * decision 1): with a segmented digit family `clock_init()` creates a shadow
 * label behind the value (the clock's own background image moves to the shadow,
 * which is the lower one), and the shadow's text is the value's text in the
 * "8" form. The stub tree declares the family active for exactly that reason.
 *
 * The defect (docs/UI_KNOWN_ISSUES.md, UI-009): `show_time()` declared
 * `bat_sym` without initialising it while the `switch ((uint8_t)(v_bat * 10))`
 * only covered 0..100. A battery above 10.2 V (12.1 V on an external supply is
 * 121 after the cast) left the pointer undefined and `snprintf` printed the
 * stack: garbage glyphs on screen, undefined behaviour on another toolchain
 * (segfault, reproduced with gcc 12 on Debian). This test fails on that
 * revision and passes once the pointer is initialised: the first character of
 * the power-clock text must be a battery glyph (the U+F240..U+F244 block),
 * never memory.
 *
 * Decisions:
 * - `show_time()` is static, so the test drives it through the timer callback
 *   that `clock_init()` registers; `clock_stubs.c` captures them.
 * - src/clock.c is compiled against a dedicated stub tree
 *   (`tests/native/stubs_clock/`) with the minimal LVGL/firmware surface it
 *   uses, so the shared stub of the rest of the native suite is untouched and
 *   the full LVGL headers are not needed.
 * - The battery glyphs are matched by their first three UTF-8 bytes, which is
 *   how LVGL encodes the symbols (`\xEF\x89\x80`..`\xEF\x89\x84`), not by
 *   pointer identity (string-literal pooling is implementation-defined).
 * - Reading an uninitialized pointer is undefined: a plain build can print a
 *   value the stack happened to hold from an earlier call. run_native_tests.sh
 *   compiles this binary with `-ftrivial-auto-var-init=pattern` (when the
 *   toolchain supports it) so a reintroduced `const char * bat_sym;` reads a
 *   poisoned pointer and the test crashes deterministically instead of passing
 *   by luck.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <string.h>

#include "test_framework.h"

#include "clock.h"
#include "params/params.h"

/* Accessors provided by tests/native/clock_stubs.c. */
const char * clock_stub_last_text(void);
const char * clock_stub_shadow_text(void);
void clock_stub_run_timers(void);

/* The battery family is the U+F240..U+F244 block: three UTF-8 bytes each,
 * sharing the first two (`\xEF\x89`). */
static bool starts_with_battery_glyph(const char * text) {
    return (unsigned char) text[0] == 0xEF &&
           (unsigned char) text[1] == 0x89 &&
           (unsigned char) text[2] >= 0x80 &&
           (unsigned char) text[2] <= 0x84;
}

static void render_power(float battery_v, float external_v, uint8_t cap, bool charging) {
    params.clock_view = CLOCK_POWER_ALLWAYS;
    clock_update_power(external_v, battery_v, cap, charging);
    clock_stub_run_timers();
}

void test_clock(void) {
    /* clock_init() has no return statement on the target (the firmware ignores
     * its result); the test does the same and drives show_time() through the
     * timer callbacks the stub captures. */
    clock_init(NULL);

    /* The in-range mapping is unchanged. */
    render_power(5.8f, 13.8f, 30, false);
    CHECK(strncmp(clock_stub_last_text(), LV_SYMBOL_BATTERY_1, 3) == 0);

    render_power(6.5f, 13.8f, 50, false);
    CHECK(strncmp(clock_stub_last_text(), LV_SYMBOL_BATTERY_2, 3) == 0);

    render_power(8.1f, 0.0f, 90, false);
    CHECK(strncmp(clock_stub_last_text(), LV_SYMBOL_BATTERY_FULL, 3) == 0);

    /* Above 10.2 V no switch case applies: the fix must still yield a battery
     * glyph (the defect was an uninitialized pointer read by snprintf). */
    render_power(12.1f, 13.8f, 76, true);
    const char * text = clock_stub_last_text();

    CHECK(starts_with_battery_glyph(text));
    CHECK(text[3] == ' ');
    CHECK(strstr(text, "12.1v") != NULL);

    /* The time view is still the wall clock: no battery glyph. */
    params.clock_view = CLOCK_TIME_ALLWAYS;
    clock_stub_run_timers();
    CHECK(!starts_with_battery_glyph(clock_stub_last_text()));

    /* El "8 fantasma" (issue #9, decision 1): con la familia segmentada el reloj
     * dibuja DOS etiquetas, la sombra primero (debajo) y el valor despues. El
     * fantasma es el mismo texto en forma de 8 -los siete segmentos de cada digito-
     * a alpha bajo, asi que tiene que medir lo mismo que el valor: eso es lo que lo
     * deja caer digito a digito sobre el. */
    CHECK(strcmp(clock_stub_shadow_text(), "88:88:88") == 0);
    CHECK(strlen(clock_stub_last_text()) == strlen(clock_stub_shadow_text()));

    /* El reloj de potencia no lleva fantasma (el valor es el simbolo de bateria mas
     * los voltios: no hay segmentos apagados que mostrar) y la sombra queda vacia,
     * solo con el fondo. */
    render_power(5.8f, 13.8f, 30, false);
    CHECK(strcmp(clock_stub_shadow_text(), "") == 0);
}
