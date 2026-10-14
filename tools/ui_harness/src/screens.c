/*
 *  Screen builders for the host UI harness.
 *
 *  The `main` screen is the firmware's own main_screen() (src/main_screen.c):
 *  it places the real spectrum, waterfall, frequency row, band plan, buttons,
 *  panel, message bars, clock, info, meter and TX info. The radio/DSP state
 *  comes from the explicit mocks; the spectrum/waterfall magnitudes come from
 *  the deterministic generator (src/signal_gen.c), because there is no SDR.
 *
 *  `dialog-about` is the same screen with the real About window opened through
 *  the firmware's own path (main_screen_action(ACTION_APP_ABOUT), src/dialog.c).
 *  `main-foco` is the same screen with the real keyboard group, so the focus
 *  chain the firmware builds (main_screen registers its spectrum in the group)
 *  can be exercised with lv_group_focus_next().
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scenario.h"

#include "lvgl/lvgl.h"

#include "buttons.h"
#include "clock.h"
#include "dialog.h"
#include "dialog_settings.h"
#include "keyboard.h"
#include "main.h"
#include "main_screen.h"
#include "meter.h"
#include "params/params.h"
#include "signal_gen.h"
#include "styles.h"
#include "visual_policy.h"

#include "mock_harness.h"
#include "mock_radio.h"

/* Voltios del fixture -> mV de la politica visual, redondeado: el binario de
 * float daria 11199 para 11.2 V si se truncara. */
static uint16_t volts_to_mv(float volts)
{
    return (uint16_t) (volts * 1000.0f + 0.5f);
}

/* The box the object actually shows on screen: its coordinates intersected with
 * the clipping of its ancestors (a parent without LV_OBJ_FLAG_OVERFLOW_VISIBLE
 * clips its children, exactly as lv_refr.c does) and with the viewport. An object
 * scrolled out of a scroll area has coordinates inside the screen but shows
 * nothing: the intersection is what tells the two apart. */
static bool visible_on_screen(lv_obj_t *obj)
{
    lv_area_t area;

    lv_obj_get_coords(obj, &area);

    for (lv_obj_t *parent = lv_obj_get_parent(obj); parent != NULL;
         parent = lv_obj_get_parent(parent)) {
        if (lv_obj_has_flag(parent, LV_OBJ_FLAG_HIDDEN)) {
            return false;
        }
        if (!lv_obj_has_flag(parent, LV_OBJ_FLAG_OVERFLOW_VISIBLE)) {
            lv_area_t parent_area;

            lv_obj_get_coords(parent, &parent_area);
            if (!_lv_area_intersect(&area, &area, &parent_area)) {
                return false;
            }
        }
    }

    lv_area_t viewport = { 0, 0, lv_disp_get_hor_res(NULL) - 1, lv_disp_get_ver_res(NULL) - 1 };

    return _lv_area_intersect(&area, &area, &viewport);
}

/* Is a label with this exact text visible on screen? Used to codify what a
 * fixture claims to show (see scenario_settle): the harness stops if the row is
 * not visible, instead of capturing another group and reporting a green run. */
static bool label_on_screen(lv_obj_t *obj, const char *text)
{
    if (lv_obj_check_type(obj, &lv_label_class) && !lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
        const char *obj_text = lv_label_get_text(obj);

        if (obj_text != NULL && strcmp(obj_text, text) == 0 && visible_on_screen(obj)) {
            return true;
        }
    }

    uint32_t children = lv_obj_get_child_cnt(obj);

    for (uint32_t i = 0; i < children; i++) {
        if (label_on_screen(lv_obj_get_child(obj, (int32_t) i), text)) {
            return true;
        }
    }

    return false;
}

static void build_main(const scenario_t *scenario)
{
    /* Push the scenario's radio state into the subjects before the components
     * read them (the firmware's radio module does this from the driver). */
    mock_radio_apply();

    /* The firmware's real main screen: loads the real components. */
    lv_obj_t *scr = main_screen();

    lv_scr_load(scr);

    /* Values the radio would report that main_screen() itself does not push:
     * the battery the clock draws and the signal the needle points at. */
    clock_update_power(mock_radio.external_v, mock_radio.battery_v,
                       mock_radio.battery_cap, mock_radio.charging);

    /* La politica visual (UI-08 incr. 2 y 3) sale del MISMO estado del pack que
     * el reloj: asi el banco miente el perfil igual que la radio (el ROM lo hace
     * en radio_tick, src/radio.c) y `--bench-natural` mide la cadencia que el
     * modo Ahorro del waterfall impone. Una sola evaluacion, con muestra
     * fresca (age 0): el fixture no tiene reloj. La aplicacion va por las mismas
     * funciones que el ROM usa, incluido el recorte de brillo: en el arnes el
     * backlight es un stub sin senal (mocks/mock_ui_deps.c), asi que el nivel no
     * toca un solo pixel y el banco sigue siendo determinista. */
    visual_policy_t        policy;
    visual_policy_result_t visual;

    visual_policy_init(&policy);
    visual = visual_policy_eval(&policy,
                                volts_to_mv(mock_radio.battery_v),
                                volts_to_mv(mock_radio.external_v),
                                mock_radio.battery_cap, mock_radio.charging,
                                0u, 0u, VP_AUTO);
    visual_policy_waterfall_apply(visual.profile, params.waterfall_battery.x);
    visual_policy_brightness_apply(visual.profile, params.brightness_normal);

    meter_update(mock_radio.smeter_db, 0.0f);

    /* Feed the components the FFT would: deterministic, seed-driven. */
    signal_gen_feed(scenario->seed);
}

bool scenario_build(const scenario_t *scenario)
{
    if (!strcmp(scenario->screen, "main") || !strcmp(scenario->screen, "home-usb-20m")) {
        build_main(scenario);
        return true;
    }

    if (!strcmp(scenario->screen, "dialog-about")) {
        /* The firmware's own path: the About action builds the real window over
         * the main screen (src/main_screen.c -> src/dialog.c -> dialog_about.c). */
        build_main(scenario);
        main_screen_action(ACTION_APP_ABOUT);
        return true;
    }

    if (!strcmp(scenario->screen, "main-foco")) {
        /* The real keyboard group has to exist before main_screen() arms its
         * 100 ms timer, so the spectrum ends up registered when it fires. */
        mock_keyboard_group_up();
        build_main(scenario);
        return true;
    }

    if (!strcmp(scenario->screen, "settings-home") || !strcmp(scenario->screen, "settings-wfbat")) {
        /* Ajustes on top of the real main screen, opened by the firmware's own
         * path: main_screen_action(ACTION_APP_SETTINGS) -> main_screen_start_app()
         * -> dialog_construct(dialog_settings) (src/main_screen.c -> src/dialog.c
         * -> src/dialog_settings.c). The keyboard group is up first so
         * dialog_item() registers every widget and settings_goto() can move the
         * focus, exactly as in the radio. */
        mock_keyboard_group_up();
        build_main(scenario);
        main_screen_action(ACTION_APP_SETTINGS);
        return true;
    }

    if (!strcmp(scenario->screen, "settings-bands")) {
        /* Ajustes over the main screen, then the real Bands action run from there:
         * main_screen_action(ACTION_APP_BANDS), the dispatcher the Ajustes
         * long-press list (`APP Bands`, src/dialog_settings.c) reaches. The
         * keyboard group is up first because the Bands route goes through
         * dialog_construct()/dialog_init(), the same as every other app.
         *
         * `dialog_bands` is an inert mock on the host (mocks/mock_main_screen.c):
         * the Bands window is not rendered, so this fixture measures the ROUTE
         * (scenario_settle codifies what it must leave behind), never its pixels.
         * The app-menu button the user reported is `app-bands`. */
        mock_keyboard_group_up();
        build_main(scenario);
        main_screen_action(ACTION_APP_SETTINGS);
        main_screen_action(ACTION_APP_BANDS);
        return true;
    }

    if (!strcmp(scenario->screen, "app-bands")) {
        /* The route the owner reported: the APP key loads the app pages
         * (src/main_screen.c, KEYPAD_APP -> buttons_load_page_group(buttons_group_app))
         * and the "Bands" button of page 3 (APP 3:6) is pressed. The page
         * presses are positional and happen in scenario_settle, after the same
         * "is the button I am about to press really Bands?" check the
         * settings-wfbat fixture uses.
         *
         * `dialog_bands` is an inert mock here too: this fixture measures whether
         * the button reaches the Bands path at all (scenario_settle), which is
         * exactly what did not happen. */
        mock_keyboard_group_up();
        build_main(scenario);
        buttons_load_page_group(buttons_group_app);
        buttons_press(0, false);    /* (APP 1:6) -> (APP 2:6) */
        buttons_press(0, false);    /* (APP 2:6) -> (APP 3:6) */
        return true;
    }

    fprintf(stderr, "scenario: unknown screen '%s'\n", scenario->screen);
    return false;
}

void scenario_settle(const scenario_t *scenario)
{
    if (!strcmp(scenario->screen, "main-foco")) {
        /* Move the focus once the timers have run and the group is populated. */
        lv_group_focus_next(keyboard_group);
    }

    if (!strcmp(scenario->screen, "settings-wfbat")) {
        /* Jump to the Spectrum/waterfall group the way the user does: the
         * bottom bar's "Mas >" loads page 2 and its "Espectro" tab calls
         * settings_goto(), which scrolls the grid and moves the focus
         * (src/dialog_settings.c). Runs after the timers so the grid layout is
         * already computed; both callbacks request LV_ANIM_OFF, so the resulting
         * scroll is immediate and deterministic. */
        buttons_press(4, false);    /* page 1, index 4: "Mas >"  */
        buttons_press(0, false);    /* page 2, index 0: "Espectro" */

        /* The presses are positional, and what this fixture *means* is that the
         * Spectrum/waterfall group's row is the one on screen now. Codified
         * instead of commented: if the bottom bar's tabs are ever reordered the
         * harness stops here instead of capturing a different group and still
         * printing a green determinism run. */
        if (!label_on_screen(lv_scr_act(), "Waterfall battery mode")) {
            fprintf(stderr,
                    "settings-wfbat: 'Waterfall battery mode' is not on screen after the "
                    "tab presses (did the bottom bar's tabs move?)\n");
            exit(1);
        }
    }

    if (!strcmp(scenario->screen, "settings-bands")) {
        /* What this fixture *means* is that the Bands action replaced the window
         * that was open and reached the Bands path — the two things the route
         * change fixes. Codified instead of commented, and checked on the
         * firmware's own state, because `dialog_bands` is an inert mock here: if
         * the action ever goes back to constructing the window on top of the
         * current one (the Ajustes window left running, `current_dialog`
         * overwritten and the Ajustes object orphaned) or reaches no dialog at
         * all (the app-menu button the user reported), the harness stops here
         * instead of capturing a green run.
         *
         * `dialog_settings` is the real descriptor (src/dialog_settings.c is
         * compiled); `run` stays true until the window's object is deleted. */
        if (dialog_settings->run) {
            fprintf(stderr,
                    "settings-bands: the Bands action left the Ajustes window running "
                    "(it was not closed before the Bands path opened)\n");
            exit(1);
        }
        if (!dialog_is_run()) {
            fprintf(stderr, "settings-bands: the Bands action reached no dialog at all\n");
            exit(1);
        }
    }

    if (!strcmp(scenario->screen, "app-bands")) {
        /* The button this fixture presses is positional: codify that it is the
         * "Bands" one before pressing it, so a reordering of the APP pages stops
         * the run instead of pressing another app and still reporting green. */
        if (!label_on_screen(lv_scr_act(), "Bands")) {
            fprintf(stderr,
                    "app-bands: 'Bands' is not in the bottom bar after the page presses "
                    "(did the APP pages move?)\n");
            exit(1);
        }

        buttons_press(4, false);    /* (APP 3:6) index 4: "Bands" */

        /* What this fixture *means*: pressing the app-menu Bands button has to
         * reach the Bands path. With the window itself an inert mock, the
         * firmware's own state is the observable: a dialog has to be running. */
        if (!dialog_is_run()) {
            fprintf(stderr,
                    "app-bands: the app-menu Bands button reached no dialog at all "
                    "(the button did nothing)\n");
            exit(1);
        }
    }
}
