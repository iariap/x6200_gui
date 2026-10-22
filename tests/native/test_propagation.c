/*
 * Unit tests: la app Propagacion (src/propagation.c).
 *
 * Todo lo que decide la app (que beacon transmite ahora, el SNR, el veredicto de
 * la banda y el registro de radiofaros) tiene que ser funcion pura: aca corre en
 * el host, sin radio, sin red y sin LVGL. Un error en la matematica del turno de
 * los beacons hace que la app mire el beacon equivocado y no se escuche nada
 * (docs/NEW_FEATURES.md, seccion de Propagacion).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "propagation.h"
#include "test_framework.h"

/* --- Registro IBP ---------------------------------------------------------- */

static void test_ibp_registry(void) {
    CHECK(propagation_ibp_band_count() == 5);
    CHECK(propagation_ibp_beacon_count() == 18);

    /* Las cinco frecuencias del IBP (NCDXF/IARU), en orden de banda. */
    CHECK(propagation_ibp_freq(0) == 14100000);
    CHECK(propagation_ibp_freq(1) == 18110000);
    CHECK(propagation_ibp_freq(2) == 21150000);
    CHECK(propagation_ibp_freq(3) == 24930000);
    CHECK(propagation_ibp_freq(4) == 28200000);

    CHECK(strcmp(propagation_ibp_band_name(0), "20m") == 0);
    CHECK(strcmp(propagation_ibp_band_name(4), "10m") == 0);

    /* El orden de los 18 beacons es el del ciclo: el 1 del ciclo (indice 0) es
     * 4U1UN en 20 m. */
    CHECK(strcmp(propagation_ibp_call(0), "4U1UN") == 0);
    CHECK(strcmp(propagation_ibp_call(1), "VE8AT") == 0);
    CHECK(strcmp(propagation_ibp_call(2), "W6WX") == 0);
    CHECK(strcmp(propagation_ibp_call(3), "KH6RS") == 0);
    CHECK(strcmp(propagation_ibp_call(16), "OA4B") == 0);
    CHECK(strcmp(propagation_ibp_call(17), "YV5B") == 0);

    /* Fuera de rango no inventa: NULL y banda invalida. */
    CHECK(propagation_ibp_call(18) == NULL);
    CHECK(propagation_ibp_call(-1) == NULL);
    CHECK(propagation_ibp_band_name(5) == NULL);
    CHECK(propagation_ibp_freq(5) == 0);

    /* Todas las llamadas son distintas y no vacias. */
    for (int i = 0; i < propagation_ibp_beacon_count(); i++) {
        const char *a = propagation_ibp_call(i);

        CHECK(a != NULL && a[0] != '\0');

        for (int j = 0; j < i; j++) {
            if (strcmp(a, propagation_ibp_call(j)) == 0) {
                printf("  (beacon %d y %d tienen la misma llamada: %s)\n", i, j, a);
                CHECK(false);
            }
        }
    }
}

/* La banda IBP de una frecuencia, y que la frecuencia exacta de cada beacon
 * caiga en su propia banda. */
static void test_ibp_band_of_freq(void) {
    CHECK(propagation_ibp_band_of_freq(14100000) == 0);
    CHECK(propagation_ibp_band_of_freq(14050000) == 0);
    CHECK(propagation_ibp_band_of_freq(18110000) == 1);
    CHECK(propagation_ibp_band_of_freq(21150000) == 2);
    CHECK(propagation_ibp_band_of_freq(24930000) == 3);
    CHECK(propagation_ibp_band_of_freq(28200000) == 4);

    /* Sin beacons IBP: 40/30/80 m (la IARU no respalda beacons debajo de 14 MHz),
     * VHF y fuera de las bandas. */
    CHECK(propagation_ibp_band_of_freq(7100000) == -1);
    CHECK(propagation_ibp_band_of_freq(10125000) == -1);
    CHECK(propagation_ibp_band_of_freq(3700000) == -1);
    CHECK(propagation_ibp_band_of_freq(50000000) == -1);
    CHECK(propagation_ibp_band_of_freq(1000000) == -1);
    CHECK(propagation_ibp_band_of_freq(0) == -1);

    /* Cada banda se halla por la frecuencia de cualquiera de sus beacons. */
    for (int b = 0; b < propagation_ibp_band_count(); b++) {
        CHECK(propagation_ibp_band_of_freq(propagation_ibp_freq(b)) == b);
    }
}

/* --- Turno de los beacons -------------------------------------------------- */

/* slot = epoch/10 (10 s); en la banda b transmite el beacon i con
 * i = (slot - b) mod 18. Un ciclo por banda = 18 slots = 180 s. */
static void test_slot_band0(void) {
    int     beacon;
    int64_t start;
    int     left;

    CHECK(propagation_slot(0, 0, &beacon, &start, &left));
    CHECK(beacon == 0);         /* 4U1UN */
    CHECK(start == 0);
    CHECK(left == 10);

    CHECK(propagation_slot(9, 0, &beacon, &start, &left));
    CHECK(beacon == 0);
    CHECK(start == 0);
    CHECK(left == 1);

    /* Cambia el slot a los 10 s: el siguiente beacon del ciclo. */
    CHECK(propagation_slot(10, 0, &beacon, &start, &left));
    CHECK(beacon == 1);         /* VE8AT */
    CHECK(start == 10);
    CHECK(left == 10);

    CHECK(propagation_slot(35, 0, &beacon, &start, &left));
    CHECK(beacon == 3);         /* KH6RS */
    CHECK(start == 30);
    CHECK(left == 5);

    /* Vuelta completa: a los 180 s vuelve a empezar. */
    CHECK(propagation_slot(180, 0, &beacon, &start, &left));
    CHECK(beacon == 0);
    CHECK(propagation_slot(189, 0, &beacon, &start, &left));
    CHECK(beacon == 0);
    CHECK(propagation_slot(190, 0, &beacon, &start, &left));
    CHECK(beacon == 1);
}

/* La tabla oficial del IBP (NCDXF/IARU): a las 00:00 UTC cada banda tiene un
 * beacon distinto y las bandas van corridas un slot (00:00 en 14.100 = 4U1UN,
 * 00:00 en 18.110 = YV5B, ..., 00:40 en 28.200 = 4U1UN). Un ciclo completo de las
 * cinco bandas tarda 15 min (900 s), pero el de UNA banda son 3 min (180 s).
 * Fuente: https://www.ncdxf.org/beacon/ y la tabla de slots del IBP. */
static void test_official_schedule(void) {
    struct {
        int64_t epoch;
        int     band;
        int     beacon;
    } cases[] = {
        {   0, 0,  0 },     /* 00:00 en 14.100: 4U1UN */
        {  10, 0,  1 },     /* 00:10 en 14.100: VE8AT */
        {  30, 0,  3 },     /* 00:30 en 14.100: KH6RS */
        { 170, 0, 17 },     /* 02:50 en 14.100: YV5B */
        {   0, 1, 17 },     /* 00:00 en 18.110: YV5B */
        {  10, 1,  0 },     /* 00:10 en 18.110: 4U1UN */
        {  20, 1,  1 },     /* 00:20 en 18.110: VE8AT */
        {   0, 4, 14 },     /* 00:00 en 28.200: CS3B */
        {  10, 4, 15 },     /* 00:10 en 28.200: LU4AA */
        {  20, 4, 16 },     /* 00:20 en 28.200: OA4B */
        {  30, 4, 17 },     /* 00:30 en 28.200: YV5B */
        {  40, 4,  0 },     /* 00:40 en 28.200: 4U1UN */
        { 900, 0,  0 },     /* 15 min despues: vuelve a empezar el ciclo de la banda */
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int     beacon;
        int64_t start;
        int     left;

        CHECK(propagation_slot(cases[i].epoch, cases[i].band, &beacon, &start, &left));
        if (beacon != cases[i].beacon) {
            printf("  (t=%lld banda %d: beacon %d, esperaba %d)\n",
                   (long long) cases[i].epoch, cases[i].band, beacon, cases[i].beacon);
        }
        CHECK(beacon == cases[i].beacon);
    }

    /* El ciclo de una banda es de 3 min; el de las cinco bandas, de 15. */
    CHECK(PROPAGATION_IBP_ROTATION_S == 180);
}

/* Las bandas van corridas un slot: en 17 m a t=0 transmite el ultimo del ciclo. */
static void test_slot_band_offset(void) {
    int     beacon;
    int64_t start;
    int     left;

    for (int b = 0; b < propagation_ibp_band_count(); b++) {
        CHECK(propagation_slot(0, b, &beacon, &start, &left));
        CHECK(beacon == ((0 - b) % 18 + 18) % 18);
        CHECK(start == 0);
        CHECK(left == 10);

        /* Y a los 10 s, el siguiente del ciclo. */
        CHECK(propagation_slot(10, b, &beacon, &start, &left));
        CHECK(beacon == ((1 - b) % 18 + 18) % 18);
    }

    /* En todo slot hay exactamente un beacon por banda: los 5 son distintos. */
    int64_t start_;
    int     left_;

    for (int64_t t = 0; t < 900; t += 10) {
        int seen[5];

        for (int b = 0; b < 5; b++) {
            CHECK(propagation_slot(t, b, &seen[b], &start_, &left_));
        }

        for (int b = 0; b < 5; b++)
            for (int c = b + 1; c < 5; c++) {
                if (seen[b] == seen[c]) {
                    printf("  (t=%lld, bandas %d y %d con el mismo beacon %d)\n",
                           (long long) t, b, c, seen[b]);
                    CHECK(false);
                }
            }
    }

    /* Banda fuera de rango y epoch negativo: no hay turno. */
    CHECK(!propagation_slot(0, 5, &beacon, &start, &left));
    CHECK(!propagation_slot(0, -1, &beacon, &start, &left));
    CHECK(!propagation_slot(-10, 0, &beacon, &start, &left));
}

/* Espera hasta el slot de un beacon concreto: 0..179 s, y el slot es el del
 * beacon pedido. */
static void test_beacon_slot(void) {
    int64_t start;
    int     wait;

    /* 4U1UN en 20 m transmite ahora (0..9 s): no hay que esperar. */
    CHECK(propagation_beacon_slot(3, 0, 0, &start, &wait));
    CHECK(start == 0);
    CHECK(wait == 0);

    /* Cuando termina su slot, espera el ciclo completo menos lo que ya paso. */
    CHECK(propagation_beacon_slot(10, 0, 0, &start, &wait));
    CHECK(start == 180);
    CHECK(wait == 170);

    /* VE8AT (indice 1) en 20 m: a t=0 faltan 10 s. */
    CHECK(propagation_beacon_slot(0, 0, 1, &start, &wait));
    CHECK(start == 10);
    CHECK(wait == 10);

    /* Y en 17 m, a t=0, transmite el ultimo del ciclo (YV5B): la banda va un slot
     * corrida respecto de 20 m, asi que VE8AT todavia no arranco. */
    CHECK(propagation_beacon_slot(0, 1, 17, &start, &wait));
    CHECK(start == 0);
    CHECK(wait == 0);

    CHECK(propagation_beacon_slot(0, 1, 1, &start, &wait));
    CHECK(start == 20);
    CHECK(wait == 20);

    /* El slot devuelto es el del beacon pedido, y al vencer la espera transmite
     * ESE beacon (la espera nunca pasa el ciclo). */
    for (int band = 0; band < propagation_ibp_band_count(); band++) {
        for (int beacon = 0; beacon < propagation_ibp_beacon_count(); beacon++) {
            for (int64_t t = 0; t < 180; t += 7) {
                int     cur;
                int64_t cur_start;
                int     cur_left;

                CHECK(propagation_beacon_slot(t, band, beacon, &start, &wait));
                CHECK(wait >= 0 && wait < 180);
                CHECK(start % 10 == 0);
                CHECK(t + wait >= start && t + wait < start + 10);

                /* Al vencer la espera transmite el beacon pedido. */
                CHECK(propagation_slot(t + wait, band, &cur, &cur_start, &cur_left));
                CHECK(cur == beacon);
            }
        }
    }

    CHECK(!propagation_beacon_slot(0, 0, 18, &start, &wait));
    CHECK(!propagation_beacon_slot(0, 5, 0, &start, &wait));
}

/* --- SNR, veredicto y escalera de potencia --------------------------------- */

static void test_snr(void) {
    CHECK(propagation_snr(-73, -100) == 27);
    CHECK(propagation_snr(-100, -73) == -27);
    CHECK(propagation_snr(-90, -90) == 0);
}

static void test_verdict(void) {
    /* Umbrales por defecto: abierta >= 12 dB, marginal >= 6 dB. */
    CHECK(propagation_verdict(20, true, 12, 6) == PROPAGATION_VERDICT_OPEN);
    CHECK(propagation_verdict(12, true, 12, 6) == PROPAGATION_VERDICT_OPEN);
    CHECK(propagation_verdict(11, true, 12, 6) == PROPAGATION_VERDICT_MARGINAL);
    CHECK(propagation_verdict(6, true, 12, 6) == PROPAGATION_VERDICT_MARGINAL);
    CHECK(propagation_verdict(5, true, 12, 6) == PROPAGATION_VERDICT_CLOSED);

    /* Silencio: no se escucho nada. La banda no dio senal (no es prueba de
     * cierre, pero el veredicto sin senal es cerrada). */
    CHECK(propagation_verdict(40, false, 12, 6) == PROPAGATION_VERDICT_CLOSED);

    /* Umbrales configurables. */
    CHECK(propagation_verdict(10, true, 20, 15) == PROPAGATION_VERDICT_CLOSED);
    CHECK(propagation_verdict(16, true, 20, 15) == PROPAGATION_VERDICT_MARGINAL);
    CHECK(propagation_verdict(21, true, 20, 15) == PROPAGATION_VERDICT_OPEN);

    CHECK(strcmp(propagation_verdict_name(PROPAGATION_VERDICT_OPEN), "OPEN") == 0);
    CHECK(strcmp(propagation_verdict_name(PROPAGATION_VERDICT_MARGINAL), "MARGINAL") == 0);
    CHECK(strcmp(propagation_verdict_name(PROPAGATION_VERDICT_CLOSED), "CLOSED") == 0);
    CHECK(strcmp(propagation_verdict_name(PROPAGATION_VERDICT_NO_DATA), "NO DATA") == 0);
}

/* La escalera 100/10/1/0.1 W y las ventanas del slot de 10 s en las que se mide
 * cada potencia (ver propagation.h: ventana fija de la v1). */
static void test_ladder(void) {
    CHECK(propagation_ladder_steps() == 4);
    /* La potencia del escalon en decimas de W: 100 W, 10 W, 1 W y 0.1 W. */
    CHECK(propagation_ladder_power_dw(0) == 1000);
    CHECK(propagation_ladder_power_dw(1) == 100);
    CHECK(propagation_ladder_power_dw(2) == 10);
    CHECK(propagation_ladder_power_dw(3) == 1);
    CHECK(propagation_ladder_power_dw(4) == 0);

    /* Piso de deteccion de un escalon: 3 dB sobre el piso de ruido del slot. */
    CHECK(PROPAGATION_DETECT_DB == 3);

    /* Las ventanas del slot: 100 W con el indicativo y el primer dash (0..5 s),
     * y los tres dashes debiles al final (5..6, 7..8 y 9..10 s). No se solapan,
     * no se salen del slot y el ultimo termina justo a los 10 s. */
    int start, len;
    int end = 0;

    for (int k = 0; k < propagation_ladder_steps(); k++) {
        CHECK(propagation_ladder_window(k, &start, &len));
        /* Las ventanas no se solapan y van en orden; entre los dashes debiles
         * hay 1 s de espacio (por eso hay huecos de 6 a 7 y de 8 a 9). */
        CHECK(start >= end);
        CHECK(len > 0);
        end = start + len;
        CHECK(end <= PROPAGATION_LADDER_SLOT_MS);
    }

    CHECK(propagation_ladder_window(0, &start, &len));
    CHECK(start == 0 && len == 5000);
    CHECK(propagation_ladder_window(3, &start, &len));
    CHECK(start == 9000 && len == 1000);
    CHECK(end == PROPAGATION_LADDER_SLOT_MS);
    CHECK(PROPAGATION_LADDER_SLOT_MS == PROPAGATION_IBP_SLOT_S * 1000);
    CHECK(!propagation_ladder_window(-1, &start, &len));
    CHECK(!propagation_ladder_window(propagation_ladder_steps(), &start, &len));
}

/* La escalera que se mide en un slot: se cuentan los escalones SEGUIDOS que
 * superan el piso de deteccion, desde el primero (cada uno baja 10x la potencia:
 * si no se oyo el de 10 W, no tiene sentido dar por oido el de 0.1 W). */
static void test_ladder_from_slot(void) {
    int snr[PROPAGATION_LADDER_STEPS];

    /* Slot limpio: 100 W 30 dB, 10 W 20 dB, 1 W 10 dB y 0.1 W 0 dB sobre el piso
     * (-100 dBm). Con piso de deteccion 3 dB se oyen tres escalones. */
    const int q1[PROPAGATION_LADDER_STEPS] = { -70, -80, -90, -100 };

    CHECK(propagation_ladder_from_slot(q1, -100, 3, snr) == 3);
    CHECK(snr[0] == 30);
    CHECK(snr[1] == 20);
    CHECK(snr[2] == 10);
    CHECK(snr[3] == 0);

    /* Sin SNR de sobra: solo el de 100 W. */
    const int q2[PROPAGATION_LADDER_STEPS] = { -90, -100, -100, -103 };

    CHECK(propagation_ladder_from_slot(q2, -100, 3, snr) == 1);
    CHECK(snr[0] == 10);

    /* Nada sobre el piso de ruido: ningun escalon (y el SNR se informa igual). */
    const int q3[PROPAGATION_LADDER_STEPS] = { -101, -100, -102, -100 };

    CHECK(propagation_ladder_from_slot(q3, -100, 3, snr) == 0);
    CHECK(snr[0] == -1);
    CHECK(snr[3] == 0);

    /* Con un slot fuerte se oyen los cuatro. */
    const int q4[PROPAGATION_LADDER_STEPS] = { -60, -70, -80, -90 };

    CHECK(propagation_ladder_from_slot(q4, -100, 3, snr) == 4);

    /* snr_out puede ser NULL: la funcion es la misma. */
    CHECK(propagation_ladder_from_slot(q4, -100, 3, NULL) == 4);

    /* Un hueco corta la escalera: el de 10 W no se oyo, asi que el de 0.1 W (mas
     * debil) no se cuenta aunque el S-meter lo haya visto. */
    const int q5[PROPAGATION_LADDER_STEPS] = { -70, -100, -95, -96 };

    CHECK(propagation_ladder_from_slot(q5, -100, 3, snr) == 1);
}

/* --- Plan del barrido ------------------------------------------------------ */

/* Los beacons que se miden: los que transmiten DESPUES del slot que corre ahora
 * (un slot ya empezado no se mide: no se vio desde su principio). */
static void test_plan(void) {
    int plan[PROPAGATION_PLAN_MAX];

    CHECK(PROPAGATION_PLAN_MAX == 5);

    /* A t=0 en 20 m transmite 4U1UN (beacon 0): el plan arranca en el 1. */
    int n = propagation_plan(0, 0, plan);

    CHECK(n == PROPAGATION_PLAN_MAX);

    for (int k = 0; k < n; k++) {
        CHECK(plan[k] == k + 1);
    }

    /* En 17 m (corrida un slot) a t=0 transmite 17: el plan arranca en el 0. */
    n = propagation_plan(0, 1, plan);
    CHECK(n == PROPAGATION_PLAN_MAX);

    for (int k = 0; k < n; k++) {
        CHECK(plan[k] == k);
    }

    /* Los cinco son distintos y cada uno tiene su turno despues del slot actual,
     * dentro de los proximos 60 s. */
    for (int band = 0; band < propagation_ibp_band_count(); band++) {
        for (int64_t t = 0; t < 180; t += 10) {
            n = propagation_plan(t, band, plan);
            CHECK(n == PROPAGATION_PLAN_MAX);

            for (int k = 0; k < n; k++) {
                int64_t start;
                int     wait;

                CHECK(propagation_beacon_slot(t, band, plan[k], &start, &wait));
                CHECK(start > t);                       /* todavia no empezo */
                CHECK(start - t <= PROPAGATION_PLAN_MAX * PROPAGATION_IBP_SLOT_S);

                for (int j = 0; j < k; j++) {
                    if (plan[j] == plan[k]) {
                        printf("  (t=%lld band=%d: el beacon %d esta dos veces)\n",
                               (long long) t, band, plan[k]);
                        CHECK(false);
                    }
                }
            }
        }
    }

    /* Banda sin IBP: no hay plan. */
    CHECK(propagation_plan(0, -1, plan) == 0);
    CHECK(propagation_plan(0, 5, plan) == 0);
    CHECK(propagation_plan(-10, 0, plan) == 0);
}

/* --- Direccion (posicion de los beacons y rumbo) --------------------------- */

/* Cada beacon del ciclo tiene su sitio y su grilla (tabla de sitios del IBP). */
static void test_ibp_positions(void) {
    for (int i = 0; i < propagation_ibp_beacon_count(); i++) {
        const char *grid = propagation_ibp_grid(i);
        const char *site = propagation_ibp_site(i);

        CHECK(grid != NULL && strlen(grid) == 4);
        CHECK(site != NULL && site[0] != '\0');

        /* Grilla Maidenhead de 4: dos letras A..R y dos digitos. */
        CHECK(grid[0] >= 'A' && grid[0] <= 'R');
        CHECK(grid[1] >= 'A' && grid[1] <= 'R');
        CHECK(grid[2] >= '0' && grid[2] <= '9');
        CHECK(grid[3] >= '0' && grid[3] <= '9');

        for (int j = 0; j < i; j++) {
            if (strcmp(grid, propagation_ibp_grid(j)) == 0) {
                printf("  (beacon %d y %d comparten la grilla %s)\n", i, j, grid);
                CHECK(false);
            }
        }
    }

    /* Los que sabemos de memoria: el primero del ciclo (Nueva York) y el ultimo
     * (Caracas). */
    CHECK(strcmp(propagation_ibp_call(0), "4U1UN") == 0);
    CHECK(strcmp(propagation_ibp_grid(0), "FN30") == 0);
    CHECK(strcmp(propagation_ibp_site(0), "New York") == 0);
    CHECK(strcmp(propagation_ibp_grid(10), "KG44") == 0);   /* ZS6DN, Pretoria */

    CHECK(propagation_ibp_grid(-1) == NULL);
    CHECK(propagation_ibp_grid(18) == NULL);
    CHECK(propagation_ibp_site(18) == NULL);
}

/* Rumbo inicial entre dos puntos (grados, 0 = norte, 90 = este). */
static void test_bearing(void) {
    /* Sobre el ecuador, hacia el este: 90 grados. */
    CHECK(propagation_bearing(0.0, 0.0, 0.0, 10.0) == 90);

    /* Hacia el norte: 0 (y 359.x se redondea a 0 o 360 -> 0). */
    CHECK(propagation_bearing(0.0, 0.0, 10.0, 0.0) == 0);
    CHECK(propagation_bearing(0.0, 0.0, -10.0, 0.0) == 180);

    /* Hacia el oeste: 270. */
    CHECK(propagation_bearing(0.0, 0.0, 0.0, -10.0) == 270);

    /* Buenos Aires (GF05tj, -34.6 -58.4) a Madrid (40.4 -3.7): al noreste. */
    int b = propagation_bearing(-34.6, -58.4, 40.4, -3.7);

    CHECK(b >= 20 && b <= 50);

    /* El rumbo de un punto a si mismo no esta definido: 0. */
    CHECK(propagation_bearing(10.0, 20.0, 10.0, 20.0) == 0);

    /* Siempre en 0..359. */
    for (double lat2 = -80.0; lat2 <= 80.0; lat2 += 13.0) {
        for (double lon2 = -170.0; lon2 <= 170.0; lon2 += 17.0) {
            b = propagation_bearing(-34.6, -58.4, lat2, lon2);
            CHECK(b >= 0 && b < 360);
        }
    }
}

/* Punto cardinal de 8 rumbos, para leer el veredicto ("abierta hacia el NE"). */
static void test_compass(void) {
    CHECK(strcmp(propagation_compass(0), "N") == 0);
    CHECK(strcmp(propagation_compass(45), "NE") == 0);
    CHECK(strcmp(propagation_compass(90), "E") == 0);
    CHECK(strcmp(propagation_compass(135), "SE") == 0);
    CHECK(strcmp(propagation_compass(180), "S") == 0);
    CHECK(strcmp(propagation_compass(225), "SO") == 0);
    CHECK(strcmp(propagation_compass(270), "O") == 0);
    CHECK(strcmp(propagation_compass(315), "NO") == 0);

    /* Los bordes: 22 grados todavia es norte, 23 ya es noreste; y 359 vuelve a
     * norte. */
    CHECK(strcmp(propagation_compass(22), "N") == 0);
    CHECK(strcmp(propagation_compass(23), "NE") == 0);
    CHECK(strcmp(propagation_compass(337), "NO") == 0);
    CHECK(strcmp(propagation_compass(338), "N") == 0);
    CHECK(strcmp(propagation_compass(359), "N") == 0);
    /* Fuera de rango se normaliza, no se sale del arreglo. */
    CHECK(strcmp(propagation_compass(-1), "N") == 0);
    CHECK(strcmp(propagation_compass(360), "N") == 0);
    CHECK(strcmp(propagation_compass(450), "E") == 0);
}

/* --- Umbrales del veredicto (presets) -------------------------------------- */

static void test_snr_presets(void) {
    CHECK(propagation_snr_preset_count() >= 2);

    /* El primero es el de fabrica: abierta >= 12 dB, marginal >= 6 dB. */
    int open_db, marg_db;

    CHECK(propagation_snr_preset(0, &open_db, &marg_db));
    CHECK(open_db == 12);
    CHECK(marg_db == 6);

    /* Todos los presets tienen sentido: marginal < abierta, y los dos en rango. */
    for (int i = 0; i < propagation_snr_preset_count(); i++) {
        CHECK(propagation_snr_preset(i, &open_db, &marg_db));
        CHECK(open_db > marg_db);
        CHECK(marg_db >= 0 && open_db <= 60);
        CHECK(propagation_snr_preset_index(open_db, marg_db) == i);
    }

    CHECK(!propagation_snr_preset(-1, &open_db, &marg_db));
    CHECK(!propagation_snr_preset(propagation_snr_preset_count(), &open_db, &marg_db));

    /* Un par que no esta en la tabla no tiene indice (la app cae al de fabrica). */
    CHECK(propagation_snr_preset_index(40, 39) == -1);
    CHECK(propagation_snr_preset_index(12, 5) == -1);
}

/* --- Registro IARU Region 2 (10 m) ----------------------------------------- */

static void test_iaru_registry(void) {
    /* El registro oficial (hojas ACTIVE e INactive de RegionTwo10mBCNs.ods) tal
     * como quedo empaquetado. Los numeros estan clavados a proposito: si el blob
     * se trunca o se re-genera distinto, el test tiene que avisar. El 2026-10-18
     * la lista oficial tenia 983 radiofaros (313 activos) despues de descartar
     * TENTATIVE, K7RA(SK) y los indicativos de mas de 10 caracteres. */
    CHECK(propagation_iaru_count() == 983);
    CHECK(propagation_iaru_active_count() == 313);

    const char *call;
    uint32_t    hz;
    bool        active;
    uint32_t    prev   = 0;
    int         calls  = 0;
    int         actives = 0;

    for (int i = 0; i < propagation_iaru_count(); i++) {
        CHECK(propagation_iaru_beacon(i, &call, &hz, &active));
        CHECK(call != NULL && call[0] != '\0');
        CHECK(hz >= 28000000 && hz <= 29700000);
        CHECK(hz >= prev);       /* ordenadas por frecuencia */

        for (const char *c = call; *c; c++) {
            if (*c < '!' || *c > '~') {
                printf("  (registro %d, llamada con byte raro: %s)\n", i, call);
                CHECK(false);
            }
        }

        prev = hz;
        calls++;

        if (active) {
            actives++;
        }
    }

    CHECK(calls == propagation_iaru_count());
    CHECK(actives == propagation_iaru_active_count());

    /* Fuera de rango: no inventa. */
    CHECK(!propagation_iaru_beacon(-1, &call, &hz, &active));
    CHECK(!propagation_iaru_beacon(propagation_iaru_count(), &call, &hz, &active));

    /* Primer y ultimo registro del blob (28.066.300 y 28.304.600 Hz). */
    CHECK(propagation_iaru_beacon(0, &call, &hz, &active));
    CHECK(hz == 28066300);
    CHECK(strcmp(call, "PY4ROE") == 0);
    CHECK(!active);
    CHECK(propagation_iaru_beacon(propagation_iaru_count() - 1, &call, &hz, &active));
    CHECK(hz == 28304600);
    CHECK(strcmp(call, "K1KWH") == 0);
    CHECK(!active);

    /* Un indicativo de 10 caracteres: la lista oficial tiene varios (HI7/N2CQR
     * en 28.233.500, activo). El registro de 10 bytes lo guarda entero; con el
     * viejo campo de 8 se perdia. */
    bool long_call = false;

    for (int i = 0; i < propagation_iaru_count(); i++) {
        CHECK(propagation_iaru_beacon(i, &call, &hz, &active));

        if (strcmp(call, "HI7/N2CQR") == 0) {
            CHECK(hz == 28233500);
            CHECK(active);
            long_call = true;
        }
    }

    CHECK(long_call);
}

/* Buscar por frecuencia: solo los radiofaros activos, y exacto. */
static void test_iaru_find(void) {
    /* 4U1UN esta en 28.200.000 MHz en el registro (y es el beacon IBP de 10 m). */
    const char *c = propagation_iaru_find(28200000);

    CHECK(c != NULL);
    CHECK(strcmp(c, "4U1UN") == 0);

    /* Una frecuencia que no esta en el registro no devuelve nada. */
    CHECK(propagation_iaru_find(28123456) == NULL);
    CHECK(propagation_iaru_find(0) == NULL);
    CHECK(propagation_iaru_find(14100000) == NULL);

    /* Un radiofaro inactivo no se ofrece como referencia. */
    CHECK(propagation_iaru_find(28066300) == NULL);
}

void test_propagation(void) {
    printf("== Propagacion (#145): turno IBP, SNR, veredicto y registro IARU R2 ==\n");
    test_ibp_registry();
    test_ibp_band_of_freq();
    test_slot_band0();
    test_official_schedule();
    test_slot_band_offset();
    test_beacon_slot();
    test_snr();
    test_verdict();
    test_ladder();
    test_ladder_from_slot();
    test_plan();
    test_ibp_positions();
    test_bearing();
    test_compass();
    test_snr_presets();
    test_iaru_registry();
    test_iaru_find();
}
