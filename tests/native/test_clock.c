/*
 * Native test for src/clock.c: el reloj permanente y la banda de estado
 * (issue #250, epic #249).
 *
 * The contract it pins, line by line (the numbers are the ones of the issue):
 *
 * 1. the panel is ONE container with `clock_style` (the theme's background
 *    bitmap once, no input: no click, no focus, no scroll), with the labels
 *    inside it: the clock line and the status line, and no label carries the
 *    style (the value keeps the image only as `bg_img_src` with
 *    `LV_OPA_TRANSP`, which is the documented path that keeps the ghost/value
 *    stacking read as design and not as two stacked texts);
 * 2. the time is `HH:MM:SS` and is ALWAYS visible: it does not depend on
 *    `params.clock_view`, on any timeout or on `radio_get_state()` (RX, TX,
 *    every persisted view); the ghost `88:88:88` keeps the value's metric;
 *    `localtime()` returning NULL shows `--:--:--`;
 * 3. the status line: external supply (plug + `%.1f V`) only when `v_ext` is
 *    finite and >= 3.0, and the battery (icon + percentage) on the right, with
 *    the charging glyph when the sample is valid and the source says so;
 * 4. a sample is valid only when `v_bat > 0`, finite and `cap <= 100`; invalid
 *    is `--%` with the icon empty/dim (`TC(text_dim)`), never an invented 0%;
 *    a valid sample prints its own percentage (0% included) and only the icon
 *    is clamped to the five levels;
 * 5. one `power_mux` snapshot per update, every LVGL call outside the mutex,
 *    the two existing timers and nothing new, and no rewrite (text, colour,
 *    font) when nothing changed (OPT-4 #150);
 * 6. the exported setters stay (API compatibility) but do not change the
 *    permanent view.
 *
 * The pixel geometry (the 61 px panel, nothing clipped, the real fonts) is
 * measured with the real LVGL in tools/ui_harness (`--inspect`), not here: this
 * binary runs against tests/native/stubs_clock/.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "test_framework.h"

#include "clock.h"
#include "clock_stubs.h"
#include "params/params.h"
#include "radio.h"
#include "styles.h"
#include "theme.h"

static void render(float external_v, float battery_v, uint8_t cap, bool charging) {
    clock_update_power(external_v, battery_v, cap, charging);
    clock_stub_run_timers();
}

static bool same_color(lv_color_t a, lv_color_t b) {
    return a.red == b.red && a.green == b.green && a.blue == b.blue;
}

static bool starts_with(const char * text, const char * prefix) {
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

void test_clock(void) {
    /* clock_init() returns the panel; the firmware ignores it and the test
     * drives show_time() through the timer callbacks the stub captures. */
    clock_init(NULL);

    /* ---- 1. El panel: un contenedor con el fondo, sin input, y sus roles ---- */
    CHECK(clock_stub_object_count() == CLOCK_STUB_OBJECTS);
    CHECK(!clock_stub_is_label(CLOCK_STUB_CONTAINER));
    CHECK(clock_stub_style_removed(CLOCK_STUB_CONTAINER));
    CHECK(clock_stub_style_adds(CLOCK_STUB_CONTAINER) == 1);
    CHECK(clock_stub_style_is_clock(CLOCK_STUB_CONTAINER));
    CHECK((clock_stub_flags(CLOCK_STUB_CONTAINER) & LV_OBJ_FLAG_CLICKABLE) == 0);
    CHECK((clock_stub_flags(CLOCK_STUB_CONTAINER) & LV_OBJ_FLAG_CLICK_FOCUSABLE) == 0);
    CHECK((clock_stub_flags(CLOCK_STUB_CONTAINER) & LV_OBJ_FLAG_SCROLLABLE) == 0);

    for (int i = CLOCK_STUB_GHOST; i < CLOCK_STUB_OBJECTS; i++) {
        CHECK(clock_stub_is_label(i));
        CHECK(clock_stub_parent(i) == CLOCK_STUB_CONTAINER);
        CHECK(clock_stub_style_removed(i));
        /* El fondo lo dibuja el contenedor UNA sola vez. */
        CHECK(!clock_stub_style_is_clock(i));
    }

    /* ---- 2. La linea del reloj: fantasma y valor con la MISMA metrica ---- */
    CHECK(strcmp(clock_stub_shadow_text(), "88:88:88") == 0);
    CHECK(clock_stub_text_opa(CLOCK_STUB_GHOST) == UI_DIGIT_SHADOW_OPA);
    CHECK(clock_stub_text_opa(CLOCK_STUB_TIME) == LV_OPA_COVER);
    CHECK(clock_stub_font(CLOCK_STUB_TIME) == font_digit(&sony_38));
    CHECK(clock_stub_font(CLOCK_STUB_GHOST) == clock_stub_font(CLOCK_STUB_TIME));
    CHECK(clock_stub_width(CLOCK_STUB_GHOST) == clock_stub_width(CLOCK_STUB_TIME));
    CHECK(clock_stub_height(CLOCK_STUB_GHOST) == clock_stub_height(CLOCK_STUB_TIME));
    CHECK(clock_stub_align(CLOCK_STUB_GHOST) == clock_stub_align(CLOCK_STUB_TIME));
    CHECK(clock_stub_x_ofs(CLOCK_STUB_GHOST) == clock_stub_x_ofs(CLOCK_STUB_TIME));
    CHECK(clock_stub_y_ofs(CLOCK_STUB_GHOST) == clock_stub_y_ofs(CLOCK_STUB_TIME));
    /* El valor conserva la imagen del panel con opacidad TRANSP: no hay un
     * segundo bitmap y `_opaque` (tools/ab_testing/quality/contracts.yaml,
     * UI-017) sigue leyendo el apilado fantasma/valor como diseno. */
    CHECK(clock_stub_bg_img_src(CLOCK_STUB_TIME) != NULL);
    CHECK(clock_stub_bg_img_src(CLOCK_STUB_TIME) ==
          clock_stub_bg_img_src(CLOCK_STUB_CONTAINER));
    CHECK(clock_stub_bg_img_opa(CLOCK_STUB_TIME) == LV_OPA_TRANSP);
    /* Los dos timers de siempre (show_time 500 ms y time-sync 200 ms): ninguno
     * nuevo (contrato 5). */
    CHECK(clock_stub_timer_count() == 2);

    /* ---- 3. Formato 24h y hora siempre visible ---- */
    clock_stub_set_wall_clock(12, 34, 56);
    const clock_view_t views[] = {CLOCK_TIME_ALLWAYS, CLOCK_TIME_POWER, CLOCK_POWER_ALLWAYS};

    for (size_t i = 0; i < sizeof(views) / sizeof(views[0]); i++) {
        params.clock_view = views[i];

        clock_stub_set_radio_state(RADIO_RX);
        clock_stub_run_timers();
        CHECK(strcmp(clock_stub_time_text(), "12:34:56") == 0);

        clock_stub_set_radio_state(RADIO_TX);
        clock_stub_run_timers();
        CHECK(strcmp(clock_stub_time_text(), "12:34:56") == 0);
    }

    clock_stub_set_wall_clock(7, 5, 9);
    clock_stub_run_timers();
    CHECK(strcmp(clock_stub_time_text(), "07:05:09") == 0);

    clock_stub_set_wall_clock(23, 59, 59);
    clock_stub_run_timers();
    CHECK(strcmp(clock_stub_time_text(), "23:59:59") == 0);

    /* Reloj del sistema en fallo: nunca una hora ficticia ni un crash. */
    clock_stub_set_localtime_null(true);
    clock_stub_run_timers();
    CHECK(strcmp(clock_stub_time_text(), "--:--:--") == 0);

    clock_stub_set_localtime_null(false);
    clock_stub_run_timers();
    CHECK(strcmp(clock_stub_time_text(), "23:59:59") == 0);

    /* ---- 6. Los setters exportados siguen existiendo y no alternan nada ---- */
    clock_set_view(CLOCK_POWER_ALLWAYS);
    clock_set_time_timeout(5);
    clock_set_power_timeout(7);
    clock_set_tx_timeout(3);
    clock_stub_run_timers();
    CHECK(strcmp(clock_stub_time_text(), "23:59:59") == 0);

    /* ---- 4. Sin telemetria: --% y el icono dim, nunca un 0% inventado ---- */
    render(0.0f, 0.0f, 0, false);
    CHECK(strcmp(clock_stub_bat_pct_text(), "--%") == 0);
    CHECK(strcmp(clock_stub_bat_icon_text(), LV_SYMBOL_BATTERY_EMPTY) == 0);
    CHECK(same_color(clock_stub_color(CLOCK_STUB_ICON), TC(text_dim)));
    CHECK(same_color(clock_stub_color(CLOCK_STUB_PCT), TC(text_dim)));
    CHECK(clock_stub_hidden(CLOCK_STUB_EXT));

    /* Las otras formas de muestra invalida: tension no finita, cero y capacidad
     * fuera de rango. */
    render(0.0f, nanf(""), 50, false);
    CHECK(strcmp(clock_stub_bat_pct_text(), "--%") == 0);
    render(0.0f, INFINITY, 50, false);
    CHECK(strcmp(clock_stub_bat_pct_text(), "--%") == 0);
    render(0.0f, 0.0f, 50, false);
    CHECK(strcmp(clock_stub_bat_pct_text(), "--%") == 0);
    render(0.0f, 12.1f, 101, false);
    CHECK(strcmp(clock_stub_bat_pct_text(), "--%") == 0);
    CHECK(strcmp(clock_stub_bat_icon_text(), LV_SYMBOL_BATTERY_EMPTY) == 0);

    /* ---- 4. Muestra valida: el porcentaje exacto y el nivel del icono ---- */
    render(0.0f, 12.1f, 82, false);
    CHECK(strcmp(clock_stub_bat_pct_text(), "82%") == 0);
    CHECK(strcmp(clock_stub_bat_icon_text(), LV_SYMBOL_BATTERY_3) == 0);
    CHECK(same_color(clock_stub_color(CLOCK_STUB_ICON), TC(text)));
    CHECK(same_color(clock_stub_color(CLOCK_STUB_PCT), TC(text)));

    /* Un 0% real (con v_bat valida) SI se muestra: la falta de telemetria es
     * otra cosa. */
    render(0.0f, 11.0f, 0, false);
    CHECK(strcmp(clock_stub_bat_pct_text(), "0%") == 0);
    CHECK(strcmp(clock_stub_bat_icon_text(), LV_SYMBOL_BATTERY_EMPTY) == 0);

    /* La tabla del icono (0..10 vacio, 11..35 nivel 1, 36..60 nivel 2,
     * 61..85 nivel 3, 86..100 completo) en sus bordes. */
    const struct {
        uint8_t      cap;
        const char * symbol;
    } levels[] = {
        {10, LV_SYMBOL_BATTERY_EMPTY}, {11, LV_SYMBOL_BATTERY_1},
        {35, LV_SYMBOL_BATTERY_1},     {36, LV_SYMBOL_BATTERY_2},
        {60, LV_SYMBOL_BATTERY_2},     {61, LV_SYMBOL_BATTERY_3},
        {85, LV_SYMBOL_BATTERY_3},     {86, LV_SYMBOL_BATTERY_FULL},
        {100, LV_SYMBOL_BATTERY_FULL},
    };

    for (size_t i = 0; i < sizeof(levels) / sizeof(levels[0]); i++) {
        char expected[8];

        snprintf(expected, sizeof(expected), "%u%%", (unsigned) levels[i].cap);
        render(0.0f, 12.1f, levels[i].cap, false);
        CHECK(strcmp(clock_stub_bat_icon_text(), levels[i].symbol) == 0);
        CHECK(strcmp(clock_stub_bat_pct_text(), expected) == 0);
    }

    /* ---- 3. Alimentacion externa: plug + %.1f V, omitida sin fuente ---- */
    render(13.8f, 12.1f, 76, false);
    CHECK(!clock_stub_hidden(CLOCK_STUB_EXT));
    CHECK(strcmp(clock_stub_ext_text(), SYMBOL_PLUG " 13.8 V") == 0);

    /* Cargando y con muestra valida: el simbolo de carga que ya existe. */
    render(13.8f, 12.1f, 76, true);
    CHECK(strcmp(clock_stub_ext_text(), SYMBOL_PLUG_CHARGE " 13.8 V") == 0);

    /* Sin fuente externa (o con una no finita) la linea no se dibuja. */
    render(2.9f, 12.1f, 76, false);
    CHECK(clock_stub_hidden(CLOCK_STUB_EXT));
    render(nanf(""), 12.1f, 76, false);
    CHECK(clock_stub_hidden(CLOCK_STUB_EXT));
    /* El umbral del contrato: 3.0 V ya es fuente externa. */
    render(3.0f, 12.1f, 76, false);
    CHECK(!clock_stub_hidden(CLOCK_STUB_EXT));
    CHECK(strcmp(clock_stub_ext_text(), SYMBOL_PLUG " 3.0 V") == 0);

    /* Con el pack sin muestra valida de bateria no se anuncia carga. */
    render(13.8f, 0.0f, 0, true);
    CHECK(strcmp(clock_stub_ext_text(), SYMBOL_PLUG " 13.8 V") == 0);

    /* ---- 5. OPT-4 #150: 100 ticks identicos no escriben ni invalidan, ni
     * re-anclan el icono de bateria (align_to = layout de la base: el costo por
     * tick tiene que ser cero; PR #279, gate de Performance de x6200-ab) ---- */
    render(13.8f, 12.1f, 76, false);

    int text_calls = clock_stub_label_set_calls();
    int font_calls = clock_stub_style_font_calls();
    int color_calls = clock_stub_style_color_calls();
    int bg_calls = clock_stub_style_bg_calls();
    int align_calls = clock_stub_align_to_calls();

    for (int i = 0; i < 100; i++) {
        clock_stub_run_timers();
    }

    CHECK(clock_stub_label_set_calls() == text_calls);
    CHECK(clock_stub_style_font_calls() == font_calls);
    CHECK(clock_stub_style_color_calls() == color_calls);
    CHECK(clock_stub_style_bg_calls() == bg_calls);
    CHECK(clock_stub_align_to_calls() == align_calls);

    /* Un cambio real si escribe el porcentaje y su icono, y RE-ANCLA el icono una
     * vez (el ancho del porcentaje cambio: es cuando la geometria se invalida). */
    render(13.8f, 12.1f, 20, false);
    CHECK(clock_stub_label_set_calls() == text_calls + 2);
    CHECK(strcmp(clock_stub_bat_pct_text(), "20%") == 0);
    CHECK(clock_stub_align_to_calls() == align_calls + 1);

    /* Y un segundo nuevo escribe la hora UNA vez, pero NO re-ancla: el porcentaje
     * no cambio, asi que la geometria del icono sigue valida (show_time corre
     * cada 500 ms; con la hora fija, los dos ticks dicen lo mismo y el segundo no
     * escribe). */
    text_calls = clock_stub_label_set_calls();
    clock_stub_set_wall_clock(13, 0, 0);
    clock_stub_run_timers();
    clock_stub_run_timers();
    CHECK(strcmp(clock_stub_time_text(), "13:00:00") == 0);
    CHECK(clock_stub_label_set_calls() == text_calls + 1);
    CHECK(clock_stub_align_to_calls() == align_calls + 1);

    /* ---- 5. Un solo snapshot del mutex por actualizacion: las caras de la
     * linea de estado son coherentes con el ULTIMO pack ---- */
    clock_update_power(13.8f, 12.1f, 40, false);
    clock_stub_run_timers();
    CHECK(strcmp(clock_stub_bat_pct_text(), "40%") == 0);
    CHECK(strcmp(clock_stub_ext_text(), SYMBOL_PLUG " 13.8 V") == 0);

    /* ---- Compatibilidad de la API (issue #121): clock_get_power y el aviso
     * verbal siguen leyendo el ultimo pack ---- */
    clock_power_t power;

    CHECK(clock_get_power(&power));
    CHECK(power.ext_v == 13.8f);
    CHECK(power.bat_v == 12.1f);
    CHECK(power.cap == 40);
    CHECK(!power.charging);
    CHECK(power.valid);

    clock_update_power(0.0f, 0.0f, 0, false);
    CHECK(!clock_get_power(&power));
    CHECK(!clock_get_power(NULL));

    clock_say_bat_info(); /* smoke: no crash */

    /* ---- 3. Factores de forma de la linea de estado ---- */
    CHECK(clock_stub_font(CLOCK_STUB_EXT) == &sony_20);
    CHECK(clock_stub_font(CLOCK_STUB_ICON) == &sony_20);
    CHECK(clock_stub_font(CLOCK_STUB_PCT) == &sony_16);
    CHECK(clock_stub_align(CLOCK_STUB_EXT) == LV_ALIGN_BOTTOM_LEFT);
    CHECK(clock_stub_align(CLOCK_STUB_PCT) == LV_ALIGN_BOTTOM_RIGHT);
    CHECK(clock_stub_aligned_to(CLOCK_STUB_ICON));
    CHECK(clock_stub_align_base(CLOCK_STUB_ICON) == CLOCK_STUB_PCT);
    CHECK(clock_stub_align(CLOCK_STUB_ICON) == LV_ALIGN_OUT_LEFT_BOTTOM);

    /* El icono de bateria arranca con un glifo de la familia (nunca ASCII): el
     * defecto UI-009 no puede volver por un puntero sin inicializar. */
    CHECK(starts_with(clock_stub_bat_icon_text(), "\xEF\x89"));
}
