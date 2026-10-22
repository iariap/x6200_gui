/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Auto-ajuste del decodificador CW (issue #139)
 *
 *  Nucleo determinista y sin UI del modo AUTO: entran las estadisticas que
 *  `src/cw.cpp` mide por ventana (piso de ruido, nivel de marca, varianzas,
 *  WPM, duracion del bloque de FFT, SNR observada y offset del tono) y salen
 *  los cuatro parametros del decodificador mas la correccion del centro del
 *  DDS. No toca LVGL, ni la radio, ni `cfg`: se prueba entero en la suite
 *  nativa con vectores sinteticos.
 *
 *  Por que en C aparte: `src/cw.cpp` no se compila en la suite nativa (necesita
 *  liquid-dsp y el lazo de audio), asi que la logica tiene que vivir en un
 *  modulo que se pueda compilar y ejercitar sin radio (misma razon que
 *  `adaptive_tuning.c` y `autotune.c`).
 *
 *  Formulas y su fundamento numerico: `docs/cw139-spec.md`.
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Tamano maximo de una ventana: 5 s a ~5,4 tramas de FFT por segundo son ~27
 * muestras; 64 deja margen si el feed se acelera o la ventana se acorta al
 * encender AUTO. */
#define CW_AUTOTUNE_MAX_WINDOW 64

/* Rango util del CW decodificable. Fuera de el se usa el extremo: el decoder
 * mide WPM solo cuando ya hubo un par punto/raya, y 0 significa "todavia no
 * medi". El integrador le pasa la velocidad configurada como respaldo. */
#define CW_AUTOTUNE_WPM_MIN 5
#define CW_AUTOTUNE_WPM_MAX 60
#define CW_AUTOTUNE_WPM_DEFAULT 20

/* Clamps del MFK (src/mfk.cpp): el AUTO no puede dejar el parametro fuera de
 * lo que el operador puede volver a ajustar a mano. */
#define CW_AUTOTUNE_SNR_MIN 3.0f
#define CW_AUTOTUNE_SNR_MAX 30.0f
#define CW_AUTOTUNE_BETA_MIN 0.10f
#define CW_AUTOTUNE_BETA_MAX 0.95f

/* La formula del SNR apunta al punto medio marca/ruido, con techo 10 dB: el
 * margen de decodificacion no gana nada por encima de eso y un valor mas alto
 * solo alarga la cola de la senal. El clamp fuerte sigue siendo 3..30. */
#define CW_AUTOTUNE_SNR_SOFT_MAX 10.0f

#define CW_AUTOTUNE_GIST_MIN 0.5f
#define CW_AUTOTUNE_GIST_MAX 3.0f

/* Guardas: sin senal no se adapta nada (el piso de ruido no es una senal) y
 * con menos de 6 dB de marca sobre ruido la sugerencia no es confiable. */
#define CW_AUTOTUNE_MIN_SIGNAL_RATIO 1.0f
#define CW_AUTOTUNE_MIN_OBSERVED_SNR_DB 6.0f

/* Seguimiento del tono: el centro del DDS del decodificador RX se mueve como
 * maximo CW_AUTOTUNE_FREQ_STEP_HZ por ventana, dentro de +/-300 Hz, y solo si
 * el error supera la banda muerta. */
#define CW_AUTOTUNE_FREQ_LIMIT_HZ 300.0f
#define CW_AUTOTUNE_FREQ_STEP_HZ 30.0f
#define CW_AUTOTUNE_FREQ_DEADBAND_HZ 10.0f

/* Banda muerta: 10 % del rango de cada parametro (SNR 3..30 -> 2,7 dB;
 * betas 0,10..0,95 -> 0,085; gist 0,5..3 -> 0,25). */
#define CW_AUTOTUNE_SNR_DEADBAND_DB 2.7f
#define CW_AUTOTUNE_GIST_DEADBAND 0.25f
#define CW_AUTOTUNE_BETA_DEADBAND 0.085f

/* Un salto mayor que dos bandas muertas entre ventanas se promedia con la
 * sugerencia anterior en vez de perseguirlo (QSB, un golpe de ruido). */
#define CW_AUTOTUNE_JUMP_FACTOR 2.0f

/* Duracion del punto Morse para un WPM: 1200/WPM ms (PARIS). El lazo del
 * umbral de pico apunta a medio punto (ver cw_autotune_tau_peak_ms). */
#define CW_AUTOTUNE_DOT_MS(wpm) (1200.0f / (float) (wpm))

typedef struct {
    float noise_db;        /* piso de ruido promedio de la ventana (dB) */
    float mark_db;         /* nivel de marca: percentil 90 de la ventana (dB) */
    float noise_var_db2;   /* varianza del piso (dB^2): que tan nervioso esta */
    float mark_var_db2;    /* varianza de la marca (dB^2): QSB dentro de la ventana */
    float signal_ratio;    /* sum_signal/noise de la ultima trama (>1 = hay senal) */
    float block_dt_s;      /* duracion de un bloque de FFT (s) */
    float peak_freq_hz;    /* offset medido del tono respecto del centro del DDS */
    int   wpm;             /* WPM medida por el decodificador (0 = sin medida) */
    bool  mark_valid;      /* la ventana tiene al menos una marca */
    bool  peak_valid;      /* la ventana tiene al menos un pico espectral */
} cw_autotune_stats_t;

typedef struct {
    bool  adapt;             /* hubo senal suficiente: la ventana es valida */
    bool  apply_snr;         /* el valor sugerido supera la banda muerta */
    bool  apply_gist;
    bool  apply_peak_beta;
    bool  apply_noise_beta;
    bool  apply_freq;
    float snr;
    float gist;
    float peak_beta;
    float noise_beta;
    /* Correccion total a sumar al centro del DDS del decodificador RX (no al
     * tono de TX ni al modo). El signo sale de la medicion de `peak_freq`
     * (docs/HALLAZGOS.md 52): el offset medido es el negativo del corrimiento,
     * asi que la correccion es -peak_freq. */
    float freq_offset_hz;
} cw_autotune_result_t;

typedef struct {
    bool  have_applied;
    bool  have_suggested;
    float snr;
    float gist;
    float peak_beta;
    float noise_beta;
    float freq_offset_hz;
    float last_snr;
    float last_gist;
    float last_peak_beta;
    float last_noise_beta;
    float last_freq;
} cw_autotune_state_t;

/* Constante de tiempo del lazo del umbral de pico: medio punto Morse, en ms.
 * wpm <= 0 usa 20 WPM (el centro del rango decodificable). */
float cw_autotune_tau_peak_ms(int wpm);

/* Estadistica robusta de una ventana: percentil por rango mas cercano
 * (q=0,9 -> noveno valor de diez) y media/varianza poblacional. n <= 0 da 0. */
float cw_autotune_percentile(const float *values, int n, float q);
float cw_autotune_mean(const float *values, int n);
float cw_autotune_variance(const float *values, int n);

/* Deja el estado como recien encendido: la proxima ventana valida aplica todos
 * los parametros (calibracion inmediata). */
void cw_autotune_reset(cw_autotune_state_t *st);

/* Calcula la sugerencia de una ventana. `out` siempre queda completo: si no
 * hay senal (adapt=false) repite los ultimos valores aplicados y no pide
 * escribir ninguno. */
void cw_autotune_compute(cw_autotune_state_t *st, const cw_autotune_stats_t *in,
                         cw_autotune_result_t *out);

#ifdef __cplusplus
}
#endif
