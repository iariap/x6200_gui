/*
 *  Scenario files for the host UI harness.
 *
 *  A scenario is a small text file of `key value` lines that fixes the radio
 *  state and names the screen to build. Fixtures live in fixtures/. Nothing is
 *  read from the environment: the same file always yields the same screen.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define SCENARIO_NAME_MAX   64
#define SCENARIO_SCREEN_MAX 32
#define SCENARIO_THEME_MAX  32
#define SCENARIO_DIGITS_MAX 24

/* Benchmark defaults; a fixture (or the CLI) may override them. See
 * bench_metrics.h for what each frame measures. */
#define SCENARIO_BENCH_WARMUP_DEFAULT 3
#define SCENARIO_BENCH_REPEATS_DEFAULT 5
#define SCENARIO_BENCH_REDRAW_PASSES_DEFAULT 1

typedef struct {
    char name[SCENARIO_NAME_MAX];
    char screen[SCENARIO_SCREEN_MAX];
    /* The theme to build, as the fixture spells it. Kept whole: it is the name
     * `theme_list()` offers (up to 31 characters) and the one the store keeps,
     * `params_str_t.theme_name.x` (32 bytes, 31 useful — before UI-013 it was 16
     * and "Outdoor-Negative" did not fit); theme_init() gets the full name. */
    char theme[SCENARIO_THEME_MAX];
    /* The segmented-digit family the frequency row and the clock draw with, as the
     * fixture spells it (a FONT_DIGIT_* id of src/fonts/font_families.h). Empty or
     * "none" keeps the default family. */
    char digits[SCENARIO_DIGITS_MAX];
    /* Deterministic seed for the synthetic spectrum/waterfall data. */
    uint32_t seed;
    /* Benchmark-only knobs. Ignored outside `--bench`, so they never change a
     * rendered pixel. */
    int bench_warmup;
    int bench_repeats;
    int bench_redraw_passes;
    /* Natural-bench-only knob: natural frames the run does. Ignored outside
     * `--bench-natural`, so it never changes a rendered pixel. */
    int natural_frames;
    /* The APP page an `apps-menu` fixture opens (1..6, the pages of
     * `buttons_group_app`). 0 means the first page: it selects which page of the
     * app menu the fixture renders, never a pixel of the page itself. */
    int apps_page;
} scenario_t;

/* Parses `path`. Unknown keys are an error (a scenario must not silently
 * ignore a value). Returns false and prints the reason to stderr. */
bool scenario_load(const char *path, scenario_t *out);

/* Builds the named screen. Returns false when the name is unknown. */
bool scenario_build(const scenario_t *scenario);

/* Fixture work that has to run after the virtual clock has advanced and the
 * components' timers have fired (e.g. moving the keyboard focus). Never changes
 * a pixel on its own. */
void scenario_settle(const scenario_t *scenario);

/* Pantalla apagada por la ruta de la ROM: la tecla POWER suelta
 * (src/keypad.c -> lv_event_send(lv_scr_act(), EVENT_KEYPAD) ->
 * main_screen_keypad_cb -> backlight_switch(), src/main_screen.c). El callback y
 * el modulo del backlight son los reales (src/backlight.c se compila en el
 * arnes); lo que el host sustituye es la tecla, igual que sustituye el DSP. La
 * pide el CLI (`--display-off`), no un fixture: el estado no es una pantalla que
 * el corpus tenga que cargar. Se llama despues de que la pantalla ya se dibujo,
 * que es cuando el usuario aprieta la tecla: el panel conserva el ultimo frame y
 * el backlight queda apagado. */
void scenario_display_off(void);
