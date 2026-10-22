/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Bluetooth device list.
 *
 *  Parses what btaudio-ctl prints, so the Settings app can show it. No UI and no
 *  LVGL here: the parsing is unit tested on the host.
 *
 *  btaudio-ctl list:
 *      DEV <mac> <connected> <role> <autoconnect> <name>
 *      HP  <1|0> <volume %>
 *      RX  <on|off>
 *      MUTE <on|off>
 *  btaudio-ctl scan (the devices that are not paired yet):
 *      NEW <mac> <name> <role>
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BT_MAX_DEV      16
#define BT_NAME_LEN     48
#define BT_MAC_LEN      20
#define BT_ROLE_LEN     8

typedef struct {
    char     mac[BT_MAC_LEN];
    char     name[BT_NAME_LEN];
    char     role[BT_ROLE_LEN];     /* "hp", "kbd" or "cat" */
    bool     connected;
    bool     autoconnect;
    bool     paired;
} bt_device_t;

typedef struct {
    bt_device_t dev[BT_MAX_DEV];
    int         n;

    bool        hp;                 /* headphones sink present */
    int         hp_volume;          /* -1 unknown */
    bool        rx;                 /* RX audio going to them */
    bool        mute;               /* speaker muted while on BT */
} bt_list_t;

/* Fills out from the text; returns the number of devices found. Lines that are
 * not recognised are ignored, so an error message from the script is harmless. */
int bt_devices_parse(const char *text, bt_list_t *out);

/* The row to draw: "name", "role" and a state word */
const char *bt_device_state(const bt_device_t *d);
