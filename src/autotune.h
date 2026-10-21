/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Fine-tuning assistant (autotune)
 *
 *  Copyright (c) 2024 LU2DSX
 *
 *  Deterministic, UI-free core of the operator's CW fine-tuning assistant
 *  (issue #190, "Auto Tune v2"). The operator asks for it explicitly; the
 *  session collects three fresh and stable CW tone measurements from the
 *  decoder's own FFT, decides ONE correction of at most +/-300 Hz (CW/CWR only,
 *  RX only), writes it once and verifies it with three new frames. When the
 *  measurement is not trustworthy it does not move the dial at all: better no
 *  adjustment than a wrong one.
 *
 *  Input contract (`autotune_sample_t`): the sample comes from
 *  `cw_get_tune_sample()` (src/cw.cpp) and carries the *normalized* error of the
 *  received tone with respect to the configured pitch - `error_hz > 0` means the
 *  tone is ABOVE the pitch, in CW and in CWR alike. The DSP layer is the only
 *  place where the FFT/DDS sign is interpreted (the raw `peak_freq` of
 *  src/cw.cpp is the negative of the tone's offset, docs/HALLAZGOS.md 52); this
 *  core never sees a raw FFT coordinate.
 *
 *  Dial ownership (`autotune_owns`, `autotune_context_broken`): the rule of
 *  issue #190 section 5 lives here too, as a comparison of the context snapshot
 *  taken at the trigger with the context read from the radio at any later
 *  moment. That is what makes "a manual/CAT/VFO/band/mode/PTT change always
 *  wins" a tested fact instead of a claim: the UI only gathers the fields and
 *  asks this module.
 *
 *  This module does no I/O, no LVGL, no radio access, no heap and no clock of
 *  its own: the caller passes `now_ms` (get_time()), the context and the band
 *  range, so the whole policy - thresholds, stability, single delta,
 *  verification and ownership - is exercised by the native tests with synthetic
 *  samples and a fake clock.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- contrato numerico (issue #190, seccion 2) ----------------------------
 *
 * Constantes internas, no settings: son el contrato de la funcion, no algo que
 * el operador ajuste.
 *  - PHASE_SAMPLES: tres tramas de FFT distintas, frescas y validas por fase.
 *  - PHASE_TIMEOUT_MS: esas tres tramas tienen que llegar dentro de este plazo.
 *  - MIN_SNR_RATIO: sum_signal/noise minimo por trama (equivale a 6 dB de
 *    potencia); ruido cero, no finitos y silencios son invalidos.
 *  - SAMPLE_ERROR_LIMIT_HZ: rango del estimador de la FFT de src/cw.cpp
 *    (+/-344 Hz de mapeo). Fuera de el la medida no es un tono.
 *  - MAX_SPREAD_HZ: max-min de las tres tramas usadas.
 *  - DEADBAND_HZ: |mediana| dentro de esto ya esta sintonizado (sin write).
 *  - MAX_DELTA_HZ: tope de la unica correccion. Por encima NO se recorta: no se
 *    mueve el dial y se reporta falta de medida confiable.
 *  - SETTLE_GUARD_MS: espera minima tras la escritura antes de aceptar tramas de
 *    verificacion (el vencimiento del timer no prueba frescura: ademas se exige
 *    secuencia nueva y timestamp posterior a la escritura).
 *  - SESSION_TIMEOUT_MS: tope total de la sesion desde el trigger.
 */
#define AUTOTUNE_PHASE_SAMPLES          3
#define AUTOTUNE_PHASE_TIMEOUT_MS       3000
#define AUTOTUNE_MIN_SNR_RATIO          4.0f
#define AUTOTUNE_SAMPLE_ERROR_LIMIT_HZ  344.0f
#define AUTOTUNE_MAX_SPREAD_HZ          20.0f
#define AUTOTUNE_DEADBAND_HZ            10.0f
#define AUTOTUNE_MAX_DELTA_HZ           300.0f
#define AUTOTUNE_SETTLE_GUARD_MS        300
#define AUTOTUNE_SESSION_TIMEOUT_MS     7000

/* Modo de recepcion: es lo unico que cambia el signo de la correccion del dial
 * (banda lateral normal vs invertida). */
typedef enum {
    AUTOTUNE_MODE_CW = 0,
    AUTOTUNE_MODE_CWR,
} autotune_mode_t;

/* Estados exactos del issue #190: IDLE -> COLLECT -> VERIFY -> IDLE. */
typedef enum {
    AUTOTUNE_STATE_IDLE = 0,
    AUTOTUNE_STATE_COLLECT,
    AUTOTUNE_STATE_VERIFY,
} autotune_state_t;

/* Una trama de FFT publicada por el hilo del DSP (src/cw.cpp). */
typedef struct {
    uint64_t sequence;      /* monotonía por trama de FFT (no por tick de LVGL) */
    uint64_t timestamp_ms;  /* get_time() de la trama */
    float    error_hz;      /* > 0: tono RX por encima del pitch objetivo */
    float    ratio;         /* sum_signal/noise de esa trama (lineal) */
    float    dds_offset_hz; /* corrimiento del DDS vigente en esa trama */
    bool     valid;         /* gate marca/ratio/finitud/rango, en el hilo del DSP */
} autotune_sample_t;

/* Contexto del dial: lo que la seccion 5 del issue exige comparar contra el
 * snapshot del trigger antes de escribir una sola frecuencia. */
typedef struct {
    int32_t mode;     /* x6200_mode_t vigente (CW/CWR son los unicos validos) */
    int32_t band_id;  /* identidad de la banda seleccionada (cfg.band_id) */
    int32_t vfo;      /* VFO A/B (cfg_cur.band->vfo) */
    int32_t pitch;    /* pitch objetivo (cfg.key_tone) */
    int32_t freq;     /* frecuencia actual del dial (Hz) */
    bool    rx;       /* radio_get_state() == RADIO_RX */
    bool    locked;   /* bloqueo de frecuencia activo */
    bool    rit;      /* RIT distinto de cero */
} autotune_context_t;

/* Que hizo `autotune_take()` con la trama. */
typedef enum {
    AUTOTUNE_TAKE_IGNORED = 0,  /* invalida, vieja, duplicada o fuera de plazo */
    AUTOTUNE_TAKE_MORE,         /* aceptada; todavia no hay tres estables */
    AUTOTUNE_TAKE_READY,        /* tres tramas validas y estables: hay mediana */
} autotune_take_t;

/* Veredicto de una fase completa. */
typedef enum {
    AUTOTUNE_RESULT_TUNED = 0,   /* ya esta dentro de +/-10 Hz: sin escritura */
    AUTOTUNE_RESULT_CORRECT,     /* hay un unico target valido para escribir */
    AUTOTUNE_RESULT_NO_ADJUST,   /* no hay ajuste confiable: no se toca el dial */
} autotune_result_t;

/* Estado completo de la sesion (sin UI, sin reloj propio, sin heap). */
typedef struct {
    autotune_state_t    state;
    autotune_mode_t     mode;
    autotune_context_t  snap;             /* contexto del trigger (seccion 5) */
    int32_t             start_freq;      /* el dial del arranque (ancla) */
    int32_t             expected_freq;   /* la ultima frecuencia que Auto Tune espera */
    bool                wrote;           /* hubo la unica escritura provisional */
    int32_t             delta_hz;        /* la correccion aplicada (con su signo) */
    /* fase en curso */
    int      n;                                   /* tramas usadas de la fase */
    float    err[AUTOTUNE_PHASE_SAMPLES];         /* errores de esas tramas */
    float    dds_offset;                          /* centro del DDS de la fase */
    uint64_t phase_start_ms;                      /* cuando arranco la fase */
    uint64_t min_ms;                              /* piso de timestamp (guarda) */
    uint64_t last_seq;                            /* ultima secuencia aceptada */
    uint64_t last_ts;                             /* timestamp de esa secuencia */
    bool     have_last;                           /* hay una trama aceptada previa */
    uint64_t session_start_ms;                    /* trigger de la sesion */
} autotune_t;

/* --- estadistica y politica puras ----------------------------------------- */

/* Mediana de `n` valores (copia + insertion sort: robusta a un atipico). n <= 0
 * devuelve 0. */
float autotune_median(const float *values, int n);

/* true si `max - min <= AUTOTUNE_MAX_SPREAD_HZ` entre las `n` tramas. n <= 0
 * devuelve false (sin tramas no hay estabilidad que afirmar). */
bool autotune_stable(const float *values, int n);

/* Gate de una trama: valida, finita, con SNR suficiente y dentro del rango del
 * estimador. Es el mismo criterio que fija `valid` en el hilo del DSP, repetido
 * en el nucleo para que la politica no dependa del productor. */
bool autotune_sample_ok(const autotune_sample_t *s);

/* Correccion de dial para una mediana y un modo, SIN recortar:
 * CW -> +round(mediana); CWR -> -round(mediana). */
int32_t autotune_dial_delta(float median_hz, autotune_mode_t mode);

/* El destino pertenece al rango configurado de la banda (inclusive). */
bool autotune_target_in_band(int32_t target, uint32_t band_start, uint32_t band_stop);

/* --- propiedad del dial (seccion 5) --------------------------------------- */

/* Misma identidad de contexto: banda, VFO y pitch. La frecuencia del dial no
 * entra: moverse el dial es una orden manual, no un cambio de contexto. */
bool autotune_context_identity_equal(const autotune_context_t *a, const autotune_context_t *b);

/* El contexto del snapshot ya no es el vigente: la sesion tiene que terminar
 * (modo/banda/VFO/pitch distintos, no-RX, lock o RIT). Un estado IDLE tambien da
 * true: sin sesion no hay contexto que respetar. */
bool autotune_context_broken(const autotune_t *at, const autotune_context_t *now);

/* Unica condicion bajo la cual Auto Tune puede escribir una frecuencia: sesion
 * viva, contexto identico al snapshot, RX, sin lock ni RIT, y el dial donde el
 * propio Auto Tune lo dejo (nadie mas lo movio). */
bool autotune_owns(const autotune_t *at, const autotune_context_t *now);

/* --- sesion --------------------------------------------------------------- */

/* Deja el estado en IDLE y descarta las muestras: el segundo trigger arranca
 * limpio y ningun estado terminal queda con actividad. */
void autotune_reset(autotune_t *at);

/* Arranca la fase COLLECT anclada en `ctx->freq`. `last_seq` es la ultima
 * secuencia observada antes del trigger (0 si no habia muestra). */
void autotune_collect_start(autotune_t *at, autotune_mode_t mode,
                            const autotune_context_t *ctx, uint64_t now_ms,
                            uint64_t last_seq);

/* Registra la unica escritura provisional: el dial pasa a ser lo que Auto Tune
 * espera haber dejado. */
void autotune_note_write(autotune_t *at, int32_t target);

/* Registra el rollback: el dial vuelve a ser el del arranque. */
void autotune_note_rollback(autotune_t *at);

/* Arranca la fase VERIFY: exige secuencia nueva y timestamp >= write_ms +
 * AUTOTUNE_SETTLE_GUARD_MS (la escritura ES el hecho que hay que superar; que
 * venza el timer no prueba frescura). */
void autotune_verify_start(autotune_t *at, uint64_t now_ms, uint64_t last_seq,
                           uint64_t write_ms);

/* Suma una trama a la fase en curso. Ignora invalidas/viejas/duplicadas, vacia
 * el buffer cuando cambia el centro del DDS (nunca mezcla dos centros) y corre
 * la ventana de tres para descartar una trama atipica que se va. */
autotune_take_t autotune_take(autotune_t *at, const autotune_sample_t *s, uint64_t now_ms);

/* true si la fase ya juntó sus tres tramas validas y estables. */
bool autotune_phase_ready(const autotune_t *at);

/* true si la fase venció su plazo sin juntar las tres tramas. */
bool autotune_phase_timeout(const autotune_t *at, uint64_t now_ms);

/* Mediana de la fase (0 si no esta lista). */
float autotune_phase_median(const autotune_t *at);

/* true si la sesion supero su tope total desde el trigger. */
bool autotune_session_timeout(const autotune_t *at, uint64_t now_ms);

/* Veredicto de la fase COLLECT ya lista: TUNED (dentro de la banda muerta, sin
 * escritura), CORRECT (`target` = el unico destino valido) o NO_ADJUST. */
autotune_result_t autotune_collect_result(const autotune_t *at, uint32_t band_start,
                                          uint32_t band_stop, int32_t *target);

/* Veredicto de la fase VERIFY ya lista: TUNED si |mediana| <= 10 Hz. */
autotune_result_t autotune_verify_result(const autotune_t *at);

#ifdef __cplusplus
}
#endif
