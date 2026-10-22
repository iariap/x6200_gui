/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Auto Tune multimode UI implementation
 *
 *  Copyright (c) 2024 LU2DSX
 *
 *  Coordinador LVGL de las tres estrategias de `ACTION_ATUNE` (issue #205).
 *  La politica vive en modulos puros (src/autotune.c para CW/CWR y
 *  src/autotune_spectrum.c para AM/SAM y USB/LSB); aca viven los Subjects, el
 *  reloj, el timer de 50 ms y la barra de mensajes. Nunca se transmite y nunca
 *  se toca el RF: Auto Tune es estrictamente RX.
 *
 *  Estrategia por modo, con una sola accion y sin pantallas ni settings nuevos:
 *
 *  - CW/CWR: la ruta v2 completa, intacta (misma maquina COLLECT/VERIFY, mismos
 *    umbrales, mismos textos y misma verificacion con tres tramas nuevas). Este
 *    archivo NO la reescribe: la mueve tal cual a `cw_tick()`, y el despacho por
 *    estrategia es lo unico que la rodea.
 *  - AM/SAM: `AM_CARRIER` detecta una portadora estrecha, unica y dominante en
 *    la instantanea del DSP, la centra con UNA correccion acotada, verifica con
 *    cuatro tramas nuevas y hace rollback condicionado si no converge.
 *  - USB/LSB: `SSB_SUGGEST` estima un desplazamiento grueso de energia dentro de
 *    la banda lateral y NO mueve el VFO: muestra la propuesta y exige una
 *    segunda pulsacion explicita. El Undo temporal es de 5000 ms.
 *  - Todo lo demas (USB-D/LSB-D, FM/NFM/WFM): `Auto Tune: mode unsupported`.
 *
 *  Propiedad del dial: la regla sigue siendo `autotune_owns()` (contexto
 *  identico al del trigger, RX, sin lock ni RIT y el dial donde lo dejo Auto
 *  Tune). La sesion espectral se siembra con `autotune_collect_start()` para que
 *  ese contrato sea el mismo que el de CW; ademas se vigila el contexto de
 *  medicion (ancho de FFT, filtro real, ATT/PRE/AGC y SPLIT en voz).
 */

#include "autotune_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "autotune.h"
#include "autotune_spectrum.h"
#include "cw.h"
#include "dsp.h"
#include "params/params.h"
#include "msg.h"
#include "util.h"
#include "main_screen.h"
#include "dialog.h"
#include "radio.h"
#include "cfg/band.h"
#include "cfg/cfg.h"

#include "lvgl/lvgl.h"

/* Una trama de FFT llega cada ~185 ms (128 muestras a 689 Hz); 50 ms alcanzan
 * para no perder ninguna (el nucleo ignora las repetidas por secuencia) sin
 * quemar CPU. El publicador espectral del DSP corre a ~15 fps: el mismo tick
 * alcanza para juntar las cuatro/cinco tramas de cada politica. */
#define AUTOTUNE_UI_TICK_MS      50

/* Los plazos de la sesion espectral (COLLECT 3000, VERIFY 2500, PREVIEW 5000 y
 * UNDO 5000 ms), la guarda post-escritura (AUTOTUNE_SETTLE_GUARD_MS = 300 ms) y
 * las publicaciones que se descartan despues de una escritura propia
 * (AUTOTUNE_POST_WRITE_DROP = 3) son constantes de la politica pura:
 * src/autotune_spectrum.h. El tope total es el del contrato CW
 * (AUTOTUNE_SESSION_TIMEOUT_MS = 7000 ms). */

/* Textos EXACTOS del contrato (todo en ingles). Los de CW son los de #190 y no
 * cambian; los multimodo son los de #205 seccion 7. */
#define AUTOTUNE_MSG_TUNING        "Auto Tune: tuning..."
#define AUTOTUNE_MSG_TUNED         "Auto Tune: tuned"
#define AUTOTUNE_MSG_FAIL          "Auto Tune: no reliable adjustment"
#define AUTOTUNE_MSG_CANCELLED     "Auto Tune: cancelled"
#define AUTOTUNE_MSG_RX_ONLY       "Auto Tune: RX only"
#define AUTOTUNE_MSG_LOCKED        "Auto Tune: frequency locked"
#define AUTOTUNE_MSG_RIT           "Auto Tune: disable RIT"
#define AUTOTUNE_MSG_APP           "Auto Tune: close current app"
#define AUTOTUNE_MSG_SPLIT         "Auto Tune: disable split"
#define AUTOTUNE_MSG_UNSUPPORTED   "Auto Tune: mode unsupported"
#define AUTOTUNE_MSG_NO_SIGNAL     "Auto Tune: no reliable signal"
#define AUTOTUNE_MSG_CENTERED      "Auto Tune: carrier centered"
#define AUTOTUNE_MSG_CENTERED_UNDO "Auto Tune: carrier centered - press again to undo"
#define AUTOTUNE_MSG_SUGGEST_FMT   "Auto Tune: suggested %+d Hz - press again"
#define AUTOTUNE_MSG_APPLIED_UNDO  "Auto Tune: applied - press again to undo"
#define AUTOTUNE_MSG_RESTORED      "Auto Tune: restored"

/* Estrategia de la sesion en curso. */
typedef enum {
    AUTOTUNE_STRATEGY_NONE = -1, /* modo sin Auto Tune: sin retune ni sugerencia */
    AUTOTUNE_STRATEGY_CW = 0,    /* la ruta v2 de siempre */
    AUTOTUNE_STRATEGY_AM,        /* AM/SAM: portadora dominante */
    AUTOTUNE_STRATEGY_SSB,       /* USB/LSB: propuesta con confirmacion */
} atune_strategy_t;

/* Estados del coordinador. CW sigue con los suyos (autotune_state_t); estos son
 * los que la sesion espectral necesita ADEMAS: PREVIEW es solo de SSB y VERIFY
 * solo de AM (SSB no escribe nada hasta que el operador confirma). Los plazos y
 * el significado de cada pulsacion son politica pura (autotune_phase_deadline,
 * autotune_phase_press en src/autotune_spectrum.c). */

static autotune_t    at;
static lv_timer_t   *timer = NULL;
static bool          active = false;
static bool          observed = false;
static uint32_t      band_start = 0;
static uint32_t      band_stop = 0;

/* Estado multimodo (no toca la sesion CW, que sigue en `at`). */
static atune_strategy_t      strategy = AUTOTUNE_STRATEGY_CW;
static autotune_phase_t      ui_state = AUTOTUNE_PHASE_IDLE;
static autotune_am_state_t   am;
static autotune_ssb_state_t  ssb;
static autotune_frame_gate_t gate;
static int32_t               suggest_hz = 0;
static uint64_t              phase_deadline_ms = 0;
static uint64_t              window_deadline_ms = 0;
static uint64_t              last_frame_seq = 0;
static int32_t               snap_width = 0;
static int32_t               snap_filter_from = 0;
static int32_t               snap_filter_to = 0;
static int32_t               snap_att = 0;
static int32_t               snap_pre = 0;
static int32_t               snap_agc = 0;
static int32_t               snap_split = 0;

static void tick_cb(lv_timer_t *t);
static void on_fg_freq_change(Subject *subj, void *user_data);

static void say(const char *text) {
    /* "%s" y no el texto directo: los mensajes del contrato son constantes, pero
     * el formateador no tiene que interpretar cualquier cosa que llegue. */
    msg_update_text_fmt("%s", text);
}

static void say_fmt(const char *fmt, int value) {
    msg_update_text_fmt(fmt, value);
}

/* --- estrategia por modo (issue #205, seccion 1) --------------------------- */

static atune_strategy_t strategy_for(int32_t mode) {
    switch (mode) {
        case x6200_mode_cw:
        case x6200_mode_cwr:
            return AUTOTUNE_STRATEGY_CW;
        case x6200_mode_am:
        case x6200_mode_sam:
            return AUTOTUNE_STRATEGY_AM;
        case x6200_mode_usb:
        case x6200_mode_lsb:
            return AUTOTUNE_STRATEGY_SSB;
        default:
            /* USB-D/LSB-D, NFM/WFM y cualquier otro: sin retune ni sugerencia.
             * No se toca el dial de FT8/JS8/WSPR ni frecuencias de datos. */
            return AUTOTUNE_STRATEGY_NONE;
    }
}

static bool strategy_is_voice(atune_strategy_t s) {
    return s == AUTOTUNE_STRATEGY_AM || s == AUTOTUNE_STRATEGY_SSB;
}

/* Ancho y resolucion que las dos politicas espectrales exigen (seccion 5): por
 * encima de eso el PSD no alcanza ni para un centrado aproximado. */
static bool width_measurable(int32_t width_hz) {
    if (width_hz <= 0 || width_hz > AUTOTUNE_SPECTRUM_MAX_WIDTH_HZ) {
        return false;
    }
    return autotune_spectrum_resolution_hz(width_hz) <= AUTOTUNE_SPECTRUM_MAX_RESOLUTION_HZ;
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
    /* La instantanea espectral vigente es de la frecuencia anterior (issue #205,
     * seccion 4): el epoch sube y ninguna trama previa puede mezclarse. */
    dsp_autotune_invalidate();
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
    autotune_am_reset(&am);
    autotune_ssb_reset(&ssb);
    autotune_frame_gate_reset(&gate);
    strategy = AUTOTUNE_STRATEGY_CW;
    ui_state = AUTOTUNE_PHASE_IDLE;
    suggest_hz = 0;
    phase_deadline_ms = 0;
    window_deadline_ms = 0;
}

/* --- la ruta CW/CWR: v2 INTACTA (issue #190) ------------------------------- */

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

/* Los tres rechazos y las dos phases de siempre. El cuerpo es el que tenia
 * tick_cb() para CW, sin una linea de diferencia. */
static void cw_tick(uint64_t now) {
    autotune_sample_t sample;

    if (autotune_session_timeout(&at, now)) {
        say(AUTOTUNE_MSG_FAIL);
        end_session(true);
        return;
    }
    /* El plazo de la fase corre SIEMPRE, llegue o no una trama: sin senal el
     * operador tiene que ver el fallo a los 3 s, no quedarse esperando al tope de
     * la session (issue #190, seccion 2). */
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

/* --- AM/SAM y USB/LSB: la sesion espectral (issue #205) -------------------- */

/* El contexto de medicion que la politica espectral exige constante: ancho de
 * FFT, filtro real (bordes de RF que publica cfg/mode.c), ATT/PRE/AGC y SPLIT en
 * voz. Un cambio en cualquiera cancela sin escribir (seccion 8): la baseline y
 * la senal solo son comparables bajo el mismo contexto. */
static bool measurement_context_changed(void) {
    if (subject_get_int(cfg_cur.fft_width) != snap_width) {
        return true;
    }
    if (subject_get_int(cfg_cur.filter.real.from) != snap_filter_from ||
        subject_get_int(cfg_cur.filter.real.to) != snap_filter_to) {
        return true;
    }
    if (subject_get_int(cfg_cur.att) != snap_att ||
        subject_get_int(cfg_cur.pre) != snap_pre ||
        subject_get_int(cfg_cur.agc) != snap_agc) {
        return true;
    }
    return subject_get_int(cfg_cur.band->split.val) != snap_split;
}

static void snapshot_context_take(void) {
    snap_width = subject_get_int(cfg_cur.fft_width);
    snap_filter_from = subject_get_int(cfg_cur.filter.real.from);
    snap_filter_to = subject_get_int(cfg_cur.filter.real.to);
    snap_att = subject_get_int(cfg_cur.att);
    snap_pre = subject_get_int(cfg_cur.pre);
    snap_agc = subject_get_int(cfg_cur.agc);
    snap_split = subject_get_int(cfg_cur.band->split.val);
}

/* Veredicto comun de "no hay senal confiable" para la sesion espectral. */
static void spectrum_fail(bool had_write) {
    say(AUTOTUNE_MSG_NO_SIGNAL);
    end_session(had_write);
}

static bool target_ok(int32_t target) {
    if (!autotune_target_in_band(target, band_start, band_stop)) {
        return false;
    }
    return radio_check_freq(target);
}

/* Epoch vigente despues de la invalidacion de una escritura propia. */
static uint64_t post_write_epoch(void) {
    dsp_autotune_spectrum_t snap;

    if (!dsp_autotune_get_spectrum(&snap)) {
        return 0;
    }
    return snap.epoch;
}

/* AM/SAM: cuatro tramas con portadora consistente -> una sola escritura
 * provisional y verificacion con cuatro tramas nuevas. */
static void am_collect(const dsp_autotune_spectrum_t *s, uint64_t now) {
    int32_t            delta = 0;
    int32_t            target;
    autotune_verdict_t verdict;

    if (autotune_am_take(&am, s) != AUTOTUNE_STEP_READY) {
        return;
    }
    verdict = autotune_am_verdict(&am, &delta);
    if (verdict == AUTOTUNE_VERDICT_NONE) {
        spectrum_fail(false);
        return;
    }
    if (verdict == AUTOTUNE_VERDICT_CENTERED) {
        /* Ya centrada: cero escrituras. */
        say(AUTOTUNE_MSG_CENTERED);
        end_session(false);
        return;
    }

    target = (int32_t) ((int64_t) at.start_freq + (int64_t) delta);
    if (!target_ok(target)) {
        spectrum_fail(false);
        return;
    }
    if (!dial_write_own(target, false)) {
        spectrum_fail(false);
        return;
    }

    /* Verificacion: minimo 300 ms de guarda, descarte de las tres primeras
     * publicaciones posteriores y cuatro tramas NUEVAS del mismo contexto. */
    autotune_am_reset(&am);
    autotune_frame_gate_arm(&gate, now + AUTOTUNE_SETTLE_GUARD_MS,
                            last_frame_seq + AUTOTUNE_POST_WRITE_DROP, post_write_epoch());
    ui_state = AUTOTUNE_PHASE_VERIFY;
    /* La guarda corre desde la escritura; el plazo de la fase es el del contrato. */
    phase_deadline_ms = autotune_phase_deadline(AUTOTUNE_PHASE_VERIFY,
                                                now + AUTOTUNE_SETTLE_GUARD_MS);
}

static void am_verify(const dsp_autotune_spectrum_t *s, uint64_t now) {
    if (autotune_am_take(&am, s) != AUTOTUNE_STEP_READY) {
        return;
    }
    if (!autotune_spectrum_am_verified(&am)) {
        spectrum_fail(true);        /* rollback condicionado */
        return;
    }
    /* Verificado: se conserva el destino y se abre la ventana de Undo. */
    ui_state = AUTOTUNE_PHASE_UNDO;
    window_deadline_ms = autotune_phase_deadline(AUTOTUNE_PHASE_UNDO, now);
    say(AUTOTUNE_MSG_CENTERED_UNDO);
}

/* USB/LSB: cinco tramas con la misma propuesta -> se MUESTRA y se espera la
 * segunda pulsacion. El VFO no se mueve hasta la confirmacion. */
static void ssb_collect(const dsp_autotune_spectrum_t *s, uint64_t now) {
    int32_t            delta = 0;
    int32_t            target;
    bool               voice_lsb = (subject_get_int(cfg_cur.mode) == x6200_mode_lsb);

    if (autotune_ssb_take(&ssb, s, snap_filter_from, snap_filter_to, voice_lsb) !=
        AUTOTUNE_STEP_READY) {
        return;
    }
    if (autotune_ssb_verdict(&ssb, &delta) != AUTOTUNE_VERDICT_SUGGEST) {
        spectrum_fail(false);
        return;
    }
    target = (int32_t) ((int64_t) at.start_freq + (int64_t) delta);
    if (!target_ok(target)) {
        spectrum_fail(false);
        return;
    }

    suggest_hz = delta;
    ui_state = AUTOTUNE_PHASE_PREVIEW;
    window_deadline_ms = autotune_phase_deadline(AUTOTUNE_PHASE_PREVIEW, now);
    say_fmt(AUTOTUNE_MSG_SUGGEST_FMT, delta);
}

/* La sesion espectral en curso (COLLECT/VERIFY). PREVIEW y UNDO no miden: los
 * cierra su ventana o la segunda pulsacion. */
static void spectrum_tick(uint64_t now) {
    dsp_autotune_spectrum_t snap;

    if (ui_state != AUTOTUNE_PHASE_COLLECT && ui_state != AUTOTUNE_PHASE_VERIFY) {
        return;
    }
    if (autotune_session_timeout(&at, now)) {
        spectrum_fail(ui_state == AUTOTUNE_PHASE_VERIFY);
        return;
    }
    if (now >= phase_deadline_ms) {
        spectrum_fail(ui_state == AUTOTUNE_PHASE_VERIFY);
        return;
    }
    if (!dsp_autotune_get_spectrum(&snap)) {
        return;                     /* el DSP todavia no publico ninguna trama */
    }
    last_frame_seq = snap.sequence;

    switch (autotune_frame_gate_take(&gate, &snap)) {
        case AUTOTUNE_FRAME_BROKEN:
            /* Otro epoch o contexto: la medicion no es comparable. */
            say(AUTOTUNE_MSG_CANCELLED);
            end_session(false);
            return;
        case AUTOTUNE_FRAME_TAKEN:
            break;
        default:
            return;
    }

    if (ui_state == AUTOTUNE_PHASE_COLLECT) {
        if (strategy == AUTOTUNE_STRATEGY_AM) {
            am_collect(&snap, now);
        } else {
            ssb_collect(&snap, now);
        }
        return;
    }
    am_verify(&snap, now);
}

/* --- second press: cancela, aplica la propuesta o deshace (seccion 6) ------ */

/* Rollback al cancelar: CW conserva su comportamiento; AM en VERIFY todavia
 * tiene una escritura sin verificar; PREVIEW no escribio nada y UNDO ya tiene un
 * valor confirmado por el operador (nunca se revierte una decision externa). */
static bool restore_on_cancel(void) {
    if (strategy == AUTOTUNE_STRATEGY_CW) {
        return true;
    }
    return autotune_phase_restores_on_cancel(ui_state);
}

static bool restore_on_tx(void) {
    /* Antes de PTT: un AM en VERIFY hace rollback condicionado; en SSB PREVIEW no
     * hay frecuencia provisional y en UNDO el valor ya fue confirmado. CW
     * conserva su contrato (rollback condicionado). */
    return restore_on_cancel();
}

/* Segunda pulsacion de ACTION_ATUNE sobre una sesion viva. */
static void second_press(void) {
    if (strategy == AUTOTUNE_STRATEGY_CW) {
        say(AUTOTUNE_MSG_CANCELLED);
        end_session(true);
        return;
    }

    switch (autotune_phase_press(ui_state)) {
        case AUTOTUNE_PRESS_APPLY: {
            /* Confirmacion explicita (PREVIEW): una sola escritura al destino
             * sugerido. La propiedad (contexto, RX, dial sin mover) la vuelve a
             * preguntar dial_write_own() antes de tocar el VFO. */
            int32_t target = (int32_t) ((int64_t) at.start_freq + (int64_t) suggest_hz);

            if (!target_ok(target) || !dial_write_own(target, false)) {
                say(AUTOTUNE_MSG_CANCELLED);
                end_session(false);
                return;
            }
            ui_state = AUTOTUNE_PHASE_UNDO;
            window_deadline_ms = autotune_phase_deadline(AUTOTUNE_PHASE_UNDO, get_time());
            say(AUTOTUNE_MSG_APPLIED_UNDO);
            return;
        }
        case AUTOTUNE_PRESS_UNDO:
            /* Deshacer dentro de la ventana: restaura el origen si sigue siendo
             * dueno del dial y del contexto. */
            if (!dial_write_own(at.start_freq, true)) {
                say(AUTOTUNE_MSG_CANCELLED);
                end_session(false);
                return;
            }
            say(AUTOTUNE_MSG_RESTORED);
            end_session(false);
            return;
        default:
            /* MEDICION (COLLECT/VERIFY): segundo trigger = cancelar; VERIFY todavia
             * tiene una escritura sin confirmar, asi que repone (condicionado). */
            say(AUTOTUNE_MSG_CANCELLED);
            end_session(restore_on_cancel());
            return;
    }
}

/* --- tick --------------------------------------------------------------- */

static void tick_cb(lv_timer_t *t) {
    (void) t;

    uint64_t           now = get_time();
    autotune_context_t ctx;

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

    if (strategy == AUTOTUNE_STRATEGY_CW) {
        cw_tick(now);
        return;
    }

    /* Contexto de medicion (ancho, filtro, ATT/PRE/AGC, SPLIT) o app abierta:
     * cancela sin escribir. */
    if (measurement_context_changed()) {
        say(AUTOTUNE_MSG_CANCELLED);
        end_session(false);
        return;
    }
    if (dialog_is_run() || strcmp(main_screen_current_app(), "home") != 0) {
        say(AUTOTUNE_MSG_APP);
        end_session(false);
        return;
    }

    if (autotune_phase_measures(ui_state) == false &&
        autotune_phase_expired(window_deadline_ms, now)) {
        /* PREVIEW vencido: se vuelve a IDLE sin tocar el dial. UNDO vencido: el
         * resultado queda (no hay auto-undo). */
        end_session(false);
        return;
    }

    spectrum_tick(now);
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
    atune_strategy_t   s;

    if (active) {
        second_press();
        return;
    }

    read_context(&ctx);
    s = strategy_for(ctx.mode);

    /* Orden exacto de rechazo (seccion 7): modo compatible, RX, lock,
     * app/dialogo, RIT, SPLIT solo en voz, filtro valido y condicion de
     * medicion. Ningun rechazo abre sesion ni escribe nada. */
    if (s == AUTOTUNE_STRATEGY_NONE) {
        say(AUTOTUNE_MSG_UNSUPPORTED);
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
    if (strategy_is_voice(s) && subject_get_int(cfg_cur.band->split.val)) {
        say(AUTOTUNE_MSG_SPLIT);
        return;
    }
    if (s == AUTOTUNE_STRATEGY_SSB &&
        !autotune_spectrum_ssb_filter_ok(subject_get_int(cfg_cur.filter.real.from),
                                         subject_get_int(cfg_cur.filter.real.to),
                                         ctx.mode == x6200_mode_lsb)) {
        say(AUTOTUNE_MSG_NO_SIGNAL);
        return;
    }
    if (s != AUTOTUNE_STRATEGY_CW && !width_measurable(subject_get_int(cfg_cur.fft_width))) {
        say(AUTOTUNE_MSG_NO_SIGNAL);
        return;
    }

    if (s == AUTOTUNE_STRATEGY_CW) {
        /* Ruta CW v2: identica al issue #190. */
        uint64_t          last_seq = 0;
        autotune_sample_t sample;

        resolve_band_range(ctx.freq, &band_start, &band_stop);

        if (cw_get_tune_sample(&sample)) {
            last_seq = sample.sequence;      /* solo tramas POSTERIORES al trigger */
        }

        autotune_collect_start(&at,
                               (ctx.mode == x6200_mode_cwr) ? AUTOTUNE_MODE_CWR
                                                            : AUTOTUNE_MODE_CW,
                               &ctx, get_time(), last_seq);
        active = true;
        strategy = AUTOTUNE_STRATEGY_CW;
        ui_state = AUTOTUNE_PHASE_COLLECT;
        timer = lv_timer_create(tick_cb, AUTOTUNE_UI_TICK_MS, NULL);
        say(AUTOTUNE_MSG_TUNING);
        return;
    }

    resolve_band_range(ctx.freq, &band_start, &band_stop);

    /* La sesion espectral se siembra con el mismo contrato de propiedad de CW. */
    autotune_collect_start(&at, AUTOTUNE_MODE_CW, &ctx, get_time(), 0);
    autotune_am_reset(&am);
    autotune_ssb_reset(&ssb);
    autotune_frame_gate_arm(&gate, 0, 0, 0);
    snapshot_context_take();
    last_frame_seq = 0;
    suggest_hz = 0;
    strategy = s;
    ui_state = AUTOTUNE_PHASE_COLLECT;
    phase_deadline_ms = autotune_phase_deadline(AUTOTUNE_PHASE_COLLECT, get_time());
    active = true;
    timer = lv_timer_create(tick_cb, AUTOTUNE_UI_TICK_MS, NULL);
    say(AUTOTUNE_MSG_TUNING);
}

void autotune_ui_cancel(void) {
    if (!active) {
        return;
    }
    say(AUTOTUNE_MSG_CANCELLED);
    end_session(restore_on_cancel());
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
    end_session(restore_on_tx());
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
