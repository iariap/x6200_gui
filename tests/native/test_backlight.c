/*
 * Native test for the increment 4 enforcement wire in src/backlight.c.
 *
 * The module is compiled here for real (see tests/native/run_native_tests.sh:
 * src/backlight.c is staged next to the stub headers under
 * tests/native/stubs_backlight/, the clock-test pattern) because no other gate
 * touches it: the UI harness mocks backlight (tools/ui_harness/mocks/
 * mock_ui_deps.c) and the host has no aether SDK.
 *
 * The observation point is the policy query of the increment 4:
 * visual_policy_brightness_rewrite() (tests/native/backlight_stubs.c records
 * it). The test drives the two rewrites that used to bypass the cap:
 *
 *   - backlight_tick() (input) has to hand it params.brightness_normal;
 *   - the idle timer has to hand it params.brightness_idle;
 *   - the OFF/ON switch, on the way back on, has to hand it
 *     params.brightness_normal (increment 5: the cap survives the toggle).
 *
 * Reverting either line to a direct backlight_set_brightness() call leaves the
 * recorder empty and this test red: that is the anti-false-green of the
 * enforcement (the clamp itself, and that both sides yield the same number, is
 * covered in tests/native/test_visual_policy.c). The params setting is only
 * read: it must come out of the rewrites untouched.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "test_framework.h"

#include "backlight.h"
#include "params/params.h"

/* Registro del stub de la politica (tests/native/backlight_stubs.c). */
void    test_rewrite_reset(void);
int     test_rewrite_calls(void);
int16_t test_rewrite_last(void);
bool    test_rewrite_last_valid(void);

/* Registro de lv_disp_enable_invalidation() (tests/native/backlight_stubs.c):
 * la ruta de pantalla apagada. */
void test_invalidation_reset(void);
int  test_invalidation_calls(void);
bool test_invalidation_last(void);

/* Dispara el callback de idle que backlight_tick() registro. */
void test_backlight_run_idle_timer(void);

void test_backlight(void) {
    /* Los settings del usuario como en la radio (params.c: normal 9, idle 1). */
    params.brightness_normal = 9;
    params.brightness_idle = 1;
    params.brightness_timeout = 10;
    params.brightness_buttons = BUTTONS_DARK;

    /* --- Tick por input: no hay timer, el modulo reescribe el nivel normal. El
     * nivel sale de params y va por la politica (el cap lo aplica ella). --- */
    test_rewrite_reset();
    backlight_tick();
    CHECK(test_rewrite_calls() == 1);
    CHECK(test_rewrite_last_valid());
    CHECK(test_rewrite_last() == params.brightness_normal);

    /* El setting del usuario no se escribe: sigue en params. */
    CHECK(params.brightness_normal == 9);
    CHECK(params.brightness_idle == 1);

    /* --- Mientras el timer vive, un tick solo lo resetea: no hay rewrite (el
     * nivel normal ya esta puesto y el cap no se re-aplica de mas). --- */
    backlight_tick();
    CHECK(test_rewrite_calls() == 1);

    /* --- Timeout idle: el callback registrado baja al nivel de reposo y el
     * rewrite tambien pasa por la politica. --- */
    test_rewrite_reset();
    test_backlight_run_idle_timer();
    CHECK(test_rewrite_calls() == 1);
    CHECK(test_rewrite_last_valid());
    CHECK(test_rewrite_last() == params.brightness_idle);

    /* --- Tras el idle, el proximo input vuelve a reescribir el normal: el otro
     * camino, con el mismo cap vigente. --- */
    test_rewrite_reset();
    backlight_tick();
    CHECK(test_rewrite_calls() == 1);
    CHECK(test_rewrite_last() == params.brightness_normal);

    /* Ningun rewrite toco los settings. */
    CHECK(params.brightness_normal == 9);
    CHECK(params.brightness_idle == 1);

    /* --- Switch OFF/ON (UI-08 incr. 5): al volver a encender, el nivel pasa por
     * la politica (asi el cap del perfil vigente sobrevive al toggle) en vez de
     * un backlight_set_brightness() directo. El apagado no emite luz, asi que no
     * pasa por el cap. Empieza encendido (arranque del modulo). --- */
    test_rewrite_reset();
    test_invalidation_reset();
    backlight_switch();                 /* encendido -> apagado: sin rewrite */
    CHECK(test_rewrite_calls() == 0);

    /* El apagado corta la invalidacion del display (issue #13, UI-07
     * remanente): con el panel oscuro no queda trabajo de render. El banco lo
     * mide con `--bench-natural --display-off`: 0 px invalidados y 0 flush
     * contra los 6745720 px con la pantalla encendida. Si el apagado volviera a
     * dejar la invalidacion prendida, esto queda rojo. */
    CHECK(test_invalidation_calls() == 1);
    CHECK(test_invalidation_last() == false);

    test_rewrite_reset();
    test_invalidation_reset();
    backlight_switch();                 /* apagado -> encendido: un rewrite */
    CHECK(test_rewrite_calls() == 1);
    CHECK(test_rewrite_last_valid());
    CHECK(test_rewrite_last() == params.brightness_normal);

    /* Y al volver la pantalla se vuelve a invalidar. */
    CHECK(test_invalidation_calls() == 1);
    CHECK(test_invalidation_last() == true);

    /* El switch tampoco toca los settings del usuario. */
    CHECK(params.brightness_normal == 9);
    CHECK(params.brightness_idle == 1);
}
