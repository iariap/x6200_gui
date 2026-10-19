/*
 * meter.h stub for the native spectrum test (issue #149).
 *
 * The S-scale macros are copied verbatim from src/meter.h (the firmware's
 * reference): the test builds its expected pixel positions from the same
 * numbers the module uses, and a change in the scale has to show up here.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#define S_MIN   (-127)
#define S4      (-103)
#define S8      (-79)
#define S9      (-73)
#define S9_20   (S9 + 20)
