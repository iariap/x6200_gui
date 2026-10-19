/*
 * rtty.h stub for the native spectrum test (issue #149): the draw callback asks
 * for the RTTY state (it draws two markers when it is not OFF).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

typedef enum {
    RTTY_OFF = 0,
} rtty_state_t;

rtty_state_t rtty_get_state(void);
