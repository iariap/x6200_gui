/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - politica visual de bateria (issue #14, UI-08).
 *
 *  Modulo casi puro: no dibuja, no toca la radio y no tiene reloj propio (el
 *  instante entra como parametro). Traduce el estado del pack (voltaje, fuente
 *  externa, capacidad, bandera de carga) en un perfil visual con hysteresis, un
 *  fallback conservador para datos viejos y un override manual que congela el
 *  modo automatico.
 *
 *  Es la primera pieza del manager de UI-08. La evaluacion es pura y no depende
 *  de nada; la unica salida al mundo es el wiring del incremento 2
 *  (visual_policy_waterfall_apply), que lleva el perfil al modo Ahorro del
 *  waterfall. El wiring del spectrum y del backlight son los incrementos 3/4.
 *
 *  Lo cubre el test nativo (tests/native/test_visual_policy.c): la evaluacion
 *  sin stubs, y el wiring contra el stub que registra la llamada a
 *  waterfall_set_battery_mode (tests/native/stubs/stubs.c).
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Perfiles visuales, de mas calidad (mas consumo) a mas ahorro. El indice es
 * parte del contrato: subir de perfil es restar uno, bajar es sumar uno. */
typedef enum {
    VP_QUALITY = 0,   /* todo encendido: la referencia visual */
    VP_BALANCED,      /* ahorro moderado */
    VP_ECO,           /* ahorro marcado (cadencia de refresco reducida) */
    VP_CRITICAL,      /* minimo indispensable */
    VP_PROFILE_COUNT
} visual_profile_t;

/* ------------------------------------------------------------------------- *
 * Tabla de umbrales DEFAULT (mV del pack, modo descarga).
 *
 * Nombrados a proposito: la tabla del .c los referencia y el codigo no lleva
 * literales. Cada banda tiene un umbral de ENTRADA (bajando) y uno de SALIDA
 * (subiendo) mas exigente; ese gap es la hysteresis que evita el flap. El test
 * nativo comprueba el orden y los dos bordes de cada umbral.
 *
 * Orden garantizado:
 *   VP_BALANCED_EXIT > VP_BALANCED_ENTER > VP_ECO_EXIT > VP_ECO_ENTER
 *                    > VP_CRITICAL_EXIT > VP_CRITICAL_ENTER
 * ------------------------------------------------------------------------- */

#define VP_BALANCED_ENTER_MV   12300u  /* <= entra a BALANCED (viniendo de QUALITY) */
#define VP_BALANCED_EXIT_MV    12700u  /* >= vuelve a QUALITY */
#define VP_ECO_ENTER_MV        11400u  /* <= entra a ECO (viniendo de BALANCED) */
#define VP_ECO_EXIT_MV         11900u  /* >= vuelve a BALANCED */
#define VP_CRITICAL_ENTER_MV   10800u  /* <= entra a CRITICAL (viniendo de ECO) */
#define VP_CRITICAL_EXIT_MV    11300u  /* >= vuelve a ECO */

/* La banda mejor (QUALITY) no tiene umbral propio: es el resto. */
#define VP_NO_THRESHOLD        0u

/* ------------------------------------------------------------------------- *
 * Validacion de la lectura.
 * ------------------------------------------------------------------------- */

#define VP_VBAT_MIN_MV         6000u   /* por debajo: lectura invalida */
#define VP_VBAT_MAX_MV         16000u  /* por encima: lectura invalida */
#define VP_BATCAP_MAX_PCT      100u    /* capacidad valida: 0..100 */
#define VP_VEXT_PRESENT_MV     1000u   /* vext >= esto: hay fuente externa */

/* ------------------------------------------------------------------------- *
 * Datos viejos -> fallback conservador declarado.
 *
 * Sin una muestra fresca por mas de VP_STALE_MS, el voltaje no es confiable:
 * se cae a VP_STALE_FALLBACK en lugar de dejar el perfil anterior. Se elige
 * ECO (ahorra sin volver ilegible la lectura; CRITICAL queda reservado para
 * una bateria medida baja, no para una incognita).
 * ------------------------------------------------------------------------- */

#define VP_STALE_MS            30000u  /* antiguedad maxima de la muestra valida */
#define VP_STALE_FALLBACK      VP_ECO  /* perfil declarado para datos viejos */

/* Override del usuario: automatico (la politica decide) o un perfil fijado
 * (el modo automatico queda congelado). Se construye con VP_AUTO o
 * VP_MANUAL(profile), nunca campo a campo. */
typedef struct {
    bool             manual;    /* true: el perfil es el que eligio el usuario */
    visual_profile_t profile;   /* valido solo con manual == true */
} visual_override_t;

static inline visual_override_t vp_override_auto(void) {
    visual_override_t override = { false, VP_BALANCED };
    return override;
}

static inline visual_override_t vp_override_manual(visual_profile_t profile) {
    visual_override_t override = { true, profile };
    return override;
}

#define VP_AUTO            (vp_override_auto())
#define VP_MANUAL(profile) (vp_override_manual(profile))

/* Estado que el manager conserva entre evaluaciones (memoria de hysteresis y
 * de override). Se inicializa con visual_policy_init(). */
typedef struct {
    visual_profile_t profile;         /* ultimo perfil AUTO (memoria de hysteresis) */
    visual_profile_t output;          /* ultimo perfil efectivo emitido */
    visual_profile_t manual_profile;  /* perfil fijado por el usuario */
    bool             manual;          /* true: AUTO congelado por el override */
    bool             stale;           /* ultima evaluacion con datos viejos */
} visual_policy_t;

/* Resultado de una evaluacion: que perfil aplicar y por que. */
typedef struct {
    visual_profile_t profile;    /* perfil efectivo a aplicar */
    visual_profile_t previous;   /* perfil efectivo de la evaluacion anterior */
    uint32_t         age_ms;     /* antiguedad de la muestra (now_ms - last_valid_ms) */
    bool             changed;    /* hubo transicion en esta evaluacion */
    bool             stale;      /* los datos de bateria estan viejos */
    bool             manual;     /* el perfil sale del override, no de AUTO */
    bool             discarded;  /* la lectura era invalida: se descarto la muestra */
} visual_policy_result_t;

/* Inicializa el estado en QUALITY, sin override y sin datos. */
void visual_policy_init(visual_policy_t *state);

/* Evalua una muestra y avanza el estado.
 *
 *   vbat_mv       voltaje del pack en mV (fuera de VP_VBAT_*: lectura invalida)
 *   vext_mv       fuente externa en mV (>= VP_VEXT_PRESENT_MV: enchufado)
 *   batcap        capacidad 0..100 % (> VP_BATCAP_MAX_PCT: lectura invalida)
 *   charging      bandera de carga del equipo (cargando sin fuente: imposible)
 *   last_valid_ms sello de la ultima muestra valida (para la antiguedad)
 *   now_ms        instante actual
 *   override      VP_AUTO o VP_MANUAL(perfil)
 *
 * Reglas:
 * - con fuente externa o cargando, el perfil es QUALITY de inmediato (sin
 *   hysteresis): el dato que manda es la presencia de fuente, no el voltaje;
 * - con datos viejos (now_ms - last_valid_ms > VP_STALE_MS) el perfil es el
 *   fallback conservador VP_STALE_FALLBACK;
 * - si no, la banda sale del voltaje del pack con hysteresis;
 * - un override manual fija el perfil y congela el AUTO hasta soltarlo;
 * - una lectura invalida se descarta entera: no hay transicion ni se toca el
 *   estado (la politica queda como estaba).
 */
visual_policy_result_t visual_policy_eval(visual_policy_t *state,
                                          uint16_t vbat_mv, uint16_t vext_mv,
                                          uint8_t batcap, bool charging,
                                          uint32_t last_valid_ms, uint32_t now_ms,
                                          visual_override_t override);

/* ------------------------------------------------------------------------- *
 * Wiring al primer consumidor (UI-08 incremento 2): el modo Ahorro del
 * waterfall, que el firmware ya tiene como toggle de usuario
 * (`params.waterfall_battery` -> `waterfall_set_battery_mode()`).
 *
 * El modulo sigue sin dibujar y sin leer params: su unica salida es esa
 * funcion, y el toggle del usuario entra como parametro (el param NUNCA se
 * escribe: el perfil solo toma el MODE runtime). El ROM (`radio.c`, en el
 * mismo punto donde ya se publica el pack en `clock_update_power`) y el arnes
 * (`screens.c`, con los valores del fixture) llaman a la MISMA funcion, asi
 * que la decision del banco y la de la radio no pueden divergir; el test
 * nativo la cubre entera contra un waterfall que registra la llamada.
 *
 * Llamar desde el hilo de LVGL: `radio.c` lo difiere por el scheduler (el
 * mismo patron que `tx_info_update`, src/tx_info.c) y el arnes ya corre en un
 * solo hilo.
 * ------------------------------------------------------------------------- */

/* Aplica un perfil ya evaluado al modo Ahorro del waterfall:
 *
 *   - ECO y CRITICAL lo fuerzan: el perfil manda sobre el toggle del usuario;
 *   - QUALITY y BALANCED no fuerzan nada: vuelve el toggle del usuario.
 *
 * Idempotente y sin estado: el llamador decide cuando llamar (el ROM lo hace
 * en la transicion del perfil, con `visual_policy_result_t.changed`). */
void visual_policy_waterfall_apply(visual_profile_t profile, bool user_battery_mode);

#ifdef __cplusplus
}
#endif
