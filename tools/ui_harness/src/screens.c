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
#include <string.h>

#include "scenario.h"

#include "lvgl/lvgl.h"

#include "clock.h"
#include "keyboard.h"
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

    /* La politica visual (UI-08 incr. 2) sale del MISMO estado del pack que el
     * reloj: asi el banco miente el perfil igual que la radio (el ROM lo hace
     * en radio_tick, src/radio.c) y `--bench-natural` mide la cadencia que el
     * modo Ahorro del waterfall impone. Una sola evaluacion, con muestra
     * fresca (age 0): el fixture no tiene reloj. La aplicacion va por la misma
     * funcion que el ROM usa. */
    visual_policy_t        policy;
    visual_policy_result_t visual;

    visual_policy_init(&policy);
    visual = visual_policy_eval(&policy,
                                volts_to_mv(mock_radio.battery_v),
                                volts_to_mv(mock_radio.external_v),
                                mock_radio.battery_cap, mock_radio.charging,
                                0u, 0u, VP_AUTO);
    visual_policy_waterfall_apply(visual.profile, params.waterfall_battery.x);

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

    fprintf(stderr, "scenario: unknown screen '%s'\n", scenario->screen);
    return false;
}

void scenario_settle(const scenario_t *scenario)
{
    if (!strcmp(scenario->screen, "main-foco")) {
        /* Move the focus once the timers have run and the group is populated. */
        lv_group_focus_next(keyboard_group);
    }
}
