/* Minimal sql_policy.h stub for the radio_thread test (issue #219). SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <aether_radio/x6200_control/control.h>

void sql_policy_apply(x6200_mode_t mode, uint8_t level);
