/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */
#include "buttons.h"

#include "controls.h"

#include <stdio.h>
#include <string>
#include <vector>

extern "C" {
    #include "styles.h"
    #include "main_screen.h"
    #include "mfk.h"
    #include "vol.h"
    #include "msg.h"
    #include "pannel.h"
    #include "params/params.h"
    #include "radio.h"
    #include "voice.h"
    #include "pubsub_ids.h"
    #include "cfg/mode.h"
    #include "scheduler.h"
    #include "dfl_inline.h"
    #include "dfl_presets.h"
    #include "dfl_context.h"
}

/* dialog.h trae helpers.h (C++): va FUERA del bloque extern "C". */
#include "dialog.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(*arr))

typedef struct {
    lv_obj_t        *label;
    lv_obj_t        *obj;       /* la celda de la barra (para la ruta tactil #183) */
    bool             touch;     /* el item cargado pide touch (favoritos 1-3) */
    button_item_t   *item;
} button_t;

static uint8_t        btn_height = 62;
static button_t       btn[BUTTONS];
static lv_obj_t      *parent_obj = NULL;
static buttons_page_t *cur_page   = NULL;

static void button_app_page_cb(button_item_t *item);
static void button_vol_update_cb(button_item_t *item);
static void button_mfk_update_cb(button_item_t *item);
static void button_mem_load_cb(button_item_t *item);

static void param_changed_cb(void * s, lv_msg_t * m);
static void label_update_cb(Subject *subj, void *user_data);

static void button_vol_hold_cb(button_item_t *item);
static void button_mfk_hold_cb(button_item_t *item);
static void button_mem_save_cb(button_item_t *item);

// Label getters

static const char * vol_label_getter();
static const char * sql_label_getter();
static const char * rfg_label_getter();
static const char * tx_power_label_getter();

static const char * filter_low_label_getter();
static const char * filter_high_label_getter();
static const char * filter_bw_label_getter();

static const char * mic_sel_label_getter();
static const char * h_mic_gain_label_getter();
static const char * i_mic_gain_label_getter();
static const char * moni_level_label_getter();

static const char * charger_label_getter();
static const char * rit_label_getter();
static const char * xit_label_getter();

static const char * agc_label_getter();
static const char * att_label_getter();
static const char * pre_label_getter();
static const char * comp_label_getter();

static const char * key_speed_label_getter();
static const char * key_volume_label_getter();
static const char * key_train_label_getter();
static const char * key_tone_label_getter();

static const char * key_mode_label_getter();
static const char * iambic_mode_label_getter();
static const char * qsk_time_label_getter();
static const char * key_ratio_label_getter();

static const char * cw_decoder_label_getter();
static const char * cw_tuner_label_getter();
static const char * cw_snr_label_getter();

static const char * cw_peak_beta_label_getter();
static const char * cw_noise_beta_label_getter();
static const char * cw_auto_label_getter();

static const char * dnf_label_getter();
static const char * dnf_center_label_getter();
static const char * dnf_width_label_getter();

static const char * nb_label_getter();
static const char * nb_level_label_getter();
static const char * nb_width_label_getter();

static const char * nr_label_getter();
static const char * nr_level_label_getter();

static void button_action_cb(button_item_t *item);

/* Make VOL button functions */
static button_item_t make_btn(const char *name, vol_mode_t data) {
    return button_item_t{
        .type = BTN_TEXT, .label = name, .press = button_vol_update_cb, .hold = button_vol_hold_cb, .data = data};
}

static button_item_t make_btn(const char *(*label_fn)(), vol_mode_t data, Subject **subj = nullptr) {
    return button_item_t{.type     = BTN_TEXT_FN,
                         .label_fn = label_fn,
                         .press    = button_vol_update_cb,
                         .hold     = button_vol_hold_cb,
                         .data     = data,
                         .subj     = subj};
}

/* Make MFK button functions */
static button_item_t make_btn(const char *name, mfk_mode_t data) {
    return button_item_t{
        .type = BTN_TEXT, .label = name, .press = button_mfk_update_cb, .hold = button_mfk_hold_cb, .data = data};
}

static button_item_t make_btn(const char *(*label_fn)(), mfk_mode_t data, Subject **subj = nullptr) {
    return button_item_t{.type     = BTN_TEXT_FN,
                         .label_fn = label_fn,
                         .press    = button_mfk_update_cb,
                         .hold     = button_mfk_hold_cb,
                         .data     = data,
                         .subj     = subj};
}

/* Make MEM buttons functions */
static button_item_t make_mem_btn(const char *name, int32_t data) {
    return button_item_t{
        .type = BTN_TEXT, .label = name, .press = button_mem_load_cb, .hold = button_mem_save_cb, .data = data};
}

static button_item_t make_app_btn(const char *name, press_action_t data) {
    return button_item_t{.type = BTN_TEXT, .label = name, .press = button_app_page_cb, .hold = nullptr, .data = data};
}
static button_item_t make_action_btn(const char *name, press_action_t data) {
    return button_item_t{.type = BTN_TEXT, .label = name, .press = button_action_cb, .hold = nullptr, .data = data};
}

static button_item_t make_page_btn(const char *name, const char *voice) {
    return button_item_t{
        .type = BTN_TEXT, .label = name, .press = button_next_page_cb, .hold = button_prev_page_cb, .voice = voice};
}

/* VOL page 1 */

static button_item_t btn_vol = {
    .type     = BTN_TEXT_FN,
    .label_fn = vol_label_getter,
    .press    = button_vol_update_cb,
    .data     = VOL_VOL,
    .subj     = &cfg.vol.val,
};

static button_item_t btn_sql = make_btn(sql_label_getter, VOL_SQL, &cfg_cur.sql_level);
static button_item_t btn_rfg = make_btn(rfg_label_getter, VOL_RFG);
static button_item_t btn_tx_pwr = make_btn(tx_power_label_getter, VOL_PWR, &cfg.pwr.val);

/* VOL page 2 */

static button_item_t btn_flt_low  = make_btn(filter_low_label_getter, VOL_FILTER_LOW, &cfg_cur.filter.low);
static button_item_t btn_flt_high = make_btn(filter_high_label_getter, VOL_FILTER_HIGH, &cfg_cur.filter.high);
static button_item_t btn_flt_bw   = make_btn(filter_bw_label_getter, VOL_FILTER_BW, &cfg_cur.filter.bw);

/* VOL page 3 */

static button_item_t btn_mic_sel   = make_btn(mic_sel_label_getter, VOL_MIC);
static button_item_t btn_hmic_gain = make_btn(h_mic_gain_label_getter, VOL_HMIC);
static button_item_t btn_imic_hain = make_btn(i_mic_gain_label_getter, VOL_IMIC);
static button_item_t btn_moni_lvl  = make_btn(moni_level_label_getter, VOL_MONI);

/* VOL page 4 */

static button_item_t btn_voice       = make_btn("Voice", VOL_VOICE_LANG);
static button_item_t btn_voice_rate  = make_btn("Voice\nRate", VOL_VOICE_RATE);
static button_item_t btn_voice_pitch = make_btn("Voice\nPitch", VOL_VOICE_PITCH);
static button_item_t btn_voice_vol   = make_btn("Voice\nVolume", VOL_VOICE_VOLUME);

/* MFK page 1 */
static button_item_t btn_agc  = make_btn(agc_label_getter, MFK_AGC, &cfg_cur.agc);
static button_item_t btn_att  = {.type     = BTN_TEXT_FN,
                                 .label_fn = att_label_getter,
                                 .press    = controls_toggle_att,
                                 .hold     = button_mfk_hold_cb,
                                 .data     = MFK_ATT,
                                 .subj     = &cfg_cur.att};
static button_item_t btn_pre  = {.type     = BTN_TEXT_FN,
                                 .label_fn = pre_label_getter,
                                 .press    = controls_toggle_pre,
                                 .hold     = button_mfk_hold_cb,
                                 .data     = MFK_PRE,
                                 .subj     = &cfg_cur.pre};
static button_item_t btn_comp = {.type     = BTN_TEXT_FN,
                                 .label_fn = comp_label_getter,
                                 .press    = controls_toggle_comp,
                                 .hold     = button_mfk_hold_cb,
                                 .data     = MFK_COMP,
                                 .subj     = &cfg.comp.val};

/* MFK page 2 */

static button_item_t btn_spectrum_min_level = make_btn("Min\nLevel", MFK_MIN_LEVEL);
static button_item_t btn_spectrum_max_level = make_btn("Max\nLevel", MFK_MAX_LEVEL);
static button_item_t btn_zoom               = make_btn("FFT\nWidth", MFK_FFT_DECIM);
static button_item_t btn_spectrum_beta      = make_btn("Spectrum\nBeta", MFK_SPECTRUM_BETA);

/* MFK page 3 */

static button_item_t btn_spectrum_fill  = make_btn("Spectrum\nFill", MFK_SPECTRUM_FILL);
static button_item_t btn_spectrum_peak  = make_btn("Spectrum\nPeak", MFK_SPECTRUM_PEAK);
static button_item_t btn_spectrum_hold  = make_btn("Peaks\nHold", MFK_PEAK_HOLD);
static button_item_t btn_spectrum_speed = make_btn("Peaks\nSpeed", MFK_PEAK_SPEED);

/* MFK page 4 */
static button_item_t btn_charger = make_btn(charger_label_getter, MFK_CHARGER);
static button_item_t btn_ant     = make_btn("Antenna", MFK_ANT);
static button_item_t btn_rit     = make_btn(rit_label_getter, MFK_RIT);
static button_item_t btn_xit     = make_btn(xit_label_getter, MFK_XIT);


/* MEM page 1 */

static button_item_t btn_mem_1 = make_mem_btn("Set 1", 1);
static button_item_t btn_mem_2 = make_mem_btn("Set 2", 2);
static button_item_t btn_mem_3 = make_mem_btn("Set 3", 3);
static button_item_t btn_mem_4 = make_mem_btn("Set 4", 4);

/* MEM page 2 */

static button_item_t btn_mem_5 = make_mem_btn("Set 5", 5);
static button_item_t btn_mem_6 = make_mem_btn("Set 6", 6);
static button_item_t btn_mem_7 = make_mem_btn("Set 7", 7);
static button_item_t btn_mem_8 = make_mem_btn("Set 8", 8);

/* CW */

static button_item_t btn_key_speed  = make_btn(key_speed_label_getter, MFK_KEY_SPEED, &cfg.key_speed.val);
static button_item_t btn_key_volume = make_btn(key_volume_label_getter, MFK_KEY_VOL, &cfg.key_vol.val);
static button_item_t btn_key_train  = {.type     = BTN_TEXT_FN,
                                       .label_fn = key_train_label_getter,
                                       .press    = controls_toggle_key_train,
                                       .hold     = button_mfk_hold_cb,
                                       .data     = MFK_KEY_TRAIN,
                                       .subj     = &cfg.key_train.val};
static button_item_t btn_key_tone   = make_btn(key_tone_label_getter, MFK_KEY_TONE, &cfg.key_tone.val);

static button_item_t btn_key_mode        = make_btn(key_mode_label_getter, MFK_KEY_MODE, &cfg.key_mode.val);
static button_item_t btn_key_iambic_mode = {.type     = BTN_TEXT_FN,
                                            .label_fn = iambic_mode_label_getter,
                                            .press    = controls_toggle_key_iambic_mode,
                                            .hold     = button_mfk_hold_cb,
                                            .data     = MFK_IAMBIC_MODE,
                                            .subj     = &cfg.iambic_mode.val};
static button_item_t btn_key_qsk_time    = make_btn(qsk_time_label_getter, MFK_QSK_TIME, &cfg.qsk_time.val);
static button_item_t btn_key_ratio       = make_btn(key_ratio_label_getter, MFK_KEY_RATIO, &cfg.key_ratio.val);

static button_item_t btn_cw_decoder = {.type     = BTN_TEXT_FN,
                                       .label_fn = cw_decoder_label_getter,
                                       .press    = controls_toggle_cw_decoder,
                                       .hold     = button_mfk_hold_cb,
                                       .data     = MFK_CW_DECODER,
                                       .subj     = &cfg.cw_decoder.val};
static button_item_t btn_cw_tuner   = {.type     = BTN_TEXT_FN,
                                       .label_fn = cw_tuner_label_getter,
                                       .press    = controls_toggle_cw_tuner,
                                       .hold     = button_mfk_hold_cb,
                                       .data     = MFK_CW_TUNE,
                                       .subj     = &cfg.cw_tune.val};
static button_item_t btn_cw_snr     = make_btn(cw_snr_label_getter, MFK_CW_DECODER_SNR, &cfg.cw_decoder_snr.val);
static button_item_t btn_cw_peak_beta =
    make_btn(cw_peak_beta_label_getter, MFK_CW_DECODER_PEAK_BETA, &cfg.cw_decoder_peak_beta.val);
static button_item_t btn_cw_noise_beta =
    make_btn(cw_noise_beta_label_getter, MFK_CW_DECODER_NOISE_BETA, &cfg.cw_decoder_noise_beta.val);
/* AUTO (issue #139): toggle. Su rotulo es el indicador de que el auto-ajuste
 * esta activo; los valores que escribe se ven en los botones de al lado porque
 * van a los mismos subjects. */
static button_item_t btn_cw_auto = {.type     = BTN_TEXT_FN,
                                    .label_fn = cw_auto_label_getter,
                                    .press    = controls_toggle_cw_decoder_auto,
                                    .hold     = button_mfk_hold_cb,
                                    .data     = MFK_CW_DECODER_AUTO,
                                    .subj     = &cfg.cw_decoder_auto.val};

/* DSP */

static button_item_t btn_dnf        = {.type     = BTN_TEXT_FN,
                                       .label_fn = dnf_label_getter,
                                       .press    = controls_loop_dnf,
                                       .hold     = button_mfk_hold_cb,
                                       .data     = MFK_DNF,
                                       .subj     = &cfg.dnf.val};
static button_item_t btn_dnf_center = make_btn(dnf_center_label_getter, MFK_DNF_CENTER, &cfg.dnf_center.val);
static button_item_t btn_dnf_width  = make_btn(dnf_width_label_getter, MFK_DNF_WIDTH, &cfg.dnf_width.val);


static button_item_t btn_nb       = {.type     = BTN_TEXT_FN,
                                     .label_fn = nb_label_getter,
                                     .press    = controls_toggle_nb,
                                     .hold     = button_mfk_hold_cb,
                                     .data     = MFK_NB,
                                     .subj     = &cfg.nb.val};
static button_item_t btn_nb_level = make_btn(nb_level_label_getter, MFK_NB_LEVEL, &cfg.nb_level.val);
static button_item_t btn_nb_width = make_btn(nb_width_label_getter, MFK_NB_WIDTH, &cfg.nb_width.val);

static button_item_t btn_nr       = {.type     = BTN_TEXT_FN,
                                     .label_fn = nr_label_getter,
                                     .press    = controls_toggle_nr,
                                     .hold     = button_mfk_hold_cb,
                                     .data     = MFK_NR,
                                     .subj     = &cfg.nr.val};
static button_item_t btn_nr_level = make_btn(nr_level_label_getter, MFK_NR_LEVEL, &cfg.nr_level.val);

/* DFL */

static button_item_t btn_eq = make_app_btn("Eq", ACTION_APP_EQ);

/* Quinta celda de la pagina DFL normal (issue #179): abre el editor DFL inline
 * sobre el waterfall. Es una accion, no una app del menu de apps: se despacha en
 * main_screen_action(). */
static button_item_t btn_dfl_editor = make_action_btn("Grafico", ACTION_APP_DFL_EDITOR);

/* Pagina DFL EXPANDIDA (issue #179, C.2): las tres primeras celdas muestran SOLO
 * nombres y al tocarse SELECCIONAN el parametro para el encoder VOL (no editan
 * ni cambian el volumen); la quinta contrae. Cero numeros duplicados: los valores
 * exactos viven en las tres filas del panel. */
static void dfl_sel_cb(button_item_t *item) {
    dfl_inline_select((dfl_sel_t) item->data);
}

static button_item_t btn_dfl_sel_low = {
    .type = BTN_TEXT, .label = "LOW", .press = dfl_sel_cb, .data = DFL_SEL_LOW};
static button_item_t btn_dfl_sel_high = {
    .type = BTN_TEXT, .label = "HIGH", .press = dfl_sel_cb, .data = DFL_SEL_HIGH};
static button_item_t btn_dfl_sel_bw = {
    .type = BTN_TEXT, .label = "BW", .press = dfl_sel_cb, .data = DFL_SEL_BW};

/* Eq en la barra expandida: primero contrae limpio y despues abre Eq. */
static void dfl_eq_cb(button_item_t *item) {
    (void) item;
    dfl_inline_collapse();
    main_screen_start_app(ACTION_APP_EQ);
}

static button_item_t btn_eq_expanded = {
    .type = BTN_TEXT, .label = "Eq", .press = dfl_eq_cb};

/* Waterfall: contrae sin confirmar/deshacer. */
static void dfl_waterfall_cb(button_item_t *item) {
    (void) item;
    dfl_inline_collapse();
}

static button_item_t btn_dfl_waterfall = {
    .type = BTN_TEXT, .label = "Waterfall", .press = dfl_waterfall_cb};

/* ============================== DFL #183 ===================================
 * Paginas A (Favoritos), D (Mas), E (Confirmar guardado) y F (Elegir origen).
 * B (Ajustar) es buttons_page_dfl_1 y C (Grafico) es buttons_page_dfl_expanded:
 * el editor inline las carga por su cuenta (src/dfl_inline.c).
 *
 * La pagina actual NO se guarda aparte: se deduce de buttons_get_cur_page().
 * Todo aplica por src/dfl_presets.c (setters vigentes, sin half-apply) y el
 * contexto de E/F va en la banda contextual del waterfall (src/dfl_context.c):
 * cero ventanas, cero popups, cero sexta tecla. */

static buttons_page_t *dfl_prev_page   = NULL;   /* barra previa al DFL corto */
static int             dfl_pending_slot = 0;     /* ranura de E/F en curso (1..2) */
static dfl_family_t    dfl_last_family = DFL_FAMILY_NONE;
static dfl_save_feedback_t dfl_last_save = DFL_SAVE_FEEDBACK_NONE;

/* TX se lee del estado REAL de la radio (no de un flag que pueda quedar viejo):
 * entrar al DFL en TX tiene que dejar los presets deshabilitados ya. */
static bool dfl_is_tx(void) {
    return radio_get_state() != RADIO_RX;
}

dfl_save_feedback_t buttons_dfl_last_save(void) {
    return dfl_last_save;
}

static void dfl_assign_cancel_cb(void);

/* --- Pagina A: FAVORITOS -------------------------------------------------- */

static void dfl_fav_press_cb(button_item_t *item);
static void dfl_fav_hold_cb(button_item_t *item);
static void dfl_adj_cb(button_item_t *item);
static void dfl_more_cb(button_item_t *item);
static void dfl_mem_press_cb(button_item_t *item);
static void dfl_save_press_cb(button_item_t *item);
static void dfl_confirm_cb(button_item_t *item);
static void dfl_cancel_cb(button_item_t *item);
static void dfl_volver_cb(button_item_t *item);
static void dfl_source_press_cb(button_item_t *item);

static const char *dfl_fav_label_1() { return dfl_presets_favorite_label(dfl_current_family(), 1); }
static const char *dfl_fav_label_2() { return dfl_presets_favorite_label(dfl_current_family(), 2); }
static const char *dfl_fav_label_3() { return dfl_presets_favorite_label(dfl_current_family(), 3); }

static button_item_t btn_dfl_fav1 = {.type      = BTN_TEXT_FN,
                                     .label_fn  = dfl_fav_label_1,
                                     .press     = dfl_fav_press_cb,
                                     .hold      = dfl_fav_hold_cb,
                                     .data      = 1,
                                     .touch_hold = true};
static button_item_t btn_dfl_fav2 = {.type      = BTN_TEXT_FN,
                                     .label_fn  = dfl_fav_label_2,
                                     .press     = dfl_fav_press_cb,
                                     .hold      = dfl_fav_hold_cb,
                                     .data      = 2,
                                     .touch_hold = true};
static button_item_t btn_dfl_fav3 = {.type      = BTN_TEXT_FN,
                                     .label_fn  = dfl_fav_label_3,
                                     .press     = dfl_fav_press_cb,
                                     .hold      = dfl_fav_hold_cb,
                                     .data      = 3,
                                     .touch_hold = true};

static button_item_t btn_dfl_adj  = {.type = BTN_TEXT, .label = "Ajustar", .press = dfl_adj_cb};
static button_item_t btn_dfl_more = {.type = BTN_TEXT, .label = "Mas", .press = dfl_more_cb};

static buttons_page_t page_dfl_fav = {
    {&btn_dfl_fav1, &btn_dfl_fav2, &btn_dfl_fav3, &btn_dfl_adj, &btn_dfl_more}
};

/* --- Pagina D: MAS -------------------------------------------------------- */

static const char *dfl_mem_label_1() { return dfl_presets_memory_label(dfl_current_family(), 1); }
static const char *dfl_mem_label_2() { return dfl_presets_memory_label(dfl_current_family(), 2); }

static button_item_t btn_dfl_mem1  = {.type     = BTN_TEXT_FN,
                                      .label_fn = dfl_mem_label_1,
                                      .press    = dfl_mem_press_cb,
                                      .data     = 1};
static button_item_t btn_dfl_mem2  = {.type     = BTN_TEXT_FN,
                                      .label_fn = dfl_mem_label_2,
                                      .press    = dfl_mem_press_cb,
                                      .data     = 2};
static button_item_t btn_dfl_save1 = {.type = BTN_TEXT, .label = "Guardar\n1", .press = dfl_save_press_cb, .data = 1};
static button_item_t btn_dfl_save2 = {.type = BTN_TEXT, .label = "Guardar\n2", .press = dfl_save_press_cb, .data = 2};
static button_item_t btn_dfl_back  = {.type = BTN_TEXT, .label = "Volver", .press = dfl_volver_cb};

static buttons_page_t page_dfl_more = {
    {&btn_dfl_mem1, &btn_dfl_mem2, &btn_dfl_save1, &btn_dfl_save2, &btn_dfl_back}
};

/* --- Pagina E: CONFIRMAR GUARDADO ---------------------------------------- */

/* Celdas `--` de la pagina E: por contrato son placeholders deshabilitados (no
 * hay una quinta operacion). Llevan un handler NO-OP explicito porque un boton
 * sin `.press` es un boton muerto y el gate tests/check_app_handlers.py no lo
 * admite; ademas quedan marcadas DIsabled al refrescar la pagina, asi que
 * buttons_press() ni las llama. */
static void dfl_noop_cb(button_item_t *item) {
    (void) item;
}

static button_item_t btn_dfl_conf    = {.type = BTN_TEXT, .label = "Confirmar", .press = dfl_confirm_cb};
static button_item_t btn_dfl_cancel  = {.type = BTN_TEXT, .label = "Cancelar", .press = dfl_cancel_cb};
static button_item_t btn_dfl_none_a  = {.type = BTN_TEXT, .label = "--", .press = dfl_noop_cb};
static button_item_t btn_dfl_none_b  = {.type = BTN_TEXT, .label = "--", .press = dfl_noop_cb};
static button_item_t btn_dfl_volver  = {.type = BTN_TEXT, .label = "Volver", .press = dfl_volver_cb};

static buttons_page_t page_dfl_confirm = {
    {&btn_dfl_conf, &btn_dfl_cancel, &btn_dfl_none_a, &btn_dfl_none_b, &btn_dfl_volver}
};

/* --- Pagina F: ELEGIR ORIGEN --------------------------------------------- */

static const char *dfl_src_label_1() { return dfl_factory_name(dfl_current_family(), 0); }
static const char *dfl_src_label_2() { return dfl_factory_name(dfl_current_family(), 1); }
static const char *dfl_src_label_3() { return dfl_factory_name(dfl_current_family(), 2); }

static button_item_t btn_dfl_src1 = {.type = BTN_TEXT_FN, .label_fn = dfl_src_label_1,
                                     .press = dfl_source_press_cb, .data = DFL_SRC_FACTORY_1};
static button_item_t btn_dfl_src2 = {.type = BTN_TEXT_FN, .label_fn = dfl_src_label_2,
                                     .press = dfl_source_press_cb, .data = DFL_SRC_FACTORY_2};
static button_item_t btn_dfl_src3 = {.type = BTN_TEXT_FN, .label_fn = dfl_src_label_3,
                                     .press = dfl_source_press_cb, .data = DFL_SRC_FACTORY_3};
static button_item_t btn_dfl_srcm1 = {.type = BTN_TEXT, .label = "Mem 1",
                                      .press = dfl_source_press_cb, .data = DFL_SRC_MEM_1};
static button_item_t btn_dfl_srcm2 = {.type = BTN_TEXT, .label = "Mem 2",
                                      .press = dfl_source_press_cb, .data = DFL_SRC_MEM_2};

static buttons_page_t page_dfl_assign = {
    {&btn_dfl_src1, &btn_dfl_src2, &btn_dfl_src3, &btn_dfl_srcm1, &btn_dfl_srcm2}
};

/* --- Estado de la pagina -------------------------------------------------- */

bool buttons_dfl_active(void) {
    return buttons_dfl_page() != DFL_PAGE_NONE;
}

dfl_page_t buttons_dfl_page(void) {
    buttons_page_t *p = buttons_get_cur_page();

    if (p == NULL) {
        return DFL_PAGE_NONE;
    }
    if (p == &page_dfl_fav) {
        return DFL_PAGE_FAVORITES;
    }
    if (p == &buttons_page_dfl_1) {
        return DFL_PAGE_ADJUST;
    }
    if (p == &buttons_page_dfl_expanded) {
        return DFL_PAGE_GRAPH;
    }
    if (p == &page_dfl_more) {
        return DFL_PAGE_MORE;
    }
    if (p == &page_dfl_confirm) {
        return DFL_PAGE_CONFIRM;
    }
    if (p == &page_dfl_assign) {
        return DFL_PAGE_ASSIGN;
    }
    return DFL_PAGE_NONE;
}

/* --- Feedback y aplicacion ------------------------------------------------ */

static void dfl_apply_feedback(dfl_family_t family, int source, dfl_apply_result_t r) {
    if (r == DFL_APPLY_OK) {
        int32_t low  = subject_get_int(cfg_cur.filter.low);
        int32_t high = subject_get_int(cfg_cur.filter.high);

        msg_update_text_fmt("%s - %d Hz", dfl_source_name(family, source), (int) (high - low));
    } else if (r == DFL_APPLY_INVALID_SOURCE) {
        msg_update_text_fmt("Memoria vacia");
    } else if (r == DFL_APPLY_INVALID_VALUES) {
        msg_update_text_fmt("Preset invalido");
    } else {
        msg_update_text_fmt("No se pudo aplicar");
    }
}

/* --- Navegacion ----------------------------------------------------------- */

void buttons_dfl_open(void) {
    dfl_prev_page    = buttons_get_cur_page();
    dfl_last_family  = dfl_current_family();
    dfl_pending_slot = 0;   /* sin suboperacion en curso al entrar */

    if (dfl_last_family == DFL_FAMILY_NONE) {
        /* NFM/WFM/undefined: sin catalogo aprobado, Ajustar directo. */
        buttons_load_page(&buttons_page_dfl_1);
    } else {
        buttons_load_page(&page_dfl_fav);
    }
}

/* C = Grafico sin pasar por A/B: el editor inline se abre directo por el long
 * press fisico de DFL (`params.long_dfl`), por la celda `Grafico` y por el
 * control remoto (main_screen_action), sin que `buttons_dfl_open()` haya corrido
 * antes. Es la MISMA pagina DFL, asi que la familia vigente se fija aca: sin
 * esto la proxima comparacion de `buttons_dfl_on_mode_change()` se hace contra
 * "sin familia" y un cambio de la MISMA familia (USB<->LSB, USB-D<->LSB-D, ...)
 * se lee como familia nueva, con lo que el editor se contrae y la barra salta a
 * Favoritos (issue #182/#183). La carga de la pagina queda en un solo lugar. */
void buttons_dfl_graph_open(void) {
    dfl_last_family  = dfl_current_family();
    dfl_pending_slot = 0;   /* el editor entra sin suboperacion en curso */

    buttons_load_page(&buttons_page_dfl_expanded);
}

void buttons_dfl_short(void) {
    if (!buttons_dfl_active()) {
        return;
    }

    /* DFL corto desde C contrae el editor; desde cualquier otra subpagina
     * cancela lo pendiente. Siempre termina en A (o en B si no hay catalogo). */
    if (buttons_dfl_page() == DFL_PAGE_GRAPH) {
        dfl_inline_collapse();
    }

    if (dfl_current_family() == DFL_FAMILY_NONE) {
        buttons_load_page(&buttons_page_dfl_1);
    } else {
        buttons_load_page(&page_dfl_fav);
    }
}

void buttons_dfl_escape(void) {
    switch (buttons_dfl_page()) {
        case DFL_PAGE_GRAPH:
            dfl_inline_collapse();       /* C -> B */
            break;

        case DFL_PAGE_CONFIRM:
            buttons_load_page(&page_dfl_more);   /* E -> D, sin escribir */
            break;

        case DFL_PAGE_ADJUST:
        case DFL_PAGE_MORE:
        case DFL_PAGE_ASSIGN:
            buttons_load_page(&page_dfl_fav);
            break;

        case DFL_PAGE_FAVORITES: {
            buttons_page_t *prev = dfl_prev_page ? dfl_prev_page : &buttons_page_vol_1;

            dfl_prev_page = NULL;
            buttons_load_page(prev);
            break;
        }

        default:
            break;
    }
}

void buttons_dfl_on_mode_change(void) {
    dfl_family_t now = dfl_current_family();

    if (!buttons_dfl_active()) {
        dfl_last_family = now;
        return;
    }

    /* USB<->LSB, USB-D<->LSB-D, CW<->CWR, AM<->SAM: misma familia, no se toca la
     * pagina ni la asociacion de favoritos/memorias. */
    if (now == dfl_last_family) {
        buttons_dfl_refresh();
        return;
    }

    dfl_last_family = now;

    /* Familia nueva: cancela E/F sin persistir ni aplicar, contrae el grafico y
     * carga la pagina A; si la familia nueva no admite presets, va a B. */
    if (buttons_dfl_page() == DFL_PAGE_GRAPH) {
        dfl_inline_collapse();
    }
    dfl_pending_slot = 0;

    if (now == DFL_FAMILY_NONE) {
        buttons_load_page(&buttons_page_dfl_1);
    } else {
        buttons_load_page(&page_dfl_fav);
    }
}

void buttons_dfl_on_radio(bool tx) {
    (void) tx;

    if (buttons_dfl_active()) {
        buttons_dfl_refresh();
    }
}

/* --- Callbacks ------------------------------------------------------------ */

static void dfl_fav_press_cb(button_item_t *item) {
    if (dfl_is_tx()) {
        return;
    }

    dfl_family_t family = dfl_current_family();
    int          slot   = item->data;
    int          source = dfl_presets_favorite_source(family, slot);
    dfl_apply_result_t r = dfl_apply_favorite(slot);

    dfl_apply_feedback(family, source, r);
    buttons_dfl_refresh();
}

static void dfl_fav_hold_cb(button_item_t *item) {
    /* Reasignar es una accion poco frecuente: NO aplica el preset primero. */
    dfl_pending_slot = item->data;
    buttons_load_page(&page_dfl_assign);
}

static void dfl_adj_cb(button_item_t *item) {
    (void) item;
    buttons_load_page(&buttons_page_dfl_1);
}

static void dfl_more_cb(button_item_t *item) {
    (void) item;
    buttons_load_page(&page_dfl_more);
}

static void dfl_mem_press_cb(button_item_t *item) {
    if (dfl_is_tx()) {
        return;
    }

    dfl_family_t family = dfl_current_family();
    int          slot   = item->data;

    if (dfl_apply_memory(slot) == DFL_APPLY_OK) {
        int32_t low  = subject_get_int(cfg_cur.filter.low);
        int32_t high = subject_get_int(cfg_cur.filter.high);

        msg_update_text_fmt("Mem %d - %d Hz", slot, (int) (high - low));
        buttons_load_page(&page_dfl_fav);
    } else {
        msg_update_text_fmt("Mem %d vacia", slot);
        (void) family;
        buttons_dfl_refresh();
    }
}

static void dfl_save_press_cb(button_item_t *item) {
    dfl_pending_slot = item->data;
    buttons_load_page(&page_dfl_confirm);
}

static void dfl_confirm_cb(button_item_t *item) {
    (void) item;

    if (dfl_pending_slot < 1 || dfl_pending_slot > 2) {
        buttons_load_page(&page_dfl_more);
        return;
    }

    dfl_family_t family = dfl_current_family();
    int32_t      low    = subject_get_int(cfg_cur.filter.low);
    int32_t      high   = subject_get_int(cfg_cur.filter.high);

    /* UNA transaccion; si falla, nunca se informa "guardada". */
    if (dfl_save_memory(family, dfl_pending_slot, low, high)) {
        dfl_last_save = DFL_SAVE_FEEDBACK_OK;
        msg_update_text_fmt("Mem %d guardada", dfl_pending_slot);
        dfl_pending_slot = 0;
        buttons_load_page(&page_dfl_more);
    } else {
        dfl_last_save = DFL_SAVE_FEEDBACK_FAILED;
        msg_update_text_fmt("No se guardo: %s", dfl_presets_last_error());
        buttons_dfl_refresh();
    }
}

static void dfl_cancel_cb(button_item_t *item) {
    (void) item;
    dfl_pending_slot = 0;
    buttons_load_page(&page_dfl_more);
}

static void dfl_volver_cb(button_item_t *item) {
    (void) item;
    buttons_load_page(&page_dfl_fav);
}

static void dfl_assign_cancel_cb(void) {
    /* Accion TACTIL de la banda contextual de F: cancela sin escribir. */
    dfl_pending_slot = 0;
    buttons_load_page(&page_dfl_fav);
}

static void dfl_source_press_cb(button_item_t *item) {
    dfl_family_t family = dfl_current_family();
    int          source = item->data;

    if (dfl_assign_favorite(family, dfl_pending_slot, source)) {
        msg_update_text_fmt("Favorito %d: %s", dfl_pending_slot, dfl_source_name(family, source));
        dfl_pending_slot = 0;
        buttons_load_page(&page_dfl_fav);
    } else {
        msg_update_text_fmt("No se guardo: %s", dfl_presets_last_error());
        buttons_dfl_refresh();
    }
}

/* --- Refresco ------------------------------------------------------------- */

void buttons_dfl_refresh(void) {
    dfl_page_t   page   = buttons_dfl_page();
    dfl_family_t family = dfl_current_family();

    switch (page) {
        case DFL_PAGE_FAVORITES: {
            for (int i = 0; i < 3; i++) {
                buttons_refresh(page_dfl_fav.items[i]);
                buttons_disabled(page_dfl_fav.items[i], dfl_is_tx());
            }

            int match = -1;

            if (family != DFL_FAMILY_NONE) {
                match = dfl_matching_slot(family, subject_get_int(cfg_cur.filter.low),
                                          subject_get_int(cfg_cur.filter.high));
            }
            for (int i = 0; i < 3; i++) {
                buttons_mark(page_dfl_fav.items[i], (match == i + 1) && !dfl_is_tx());
            }

            if (family != DFL_FAMILY_NONE && match < 0) {
                dfl_context_show_text("Personalizado");
            } else {
                dfl_context_hide();
            }
            break;
        }

        case DFL_PAGE_MORE:
            buttons_refresh(&btn_dfl_mem1);
            buttons_refresh(&btn_dfl_mem2);
            buttons_disabled(&btn_dfl_mem1, dfl_is_tx() || !dfl_presets_memory_present(family, 1));
            buttons_disabled(&btn_dfl_mem2, dfl_is_tx() || !dfl_presets_memory_present(family, 2));
            buttons_disabled(&btn_dfl_save1, dfl_is_tx());
            buttons_disabled(&btn_dfl_save2, dfl_is_tx());
            dfl_context_hide();
            break;

        case DFL_PAGE_CONFIRM: {
            char ctx[112];
            int  slot = (dfl_pending_slot >= 1 && dfl_pending_slot <= 2) ? dfl_pending_slot : 1;

            if (dfl_presets_memory_present(family, slot)) {
                snprintf(ctx, sizeof(ctx), "Guardar Mem %d - LOW %d - HIGH %d - Se reemplazara Mem %d",
                         slot, (int) subject_get_int(cfg_cur.filter.low),
                         (int) subject_get_int(cfg_cur.filter.high), slot);
            } else {
                snprintf(ctx, sizeof(ctx), "Guardar Mem %d - LOW %d - HIGH %d", slot,
                         (int) subject_get_int(cfg_cur.filter.low),
                         (int) subject_get_int(cfg_cur.filter.high));
            }
            dfl_context_show_text(ctx);

            buttons_disabled(&btn_dfl_none_a, true);
            buttons_disabled(&btn_dfl_none_b, true);
            break;
        }

        case DFL_PAGE_ASSIGN: {
            char ctx[48];

            buttons_refresh(&btn_dfl_srcm1);
            buttons_refresh(&btn_dfl_srcm2);
            buttons_disabled(&btn_dfl_srcm1, !dfl_presets_memory_present(family, 1));
            buttons_disabled(&btn_dfl_srcm2, !dfl_presets_memory_present(family, 2));

            snprintf(ctx, sizeof(ctx), "Asignar tecla %d",
                     (dfl_pending_slot >= 1 && dfl_pending_slot <= 3) ? dfl_pending_slot : 1);
            dfl_context_show_action(ctx, "Cancelar", dfl_assign_cancel_cb);
            break;
        }

        default:
            dfl_context_hide();
            break;
    }
}

/* Ruta TACTIL de la barra (issue #183): solo las celdas cargadas con
 * touch_hold (los favoritos 1-3) disparan la pulsacion/hold; en el resto de las
 * paginas la barra sigue siendo una leyenda de teclas fisicas. */
static void button_bar_event_cb(lv_event_t *e) {
    uint8_t i = (uint8_t) (intptr_t) lv_event_get_user_data(e);

    if (i >= BUTTONS || !btn[i].touch) {
        return;
    }
    buttons_press(i, lv_event_get_code(e) == LV_EVENT_LONG_PRESSED);
}

/* Los Subjects del filtro cambian por el editor inline, por la rueda VOL o por
 * la aplicacion de un preset: las etiquetas/marcas de las paginas DFL salen de
 * ahi. Es un observer DIFERIDO (se notifica en el hilo de GUI, como el resto de
 * buttons.cpp; precedente #131). */
static ObserverDelayed *dfl_obs_low  = NULL;
static ObserverDelayed *dfl_obs_high = NULL;

static void dfl_subject_cb(Subject *subj, void *user_data) {
    (void) subj;
    (void) user_data;

    if (buttons_dfl_active()) {
        buttons_dfl_refresh();
    }
}

/* APP */

static button_item_t btn_rtty = make_app_btn("RTTY", ACTION_APP_RTTY);
static button_item_t btn_ft8  = make_app_btn("FT8", ACTION_APP_FT8);
static button_item_t btn_swr  = make_app_btn("SWR\nScan", ACTION_APP_SWRSCAN);
/* Asistente de sintonia fina (issue #100): accion, no app. No abre dialogo: mueve
 * el dial por los probes y lo deja en el mejor spot (o lo restaura). */
static button_item_t btn_atune = make_action_btn("Auto\nTune", ACTION_ATUNE);

static button_item_t btn_rec      = make_app_btn("Recorder", ACTION_APP_RECORDER);
static button_item_t btn_qth      = make_action_btn("QTH", ACTION_APP_QTH);
static button_item_t btn_callsign = make_action_btn("Callsign", ACTION_APP_CALLSIGN);
static button_item_t btn_settings = make_app_btn("Settings", ACTION_APP_SETTINGS);

/* Resto de las apps: estaban solo en la lista del long press (Ajustes), asi que no se
 * encontraban. Ahora tienen boton en las paginas APP (docs/HALLAZGOS.md 29). */
static button_item_t btn_js8        = make_app_btn("JS8", ACTION_APP_JS8);
static button_item_t btn_wspr       = make_app_btn("WSPR", ACTION_APP_WSPR);
static button_item_t btn_sstv       = make_app_btn("SSTV", ACTION_APP_SSTV);
static button_item_t btn_wefax      = make_app_btn("WeFax", ACTION_APP_WEFAX);
static button_item_t btn_navtex     = make_app_btn("NavTex", ACTION_APP_NAVTEX);
static button_item_t btn_cluster    = make_app_btn("Cluster", ACTION_APP_CLUSTER);
static button_item_t btn_field      = make_app_btn("Field\nOps", ACTION_APP_FIELD);
static button_item_t btn_gps        = make_app_btn("GPS", ACTION_APP_GPS);
static button_item_t btn_bands      = make_app_btn("Bands", ACTION_APP_BANDS);
static button_item_t btn_cw_kbd     = make_app_btn("CW\nKbd", ACTION_APP_CW_KBD);
static button_item_t btn_cw_trainer = make_app_btn("CW\nTrainer", ACTION_APP_CWTRAINER);
static button_item_t btn_status     = make_app_btn("Status", ACTION_APP_STATUS);
/* App Propagacion (issue #145): beacons IBP/IARU R2 de la banda actual, RX. */
static button_item_t btn_propagation = make_app_btn("Propagation", ACTION_APP_PROPAGATION);

static button_item_t  btn_wifi   = make_app_btn("WiFi", ACTION_APP_WIFI);
/* El de Bluetooth va al lado del de WiFi: es la UX para elegir a que dispositivo emparejarse
 * (abre dialog_bt: escanear, lista de pareados y encontrados, parear, conectar/desconectar). */
static button_item_t  btn_bt     = make_app_btn("BT", ACTION_APP_BT);
/* App Logs (subida de QSO a QRZ/eQSL/Clublog): tenia `case` en main_screen_start_app y
 * entrada en el control remoto, pero ningun boton en las paginas APP, asi que solo se
 * llegaba por long press y tests/check_apps_menu.py la marcaba. Tiene su pagina propia
 * (APP 7:7) porque las 6 paginas ya estan completas (4 apps + boton de pagina = BUTTONS). */
static button_item_t  btn_logs   = make_app_btn("Logs", ACTION_APP_LOGS);

/* RTTY */
static button_item_t btn_rtty_p1 = {
    .type  = BTN_TEXT,
    .label = "(RTTY 1:1)",
    .press = NULL,
};
static button_item_t btn_rtty_rate = {
    .type  = BTN_TEXT,
    .label = "Rate",
    .press = button_mfk_update_cb,
    .data  = MFK_RTTY_RATE,
};
static button_item_t btn_rtty_shift = {
    .type  = BTN_TEXT,
    .label = "Shift",
    .press = button_mfk_update_cb,
    .data  = MFK_RTTY_SHIFT,
};
static button_item_t btn_rtty_center = {
    .type  = BTN_TEXT,
    .label = "Center",
    .press = button_mfk_update_cb,
    .data  = MFK_RTTY_CENTER,
};
static button_item_t btn_rtty_reverse = {
    .type  = BTN_TEXT,
    .label = "Reverse",
    .press = button_mfk_update_cb,
    .data  = MFK_RTTY_REVERSE,
};


/* VOL pages */
static button_item_t btn_vol_p1 = make_page_btn("(VOL 1:3)", "Volume|page 1");
static button_item_t btn_vol_p2 = make_page_btn("(VOL 2:3)", "Volume|page 2");
static button_item_t btn_vol_p3 = make_page_btn("(VOL 3:3)", "Volume|page 3");

buttons_page_t buttons_page_vol_1 = {
    {&btn_vol_p1, &btn_vol, &btn_sql, &btn_rfg, &btn_tx_pwr}
};
static buttons_page_t page_vol_2 = {
    {&btn_vol_p2, &btn_mic_sel, &btn_hmic_gain, &btn_imic_hain, &btn_moni_lvl}
};
static buttons_page_t page_vol_3 = {
    {&btn_vol_p3, &btn_voice, &btn_voice_rate, &btn_voice_pitch, &btn_voice_vol}
};

/* MFK pages */
static button_item_t btn_mfk_p1 = make_page_btn("(MFK 1:4)", "MFK|page 1");
static button_item_t btn_mfk_p2 = make_page_btn("(MFK 2:4)", "MFK|page 2");
static button_item_t btn_mfk_p3 = make_page_btn("(MFK 3:4)", "MFK|page 3");
static button_item_t btn_mfk_p4 = make_page_btn("(MFK 4:4)", "MFK|page 4");

static buttons_page_t page_mfk_1 = {
    {&btn_mfk_p1, &btn_agc, &btn_att, &btn_pre, &btn_comp}
};
static buttons_page_t page_mfk_2 = {
    {&btn_mfk_p2, &btn_spectrum_min_level, &btn_spectrum_max_level, &btn_zoom, &btn_spectrum_beta}
};
static buttons_page_t page_mfk_3 = {
    {&btn_mfk_p3, &btn_spectrum_fill, &btn_spectrum_peak, &btn_spectrum_hold, &btn_spectrum_speed}
};
static buttons_page_t page_mfk_4 = {
    {&btn_mfk_p4, &btn_charger, &btn_ant, &btn_rit, &btn_xit}
};

/* MEM pages */

static button_item_t btn_mem_p1 = make_page_btn("(MEM 1:2)", "Memory|page 1");
static button_item_t btn_mem_p2 = make_page_btn("(MEM 2:2)", "Memory|page 2");

static buttons_page_t page_mem_1 = {
    {&btn_mem_p1, &btn_mem_1, &btn_mem_2, &btn_mem_3, &btn_mem_4}
};
static buttons_page_t page_mem_2 = {
    {&btn_mem_p2, &btn_mem_5, &btn_mem_6, &btn_mem_7, &btn_mem_8}
};

/* KEY pages */
static button_item_t btn_key_p1 = make_page_btn("(KEY 1:2)", "Key|page 1");
static button_item_t btn_key_p2 = make_page_btn("(KEY 2:2)", "Key|page 2");
static button_item_t btn_cw_p1  = make_page_btn("(CW 1:2)", "CW|page 1");
static button_item_t btn_cw_p2  = make_page_btn("(CW 2:2)", "CW|page 2");

static buttons_page_t page_key_1 = {
    {&btn_key_p1, &btn_key_speed, &btn_key_volume, &btn_key_train, &btn_key_tone}
};
static buttons_page_t page_key_2 = {
    {&btn_key_p2, &btn_key_mode, &btn_key_iambic_mode, &btn_key_qsk_time, &btn_key_ratio}
};
static buttons_page_t page_cw_decoder_1 = {
    {&btn_cw_p1, &btn_cw_decoder, &btn_cw_tuner, &btn_cw_snr}
};
static buttons_page_t page_cw_decoder_2 = {
    {&btn_cw_p2, &btn_cw_peak_beta, &btn_cw_noise_beta, &btn_cw_auto}
};

/* DFN pages */
static button_item_t btn_dfn_p1 = make_page_btn("(DFN 1:3)", "DNF page");
static button_item_t btn_dfn_p2 = make_page_btn("(DFN 2:3)", "NB page");
static button_item_t btn_dfn_p3 = make_page_btn("(DFN 3:3)", "NR page");

static buttons_page_t page_dfn_1 = {
    {&btn_dfn_p1, &btn_dnf, &btn_dnf_center, &btn_dnf_width}
};
static buttons_page_t page_dfn_2 = {
    {&btn_dfn_p2, &btn_nb, &btn_nb_level, &btn_nb_width}
};
static buttons_page_t page_dfn_3 = {
    {&btn_dfn_p3, &btn_nr, &btn_nr_level}
};

/* DFL pages */
buttons_page_t buttons_page_dfl_1 = {
    {&btn_flt_low, &btn_flt_high, &btn_flt_bw, &btn_eq, &btn_dfl_editor}
};

buttons_page_t buttons_page_dfl_expanded = {
    {&btn_dfl_sel_low, &btn_dfl_sel_high, &btn_dfl_sel_bw, &btn_eq_expanded, &btn_dfl_waterfall}
};

/* App pages: TODAS las apps de main_screen_start_app tienen que estar aca (el chequeo
 * tests/check_apps_menu.py lo verifica). El boton de pagina ocupa un lugar, asi que entran 4
 * apps por pagina. */
static button_item_t btn_app_p1 = make_page_btn("(APP 1:7)", "Application|page 1");
static button_item_t btn_app_p2 = make_page_btn("(APP 2:7)", "Application|page 2");
static button_item_t btn_app_p3 = make_page_btn("(APP 3:7)", "Application|page 3");
static button_item_t btn_app_p4 = make_page_btn("(APP 4:7)", "Application|page 4");
static button_item_t btn_app_p5 = make_page_btn("(APP 5:7)", "Application|page 5");
static button_item_t btn_app_p6 = make_page_btn("(APP 6:7)", "Application|page 6");
static button_item_t btn_app_p7 = make_page_btn("(APP 7:7)", "Application|page 7");

static buttons_page_t page_app_1 = {   /* modos digitales */
    {&btn_app_p1, &btn_ft8, &btn_js8, &btn_wspr, &btn_rtty}
};
static buttons_page_t page_app_2 = {   /* imagen y avisos */
    {&btn_app_p2, &btn_sstv, &btn_wefax, &btn_navtex, &btn_cluster}
};
static buttons_page_t page_app_3 = {   /* operacion de campo */
    {&btn_app_p3, &btn_rec, &btn_field, &btn_gps, &btn_bands}
};
static buttons_page_t page_app_4 = {   /* herramientas */
    {&btn_app_p4, &btn_cw_kbd, &btn_cw_trainer, &btn_eq, &btn_status}
};
static buttons_page_t page_app_5 = {   /* configuracion */
    {&btn_app_p5, &btn_settings, &btn_callsign, &btn_qth, &btn_propagation}
};
static buttons_page_t page_app_6 = {   /* radio y red */
    {&btn_app_p6, &btn_wifi, &btn_bt, &btn_swr, &btn_atune}
};
static buttons_page_t page_app_7 = {   /* registro de QSO y subida a logs online */
    {&btn_app_p7, &btn_logs}
};

/* RTTY */

buttons_page_t buttons_page_rtty = {
    {&btn_rtty_p1, &btn_rtty_rate, &btn_rtty_shift, &btn_rtty_center, &btn_rtty_reverse}
};

buttons_group_t buttons_group_gen = {
    &buttons_page_vol_1,
    &page_vol_2,
    &page_vol_3,
    &page_mfk_1,
    &page_mfk_2,
    &page_mfk_3,
    &page_mfk_4,
    &page_mem_1,
    &page_mem_2,
};

buttons_group_t buttons_group_app = {
    &page_app_1,
    &page_app_2,
    &page_app_3,
    &page_app_4,
    &page_app_5,
    &page_app_6,
    &page_app_7,
};

buttons_group_t buttons_group_key = {
    &page_key_1,
    &page_key_2,
    &page_cw_decoder_1,
    &page_cw_decoder_2,
};

buttons_group_t buttons_group_dfn = {
    &page_dfn_1,
    &page_dfn_2,
    &page_dfn_3,
};

buttons_group_t buttons_group_dfl = {
    &page_dfl_fav,
};


static struct {
    buttons_page_t **group;
    size_t           size;
} groups[] = {
    {buttons_group_gen, ARRAY_SIZE(buttons_group_gen)},
    {buttons_group_app, ARRAY_SIZE(buttons_group_app)},
    {buttons_group_key, ARRAY_SIZE(buttons_group_key)},
    {buttons_group_dfn, ARRAY_SIZE(buttons_group_dfn)},
    {buttons_group_dfl, ARRAY_SIZE(buttons_group_dfl)},
};

void buttons_init(lv_obj_t *parent) {

    btn_rfg.subj = &cfg_cur.band->rfg.val;

    /* Fill prev/next pointers */
    for (size_t i = 0; i < ARRAY_SIZE(groups); i++) {
        buttons_page_t **group = groups[i].group;
        for (size_t j = 0; j < groups[i].size; j++) {
            if (group[j]->items[0]->press == button_next_page_cb) {
                uint16_t next_id = (j + 1) % groups[i].size;
                group[j]->items[0]->next = group[next_id];
            } else {
                LV_LOG_USER("First button in page=%u, group=%u press cb is not next", j, i);
            }
            if (group[j]->items[0]->hold == button_prev_page_cb) {
                uint16_t prev_id = (groups[i].size + j - 1) % groups[i].size;
                group[j]->items[0]->prev = group[prev_id];
            } else {
                LV_LOG_USER("First button in page=%u, group=%u hold cb is not prev", j, i);
            }
        }
    }

    uint16_t y = 480 - btn_height;
    uint16_t x = 0;
    uint16_t width = 800 / 5;

    for (uint8_t i = 0; i < 5; i++) {
        lv_obj_t *f = lv_obj_create(parent);

        lv_obj_remove_style_all(f);
        lv_obj_add_style(f, &btn_style, 0);
        lv_obj_add_style(f, &btn_active_style, LV_STATE_CHECKED);
        lv_obj_add_style(f, &btn_disabled_style, LV_STATE_DISABLED);

        lv_obj_set_pos(f, x, y);
        lv_obj_set_size(f, width, btn_height);
        x += width;

        lv_obj_t *label = lv_label_create(f);

        lv_obj_center(label);
        lv_obj_set_user_data(f, label);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

        /* Ruta tactil (#183): la celda avisa su pulsacion/hold SOLO si el item
         * cargado lo pide (los favoritos 1-3); el resto de las paginas queda
         * igual. LVGL suprime el CLICKED cuando ya mando LONG_PRESSED. */
        lv_obj_add_event_cb(f, button_bar_event_cb, LV_EVENT_CLICKED, (void *) (intptr_t) i);
        lv_obj_add_event_cb(f, button_bar_event_cb, LV_EVENT_LONG_PRESSED, (void *) (intptr_t) i);

        btn[i].obj   = f;
        btn[i].touch = false;
        btn[i].label = label;
    }


    parent_obj = parent;
    lv_msg_subscribe(MSG_PARAM_CHANGED, param_changed_cb, NULL);

    /* Las etiquetas de las paginas DFL siguen a los Subjects del filtro. */
    dfl_obs_low  = subject_add_delayed_observer(cfg_cur.filter.low, dfl_subject_cb, NULL);
    dfl_obs_high = subject_add_delayed_observer(cfg_cur.filter.high, dfl_subject_cb, NULL);
}

void buttons_refresh(button_item_t *item) {
    if (item->label_obj) {
        if (item->type == BTN_TEXT) {
            lv_label_set_text(item->label_obj, item->label);
        } else if (item->type == BTN_TEXT_FN) {
            lv_label_set_text(item->label_obj, item->label_fn());
        } else {
            lv_label_set_text(item->label_obj, "--");
        }

    } else {
        LV_LOG_WARN("Button item label obj is null");
    }
}

void buttons_mark(button_item_t *item, bool on) {
    if (item->label_obj) {
        lv_obj_t *btn = lv_obj_get_parent(item->label_obj);
        if (on) {
            lv_obj_add_state(btn, LV_STATE_CHECKED);

        } else {
            lv_obj_clear_state(btn, LV_STATE_CHECKED);
        }
    }
}

void buttons_disabled(button_item_t *item, bool on) {
    if (item->label_obj) {
        lv_obj_t *btn = lv_obj_get_parent(item->label_obj);
        if (on) {
            lv_obj_add_state(btn, LV_STATE_DISABLED);

        } else {
            lv_obj_clear_state(btn, LV_STATE_DISABLED);
        }
    }
}

void buttons_load(uint8_t n, button_item_t *item) {
    button_item_t *prev_item = btn[n].item;
    if (prev_item) {
        prev_item->label_obj = NULL;
        if (prev_item->observer) {
            delete prev_item->observer;
            prev_item->observer = NULL;
        }
    }

    lv_obj_t *label = btn[n].label;
    if (item) {
        item->label_obj = label;
        buttons_refresh(item);
        if (item->type == BTN_TEXT_FN) {
            if (item->subj && *item->subj) {
                item->observer = (*item->subj)->subscribe_delayed(label_update_cb, item);
            } else {
                lv_obj_set_user_data(label, (void *)item->label_fn);
            }
        }
    } else {
        lv_label_set_text(label, "");
    }
    btn[n].item = item;

    /* Touch (#183): la celda solo captura toques si el item lo pide. */
    btn[n].touch = item != NULL && item->touch_hold;
    if (btn[n].obj != NULL) {
        if (btn[n].touch) {
            lv_obj_add_flag(btn[n].obj, LV_OBJ_FLAG_CLICKABLE);
        } else {
            lv_obj_clear_flag(btn[n].obj, LV_OBJ_FLAG_CLICKABLE);
        }
    }
}

/* Un hilo ajeno al de LVGL (el keyer CW, los mensajes de voz) llama a estas dos
 * funciones: su cuerpo toca LVGL y corrompe la lista de eventos (issue #131).
 * Cuando el llamador no es el hilo de GUI, se difiere con el scheduler y el
 * cuerpo corre en el hilo de LVGL, en el mismo orden en que se pidio. Desde el
 * hilo de GUI el comportamiento es el de siempre (sin diferir). */
static void buttons_load_page_job(void *arg) {
    buttons_load_page(*(buttons_page_t **) arg);
}

static void buttons_unload_page_job(void *arg) {
    (void) arg;
    buttons_unload_page();
}

void buttons_load_page(buttons_page_t *page) {
    if (!scheduler_in_main_thread()) {
        if (!scheduler_put(buttons_load_page_job, &page, sizeof(page))) {
            LV_LOG_WARN("buttons: no se pudo diferir la pagina (cola del scheduler llena)");
        }
        return;
    }
    if (!page) {
        LV_LOG_ERROR("NULL pointer to buttons page");
        return;
    }
    if (cur_page) {
        buttons_unload_page();
    }
    cur_page = page;
    for (uint8_t i = 0; i < BUTTONS; i++) {
        buttons_load(i, page->items[i]);
    }
    /* Una pagina puede no tener ningun boton (la de Status, por ejemplo): items[0] es NULL y
     * leer items[0]->voice es un SIGSEGV (medido: fallo 0x14 dentro de buttons_load_page).
     * docs/HALLAZGOS.md 38. */
    if (page->items[0] != NULL && page->items[0]->voice != NULL) {
        voice_say_text_fmt("%s", page->items[0]->voice);
    }

    /* Paginas DFL (#183): rearma etiquetas/marcas/disabled y la banda contextual.
     * Fuera del DFL, la banda contextual se oculta. */
    if (buttons_dfl_page() != DFL_PAGE_NONE) {
        buttons_dfl_refresh();
    } else {
        dfl_context_hide();
    }
}

void buttons_unload_page() {
    if (!scheduler_in_main_thread()) {
        scheduler_put_noargs(buttons_unload_page_job);
        return;
    }
    cur_page = NULL;
    for (uint8_t i = 0; i < BUTTONS; i++) {
        lv_obj_t        *label = btn[i].label;
        lv_label_set_text(label, "");
        lv_obj_set_user_data(label, NULL);
        lv_obj_clear_state(lv_obj_get_parent(label), LV_STATE_CHECKED);
        lv_obj_clear_state(lv_obj_get_parent(label), LV_STATE_DISABLED);
        btn[i].touch = false;
        if (btn[i].obj != NULL) {
            lv_obj_clear_flag(btn[i].obj, LV_OBJ_FLAG_CLICKABLE);
        }
        if (btn[i].item) {
            btn[i].item->label_obj = NULL;
            if (btn[i].item->observer) {
                delete btn[i].item->observer;
                btn[i].item->observer = NULL;
            }
            btn[i].item = NULL;
        }
    }
}

void button_next_page_cb(button_item_t *item) {
    buttons_unload_page();
    buttons_load_page(item->next);
}

void button_prev_page_cb(button_item_t *item) {
    buttons_unload_page();
    buttons_load_page(item->prev);
}

void button_update_label(button_item_t *item, char *label) {
    if (item->type == BTN_TEXT) {
        item->label = label;
        lv_label_set_text(item->label_obj, item->label);
    }
}

static void button_app_page_cb(button_item_t *item) {
    main_screen_start_app((press_action_t)item->data);
}

static void button_action_cb(button_item_t *item) {
    main_screen_action((press_action_t)item->data);
}

static void button_vol_update_cb(button_item_t *item) {
    vol_set_mode((vol_mode_t)item->data);
    vol_update(0, true);
}

static void button_mfk_update_cb(button_item_t *item) {
    mfk_set_mode((mfk_mode_t)item->data);
    mfk_update(0, true);
}

static void button_vol_hold_cb(button_item_t *item) {
    uint64_t        mask = (uint64_t) 1L << item->data;

    if (params.vol_modes ^ mask) {
        params_lock();
        params.vol_modes ^= mask;
        params_unlock(&params.dirty.vol_modes);

        if (params.vol_modes & mask) {
            msg_update_text_fmt("Added to VOL encoder");
            voice_say_text_fmt("Added to volume encoder");
        } else {
            msg_update_text_fmt("Removed from VOL encoder");
            voice_say_text_fmt("Removed from volume encoder");
        }
    }
}

static void button_mfk_hold_cb(button_item_t *item) {
    uint64_t        mask = (uint64_t) 1L << item->data;

    if (params.mfk_modes ^ mask) {
        params_lock();
        params.mfk_modes ^= mask;
        params_unlock(&params.dirty.mfk_modes);

        if (params.mfk_modes & mask) {
            msg_update_text_fmt("Added to MFK encoder");
            voice_say_text_fmt("Added to MFK encoder");
        } else {
            msg_update_text_fmt("Removed from MFK encoder");
            voice_say_text_fmt("Removed from MFK encoder");
        }
    }
}

static void button_mem_load_cb(button_item_t *item) {
    mem_load(item->data);
    voice_say_text_fmt("Memory %i loaded", item->data);
}

static void button_mem_save_cb(button_item_t *item) {
    mem_save(item->data);
    voice_say_text_fmt("Memory %i stored", item->data);
}

void buttons_press(uint8_t n, bool hold) {
    button_item_t *item = btn[n].item;
    if (item == NULL) {
        LV_LOG_WARN("Button %u is NULL", n);
        return;
    }
    /* Un item puede estar cargado en la fila y a la vez haber quedado sin label_obj (por ejemplo
     * si el mismo item se usa en dos paginas y se descarga una): lv_obj_get_parent(NULL) es un
     * SIGSEGV. Se avisa y se sale, como con el item NULL de mas arriba (docs/HALLAZGOS.md 33). */
    if (item->label_obj == NULL) {
        LV_LOG_WARN("Button %u has no label object (item not loaded)", n);
        return;
    }

    lv_obj_t *btn_obj = lv_obj_get_parent(item->label_obj);
    if (lv_obj_has_state(btn_obj, LV_STATE_DISABLED)) {
        LV_LOG_INFO("Button %s is disabled", lv_label_get_text(item->label_obj));
        return;
    }
    if (hold) {
        if (item->hold) {
            item->hold(item);
        } else {
            LV_LOG_USER("Button %u hold action is NULL", n);
        }
    } else {
        if (item->press) {
            item->press(item);
        } else {
            LV_LOG_USER("Button %u press action is NULL", n);
        }
    }
}

void buttons_load_page_group(buttons_group_t group) {
    size_t group_size = 0;
    for (size_t i = 0; i < ARRAY_SIZE(groups); i++) {
        if (groups[i].group == group) {
            group_size = groups[i].size;
            break;
        }
    }
    if (group_size <= 0) {
        return;
    }
    for (size_t i = 0; i < group_size; i++) {
        if ((group[i] == cur_page) && (cur_page->items[0] != NULL) && (cur_page->items[0]->next)) {
            // load next
            cur_page->items[0]->press(cur_page->items[0]);
            return;
        }
    }
    // Load first
    buttons_load_page(group[0]);
}

buttons_page_t *buttons_get_cur_page() {
    return cur_page;
}

uint8_t buttons_bar_height(void) {
    return btn_height;
}

void buttons_dfl_expanded_sync(int sel) {
    buttons_mark(&btn_dfl_sel_low, sel == DFL_SEL_LOW);
    buttons_mark(&btn_dfl_sel_high, sel == DFL_SEL_HIGH);
    buttons_mark(&btn_dfl_sel_bw, sel == DFL_SEL_BW);
}

lv_obj_t *buttons_cell_obj(uint8_t n) {
    return (n < BUTTONS) ? btn[n].obj : NULL;
}

static const char * vol_label_getter() {
    static char buf[16];
    sprintf(buf, "Volume:\n%zi", subject_get_int(cfg.vol.val));
    return buf;
}

static const char * sql_label_getter() {
    static char buf[16];
    sprintf(buf, "Squelch:\n%zu", subject_get_int(cfg_cur.sql_level));
    return buf;
}

static const char * rfg_label_getter() {
    static char buf[16];
    sprintf(buf, "RF gain:\n%zu", subject_get_int(cfg_cur.band->rfg.val));
    return buf;
}

static const char * tx_power_label_getter() {
    static char buf[20];
    sprintf(buf, "TX power:\n%0.1f W", subject_get_float(cfg.pwr.val));
    return buf;
}

static const char * filter_low_label_getter() {
    static char buf[22];
    sprintf(buf, "Filter low:\n%d Hz", subject_get_int(cfg_cur.filter.low));
    return buf;
}
static const char * filter_high_label_getter() {
    static char buf[22];
    sprintf(buf, "Filter high:\n%d Hz", subject_get_int(cfg_cur.filter.high));
    return buf;
}

static const char * filter_bw_label_getter() {
    static char buf[22];
    sprintf(buf, "Filter BW:\n%i Hz", subject_get_int(cfg_cur.filter.bw));
    return buf;
}


static const char * mic_sel_label_getter() {
    static char buf[22];
    sprintf(buf, "MIC Sel:\n%s", params_mic_str_get(params.mic));
    return buf;
}


static const char * h_mic_gain_label_getter() {
    static char buf[22];
    sprintf(buf, "H-Mic gain:\n%zu", params.hmic);
    return buf;
}

static const char * i_mic_gain_label_getter() {
    static char buf[22];
    sprintf(buf, "I-Mic gain:\n%zu", params.imic);
    return buf;
}

static const char * moni_level_label_getter() {
    static char buf[22];
    sprintf(buf, "Moni level:\n%zu", params.moni);
    return buf;
}

static const char * charger_label_getter() {
    static char buf[22];
    sprintf(buf, "Charger:\n%s", params_charger_str_get(params.charger));
    return buf;
}


static const char * rit_label_getter() {
    static char buf[22];
    sprintf(buf, "RIT:\n%+zi", params.rit);
    return buf;
}

static const char * xit_label_getter() {
    static char buf[22];
    sprintf(buf, "XIT:\n%+zi", params.xit);
    return buf;
}

static const char * agc_label_getter() {
    const char * agc_str = cfg_mode_agc_label((x6200_agc_t)subject_get_int(cfg_cur.agc));
    static char buf[22];
    sprintf(buf, "AGC:\n%s", agc_str);
    return buf;
}

static const char * att_label_getter() {
    static char buf[22];
    sprintf(buf, "ATT:\n%s", subject_get_int(cfg_cur.att) ? "On": "Off");
    return buf;
}

static const char * pre_label_getter() {
    static char buf[22];
    sprintf(buf, "PRE:\n%s", subject_get_int(cfg_cur.pre) ? "On": "Off");
    return buf;
}

static const char * comp_label_getter() {
    static char buf[22];
    sprintf(buf, "Comp:\n%s", subject_get_int(cfg.comp.val) ? "On": "Off");
    return buf;
}

static const char * key_speed_label_getter() {
    static char buf[22];
    sprintf(buf, "Speed:\n%zu wpm", subject_get_int(cfg.key_speed.val));
    return buf;
}

static const char * key_volume_label_getter() {
    static char buf[22];
    sprintf(buf, "Volume:\n%zu", subject_get_int(cfg.key_vol.val));
    return buf;
}

static const char * key_train_label_getter() {
    static char buf[22];
    sprintf(buf, "Train:\n%s", subject_get_int(cfg.key_train.val) ? "On": "Off");
    return buf;
}

static const char * key_tone_label_getter() {
    static char buf[22];
    sprintf(buf, "Tone:\n%zu Hz", subject_get_int(cfg.key_tone.val));
    return buf;
}

static const char * key_mode_label_getter() {
    static char buf[22];
    sprintf(buf, "Mode:\n%s", params_key_mode_str_get((x6200_key_mode_t)subject_get_int(cfg.key_mode.val)));
    return buf;
}

static const char * iambic_mode_label_getter() {
    static char buf[22];
    sprintf(buf, "Iambic:\n%s mode", params_iambic_mode_str_ger((x6200_iambic_mode_t)subject_get_int(cfg.iambic_mode.val)));
    return buf;
}

static const char * qsk_time_label_getter() {
    static char buf[22];
    sprintf(buf, "QSK time:\n%zu ms", subject_get_int(cfg.qsk_time.val));
    return buf;
}

static const char * key_ratio_label_getter() {
    static char buf[22];
    sprintf(buf, "Ratio:\n%0.1f", subject_get_float(cfg.key_ratio.val));
    return buf;
}

static const char * cw_decoder_label_getter() {
    static char buf[22];
    sprintf(buf, "Decoder:\n%s", subject_get_int(cfg.cw_decoder.val) ? "On": "Off");
    return buf;
}

static const char * cw_tuner_label_getter() {
    static char buf[22];
    sprintf(buf, "Tuner:\n%s", subject_get_int(cfg.cw_tune.val) ? "On": "Off");
    return buf;
}

static const char * cw_snr_label_getter() {
    static char buf[22];
    sprintf(buf, "Dec SNR:\n%0.1f dB", subject_get_float(cfg.cw_decoder_snr.val));
    return buf;
}

static const char * cw_peak_beta_label_getter() {
    static char buf[22];
    sprintf(buf, "Peak beta:\n%0.2f", subject_get_float(cfg.cw_decoder_peak_beta.val));
    return buf;
}

static const char * cw_noise_beta_label_getter() {
    static char buf[22];
    sprintf(buf, "Noise beta:\n%0.2f", subject_get_float(cfg.cw_decoder_noise_beta.val));
    return buf;
}

static const char * cw_auto_label_getter() {
    static char buf[22];
    sprintf(buf, "Auto:\n%s", subject_get_int(cfg.cw_decoder_auto.val) ? "On": "Off");
    return buf;
}

static const char * dnf_label_getter() {
    static char buf[22];
    sprintf(buf, "DNF:\n%s", cfg_dnf_label_get());
    return buf;
}

static const char * dnf_center_label_getter() {
    static char buf[22];
    sprintf(buf, "DNF freq:\n%zu Hz", subject_get_int(cfg.dnf_center.val));
    return buf;
}

static const char * dnf_width_label_getter() {
    static char buf[22];
    sprintf(buf, "DNF width:\n%zu Hz", subject_get_int(cfg.dnf_width.val));
    return buf;
}


static const char * nb_label_getter() {
    static char buf[22];
    sprintf(buf, "NB:\n%s", subject_get_int(cfg.nb.val) ? "On": "Off");
    return buf;
}

static const char * nb_level_label_getter() {
    static char buf[22];
    sprintf(buf, "NB level:\n%zu", subject_get_int(cfg.nb_level.val));
    return buf;
}

static const char * nb_width_label_getter() {
    static char buf[22];
    sprintf(buf, "NB width:\n%zu Hz", subject_get_int(cfg.nb_width.val));
    return buf;
}

static const char * nr_label_getter() {
    static char buf[22];
    sprintf(buf, "NR:\n%s", subject_get_int(cfg.nr.val) ? "On": "Off");
    return buf;
}

static const char * nr_level_label_getter() {
    static char buf[22];
    sprintf(buf, "NR level:\n%zu", subject_get_int(cfg.nr_level.val));
    return buf;
}


/* lv_msg_send() no es thread-safe en LVGL 8: cuando MSG_PARAM_CHANGED lo manda
 * un hilo ajeno (por ejemplo el hilo del CAT al cambiar de frecuencia, que
 * dispara el observer de banda y este mensaje), recorrer los labels aca mismo
 * toca LVGL fuera del hilo de GUI (issue #131). Se difiere al hilo de GUI; desde
 * el hilo de GUI el comportamiento es el de siempre. */
static void param_changed_apply(void *arg) {
    (void) arg;

    for (size_t i = 0; i < BUTTONS; i++) {
        lv_obj_t  *label = btn[i].label;
        if (!label) continue;
        auto label_getter = (char *(*)(void))lv_obj_get_user_data(label);
        if (!label_getter) continue;
        lv_label_set_text(label, label_getter());
    }
}

static void param_changed_cb(void * s, lv_msg_t * m) {
    (void) s;
    (void) m;

    if (!scheduler_in_main_thread()) {
        scheduler_put_noargs(param_changed_apply);
        return;
    }
    param_changed_apply(NULL);
}


static void label_update_cb(Subject *subj, void *user_data) {
    button_item_t *item = (button_item_t*)user_data;
    if (item->label_obj) {
        lv_label_set_text(item->label_obj, item->label_fn());
    } else {
        LV_LOG_WARN("Can't update label: it's NULL");
    }
}

