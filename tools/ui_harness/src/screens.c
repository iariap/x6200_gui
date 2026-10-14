/*
 *  Screen builders for the host UI harness.
 *
 *  Each builder places real firmware components on the LVGL screen. The
 *  home-usb-20m fixture renders the top bar exactly as src/main_screen.c does:
 *  info_init() top-left, meter_init() top-middle, clock_init() top-right, all
 *  with their real styles. The rest of the main screen (spectrum, waterfall,
 *  frequency strip, band plan, spots, buttons, panels) is not rendered in
 *  milestone 1 and is declared UNSUPPORTED in the coverage table.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <string.h>

#include "scenario.h"

#include "lvgl/lvgl.h"

#include "styles.h"
#include "clock.h"
#include "info.h"
#include "meter.h"

#include "mock_radio.h"

static void build_home_usb_20m(void)
{
    /* Push the scenario's radio state into the subjects before the components
     * read them (the firmware's radio module does this from the driver). */
    mock_radio_apply();

    lv_obj_t *scr = lv_scr_act();

    lv_obj_add_style(scr, &background_style, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Order and positions are the ones the real main_screen() uses; each
     * component's style carries its align and size (styles.c). */
    info_init(scr);
    clock_init(scr);
    meter_init(scr);

    /* Battery shown by the clock and an S9 signal on the needle S-meter. */
    clock_update_power(mock_radio.external_v, mock_radio.battery_v,
                       mock_radio.battery_cap, mock_radio.charging);
    meter_update(mock_radio.smeter_db, 0.0f);
}

bool scenario_build(const scenario_t *scenario)
{
    if (!strcmp(scenario->screen, "home-usb-20m")) {
        build_home_usb_20m();
        return true;
    }

    fprintf(stderr, "scenario: unknown screen '%s'\n", scenario->screen);
    return false;
}
