/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Auto Tune multimode spectrum policies (issue #205)
 *
 *  Copyright (c) 2024 LU2DSX
 *
 *  Politica pura (sin LVGL, sin radio, sin Subjects, sin reloj propio) de las
 *  dos estrategias espectrales del Auto Tune multimodo. El nucleo CW/CWR
 *  (src/autotune.h) queda intacto: esto es una pareja nueva, no una extension
 *  de aquel.
 *
 *  La instantanea del DSP (`dsp_autotune_spectrum_t`) vive en este header y
 *  src/dsp.h la reexporta: es el contrato de la fuente espectral que produce
 *  src/dsp.cpp y que consume el coordinador. El tipo tiene que poder compilarse
 *  sin `liquid/liquid.h` ni `radio.h` (los que arrastra src/dsp.h), asi que la
 *  declaracion vive aca y el DSP la incluye.
 *
 *  Coordenadas (issue #205, seccion 4): N=800 bins, centro en 400 y
 *  `offset_hz(i) = (i - 400) * width_hz / 800`. El frame INTERPOLA 512 bins
 *  fisicos, asi que la resolucion conservadora es `width_hz / 512` y nunca
 *  `width_hz / 800`: por eso los candidatos se cuantizan a
 *  `step_hz = max(1, round(resolution_hz))` y no se promete precision de subbin.
 *
 *  Las dos politicas son CONSERVADORAS por diseno. AM/SAM mueve el dial una
 *  sola vez, con verificacion y rollback, y solo ante una portadora estrecha,
 *  unica y dominante. USB/LSB nunca mueve el dial: propone un desplazamiento
 *  grueso de energia y el operador tiene que confirmarlo con una segunda
 *  pulsacion. Ninguna de las dos promete exactitud de portadora ni afinacion de
 *  voz: la resolucion del PSD del equipo no da para eso.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- la instantanea del DSP (seccion 4 del issue) -------------------------- */

#define AUTOTUNE_SPECTRUM_BINS      800
#define AUTOTUNE_SPECTRUM_CENTER    400
/* Bins FISICOS que el frame de 800 interpola (AveragedPSD<512, 800>): la
 * resolucion real del PSD y el tope de lo que cualquier politica puede afirmar. */
#define AUTOTUNE_SPECTRUM_PHYSICAL_BINS 512

typedef struct {
    uint64_t sequence;          /* monotonia por publicacion (no por tick de LVGL) */
    uint64_t timestamp_ms;      /* get_time() de la publicacion */
    uint64_t epoch;             /* sube en cada invalidacion (write propio, QSY, modo, ancho, TX/RX) */
    int32_t  center_freq_hz;    /* frecuencia del dial en esa publicacion */
    int32_t  mode;              /* x6200_mode_t vigente */
    int32_t  width_hz;          /* ancho mostrado (cfg_cur.fft_width), nunca una suposicion */
    bool     tx;                /* la publicacion es de TX: no sirve para medir RX */
    bool     valid;             /* sin TX, ancho positivo y todos los bins finitos */
    float    db[AUTOTUNE_SPECTRUM_BINS];
} dsp_autotune_spectrum_t;

/* --- constantes de las politicas (internas, no settings) ------------------- */

/* Ambos modos: por encima de este ancho o de esta resolucion la medida no
 * alcanza ni para un centrado aproximado (seccion 5: deja pasar la vista habitual
 * de ~100 kHz/512 ~= 195 Hz y rechaza menos resolucion). */
#define AUTOTUNE_SPECTRUM_MAX_WIDTH_HZ       100000
#define AUTOTUNE_SPECTRUM_MAX_RESOLUTION_HZ  250.0f

/* AM/SAM (seccion 5): cuatro tramas validas, ventana de portadora, piso de ruido
 * a los costados, dominancia sobre el segundo pico y tope del unico movimiento. */
#define AUTOTUNE_AM_FRAMES                   4
#define AUTOTUNE_AM_CARRIER_WINDOW_HZ        1000
#define AUTOTUNE_AM_NOISE_MIN_HZ             2500
#define AUTOTUNE_AM_NOISE_MAX_HZ             5000
#define AUTOTUNE_AM_NOISE_MIN_BINS           12
#define AUTOTUNE_AM_PEAK_MIN_DB              12.0f
#define AUTOTUNE_AM_DOMINANCE_MIN_DB         6.0f
#define AUTOTUNE_AM_MAX_DELTA_HZ             1000

/* USB/LSB (seccion 6): cinco tramas, piso de ruido mas lejos del filtro, tope de
 * la sugerencia y los tres umbrales del score de energia ocupada. */
#define AUTOTUNE_SSB_FRAMES                  5
#define AUTOTUNE_SSB_NOISE_MIN_HZ            3500
#define AUTOTUNE_SSB_NOISE_MAX_HZ            5000
#define AUTOTUNE_SSB_NOISE_MIN_BINS          12
#define AUTOTUNE_SSB_MAX_DELTA_HZ            800
#define AUTOTUNE_SSB_MIN_DELTA_HZ            200
#define AUTOTUNE_SSB_MIN_SCORE               5.0f
#define AUTOTUNE_SSB_RATIO_VS_ZERO           1.7f
#define AUTOTUNE_SSB_RATIO_VS_SECOND         1.25f
#define AUTOTUNE_SSB_MIN_WINDOW_BINS         8
#define AUTOTUNE_SSB_FILTER_MIN_HZ           1800
#define AUTOTUNE_SSB_FILTER_MAX_HZ           3500
/* Tope de la mediana: mas que esto no es un desplazamiento grueso, es otra cosa. */
#define AUTOTUNE_SSB_MEDIAN_MAX_HZ           800

/* --- geometria y resolucion (puras, compartidas por politica y tests) ------ */

/* Offset en Hz del bin `i` respecto del centro del espectro. */
static inline float autotune_spectrum_offset_hz(int32_t bin, int32_t width_hz) {
    return ((float) (bin - AUTOTUNE_SPECTRUM_CENTER)) * (float) width_hz /
           (float) AUTOTUNE_SPECTRUM_BINS;
}

/* Resolucion FISICA del PSD: 512 bins, no 800. */
static inline float autotune_spectrum_resolution_hz(int32_t width_hz) {
    return (float) width_hz / (float) AUTOTUNE_SPECTRUM_PHYSICAL_BINS;
}

/* Paso de cuantizacion de los candidatos: entero, >= 1 Hz, alejandose del cero. */
static inline int32_t autotune_spectrum_step_hz(int32_t width_hz) {
    float res = autotune_spectrum_resolution_hz(width_hz);
    int32_t step = (int32_t) (res + 0.5f);

    return (step < 1) ? 1 : step;
}

/* El ancho y la resolucion que ambas politicas exigen para medir. */
static inline bool autotune_spectrum_measurable(const dsp_autotune_spectrum_t *s) {
    if (s == NULL || !s->valid || s->tx) {
        return false;
    }
    if (s->width_hz <= 0 || s->width_hz > AUTOTUNE_SPECTRUM_MAX_WIDTH_HZ) {
        return false;
    }
    return autotune_spectrum_resolution_hz(s->width_hz) <= AUTOTUNE_SPECTRUM_MAX_RESOLUTION_HZ;
}

/* --- gate de la trama: frescura y coherencia del contexto ------------------ */

/* Que hizo el gate con una publicacion. */
typedef enum {
    AUTOTUNE_FRAME_SKIP = 0,    /* invalida, de TX, vieja, repetida o anterior a la guarda */
    AUTOTUNE_FRAME_TAKEN,       /* fresca y del mismo contexto de medicion */
    AUTOTUNE_FRAME_BROKEN,      /* el contexto de la medicion cambio: hay que cancelar */
} autotune_frame_verdict_t;

/* Frescura real, la misma disciplina del nucleo CW: secuencia estrictamente
 * creciente y timestamp estrictamente creciente, piso de tiempo (guarda
 * post-escritura) y descarte de las primeras publicaciones posteriores a una
 * escritura. Ademas fija el contexto de la fase (epoch, centro, modo, ancho) con
 * la PRIMERA trama aceptada y lo exige igual en las siguientes: si el epoch cambia
 * (escritura propia, QSY, modo, ancho, TX/RX) la medida no es comparable y el gate
 * lo dice con BREAK, nunca mezcla tramas de dos contextos. */
typedef struct {
    bool     have_last;
    uint64_t last_seq;
    uint64_t last_ts;
    bool     latched;
    bool     epoch_set;   /* true: el epoch esperado lo fijo el llamador (post-escritura) */
    uint64_t epoch;
    int32_t  center_freq_hz;
    int32_t  mode;
    int32_t  width_hz;
    uint64_t min_ts;    /* piso de timestamp (get_time() + guarda) */
    uint64_t min_seq;   /* secuencias <= a esta se descartan (las N post-escritura) */
} autotune_frame_gate_t;

void autotune_frame_gate_reset(autotune_frame_gate_t *g);

/* Arma el piso de frescura de una fase nueva: `min_ts` es el instante minimo de
 * publicacion, `min_seq` la ultima secuencia que NO vale (0 = sin descarte) y
 * `epoch` el epoch exigido (0 = el que traiga la primera trama aceptada). El
 * epoch explicito es lo que deja afuera las publicaciones anteriores a una
 * escritura propia cuando el DSP todavia no volvio a publicar. */
void autotune_frame_gate_arm(autotune_frame_gate_t *g, uint64_t min_ts, uint64_t min_seq,
                             uint64_t epoch);

autotune_frame_verdict_t autotune_frame_gate_take(autotune_frame_gate_t *g,
                                                  const dsp_autotune_spectrum_t *s);

/* --- AM/SAM: portadora estrecha, unica y dominante (seccion 5) ------------- */

typedef enum {
    AUTOTUNE_STEP_REJECTED = 0,   /* la trama no cumple la politica de la trama */
    AUTOTUNE_STEP_COLLECTING,     /* aceptada; todavia no hay N tramas consistentes */
    AUTOTUNE_STEP_READY,          /* hay N tramas consistentes: la mediana es la respuesta */
} autotune_step_t;

typedef enum {
    AUTOTUNE_VERDICT_NONE = 0,    /* sin portadora/sugerencia confiable: no se mueve el dial */
    AUTOTUNE_VERDICT_CENTERED,    /* AM/SAM ya centrado dentro de la resolucion: sin escritura */
    AUTOTUNE_VERDICT_MOVE,        /* AM/SAM: hay un unico desplazamiento valido */
    AUTOTUNE_VERDICT_SUGGEST,     /* USB/LSB: hay una propuesta que el operador debe confirmar */
} autotune_verdict_t;

typedef struct {
    int32_t offset_hz[AUTOTUNE_AM_FRAMES];
    int32_t n;
    int32_t sign;        /* signo de los candidatos ya aceptados (0 = ninguno aun) */
    float   resolution_hz;
    bool    have_resolution;
} autotune_am_state_t;

/* Candidato de portadora de UNA trama: cuantizado a step_hz, sin interpolacion
 * subbin. false = la trama no tiene portadora inequivoca (ancho/resolucion,
 * falta de bins, piso de ruido ausente, no finitos, empate, pico debil o segundo
 * pico demasiado cerca). */
bool autotune_spectrum_am_candidate(const dsp_autotune_spectrum_t *s, int32_t *offset_hz);

void autotune_am_reset(autotune_am_state_t *st);

/* Suma una trama. Ventana corrediza: si la trama nueva rompe la consistencia
 * (spread > resolucion) se descarta la mas vieja y se sigue juntando. Nunca
 * mezcla signos. */
autotune_step_t autotune_am_take(autotune_am_state_t *st, const dsp_autotune_spectrum_t *s);

/* Veredicto con el estado ya READY: CENTERED si |mediana| <= resolucion/2,
 * NONE si la mediana supera el tope, MOVE con `delta_hz` en los demas casos. */
autotune_verdict_t autotune_am_verdict(const autotune_am_state_t *st, int32_t *delta_hz);

/* true si `delta_hz` (ya aplicado) deja una portadora dentro de la resolucion en
 * la fase de verificacion. */
bool autotune_spectrum_am_verified(const autotune_am_state_t *st);

/* --- USB/LSB: energia ocupada dentro de la banda lateral (seccion 6) ------- */

typedef struct {
    int32_t delta_hz[AUTOTUNE_SSB_FRAMES];
    int32_t n;
    int32_t sign;
    float   resolution_hz;
    bool    have_resolution;
} autotune_ssb_state_t;

/* Sugerencia de UNA trama: el desplazamiento `d` (multiplo de step_hz, |d| <=
 * AUTOTUNE_SSB_MAX_DELTA_HZ) que maximiza la energia ocupada dentro de la banda
 * RX desplazada. `from`/`to` son los bordes REALES del filtro (cfg_cur.filter.real)
 * y `voice` elige la banda lateral (USB: from >= 0; LSB: to <= 0).
 * false = no hay sugerencia (filtro fuera del contrato, mismo lado esperado
 * ausente, piso de ruido ausente, no finitos, umbrales del score sin cumplir,
 * empate, o desplazamiento nulo/infimo/por encima del tope).
 * Esto es un indicador de ENERGIA OCUPADA, no un score de calidad de voz. */
bool autotune_spectrum_ssb_candidate(const dsp_autotune_spectrum_t *s, int32_t from, int32_t to,
                                     bool voice_lsb, int32_t *delta_hz);

void autotune_ssb_reset(autotune_ssb_state_t *st);

autotune_step_t autotune_ssb_take(autotune_ssb_state_t *st, const dsp_autotune_spectrum_t *s,
                                  int32_t from, int32_t to, bool voice_lsb);

/* Veredicto con el estado READY: SUGGEST (con la mediana redondeada) o NONE. */
autotune_verdict_t autotune_ssb_verdict(const autotune_ssb_state_t *st, int32_t *delta_hz);
/* --- bandas laterales (pura, la comparten el coordinador y los tests) ------ */

/* El filtro real declarado corresponde a la banda lateral que la politica exige:
 * USB `from >= 0 && to > from`; LSB `from < to && to <= 0`; y el ancho `to - from`
 * dentro de [1800, 3500] Hz. */
bool autotune_spectrum_ssb_filter_ok(int32_t from, int32_t to, bool voice_lsb);

/* --- ventanas y pulsaciones de la session (secciones 4, 6 y 7) -------------
 *
 * El coordinador es el unico que tiene reloj, pero las decisiones que dependen de
 * el (que plazo tiene cada fase, cuando vencio y que significa una pulsacion de
 * ACTION_ATUNE) son politica pura: viven aca para que el test nativo las pueda
 * verificar sin LVGL. */

#define AUTOTUNE_COLLECT_MS       3000
#define AUTOTUNE_VERIFY_MS        2500
#define AUTOTUNE_PREVIEW_MS       5000
#define AUTOTUNE_UNDO_MS          5000
#define AUTOTUNE_POST_WRITE_DROP  3

typedef enum {
    AUTOTUNE_PHASE_IDLE = 0,
    AUTOTUNE_PHASE_COLLECT,
    AUTOTUNE_PHASE_VERIFY,    /* solo AM/SAM: provisional escrita, sin confirmar */
    AUTOTUNE_PHASE_PREVIEW,   /* solo USB/LSB: propuesta mostrada, sin escritura */
    AUTOTUNE_PHASE_UNDO,      /* AM/SAM verificado o USB/LSB aplicado: Undo vivo */
} autotune_phase_t;

/* Que significa una pulsacion de ACTION_ATUNE sobre la fase en curso. */
typedef enum {
    AUTOTUNE_PRESS_CANCEL = 0,  /* termina la sesion (el rollback lo decide la fase) */
    AUTOTUNE_PRESS_APPLY,       /* PREVIEW: segunda pulsacion explicita, una escritura */
    AUTOTUNE_PRESS_UNDO,        /* UNDO: restaura el origen dentro de la ventana */
} autotune_press_t;

/* Plazo de una fase abierta en `now_ms` (VERIFY suma aparte la guarda de 300 ms
 * posterior a la escritura: el llamador ya se la paso sumada en `now_ms`). */
uint64_t autotune_phase_deadline(autotune_phase_t phase, uint64_t now_ms);

/* true si la ventana venció (el vencimiento no hace valida ninguna muestra). */
bool autotune_phase_expired(uint64_t deadline_ms, uint64_t now_ms);

/* true en las fases que miden (COLLECT/VERIFY): PREVIEW y UNDO solo esperan. */
bool autotune_phase_measures(autotune_phase_t phase);

/* Significado de la pulsacion en esa fase. IDLE no es una pulsacion sobre una
 * sesion viva: devuelve CANCEL y el llamador no la usa. */
autotune_press_t autotune_phase_press(autotune_phase_t phase);

/* Rollback al cancelar: solo la fase con una escritura SIN confirmar (VERIFY)
 * repone el origen. PREVIEW no escribio y UNDO ya tiene un valor que el operador
 * confirmo: jamas se revierte una decision externa. */
bool autotune_phase_restores_on_cancel(autotune_phase_t phase);

#ifdef __cplusplus
}
#endif
