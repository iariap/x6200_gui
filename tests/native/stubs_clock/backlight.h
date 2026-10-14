/*
 * Minimal backlight.h for the native clock test. On the target it is the header
 * that brings the params declaration into src/clock.c, so the stub keeps that
 * include and nothing else (clock.c does not call backlight_*).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "params/params.h"
