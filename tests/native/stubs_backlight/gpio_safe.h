/*
 * Minimal gpio_safe.h for the native backlight test.
 *
 * src/backlight.c only calls gpio_safe_set() (the buttons light); the real
 * header is a thinner include, but the stub keeps the same signature so the
 * stub implementation is checked against it.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

void gpio_safe_set(int pin, int value);
