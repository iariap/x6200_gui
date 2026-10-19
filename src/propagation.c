/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - App "Propagation" (issue #145)
 *
 *  La matematica de la app: el turno de los beacons IBP (NCDXF/IARU), el SNR, el
 *  veredicto de la banda y la lectura del registro IARU Region 2. Todo puro (sin
 *  radio, sin red, sin LVGL) para que corra en los tests nativos; el dialogo
 *  (src/dialog_propagation.c) es el que toca la radio, y solo para escuchar.
 *
 *  Fuentes del registro y del turno:
 *   - NCDXF/IARU IBP: https://www.ncdxf.org/beacon/ (18 beacons, 5 bandas,
 *     slot de 10 s, una vuelta de los 18 cada 180 s, las bandas corridas un slot
 *     entre si, y la escalera 100/10/1/0.1 W dentro del slot).
 *   - IARU Region 2, radiofaros de 10 m: la lista oficial empaquetada en
 *     src/propagation_data.c.
 */

#include "propagation.h"

#include "propagation_data.h"

#include <math.h>
#include <string.h>

/* --- Registro IBP --------------------------------------------------------- */

/* Las cinco bandas del IBP, en el orden del ciclo. Las ventanas son las de las
 * bandas de aficionado (las mismas que src/cluster/spots.c), no el ancho del
 * beacon: asi la app sabe que banda esta usando el operador y si tiene beacons. */
static const struct {
    const char *name;
    uint32_t    beacon_hz;
    uint32_t    low_hz;
    uint32_t    high_hz;
} ibp_bands[PROPAGATION_IBP_BANDS] = {
    { "20m", 14100000, 14000000, 14350000 },
    { "17m", 18110000, 18068000, 18168000 },
    { "15m", 21150000, 21000000, 21450000 },
    { "12m", 24930000, 24890000, 24990000 },
    { "10m", 28200000, 28000000, 29700000 },
};

/* Los 18 beacons del IBP, en el orden del ciclo: al primero del ciclo le toca el
 * slot 0 en 20 m. El sitio y la grilla Maidenhead (4 caracteres) son los de la
 * tabla de sitios del IBP (NCDXF/IARU, docs/NEW_FEATURES.md, seccion de
 * Propagacion): sin la posicion no se puede dar la direccion del veredicto. */
static const struct {
    const char *call;
    const char *site;
    const char *grid;
} ibp_calls[PROPAGATION_IBP_BEACONS] = {
    { "4U1UN", "New York",      "FN30" },
    { "VE8AT", "Inuvik, NT",    "CP38" },
    { "W6WX",  "Mt. Umunhum",   "CM97" },
    { "KH6RS", "Maui",          "BL10" },
    { "ZL6B",  "Masterton",     "RE78" },
    { "VK6RBP","Roleystone",    "OF87" },
    { "JA2IGY","Mt. Asama",     "PM84" },
    { "RR9O",  "Novosibirsk",   "NO14" },
    { "VR2B",  "Hong Kong",     "OL72" },
    { "4S7B",  "Colombo",       "MJ96" },
    { "ZS6DN", "Pretoria",      "KG44" },
    { "5Z4B",  "Kariobangi",    "KI88" },
    { "4X6TU", "Tel Aviv",      "KM72" },
    { "OH2B",  "Lohja",         "KP20" },
    { "CS3B",  "Madeira",       "IM12" },
    { "LU4AA", "Buenos Aires",  "GF05" },
    { "OA4B",  "Lima",          "FH17" },
    { "YV5B",  "Caracas",       "FJ69" },
};

int propagation_ibp_band_count(void) {
    return PROPAGATION_IBP_BANDS;
}

int propagation_ibp_beacon_count(void) {
    return PROPAGATION_IBP_BEACONS;
}

const char *propagation_ibp_band_name(int band) {
    if (band < 0 || band >= PROPAGATION_IBP_BANDS) {
        return NULL;
    }

    return ibp_bands[band].name;
}

uint32_t propagation_ibp_freq(int band) {
    if (band < 0 || band >= PROPAGATION_IBP_BANDS) {
        return 0;
    }

    return ibp_bands[band].beacon_hz;
}

const char *propagation_ibp_call(int beacon) {
    if (beacon < 0 || beacon >= PROPAGATION_IBP_BEACONS) {
        return NULL;
    }

    return ibp_calls[beacon].call;
}

const char *propagation_ibp_site(int beacon) {
    if (beacon < 0 || beacon >= PROPAGATION_IBP_BEACONS) {
        return NULL;
    }

    return ibp_calls[beacon].site;
}

const char *propagation_ibp_grid(int beacon) {
    if (beacon < 0 || beacon >= PROPAGATION_IBP_BEACONS) {
        return NULL;
    }

    return ibp_calls[beacon].grid;
}

int propagation_ibp_band_of_freq(uint32_t hz) {
    for (int b = 0; b < PROPAGATION_IBP_BANDS; b++) {
        if (hz >= ibp_bands[b].low_hz && hz <= ibp_bands[b].high_hz) {
            return b;
        }
    }

    return -1;
}

/* --- Turno de los beacons ------------------------------------------------- */

/* Indice del beacon que transmite en el slot `slot` de la banda `band`. Los
 * slots son de 10 s y la rotacion de los 18 beacons es de 180 s; las bandas van
 * corridas un slot, asi que en todo momento hay exactamente un beacon por banda. */
static int slot_beacon(int64_t slot, int band) {
    int64_t i = (slot - band) % PROPAGATION_IBP_BEACONS;

    if (i < 0) {
        i += PROPAGATION_IBP_BEACONS;
    }

    return (int) i;
}

bool propagation_slot(int64_t epoch, int band, int *beacon, int64_t *slot_start, int *slot_left) {
    if (epoch < 0 || band < 0 || band >= PROPAGATION_IBP_BANDS) {
        return false;
    }

    int64_t slot  = epoch / PROPAGATION_IBP_SLOT_S;
    int64_t start = slot * PROPAGATION_IBP_SLOT_S;

    if (beacon) {
        *beacon = slot_beacon(slot, band);
    }

    if (slot_start) {
        *slot_start = start;
    }

    if (slot_left) {
        *slot_left = (int) (start + PROPAGATION_IBP_SLOT_S - epoch);
    }

    return true;
}

bool propagation_beacon_slot(int64_t epoch, int band, int beacon, int64_t *slot_start, int *wait_s) {
    if (epoch < 0 || band < 0 || band >= PROPAGATION_IBP_BANDS ||
        beacon < 0 || beacon >= PROPAGATION_IBP_BEACONS) {
        return false;
    }

    int64_t slot      = epoch / PROPAGATION_IBP_SLOT_S;
    int64_t cur       = slot_beacon(slot, band);
    int64_t wait_slots = (beacon - cur) % PROPAGATION_IBP_BEACONS;

    if (wait_slots < 0) {
        wait_slots += PROPAGATION_IBP_BEACONS;
    }

    int64_t start = (slot + wait_slots) * PROPAGATION_IBP_SLOT_S;
    int64_t wait  = start - epoch;

    /* Si el beacon esta transmitiendo ahora, su slot ya empezo: la espera es 0. */
    if (wait < 0) {
        wait = 0;
    }

    if (slot_start) {
        *slot_start = start;
    }

    if (wait_s) {
        *wait_s = (int) wait;
    }

    return true;
}

int propagation_plan(int64_t epoch, int band, int beacons[PROPAGATION_PLAN_MAX]) {
    if (epoch < 0 || band < 0 || band >= PROPAGATION_IBP_BANDS || beacons == NULL) {
        return 0;
    }

    int64_t slot = epoch / PROPAGATION_IBP_SLOT_S;
    int     n    = 0;

    /* El slot que corre ahora ya empezo y no se puede medir desde su principio,
     * asi que el plan arranca en el siguiente. */
    for (int k = 1; k <= PROPAGATION_PLAN_MAX; k++) {
        beacons[n++] = slot_beacon(slot + k, band);
    }

    return n;
}

/* --- Direccion ------------------------------------------------------------ */

/* Grados a radianes. */
#define PROPAGATION_RAD(deg) ((deg) * M_PI / 180.0)

int propagation_bearing(double lat1, double lon1, double lat2, double lon2) {
    double p1 = PROPAGATION_RAD(lat1);
    double p2 = PROPAGATION_RAD(lat2);
    double dl = PROPAGATION_RAD(lon2 - lon1);

    /* Formula del rumbo inicial de un circulo maximo. En los polos el rumbo no
     * esta definido (sin y cos se anulan): ahi vale 0 (norte), como en el punto
     * consigo mismo. */
    double y = sin(dl) * cos(p2);
    double x = cos(p1) * sin(p2) - sin(p1) * cos(p2) * cos(dl);

    if (y == 0.0 && x == 0.0) {
        return 0;
    }

    /* Se normaliza ANTES de redondear: redondear un angulo negativo y truncar
     * despues da un grado de mas (el oeste daria 271 en vez de 270). */
    double deg = atan2(y, x) * 180.0 / M_PI;

    if (deg < 0.0) {
        deg += 360.0;
    }

    return (int) (deg + 0.5) % 360;
}

const char *propagation_compass(int bearing_deg) {
    static const char *const points[8] = { "N", "NE", "E", "SE", "S", "SO", "O", "NO" };

    int deg = bearing_deg % 360;

    if (deg < 0) {
        deg += 360;
    }

    /* Los ocho sectores de 45 grados, con el centro en el punto cardinal: 0, 45,
     * 90... El redondeo a la decena del medio (22) todavia es del sector
     * anterior, y 23 ya es del siguiente. */
    int i = ((deg + 22) % 360) / 45;

    return points[i];
}

/* --- Medicion e interpretacion ------------------------------------------- */

int propagation_snr(int signal_dbm, int noise_dbm) {
    return signal_dbm - noise_dbm;
}

propagation_verdict_t propagation_verdict(int snr_db, bool heard, int open_thr, int marginal_thr) {
    /* Sin senal distinguible del piso de ruido no hay nada que interpretar: la
     * banda no dio senal. (Silencio no es lo mismo que "cerrada": la pantalla lo
     * dice con todas las letras.) */
    if (!heard) {
        return PROPAGATION_VERDICT_CLOSED;
    }

    if (snr_db >= open_thr) {
        return PROPAGATION_VERDICT_OPEN;
    }

    if (snr_db >= marginal_thr) {
        return PROPAGATION_VERDICT_MARGINAL;
    }

    return PROPAGATION_VERDICT_CLOSED;
}

const char *propagation_verdict_name(propagation_verdict_t v) {
    switch (v) {
        case PROPAGATION_VERDICT_OPEN:     return "OPEN";
        case PROPAGATION_VERDICT_MARGINAL: return "MARGINAL";
        case PROPAGATION_VERDICT_CLOSED:   return "CLOSED";
        case PROPAGATION_VERDICT_NO_DATA:  break;
    }

    return "NO DATA";
}

/* --- Escalera de potencia (medicion por ventana del slot) ----------------- */

int propagation_ladder_steps(void) {
    return PROPAGATION_LADDER_STEPS;
}

int propagation_ladder_power_dw(int step) {
    if (step < 0 || step >= PROPAGATION_LADDER_STEPS) {
        return 0;
    }

    /* 100 W -> 1000 dw, y cada escalon baja 10x */
    const int dw[PROPAGATION_LADDER_STEPS] = { 1000, 100, 10, 1 };

    return dw[step];
}

bool propagation_ladder_window(int step, int *start_ms, int *len_ms) {
    /* 100 W ocupa el indicativo y el primer dash; 10/1/0.1 W son un dash de 1 s
     * cada uno, con 1 s de espacio entre ellos y el ultimo pegado al final del
     * slot (ver propagation.h). */
    static const int start[PROPAGATION_LADDER_STEPS] = { 0, 5000, 7000, 9000 };

    if (step < 0 || step >= PROPAGATION_LADDER_STEPS) {
        return false;
    }

    if (start_ms) {
        *start_ms = start[step];
    }

    if (len_ms) {
        *len_ms = (step == 0) ? 5000 : PROPAGATION_LADDER_WINDOW_MS;
    }

    return true;
}

int propagation_ladder_from_slot(const int step_max_db[PROPAGATION_LADDER_STEPS], int noise_db,
                                 int detect_db, int snr_out[PROPAGATION_LADDER_STEPS]) {
    int steps = 0;

    if (step_max_db == NULL) {
        return 0;
    }

    for (int k = 0; k < PROPAGATION_LADDER_STEPS; k++) {
        int snr = propagation_snr(step_max_db[k], noise_db);

        if (snr_out) {
            snr_out[k] = snr;
        }

        /* El primero que no se distingue del ruido corta la escalera: los que
         * siguen son 10x mas debiles, no tiene sentido contarlos. */
        if (snr < detect_db) {
            break;
        }

        steps++;
    }

    return steps;
}

/* --- Umbrales del veredicto (presets) ------------------------------------ */

/* Pares (abierta, marginal) en dB. El primero es el de fabrica (12/6) y los
 * demas son los mismos criterios corridos: mas estricto = mas abierta. Cada
 * umbral bajo pide mas senal para declarar la banda abierta, y el marginal queda
 * siempre por debajo. */
static const struct {
    int open_db;
    int marg_db;
} snr_presets[] = {
    { 12, 6 },
    { 15, 8 },
    { 18, 10 },
    { 21, 12 },
    { 9, 4 },
};

int propagation_snr_preset_count(void) {
    return (int) (sizeof(snr_presets) / sizeof(snr_presets[0]));
}

bool propagation_snr_preset(int i, int *open_db, int *marg_db) {
    if (i < 0 || i >= propagation_snr_preset_count()) {
        return false;
    }

    if (open_db) {
        *open_db = snr_presets[i].open_db;
    }

    if (marg_db) {
        *marg_db = snr_presets[i].marg_db;
    }

    return true;
}

int propagation_snr_preset_index(int open_db, int marg_db) {
    for (int i = 0; i < propagation_snr_preset_count(); i++) {
        if (snr_presets[i].open_db == open_db && snr_presets[i].marg_db == marg_db) {
            return i;
        }
    }

    return -1;
}

/* --- Registro IARU Region 2 (10 m) -------------------------------------- */

/* El blob son registros de ancho fijo sin terminador (ver propagation_data.h): el
 * indicativo se copia a este buffer para poder pasarlo como cadena. Se reusa en
 * cada llamada, asi que hay que copiarlo antes de volver a pedir un registro. */
static char iaru_call_buf[PROPAGATION_IARU_CALL_LEN + 1];

int propagation_iaru_count(void) {
    return (int) (propagation_iaru_blob_len / PROPAGATION_IARU_REC);
}

/* Descompone el registro `i`. `call` puede ser NULL. */
static bool iaru_at(int i, const char **call, uint32_t *hz, bool *active) {
    if (i < 0 || i >= propagation_iaru_count()) {
        return false;
    }

    const char *rec = propagation_iaru_blob + (size_t) i * PROPAGATION_IARU_REC;

    if (call) {
        int n = PROPAGATION_IARU_CALL_LEN;

        while (n > 0 && rec[n - 1] == ' ') {
            n--;
        }

        memcpy(iaru_call_buf, rec, (size_t) n);
        iaru_call_buf[n] = '\0';
        *call = iaru_call_buf;
    }

    if (hz) {
        uint32_t v = 0;

        for (int k = PROPAGATION_IARU_CALL_LEN; k < PROPAGATION_IARU_CALL_LEN + 8; k++) {
            v = v * 10 + (uint32_t) (rec[k] - '0');
        }

        *hz = v;
    }

    if (active) {
        *active = rec[PROPAGATION_IARU_CALL_LEN + 8] == '1';
    }

    return true;
}

bool propagation_iaru_beacon(int i, const char **call, uint32_t *hz, bool *active) {
    return iaru_at(i, call, hz, active);
}

int propagation_iaru_active_count(void) {
    int n = 0;

    for (int i = 0; i < propagation_iaru_count(); i++) {
        bool active;

        if (iaru_at(i, NULL, NULL, &active) && active) {
            n++;
        }
    }

    return n;
}

const char *propagation_iaru_find(uint32_t hz) {
    for (int i = 0; i < propagation_iaru_count(); i++) {
        uint32_t   f;
        bool       active;
        const char *call;

        if (!iaru_at(i, &call, &f, &active) || !active) {
            continue;
        }

        if (f == hz) {
            return call;
        }

        /* Los registros estan ordenados por frecuencia: pasado el punto, no
         * esta. */
        if (f > hz) {
            return NULL;
        }
    }

    return NULL;
}
