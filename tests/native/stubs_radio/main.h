/* Minimal main.h stub for the radio_thread test (issue #219): src/radio.c only
 * needs the clean-shutdown marker (issue #121). SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

void lastbreath_mark_clean_now(void);
