/*
 *  Host mock for the DSP front-end.
 *
 *  On the radio the FFT lives in dsp.cpp and pushes magnitudes into
 *  spectrum_data()/waterfall_data(). The harness does not run the DSP: it feeds
 *  those two functions from a deterministic generator (src/signal_gen.c), so
 *  the only DSP call the rendered components make is the spectrum-beta setter
 *  from mfk.cpp. It is stored and never used, because no FFT runs here.
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "dsp.h"

static float spectrum_beta = 0.5f;

void dsp_set_spectrum_beta(float x)
{
    spectrum_beta = x;
}

float dsp_get_spectrum_beta(void)
{
    return spectrum_beta;
}
