/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - radio state for the ported remote server (see
 *  settings_manager.h).
 */

#include "settings_manager.h"

#include "cfg.h"
#include "../params/params.h"

/* C bridge: this GUI's headers have no extern "C" guards (see cfg/cxx_bridge.c) */
extern "C" bool x6200_bridge_radio_check_freq(int32_t freq);

namespace {

std::function<int32_t()> subject_reader(Subject *s) {
    return [s]() { return subject_get_int(s); };
}

std::function<void(int32_t)> subject_writer(Subject *s) {
    return [s](int32_t v) { subject_set_int(s, v); };
}

/* A parameter backed by one of our persisted configuration items */
Param item_param(cfg_item_t *item) {
    return Param(subject_reader(item->val), subject_writer(item->val));
}

Param subject_param(Subject *s) {
    return Param(subject_reader(s), subject_writer(s));
}

}   // namespace

SettingsManager cfg_sm;

/* Puente para C: cfg.c es C y no puede llamar a un método de la clase. Se usa después de
 * crear los subjects cfg_cur.* para que el mapeo de cfg_sm deje de apuntar a NULL. */
extern "C" void cfg_sm_init_live(void) {
    cfg_sm.init_live();
    cfg_sm.check_live();
}

SettingsManager::SettingsManager() {
    p_volume     = item_param(&cfg.vol);
    p_squelch    = item_param(&cfg.sql);
    p_nb         = item_param(&cfg.nb);
    p_nb_level   = item_param(&cfg.nb_level);
    p_nb_width   = item_param(&cfg.nb_width);
    p_nr         = item_param(&cfg.nr);
    p_nr_level   = item_param(&cfg.nr_level);
    p_band_id    = item_param(&cfg.band_id);

    /* The power carries a fraction: 0.1 W steps */
    p_pwr = Param();
    p_pwr.set(5);
    p_pwr.tief([this]() { return subject_get_float(cfg.pwr.val); },
               [this](float v) { subject_set_float(cfg.pwr.val, v); });

    p_cw_decoder = item_param(&cfg.cw_decoder);

    /* Our FT8 protocol lives in params, not in a subject */
    p_ft8_protocol = Param([this]() { return (int32_t) params.ft8_protocol; },
                           [this](int32_t v) { params.ft8_protocol = (ftx_protocol_t) v; });

    /* Not exposed by this platform: kept in memory (see the header) */
    p_rfgain           = Param([this]() { return rfgain_; }, [this](int32_t v) { rfgain_ = v; });
    p_band_current_vfo = Param([this]() { return band_vfo_; }, [this](int32_t v) { band_vfo_ = v; });
    p_band_if_shift    = Param([this]() { return band_if_shift_; }, [this](int32_t v) { band_if_shift_ = v; });

    /* Live state: ver init_live() - acá cfg_cur.* todavía no existe */
    init_live();
}

/* Los subjects de estado vivo los crea la carga de la configuración (cfg_band_params_init y
 * cfg_mode_params_init), que corre después de este constructor: hay que volver a mapearlos
 * una vez creados, si no los Param quedan con el subject NULL y el control remoto -y todo lo
 * que use cfg_sm.cp_*- no hace nada. */
void SettingsManager::init_live(void) {
    cp_fg_freq = subject_param(cfg_cur.fg_freq);
    cp_bg_freq = subject_param(cfg_cur.bg_freq);
    cp_cur_mode = subject_param(cfg_cur.mode);
    cp_cur_agc = subject_param(cfg_cur.agc);
    cp_cur_att = subject_param(cfg_cur.att);
    cp_cur_pre = subject_param(cfg_cur.pre);
    cp_cur_filter_bw = subject_param(cfg_cur.filter.bw);
    cp_cur_filter_low = subject_param(cfg_cur.filter.low);
    cp_cur_filter_high = subject_param(cfg_cur.filter.high);
    cp_mode_lo_offset = subject_param(cfg_cur.lo_offset);
}

int32_t SettingsManager::current_band_id() const {
    return subject_get_int(cfg.band_id.val);
}

bool SettingsManager::is_valid_hw_freq(int32_t f) const {
    return x6200_bridge_radio_check_freq(f);
}

/* Los Param de estado vivo tienen que quedar con un subject de verdad: si alguno queda en
 * NULL, leer devuelve el valor neutro y escribir no hace nada, que es exactamente el bug que
 * dejaba al control remoto sin efecto y sin ningún error a la vista. */
void SettingsManager::check_live(void) {
    struct { const char *nombre; const Subject *s; } vivos[] = {
        {"frecuencia VFO A", cfg_cur.fg_freq}, {"frecuencia VFO B", cfg_cur.bg_freq},
        {"modo", cfg_cur.mode},                {"AGC", cfg_cur.agc},
        {"atenuador", cfg_cur.att},            {"preamplificador", cfg_cur.pre},
        {"ancho de filtro", cfg_cur.filter.bw}, {"filtro bajo", cfg_cur.filter.low},
        {"filtro alto", cfg_cur.filter.high},  {"offset de LO", cfg_cur.lo_offset},
    };
    int faltan = 0;

    for (size_t i = 0; i < sizeof(vivos) / sizeof(*vivos); i++) {
        if (vivos[i].s == NULL) {
            LV_LOG_ERROR("Sin subject de %s: el control remoto no podrá cambiarlo", vivos[i].nombre);
            faltan++;
        }
    }

    if (faltan == 0) {
        LV_LOG_USER("Estado vivo de cfg_sm enlazado (%zu parámetros)", sizeof(vivos) / sizeof(*vivos));
    }
}
