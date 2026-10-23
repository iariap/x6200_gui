/*
 * Fixture de diagnostico del issue #292 (LSB: la senal audible aparece a la
 * derecha de la raya) para el arnes de host del espectro.
 *
 * Correlo contra el spectrum.c REAL (el mismo archivo que corre en la radio) y
 * los stubs de tests/native (ver run.sh). No toca radio, red ni hardware.
 *
 * Pregunta que responde, de forma falsable:
 *
 *   Con un eje px<->Hz DECLARADO (bin 400 = frecuencia sintonizada, bin i =
 *   (i-400)*span/800 Hz, +Hz a la derecha), una senal sintetica en un offset
 *   conocido cae SIEMPRE en el mismo x, en AM, LSB y USB; lo unico que cambia
 *   por modo es la BARRA del passband. Si eso se cumple, el render es
 *   internamente coherente: la posicion de la senal y la de la barra usan la
 *   MISMA escala, y "barra a la izquierda + senal a la derecha" solo puede venir
 *   de que los DATOS (la PSD que manda el BASE) traigan la energia en el offset
 *   positivo, no de una inconsistencia del GUI.
 *
 * El fixture mide, con el callback de dibujo real:
 *   - signal_x : el x del pico de la traza principal (la linea no vertical con
 *                el y1 minimo, o sea el bin mas fuerte);
 *   - band     : el rectangulo del passband (spectrum_filter_band()).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <string.h>

#include "spectrum.h"
#include "spectrum_stubs.h"

#include "cfg/cfg.h"
#include "params/params.h"
#include "radio.h"

/* El ancho de FFT de la captura del owner (barra de 2,7k -> 45 px a 48 kHz:
 * 2700/48000*800 = 45). Ver docs/HALLAZGOS.md y el issue #292. */
#define SPAN_HZ 48000
#define W 800

/* Filtro de la captura (boton "Normal 2700 Hz"): low=150, high=2850. */
#define FILT_LOW  150
#define FILT_HIGH 2850

/* Espejo EXACTO de src/cfg/mode.c::update_real_filters(): la unica fuente de
 * cfg_cur.filter.real.from/to. Se copia para alimentar la geometria con el
 * mapeo documentado, no para reimplementarlo. */
static void real_edges(x6200_mode_t mode, int32_t low, int32_t high, int32_t *from, int32_t *to)
{
    switch (mode) {
        case x6200_mode_lsb:
        case x6200_mode_lsb_dig:
            *from = -high;
            *to   = -low;
            break;

        case x6200_mode_usb:
        case x6200_mode_usb_dig:
        case x6200_mode_cw:
        case x6200_mode_cwr:
            *from = low;
            *to   = high;
            break;

        case x6200_mode_am:
        case x6200_mode_sam:
        case x6200_mode_nfm:
        case x6200_mode_wfm:
            *from = -high;
            *to   = high;
            break;

        default:
            *from = low;
            *to   = high;
            break;
    }
}

/* Offset (en Hz) del bin `px` respecto del centro, con el eje declarado. */
static int32_t offset_hz_from_px(int32_t px, int32_t span) { return px * span / W; }

/* Llena el buffer del espectro con piso de ruido y UN portador triangular en
 * `carrier_bin`. Sin trascendentales (determinista). */
static void feed_carrier(int32_t carrier_bin)
{
    static float buf[800];
    const float  floor_db = -100.0f;
    const float  peak_db  = -55.0f;
    const float  half_width = 6.0f;

    for (int i = 0; i < 800; i++) {
        buf[i] = floor_db;
    }

    for (int i = 0; i < 800; i++) {
        float d = (float) (i - carrier_bin);
        if (d < 0) d = -d;
        if (d < half_width) {
            buf[i] = peak_db * (1.0f - d / half_width) + floor_db * (d / half_width);
        }
    }

    spectrum_data(buf, 800, false);
}

/* x del pico de la traza principal: la linea no vertical con el y1 minimo. */
static int32_t main_peak_x(void)
{
    int32_t best_y = 1000000;
    int32_t best_x = -1;

    for (unsigned i = 0; i < spectrum_stub_lines_n; i++) {
        const spectrum_stub_line_t *l = &spectrum_stub_lines[i];

        if (l->color != SPECTRUM_STUB_COLOR_MAIN) continue;
        if (l->x1 == l->x2) continue;   /* la raya central es vertical */

        if (l->y1 < best_y) {
            best_y = l->y1;
            best_x = l->x1;
        }
    }

    return best_x;
}

static void measure(const char *name, x6200_mode_t mode, int32_t carry_px)
{
    int32_t from, to;

    real_edges(mode, FILT_LOW, FILT_HIGH, &from, &to);

    spectrum_stub_subject_set(cfg_cur.fft_width, SPAN_HZ);
    spectrum_stub_subject_set(cfg_cur.filter.real.from, from);
    spectrum_stub_subject_set(cfg_cur.filter.real.to, to);
    spectrum_stub_subject_set(cfg_cur.filter.bw, FILT_HIGH - FILT_LOW);

    feed_carrier(W / 2 + carry_px);
    spectrum_stub_clear_records();
    spectrum_stub_draw();

    int32_t peak_x = main_peak_x();

    int32_t band_left = -1, band_right = -1;
    if (spectrum_stub_rects_n == 1) {
        band_left  = spectrum_stub_rects[0].x1;
        band_right = spectrum_stub_rects[0].x2;
    }

    printf("%-4s | offset portador %+4d px (%+6d Hz) | pico en x=%3d | "
           "banda [%3d..%3d] px (%+6d..%+6d Hz) | ",
           name, carry_px, offset_hz_from_px(carry_px, SPAN_HZ), peak_x,
           band_left, band_right,
           offset_hz_from_px(band_left - W / 2, SPAN_HZ),
           offset_hz_from_px(band_right - W / 2, SPAN_HZ));

    if (peak_x != W / 2 + carry_px) {
        printf("FALLO: el pico cae en %d, esperado %d\n", peak_x, W / 2 + carry_px);
        return;
    }

    printf("OK\n");
}

int main(void)
{
    spectrum_stub_reset();
    spectrum_stub_gui_thread();

    if (spectrum_init(NULL) == NULL) {
        printf("FALLO: spectrum_init() devolvio NULL\n");
        return 1;
    }

    printf("== Fixture #292: eje px<->Hz del arnes de host, spectrum.c real ==\n");
    printf("span=%d Hz | w=%d px | %d Hz/px | centro x=%d\n",
           SPAN_HZ, W, SPAN_HZ / W, W / 2);
    printf("eje declarado: bin 400 = frecuencia sintonizada; bin i = (i-400)*%d/800 Hz; +Hz a la derecha\n\n",
           SPAN_HZ);

    /* El mismo portador, los tres modos. La barra cambia; el pico NO debe. */
    printf("-- portador a +1000 Hz (a la DERECHA del centro) --\n");
    measure("AM",  x6200_mode_am,  17);
    measure("LSB", x6200_mode_lsb, 17);
    measure("USB", x6200_mode_usb, 17);

    printf("\n-- portador a -1000 Hz (a la IZQUIERDA del centro) --\n");
    measure("AM",  x6200_mode_am,  -17);
    measure("LSB", x6200_mode_lsb, -17);
    measure("USB", x6200_mode_usb, -17);

    /* El caso del owner: portador en +51..55 px de la raya (su captura) con la
     * barra de LSB a la izquierda. */
    printf("\n-- caso del owner: portador a +53 px (offset medido en la captura), LSB --\n");
    measure("LSB", x6200_mode_lsb, 53);

    /* El portador JUSTO en el borde derecho del passband de LSB (-150 Hz) cae,
     * por construccion, exactamente en el borde de la barra: prueba de que la
     * escala de la senal y la de la barra son la MISMA. */
    printf("\n-- coherencia de escala: portador en el borde del passband de LSB (-150 Hz) --\n");
    measure("LSB", x6200_mode_lsb, -150 * W / SPAN_HZ);   /* -150 Hz -> -2.5 px -> -2..-3 */

    return 0;
}
