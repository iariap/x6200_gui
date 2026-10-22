/*
 * Minimal pubsub_ids.h for the WiFi idle-pump native stage (issue #218, F-06).
 * Mismos ids que src/pubsub_ids.h (el enum arranca en MSG_PARAM_CHANGED); el stage usa
 * MSG_WIFI_STATE_CHANGED para contar los avisos al bus que el modulo ya enviaba.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

enum {
    MSG_PARAM_CHANGED,
    MSG_WIFI_STATE_CHANGED,
    MSG_USB_DEVICE_CHANGED,
};
