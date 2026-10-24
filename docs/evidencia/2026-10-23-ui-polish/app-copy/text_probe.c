/* Mide el ancho/alto RENDERIZADO de cada cadena cambiada con la fuente real
 * (metrics de LVGL, sin OCR), contra la geometria real del label en el firmware.
 *
 * Issue #288, Fase 2: `dialog_field.c` y `dialog_propagation.c` no los compila
 * ningun target del arnes de host (son inertes por la regla de la epica) y en
 * este entorno no hay binario ARM (sysroot runtime-only, sin crt), asi que no se
 * pueden capturar. Esta sonda responde la pregunta que importa para un cambio de
 * copy: ¿la cadena nueva entra en la MISMA caja que la vieja?
 *
 * La fuente y el `lv_txt_get_size` son los del render real (los mismos objetos
 * `src/fonts/sony_*.c` y la misma LVGL 8.3.11 del arnes). Si la cadena nueva mide
 * mas alto que una linea o mas ancho que el label, el label envuelve/recorta
 * distinto que antes: eso es un hallazgo, no un detalle.
 *
 * Uso (dentro del contenedor, con el arnes ya compilado):
 *   cc -std=gnu11 -O2 -DLV_CONF_INCLUDE_SIMPLE=1 -I/src -I/src/lvgl \
 *      -o /scratch/text_probe text_probe.c /src/src/fonts/sony_*.c \
 *      /scratch/build-post/lib/liblvgl.a -lm -lpthread
 *   /scratch/text_probe
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "lvgl/lvgl.h"

#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(sony_18);
LV_FONT_DECLARE(sony_20);
LV_FONT_DECLARE(sony_22);
LV_FONT_DECLARE(sony_26);
LV_FONT_DECLARE(sony_28);

typedef struct {
    const char      *app;
    const char      *what;
    const lv_font_t *font;
    lv_coord_t       box_w;   /* ancho del label en el firmware (0 = auto) */
    lv_coord_t       box_h;   /* alto del label en el firmware (0 = auto) */
    const char      *before;
    const char      *after;
} case_t;

static void measure_one(const case_t *c, const char *text, const char *tag)
{
    lv_point_t size = {0, 0};
    /* Con el ancho del label: LVGL envuelve; el alto dice cuantas lineas ocupa. */
    lv_txt_get_size(&size, text, c->font, 0, 0,
                    c->box_w > 0 ? c->box_w : LV_COORD_MAX, LV_TEXT_FLAG_NONE);

    int lines = c->font->line_height > 0
                    ? (size.y + c->font->line_height - 1) / c->font->line_height
                    : 0;
    int fits_w = c->box_w == 0 || size.x <= c->box_w;
    int fits_h = c->box_h == 0 || size.y <= c->box_h;

    printf("%-12s %-34s %-6s %4dx%-4d  (%dx%d, %d lineas)  ancho:%s alto:%s\n",
           c->app, c->what, tag, size.x, size.y, c->box_w, c->box_h, lines,
           fits_w ? "OK" : "EXCEDE", fits_h ? "OK" : "EXCEDE");
}

int main(void)
{
    /* Cada caso: el label REAL del firmware (fuente y caja de dialog_*.c) y el
     * par de cadenas vieja/nueva del mapeo de #288 (Fase 1 + Fase 1b).
     *
     * Las cadenas con `%` se miden con el peor caso ya usado en la verificacion
     * de truncacion (banda de 31 caracteres, enteros maximos). */
    const char *band31 = "160m-current-band-name-1234567";

    char band_before[256];
    char band_after[256];
    char status_before[256];
    char status_after[256];
    char sync_before[256];
    char sync_after[256];
    char measure_before[256];
    char measure_after[256];
    char cov_before[256];
    char cov_after[256];
    char listen_before[256];
    char listen_after[256];
    char dir_before[256];
    char dir_after[256];

    snprintf(band_before, sizeof(band_before),
             "Banda %s  |  %d beacons IBP aplican  |  %d en el plan", band31, 18, 5);
    snprintf(band_after, sizeof(band_after),
             "Band %s  |  %d eligible IBP beacons  |  %d scheduled", band31, 18, 5);

    snprintf(status_before, sizeof(status_before),
             "Banda actual y turno de los beacons listos.\nReloj NTP: %s.\n"
             "Pulsa Run (o Repetir) para medir.",
             "SIN sincronizar (el turno puede estar corrido)");
    snprintf(status_after, sizeof(status_after),
             "Current band and IBP schedule ready.\nNTP clock: %s.\n"
             "Press Run or Repeat to measure.",
             "NOT synchronized (slot timing may drift)");

    snprintf(sync_before, sizeof(sync_before),
             "Reloj NTP: %s\nTransmitiendo ahora: %s (%s, quedan %d s)\n%s",
             "NO: sin sincronizar", "4U1UN", band31, 999,
             "El turno de los beacons puede estar corrido (sin NTP).");
    snprintf(sync_after, sizeof(sync_after),
             "NTP clock: %s\nBeacon on air: %s (%s, %d s left)\n%s",
             "NO: unsynchronized", "4U1UN", band31, 999,
             "Beacon slot timing may drift without NTP.");

    snprintf(measure_before, sizeof(measure_before),
             "Beacon %d de %d: %s en %u.%03u MHz\n%s\n%s", 5, 5, "4U1UN", 28, 200,
             "Slot en curso: midiendo la escalera...", "100 W > 10 W > 1 W > 0.1 W");
    snprintf(measure_after, sizeof(measure_after),
             "Beacon %d of %d: %s at %u.%03u MHz\n%s\n%s", 5, 5, "4U1UN", 28, 200,
             "Slot active: measuring power steps...", "100 W > 10 W > 1 W > 0.1 W");

    snprintf(cov_before, sizeof(cov_before),
             "Medidos %d de %d beacons IBP, %d escuchados. Registro IARU R2 "
             "(10 m): %d activos de %d.", 5, 18, 3, 12, 20);
    snprintf(cov_after, sizeof(cov_after),
             "Measured %d/%d IBP beacons, %d heard. IARU R2 "
             "(10 m): %d active of %d.", 5, 18, 3, 12, 20);

    snprintf(listen_before, sizeof(listen_before),
             "Escuchando %s en %u.%03u MHz: Parar/Cerrar vuelve al VFO.\n", "4U1UN", 28, 200);
    snprintf(listen_after, sizeof(listen_after),
             "Listening to %s at %u.%03u MHz: Stop/Close restores VFO.\n", "4U1UN", 28, 200);

    snprintf(dir_before, sizeof(dir_before),
             "Hacia %s (%s): %s %d deg, %.0f km.", "United States of America", "AA00AA", "NE",
             359, 20015.0);
    snprintf(dir_after, sizeof(dir_after),
             "Toward %s (%s): %s %d deg, %.0f km.", "United States of America", "AA00AA", "NE",
             359, 20015.0);

    const case_t cases[] = {
        /* --- Field Ops: header (label x=14, y=9, alto de HEAD_H; auto ancho) --- */
        {"field", "header X6100->X6200", &sony_26, 0, 0, "X6100", "X6200"},

        /* --- Propagation: cajas REALES de construct_cb --- */
        {"propagation", "lbl_band (760, auto)", &sony_22, 760, 0, band_before, band_after},
        {"propagation", "lbl_status (760x58)", &sony_20, 760, 58, status_before, status_after},
        {"propagation", "lbl_status sync (760x58)", &sony_20, 760, 58, sync_before, sync_after},
        {"propagation", "lbl_status measure", &sony_20, 760, 58, measure_before, measure_after},
        {"propagation", "lbl_detail (760x40)", &sony_18, 760, 40, cov_before, cov_after},
        {"propagation", "nota listening (760)", &sony_20, 760, 0, listen_before, listen_after},
        {"propagation", "direction (160 chars)", &sony_20, 0, 0, dir_before, dir_after},
        {"propagation", "step (auto)", &sony_22, 0, 0, "Paso 1/4  |  Preparando",
         "Step 1/4  |  Preparing"},
        {"propagation", "verdict (auto)", &sony_26, 0, 0, "ABIERTA", "OPEN"},
        {"propagation", "note no-IBP (192)", &sony_20, 760, 0,
         "Sin beacons IBP en 20m. La IARU no respalda beacons debajo de 14 MHz; abajo van "
         "los spots del cluster, si los hay.",
         "No IBP beacons on 20m. IARU IBP beacons start at 14 MHz; cluster spots follow if "
         "available."},
        {"propagation", "note sin slot", &sony_20, 760, 0,
         "No se escucho ningun beacon en su slot: silencio no es prueba de banda cerrada "
         "(puede ser QRM/QRN).",
         "No beacon heard in its slot. Silence does not prove the band is closed (QRM/QRN "
         "possible)."},
    };

    printf("%-12s %-34s %-6s %s\n", "app", "label", "caso", "medido");
    printf("--------------------------------------------------------------------------------------\n");

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        measure_one(&cases[i], cases[i].before, "antes");
        measure_one(&cases[i], cases[i].after, "despues");
    }

    return 0;
}
