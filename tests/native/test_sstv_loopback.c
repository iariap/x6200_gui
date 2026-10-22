/*
 * SSTV TX -> RX loopback on the host: a test picture encoded by src/sstv/encoder.c in every mode the
 * app sends, with noise added, decoded by src/sstv/decoder.c as the app's TEST path does (VIS
 * detection, then the same RAM decoder as live RF), and compared with the picture.
 *
 *   tests/native/run_sstv_tests.sh
 *
 * Exit status 0 when every mode passes.
 */

#include "sstv/decoder.h"
#include "sstv/encoder.h"

#include <math.h>
#include <png.h>
#include <sndfile.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PIC "/tmp/sstv_loopback.png"
#define WAV "/tmp/sstv_loopback.wav"
#define SNR_DB 20.0

typedef int (*enc_fn)(const char *, unsigned, int16_t **, size_t *);

static void write_picture(void) {
    static uint8_t rows[256][320 * 3];
    png_bytep      rp[256];

    for (int y = 0; y < 256; y++) {
        for (int x = 0; x < 320; x++) {
            uint8_t *p = &rows[y][x * 3];
            int      bar = x / 40;                          /* colour bars on top, gradients below */
            if (y < 128) {
                p[0] = bar & 1 ? 255 : 0;
                p[1] = bar & 2 ? 255 : 0;
                p[2] = bar & 4 ? 255 : 0;
            } else {
                p[0] = (uint8_t)(x * 255 / 319);
                p[1] = (uint8_t)((y - 128) * 255 / 127);
                p[2] = (uint8_t)(255 - x * 255 / 319);
            }
        }
        rp[y] = rows[y];
    }
    FILE       *f   = fopen(PIC, "wb");
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop   inf = png_create_info_struct(png);
    png_init_io(png, f);
    png_set_IHDR(png, inf, 320, 256, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, inf);
    png_write_image(png, rp);
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &inf);
    fclose(f);
}

static double gauss(void) {
    double u = (rand() + 1.0) / (RAND_MAX + 2.0), v = (rand() + 1.0) / (RAND_MAX + 2.0);
    return sqrt(-2 * log(u)) * cos(2 * M_PI * v);
}

/* Luma correlation and mean abs error (0-255) of the decoded rows against the picture */
static void compare(const uint8_t *got, const uint8_t *want, int w, int rows, double *corr, double *mae) {
    double sa = 0, sb = 0, saa = 0, sbb = 0, sab = 0, err = 0;
    long   n = (long)w * rows;
    for (long i = 0; i < n; i++) {
        double a = 0.299 * got[i * 3] + 0.587 * got[i * 3 + 1] + 0.114 * got[i * 3 + 2];
        double b = 0.299 * want[i * 3] + 0.587 * want[i * 3 + 1] + 0.114 * want[i * 3 + 2];
        sa += a, sb += b, saa += a * a, sbb += b * b, sab += a * b, err += fabs(a - b);
    }
    double cov = sab / n - sa / n * sb / n, va = saa / n - sa / n * sa / n, vb = sbb / n - sb / n * sb / n;
    *corr = cov / sqrt(va * vb + 1e-9);
    *mae  = err / n;
}

int main(void) {
    static const struct {
        const char *name;
        enc_fn      enc;
        sstv_mode_t mode;
    } modes[] = {
        {"Robot 8 BW", sstv_encode_robot8_png_pcm, SSTV_MODE_ROBOT8_BW},
        {"Robot 36", sstv_encode_robot36_png_pcm, SSTV_MODE_ROBOT36},
        {"Robot 72", sstv_encode_robot72_png_pcm, SSTV_MODE_ROBOT72},
        {"Martin M1", sstv_encode_martin_m1_png_pcm, SSTV_MODE_MARTIN_M1},
        {"Martin M2", sstv_encode_martin_m2_png_pcm, SSTV_MODE_MARTIN_M2},
        {"Scottie S1", sstv_encode_scottie_s1_png_pcm, SSTV_MODE_SCOTTIE_S1},
        {"Scottie S2", sstv_encode_scottie_s2_png_pcm, SSTV_MODE_SCOTTIE_S2},
    };
    static uint8_t rgb[SSTV_MAX_W * SSTV_MAX_H * 3], want[SSTV_MAX_W * SSTV_MAX_H * 3];
    int            fails = 0;

    write_picture();
    srand(7);
    for (unsigned m = 0; m < sizeof(modes) / sizeof(modes[0]); m++) {
        int16_t *pcm = NULL;
        size_t   n   = 0;
        if (modes[m].enc(PIC, SSTV_SAMPLE_RATE, &pcm, &n) != 0 || !n) {
            printf("%-11s encode FAILED\n", modes[m].name);
            fails++;
            continue;
        }
        /* a second of silence before and after, white noise at SNR_DB in the audio band */
        size_t lead = SSTV_SAMPLE_RATE, total = n + 2 * lead;
        float *s    = calloc(total, sizeof(float));
        double pw   = 0;
        for (size_t i = 0; i < n; i++) pw += (double)pcm[i] * pcm[i];
        double sig = sqrt(pw / n) / 32768.0, nz = sig / pow(10, SNR_DB / 20);
        for (size_t i = 0; i < total; i++) {
            double v = (i >= lead && i < lead + n ? pcm[i - lead] / 32768.0 : 0) + nz * gauss();
            s[i]     = (float)(v > 1 ? 1 : v < -1 ? -1 : v);
        }
        free(pcm);
        SF_INFO  si = {.samplerate = SSTV_SAMPLE_RATE, .channels = 1, .format = SF_FORMAT_WAV | SF_FORMAT_PCM_16};
        SNDFILE *sf = sf_open(WAV, SFM_WRITE, &si);
        sf_writef_float(sf, s, (sf_count_t)total);
        sf_close(sf);
        free(s);

        sstv_info_t info = {0};
        memset(rgb, 0, sizeof(rgb));
        int rows = sstv_decode_wav(WAV, rgb, &info);
        if (rows <= 0 || info.mode != modes[m].mode) {
            printf("%-11s decode FAILED: rows %d, mode %s\n", modes[m].name, rows, info.name ? info.name : "?");
            fails++;
            continue;
        }
        sstv_load_png_scaled(PIC, want, info.width, info.height);
        double corr, mae;
        compare(rgb, want, info.width, rows < info.height ? rows : info.height, &corr, &mae);
        bool ok = rows >= info.height * 95 / 100 && corr > 0.90;
        printf("%-11s VIS %3d %dx%d, %d rows, luma correlation %.3f, mean error %.1f: %s\n", modes[m].name, info.vis,
               info.width, info.height, rows, corr, mae, ok ? "OK" : "FAIL");
        fails += !ok;
    }
    printf(fails ? "%d FAILED\n" : "all OK\n", fails);
    return fails != 0;
}
