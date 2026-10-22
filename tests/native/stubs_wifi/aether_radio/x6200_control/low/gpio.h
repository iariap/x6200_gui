/*
 * Minimal aether_radio/x6200_control/low/gpio.h for the WiFi idle-pump native stage
 * (issue #218, F-06). Solo el pin que src/wifi.cpp usa (x6200_pin_wifi); el SDK real vive
 * en tools/ui_harness/third_party y no hace falta para medir el pump. El valor del pin lo
 * fija el fixture (tests/native/wifi_idle_stubs.cpp): el modulo solo lo pasa a gpio_safe_set().
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

extern int x6200_pin_wifi;

#ifdef __cplusplus
}
#endif
