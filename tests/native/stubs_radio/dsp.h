/* Minimal dsp.h stub for the radio_thread test (issue #219). SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdbool.h>
#include <stdint.h>

void dsp_samples(float *buf_samples, uint16_t size, bool tx, int16_t dbm);
void dsp_reset(void);
