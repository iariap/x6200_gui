#include <stdio.h>
#include <math.h>
#include <complex.h>
#include <liquid/liquid.h>

#define FFT 128
int main(void) {
    float complex x[FFT], y[FFT];
    fftplan p = fft_create_plan(FFT, x, y, LIQUID_FFT_FORWARD, 0);
    for (int k = -3; k <= 3; k++) {
        if (k == 0) continue;
        float hz = k * 100.0f;
        float f = hz / 689.0625f; /* bin offset in cycles/sample */
        for (int i = 0; i < FFT; i++) x[i] = cexpf(_Complex_I * 2.0f * (float)M_PI * f * i);
        fft_execute(p);
        size_t max_pos = 0; float max = -1e30f;
        for (int i = 0; i < FFT; i++) { float v = crealf(y[i]*conjf(y[i])); if (v > max) { max = v; max_pos = i; } }
        float peak = (((float)(FFT - (max_pos + FFT/2) % FFT) / FFT) - 0.5f) * (44100.0f/64.0f);
        printf("tone %+7.1f Hz -> bin %3zu -> formula %+8.2f Hz\n", hz, max_pos, peak);
    }
    fft_destroy_plan(p);
    return 0;
}
