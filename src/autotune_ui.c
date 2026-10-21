/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Fine-tuning assistant UI implementation
 *
 *  Copyright (c) 2024 LU2DSX
 */

#include "autotune_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "autotune.h"
#include "cw.h"
#include "params/params.h"
#include "msg.h"
#include "util.h"
#include "main_screen.h"
#include "dialog.h"
#include "radio.h"
#include "cfg/band.h"

#include "lvgl/lvgl.h"

/* Una trama de FFT llega cada ~185 ms (128 muestras a 689 Hz); 50 ms alcanzan
 * para no perder ninguna (el nucleo ignora las repetidas por secuencia) sin
 * quemar CPU. */
#define AUTOTUNE_UI_TICK_MS      50

/* Textos EXACTOS del issue #190 seccion 6 (todo en ingles). */
#define AUTOTUNE_MSG_TUNING      "Auto Tune: tuning..."
#define AUTOTUNE_MSG_TUNED       "Auto Tune: tuned"
#define AUTOTUNE_MSG_FAIL        "Auto Tune: no reliable adjustment"
#define AUTOTUNE_MSG_CANCELLED   "Auto Tune: cancelled"
#define AUTOTUNE_MSG_CW_ONLY     "Auto Tune: CW/CWR only"
#define AUTOTUNE_MSG_RX_ONLY     "Auto Tune: RX only"
#define AUTOTUNE_MSG_LOCKED      "Auto Tune: frequency locked"
#define AUTOTUNE_MSG_RIT         "Auto Tune: disable RIT"
#define AUTOTUNE_MSG_APP         "Auto Tune: close current app"

static autotune_t    at;
static lv_timer_t   *timer = NULL;
static bool          active = false;
static bool          observed = false;
static uint32_t      band_start = 0;
static uint32_t      band_stop = 0;

static void tick_cb(lv_timer_t *t);
static void on_fg_freq_change(Subject *subj, void *user_data);

static void say(const char *text) {
    /* "%s" y no el texto directo: los mensajes del contrato son constantes, pero
     * el formateador no tiene que interpretar cualquier cosa que llegue. */
    msg_update_text_fmt("%s", text);
}

/* --- contexto y propiedad (issue #190, seccion 5) -------------------------- */

static void read_context(autotune_context_t *ctx) {
    ctx->mode = subject_get_int(cfg_cur.mode);
    ctx->band_id = subject_get_int(cfg.band_id.val);
    ctx->vfo = subject_get_int(cfg_cur.band->vfo.val);
    ctx->pitch = subject_get_int(cfg.key_tone.val);
    ctx->freq = subject_get_int(cfg_cur.fg_freq);
    ctx->rx = (radio_get_state() == RADIO_RX);
    ctx->locked = main_screen_freq_locked();
    ctx->rit = (params.rit != 0);
}

/* Rango de la banda configurada para el destino (seccion 2): la fila del
 * catalogo que contiene la frecuencia del dial. src/cfg/band.c mantiene
 * cfg.band_id sincronizado con el dial (on_fg_freq_change ->
 * cfg_band_set_freq_vfo -> subject_set_int(cfg.band_id.val)), asi que "la banda
 * configurada" y "la que contiene el dial" son la misma. Si el dial cae en un
 * hueco del catalogo no hay rango y el destino se rechaza (conservador). */
static void resolve_band_range(int32_t freq, uint32_t *start, uint32_t *stop) {
    band_info_t *rows = NULL;
    int32_t      cap = 0;
    uint32_t     n;
    int32_t      best = -1;

    *start = 0;
    *stop = 0;

    n = cfg_band_read_all_bands(&rows, &cap);
    if (rows == NULL) {
        return;
    }

    for (uint32_t i = 0; (int32_t) i < (int32_t) n; i++) {
        if (freq <= 0 || (uint32_t) freq < rows[i].start_freq ||
            (uint32_t) freq > rows[i].stop_freq) {
            continue;
        }
        if (best < 0 ||
            (rows[i].stop_freq - rows[i].start_freq) <
                (rows[best].stop_freq - rows[best].start_freq)) {
            best = (int32_t) i;
        }
    }
    if (best >= 0) {
        *start = rows[best].start_freq;
        *stop = rows[best].stop_freq;
    }
    for (uint32_t i = 0; (int32_t) i < (int32_t) n; i++) {
        free(rows[i].name);
    }
    free(rows);
}

/* La unica via de escritura del dial. Pregunta primero (contexto identico,
 * RX, sin lock ni RIT, dial donde lo dejo Auto Tune), deja `expected_freq`
 * marcado ANTES de tocar el subject (para que el observador propio no se
 * confunda con un cambio externo) y avisa al DSP para que invalide su primera
 * ventana. */
static bool dial_write_own(int32_t target, bool rollback) {
    autotune_context_t now;

    read_context(&now);
    if (!autotune_owns(&at, &now)) {
        return false;
    }
    if (!radio_check_freq(target)) {
        return false;
    }

    if (rollback) {
        autotune_note_rollback(&at);
    } else {
        autotune_note_write(&at, target);
    }
    cw_tune_note_dial_write();
    if (subject_get_int(cfg_cur.fg_freq) != target) {
        subject_set_int(cfg_cur.fg_freq, target);
    }
    return true;
}

static void stop_timer(void) {
    if (timer != NULL) {
        lv_timer_del(timer);
        timer = NULL;
    }
}

/* Cierre unico de la sesion. `restore` pide el rollback condicionado: se ejecuta
 * SOLO si el core reconoce todavia la propiedad del dial (seccion 5). */
static void end_session(bool restore) {
    if (!active) {
        return;
    }
    if (restore && at.wrote) {
        dial_write_own(at.start_freq, true);
    }
    stop_timer();
    active = false;
    autotune_reset(&at);
}

/* --- las dos fases --------------------------------------------------------- */

static void phase_collect(const autotune_sample_t *sample, uint64_t now) {
    int32_t           target = 0;
    autotune_result_t result = autotune_collect_result(&at, band_start, band_stop, &target);

    if (result == AUTOTUNE_RESULT_TUNED) {
        /* Ya estaba dentro de +/-10 Hz: terminar Tuned sin ninguna escritura. */
        say(AUTOTUNE_MSG_TUNED);
        end_session(false);
        return;
    }
    if (result == AUTOTUNE_RESULT_NO_ADJUST) {
        say(AUTOTUNE_MSG_FAIL);
        end_session(false);
        return;
    }

    /* La UNICA escritura provisional de la invocacion. */
    if (!dial_write_own(target, false)) {
        say(AUTOTUNE_MSG_FAIL);
        end_session(false);
        return;
    }
    autotune_verify_start(&at, now, sample->sequence, now);
}

static void phase_verify(void) {
    if (autotune_verify_result(&at) == AUTOTUNE_RESULT_TUNED) {
        say(AUTOTUNE_MSG_TUNED);
        end_session(false);
        return;
    }
    /* La correccion no verifico con tramas nuevas: rollback condicionado. */
    say(AUTOTUNE_MSG_FAIL);
    end_session(true);
}

static void tick_cb(lv_timer_t *t) {
    (void) t;

    uint64_t           now = get_time();
    autotune_context_t ctx;
    autotune_sample_t   sample;

    if (!active) {
        return;
    }

    read_context(&ctx);

    if (ctx.rit) {
        /* RIT cambia la frecuencia RX efectiva: no se traduce a VFO. */
        say(AUTOTUNE_MSG_RIT);
        end_session(false);
        return;
    }
    if (autotune_context_broken(&at, &ctx)) {
        /* Modo/banda/VFO/estado/lock cambiaron: el contexto del trigger ya no
         * existe, ni el rollback seria en el contexto original. */
        say(AUTOTUNE_MSG_CANCELLED);
        end_session(false);
        return;
    }
    if (autotune_session_timeout(&at, now)) {
        say(AUTOTUNE_MSG_FAIL);
        end_session(true);
        return;
    }
    /* El plazo de la fase corre SIEMPRE, llegue o no una trama: sin señal el
     * operador tiene que ver el fallo a los 3 s, no quedarse esperando al tope de
     * la sesion (issue #190, seccion 2). */
    if (autotune_phase_timeout(&at, now)) {
        say(AUTOTUNE_MSG_FAIL);
        end_session(true);
        return;
    }
    if (!cw_get_tune_sample(&sample)) {
        return;                 /* el DSP todavia no publico ninguna trama */
    }
    if (autotune_take(&at, &sample, now) != AUTOTUNE_TAKE_READY) {
        return;
    }

    if (at.state == AUTOTUNE_STATE_COLLECT) {
        phase_collect(&sample, now);
    } else {
        phase_verify();
    }
}

/* --- API ------------------------------------------------------------------ */

void autotune_ui_init(void) {
    if (observed) {
        return;
    }
    observed = true;
    autotune_reset(&at);
    /* Cambio externo de frecuencia (CAT, memoria, app, otro observador): aborta
     * sin escribir. Los caminos manuales de GUI (rueda, teclas, CB) llaman a
     * autotune_ui_abort_manual() ANTES de escribir; este observador es la red
     * del resto. */
    subject_add_delayed_observer(cfg_cur.fg_freq, on_fg_freq_change, NULL);
}

void autotune_ui_toggle(void) {
    autotune_context_t ctx;

    if (active) {
        say(AUTOTUNE_MSG_CANCELLED);
        end_session(true);
        return;
    }

    read_context(&ctx);

    /* Orden de rechazo del issue, seccion 5: modo, estado de radio, bloqueo,
     * dialogo/app activo, RIT. Ninguno entra a COLLECT ni escribe nada. */
    if (ctx.mode != x6200_mode_cw && ctx.mode != x6200_mode_cwr) {
        say(AUTOTUNE_MSG_CW_ONLY);
        return;
    }
    if (!ctx.rx) {
        say(AUTOTUNE_MSG_RX_ONLY);
        return;
    }
    if (ctx.locked) {
        say(AUTOTUNE_MSG_LOCKED);
        return;
    }
    if (dialog_is_run() || strcmp(main_screen_current_app(), "home") != 0) {
        say(AUTOTUNE_MSG_APP);
        return;
    }
    if (ctx.rit) {
        say(AUTOTUNE_MSG_RIT);
        return;
    }

    resolve_band_range(ctx.freq, &band_start, &band_stop);

    uint64_t         last_seq = 0;
    autotune_sample_t sample;

    if (cw_get_tune_sample(&sample)) {
        last_seq = sample.sequence;      /* solo tramas POSTERIORES al trigger */
    }

    autotune_collect_start(&at,
                           (ctx.mode == x6200_mode_cwr) ? AUTOTUNE_MODE_CWR
                                                        : AUTOTUNE_MODE_CW,
                           &ctx, get_time(), last_seq);
    active = true;
    timer = lv_timer_create(tick_cb, AUTOTUNE_UI_TICK_MS, NULL);
    say(AUTOTUNE_MSG_TUNING);
}

void autotune_ui_cancel(void) {
    if (!active) {
        return;
    }
    say(AUTOTUNE_MSG_CANCELLED);
    end_session(true);
}

void autotune_ui_abort_manual(void) {
    if (!active) {
        return;
    }
    say(AUTOTUNE_MSG_CANCELLED);
    end_session(false);
}

void autotune_ui_cancel_before_tx(void) {
    if (!active) {
        return;
    }
    say(AUTOTUNE_MSG_CANCELLED);
    end_session(true);
}

void autotune_ui_abort_on_tx(void) {
    if (!active) {
        return;
    }
    /* TX ya activo: no se escribe fg_freq ni el rollback. */
    say(AUTOTUNE_MSG_CANCELLED);
    end_session(false);
}

bool autotune_ui_running(void) {
    return active;
}

static void on_fg_freq_change(Subject *subj, void *user_data) {
    (void) user_data;

    if (!active) {
        return;
    }
    if (subject_get_int(subj) == at.expected_freq) {
        return;                 /* es la escritura propia */
    }
    say(AUTOTUNE_MSG_CANCELLED);
    end_session(false);
}
