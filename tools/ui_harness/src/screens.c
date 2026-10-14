/*
 *  Screen builders for the host UI harness.
 *
 *  The `main` screen is the firmware's own main_screen() (src/main_screen.c):
 *  it places the real spectrum, waterfall, frequency row, band plan, buttons,
 *  panel, message bars, clock, info, meter and TX info. The radio/DSP state
 *  comes from the explicit mocks; the spectrum/waterfall magnitudes come from
 *  the deterministic generator (src/signal_gen.c), because there is no SDR.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <string.h>

#include "scenario.h"

#include "lvgl/lvgl.h"

#include "clock.h"
#include "main_screen.h"
#include "meter.h"
#include "signal_gen.h"
#include "styles.h"

#include "mock_radio.h"

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

    fprintf(stderr, "scenario: unknown screen '%s'\n", scenario->screen);
    return false;
}
