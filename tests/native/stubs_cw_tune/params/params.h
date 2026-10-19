/*
 * params/params.h stub for the native cw_tune_ui test (issue #131). The real
 * header pulls in cfg/cfg.h and radio.h (for the mode enum); the stub keeps that
 * include chain. src/cw_tune_ui.c no longer reads params at runtime.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../cfg/cfg.h"
#include "../radio.h"
