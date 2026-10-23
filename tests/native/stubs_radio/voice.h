/* Minimal voice.h stub for the radio_thread test (issue #219): src/radio.c only
 * speaks the TX-block notice. SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

void voice_say_text_fmt(const char *fmt, ...);
