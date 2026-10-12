/*
 * WeFax TX -> RX loopback on the host: encodes a test picture with
 * src/wefax/encoder.c at the radio's play rate, decimates it to the
 * decoder's rate and decodes it with src/wefax/decoder.c in Auto mode, as
 * the WeFax app does (rows are kept only after the fax is confirmed). Checks
 * APT start and stop detection and that the picture comes back (polarity,
 * timing) by correlation.
 *
 *   tests/native/run_wefax_tests.sh
 */

#include "wefax/encoder.h"

#include <math.h>
#include <png.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLAY_RATE 44100     /* AUDIO_PLAY_RATE */
#define PIC_H     300
#define PIC_PATH  "/tmp/wefax_loopback.png"
#define MAX_OUT   (PIC_H + 200)

bool     wefax_decoder_auto_fax_confirmed(const wefax_decoder_t *decoder);
uint32_t wefax_decoder_auto_start_generation(const wefax_decoder_t *decoder);

static uint8_t  pic[PIC_H][WEFAX_WIDTH];
static uint8_t  out[MAX_OUT][WEFAX_WIDTH];
static unsigned out_n;
static wefax_decoder_t *dec;

/* Gradient on the left, a checkerboard on the right, black bars every 60 lines */
static int write_pic(const char *path, unsigned w, unsigned h)
{
    FILE       *fp = fopen(path, "wb");
    png_structp p  = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop   in = png_create_info_struct(p);
    uint8_t     row[WEFAX_WIDTH];

    if (!fp)
        return -1;
    png_init_io(p, fp);
    png_set_IHDR(p, in, w, h, 8, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(p, in);
    for (unsigned y = 0; y < h; y++) {
        for (unsigned x = 0; x < w; x++) {
            uint8_t g = x < w / 2 ? (y % 60 < 6 ? 0 : x * 255 / (w / 2)) : ((y / 25 + x / 100) & 1) * 255;

            row[x] = g;
            if (y < PIC_H && w == WEFAX_WIDTH)
                pic[y][x] = g;
        }
        png_write_row(p, row);
    }
    png_write_end(p, NULL);
    png_destroy_write_struct(&p, &in);
    fclose(fp);
    return 0;
}

static void row_cb(const uint8_t *row, unsigned int width, unsigned int n, void *ctx)
{
    (void)n;
    (void)ctx;
    if (width == WEFAX_WIDTH && wefax_decoder_auto_fax_confirmed(dec) && out_n < MAX_OUT)
        memcpy(out[out_n++], row, WEFAX_WIDTH);
}

/* Pearson correlation of pic with out from row r0, columns shifted by sh */
static double corr(unsigned r0, unsigned sh, unsigned rows, double *mae)
{
    double sa = 0, sb = 0, saa = 0, sbb = 0, sab = 0, e = 0, n = 0;

    for (unsigned y = 0; y < rows && r0 + y < out_n; y++)
        for (unsigned x = 0; x < WEFAX_WIDTH; x++) {
            double a = pic[y][x], b = out[r0 + y][(x + sh) % WEFAX_WIDTH];

            sa += a; sb += b; saa += a * a; sbb += b * b; sab += a * b; e += fabs(a - b); n++;
        }
    if (mae)
        *mae = e / n;
    return (n * sab - sa * sb) / sqrt((n * saa - sa * sa) * (n * sbb - sb * sb));
}

int main(void)
{
    int fails = 0, rc;

    /* Aspect: a tall picture is fitted into 800 lines */
    write_pic("/tmp/wefax_loopback_tall.png", 1000, 1000);
    wefax_encoder_t *tall = wefax_encoder_open("/tmp/wefax_loopback_tall.png", PLAY_RATE, &rc);
    printf("tall 1000x1000 -> %u lines\n", tall ? wefax_encoder_rows(tall) : 0);
    fails += !tall || wefax_encoder_rows(tall) != WEFAX_TX_MAX_ROWS;
    wefax_encoder_close(tall);

    write_pic(PIC_PATH, WEFAX_WIDTH, PIC_H);
    wefax_encoder_t *enc = wefax_encoder_open(PIC_PATH, PLAY_RATE, &rc);
    if (!enc) {
        printf("encoder open failed %d\n", rc);
        return 1;
    }

    dec = wefax_decoder_create();
    wefax_decoder_set_row_callback(dec, row_cb, NULL);
    wefax_decoder_set_mode(dec, WEFAX_MODE_AUTO);

    /* 3 s of noise first, then the fax with a little noise, then 3 s of noise */
    static int16_t pcm[4096];
    float          rx[1024];
    size_t         n;
    unsigned       noise = 3 * WEFAX_SAMPLE_RATE, k;
    bool           stop  = false;

    srand(1);
    for (int part = 0; part < 3; part++) {
        for (;;) {
            if (part == 1) {
                n = wefax_encoder_fill(pcm, sizeof(pcm) / sizeof(pcm[0]), enc);
                if (!n)
                    break;
            } else {
                if (!noise) {
                    noise = 3 * WEFAX_SAMPLE_RATE;
                    break;
                }
                n = 4 * (noise < 1024 ? noise : 1024);
                memset(pcm, 0, n * sizeof(pcm[0]));
                noise -= n / 4;
            }
            for (k = 0; k < n / 4; k++) {
                float s = (pcm[4 * k] + pcm[4 * k + 1] + pcm[4 * k + 2] + pcm[4 * k + 3]) / (4 * 32768.0f);

                rx[k] = s + 0.02f * ((float)rand() / RAND_MAX - 0.5f);
            }
            wefax_decoder_process(dec, rx, k);
            stop |= wefax_decoder_auto_stop_detected(dec);
        }
    }

    printf("start generations %u, confirmed %d, stop %d, rows kept %u\n",
           wefax_decoder_auto_start_generation(dec), wefax_decoder_auto_fax_confirmed(dec), stop, out_n);
    fails += !stop || out_n < PIC_H;

    /* Column shift from the phasing pulse in the last kept phasing row (black
     * with the 5% white pulse, mean 5..39): the decoder aligns the line during
     * the phasing */
    unsigned sh = 0, best = 0, ph = 0;
    for (unsigned r = 0; r < out_n; r++) {
        unsigned s = 0;
        for (unsigned x = 0; x < WEFAX_WIDTH; x++)
            s += out[r][x];
        if (s >= 40 * WEFAX_WIDTH)
            break;
        if (s >= 5 * WEFAX_WIDTH)
            ph = r;
    }
    for (unsigned x = 0; x < WEFAX_WIDTH; x++) {
        unsigned s = 0;
        for (unsigned i = 0; i < WEFAX_WIDTH / 20; i++)
            s += out[ph][(x + i) % WEFAX_WIDTH];
        if (s > best) {
            best = s;
            sh   = x;
        }
    }

    sh = (sh + WEFAX_WIDTH / 20) % WEFAX_WIDTH;   /* the picture starts after the pulse */

    /* First picture row */
    double   c = -2, cc, mae;
    unsigned r0 = 0;
    for (unsigned r = 0; r + PIC_H / 2 < out_n; r++) {
        cc = corr(r, sh, PIC_H / 2, NULL);
        if (cc > c) {
            c  = cc;
            r0 = r;
        }
    }
    c = corr(r0, sh, PIC_H, &mae);
    printf("phasing pulse at column %u, picture from kept row %u, %u of %u lines, correlation %.4f, mean abs error %.1f\n",
           sh, r0, out_n - r0 < PIC_H ? out_n - r0 : PIC_H, PIC_H, c, mae);
    fails += c < 0.95 || out_n - r0 < PIC_H;

    wefax_decoder_destroy(dec);
    wefax_encoder_close(enc);
    printf(fails ? "FAIL\n" : "PASS\n");
    return fails != 0;
}
