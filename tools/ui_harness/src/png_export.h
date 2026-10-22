/*
 *  PNG export for the host UI harness (see display.h for the framebuffer).
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Writes the framebuffer to `path` as an 8-bit RGB PNG. Returns false and
 * leaves a message on stderr when it cannot. */
bool png_export(const char *path);

/* FNV-1a hash of the raw framebuffer bytes, so two runs can be compared
 * without reading the PNG. */
uint64_t png_framebuffer_hash(void);
