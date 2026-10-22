/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - new release alert
 *
 *  Concept taken from the X6100 firmware by IU1WXY (src/update_check.c),
 *  rewritten here to be UI-free (the caller passes a notify callback, so the
 *  whole thing can be unit tested on the host) and to read GitHub releases
 *  instead of a vendor-specific JSON endpoint. The telemetry "ping" of the
 *  original is deliberately not ported.
 *
 *  Config file: /mnt/update_check.conf (override with $UPDATE_CHECK_CONF)
 *      enabled=1
 *      source=https://api.github.com/repos/<owner>/<repo>/releases/latest
 *
 *  The fetch runs python3 (already in the image for the web configurator);
 *  $UPDATE_CHECK_CMD overrides the whole command, which is what the tests use.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

/* True when version a is newer than b ("v0.112" > "v0.99"). */
bool update_newer(const char *a, const char *b);

/* Pull the release tag out of a GitHub releases JSON body; also accepts a bare
 * version line. Returns false when nothing usable is found. */
bool update_check_parse_tag(const char *body, char *out, size_t out_sz);

/* Fetch the configured source and compare with `current`. Returns true and
 * fills newer_out when a newer release is published. */
bool update_check_run_once(const char *current, char *newer_out, size_t out_sz);

/* Once after start, in its own thread: waits for the network and calls
 * notify(version) if a newer release is published. `current` is the running
 * version (VERSION from main.h). Non-blocking. */
void update_check_start(const char *current, void (*notify)(const char *version));
