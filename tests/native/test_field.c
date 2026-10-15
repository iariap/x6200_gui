/*
 * Unit tests: Field Ops helpers (field/refs.c, field/flog.c, field/bands.c).
 *
 * Ported from the X6100's tests/test_field.cpp to this project's framework:
 * SOTA/POTA reference lookup, the ADIF activation log and the band helpers.
 * All three are pure C, so they run on the host.
 *
 * It also checks the tiles' prefix against the assets the ROM installs (issue #53):
 * the dialog asks for `FIELD_ICONS/<name>_<size>.png` and `rootfs/` is what lands in
 * the image, so the files the constant names have to be there.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "field/bands.h"
#include "field/flog.h"
#include "field/refs.h"
#include "test_framework.h"

#define REFS_PATH "/tmp/test_field_refs.tsv"
#define LOG_PATH  "/tmp/test_field_log.adi"

static void test_refs_nearest(void) {
    FILE *f = fopen(REFS_PATH, "w");

    CHECK(f != NULL);

    if (!f) {
        return;
    }

    fputs("P\tIT-0001\tFar park\t46.00000\t10.00000\t\t\n"
          "S\tI/LO-001\tNear summit\t45.47000\t9.19000\t1200\t4\n"
          "P\tIT-0002\tNear park\t45.46500\t9.19000\t\t\n"
          "P\tIT-0003\tMiddle park\t45.60000\t9.19000\t\t\n", f);
    fclose(f);

    field_ref_t r[2];

    CHECK(field_refs_near(REFS_PATH, 'P', 45.4642, 9.1900, r, 2) == 2);
    CHECK(strcmp(r[0].ref, "IT-0002") == 0);
    CHECK(strcmp(r[1].ref, "IT-0003") == 0);
    CHECK(r[0].km < 0.2f);

    CHECK(field_refs_near(REFS_PATH, 'S', 45.4642, 9.1900, r, 2) == 1);
    CHECK(strcmp(r[0].name, "Near summit") == 0);
    CHECK(r[0].alt_m == 1200);
    CHECK(r[0].points == 4);

    /* Finding by reference is case insensitive */
    field_ref_t one;

    CHECK(field_refs_find(REFS_PATH, "i/lo-001", &one));
    CHECK(one.type == 'S');
    CHECK(!field_refs_find(REFS_PATH, "IT-9999", &one));

    /* A missing file is an error, not "nothing near" */
    CHECK(field_refs_near("/tmp/does-not-exist.tsv", 'P', 0, 0, r, 2) == -1);

    unlink(REFS_PATH);
}

static void test_adif_record(void) {
    field_station_t st = { .call = "IU1WXY/P", .grid = "JN35xx", .prog = 'S', .ref = "I/LO-001" };
    field_qso_t     q  = { .t = 1790685660, .freq_hz = 14062000, .mode = "CW", .rst_s = "599", .rst_r = "579" };
    char            rec[512];

    snprintf(q.call, sizeof(q.call), "%s", "DL1ABC");

    field_adif_record(rec, sizeof(rec), &q, &st);

    CHECK(strstr(rec, "<CALL:6>DL1ABC ") != NULL);
    CHECK(strstr(rec, "<QSO_DATE:8>20260929 <TIME_ON:6>124100 <BAND:3>20m <FREQ:9>14.062000 <MODE:2>CW") != NULL);
    CHECK(strstr(rec, "<MY_SOTA_REF:8>I/LO-001") != NULL);
    CHECK(strstr(rec, "<eor>") != NULL);
    CHECK(strstr(rec, "<NAME:") == NULL);         /* unknown fields are not written */

    /* A QRZ lookup fills the extra fields */
    field_qso_t looked = q;

    snprintf(looked.name, sizeof(looked.name), "%s", "Hans Muster");
    snprintf(looked.qth, sizeof(looked.qth), "%s", "Berlin");
    snprintf(looked.grid, sizeof(looked.grid), "%s", "JO62qm");
    snprintf(looked.country, sizeof(looked.country), "%s", "Germany");

    field_adif_record(rec, sizeof(rec), &looked, &st);
    CHECK(strstr(rec, "<NAME:11>Hans Muster <QTH:6>Berlin <GRIDSQUARE:6>JO62qm <COUNTRY:7>Germany ") != NULL);
}

static void test_log_round_trip(void) {
    field_station_t st = { .call = "IU1WXY/P", .grid = "JN35xx", .prog = 'S', .ref = "I/LO-001" };
    field_qso_t     q  = { .t = 1790685660, .freq_hz = 14062000, .mode = "CW", .rst_s = "599", .rst_r = "579" };

    snprintf(q.call, sizeof(q.call), "%s", "DL1ABC");
    unlink(LOG_PATH);

    CHECK(field_log_add(LOG_PATH, &q, &st));

    st.prog = 'P';
    snprintf(st.ref, sizeof(st.ref), "%s", "IT-0002");
    snprintf(q.call, sizeof(q.call), "%s", "F5XYZ");
    q.t += 60;

    CHECK(field_log_add(LOG_PATH, &q, &st));

    field_qso_t out[8];
    int         n = field_log_read(LOG_PATH, out, 8);

    CHECK(n == 2);
    CHECK(strcmp(out[0].call, "DL1ABC") == 0);
    CHECK(strcmp(out[1].call, "F5XYZ") == 0);
    CHECK(out[1].t == out[0].t + 60);

    /* The log is ADIF: the records are there as text */
    FILE *f = fopen(LOG_PATH, "r");

    CHECK(f != NULL);

    if (f) {
        char line[1024];
        int  records = 0;

        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "<eor>")) {
                records++;
            }
        }

        fclose(f);
        CHECK(records == 2);
    }

    unlink(LOG_PATH);
}

static void test_bands(void) {
    CHECK(strcmp(field_band(14062000), "20m") == 0);
    CHECK(strcmp(field_band(7038600), "40m") == 0);

    /* CW and phone/data have different centres in the Field Ops band list */
    int32_t cw = field_band_freq(0, true);

    CHECK(cw > 0);

    field_bands_t b;
    field_bands(&b);

    /* The word for a level is used in the app's chips */
    CHECK(field_band_word(0) != NULL);
    CHECK(field_band_word(5) != NULL);
}

/* El prefijo del que `icon()` arma la ruta de cada tile (`FIELD_ICONS`, en
 * src/dialog_field.c) tiene que ser el directorio que la imagen realmente instala:
 * `install(DIRECTORY rootfs/ DESTINATION /)` pone `rootfs/<ruta>` en la ROM. El
 * issue #53 fue un prefijo heredado del X6100 (`/usr/share/x6100/field`), carpeta
 * que en la imagen no existe: los cuatro tiles salían sin icono. Se lee el prefijo
 * del fuente y se abren los PNG que el diálogo pide en `rootfs/`, así un prefijo
 * equivocado (o un asset que deje de instalarse) falla acá, en el host. */
static void test_icons_shipped(void) {
    const char *src = getenv("X6200_DIALOG_FIELD_SRC");

    if (src == NULL || *src == 0) {
        return;                         /* el runner no lo pasó: nada que comprobar */
    }

    FILE *f = fopen(src, "r");

    if (!f) {
        CHECK(f != NULL);
        return;
    }

    char prefix[128] = "";
    char line[512];

    while (fgets(line, sizeof(line), f) != NULL) {
        const char *p = strstr(line, "#define FIELD_ICONS");

        if (p == NULL) {
            continue;
        }

        p += strlen("#define FIELD_ICONS");

        while (*p == ' ' || *p == '\t') {
            p++;
        }

        if (*p != '"') {
            break;
        }

        p++;

        size_t n = strcspn(p, "\"");

        if (n < sizeof(prefix)) {
            memcpy(prefix, p, n);
            prefix[n] = 0;
        }

        break;
    }
    fclose(f);

    CHECK(prefix[0] == '/');            /* el prefijo es una ruta dentro del rootfs */

    /* Raíz del checkout: <raíz>/src/dialog_field.c -> dirname(dirname(...)) */
    char root[512];
    int  dirs = 2;

    snprintf(root, sizeof(root), "%s", src);

    while (dirs-- > 0) {
        char *slash = strrchr(root, '/');

        if (slash == NULL) {
            break;
        }

        *slash = 0;
    }

    /* Los cuatro tiles del HOME y las cabeceras, en los dos tamaños que usa el
     * diálogo (icon(): 96 px el tile, 64 px la cabecera y el program panel). */
    static const char *const names[] = {"sota", "pota", "bands", "log"};
    static const int         sizes[] = {64, 96};

    for (size_t k = 0; k < sizeof(names) / sizeof(*names); k++) {
        for (size_t j = 0; j < sizeof(sizes) / sizeof(*sizes); j++) {
            char  path[768];
            FILE *icon;

            snprintf(path, sizeof(path), "%s/rootfs%s/%s_%d.png", root, prefix, names[k], sizes[j]);
            icon = fopen(path, "rb");
            CHECK(icon != NULL);

            if (icon != NULL) {
                fclose(icon);
            }
        }
    }
}

void test_field(void) {
    test_refs_nearest();
    test_adif_record();
    test_log_round_trip();
    test_bands();
    test_icons_shipped();
}
