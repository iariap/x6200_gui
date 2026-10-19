/*
 * Minimal voice stub for the native CW encoder test (issue #160).
 *
 * src/cw_encoder.c announces the CB TX interlock refusal by voice, the same as
 * radio_set_ptt() does.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

void voice_say_text_fmt(const char *fmt, ...);
