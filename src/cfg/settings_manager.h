/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - radio state for the ported remote server.
 *
 *  The X6100 reads and writes the radio through its SettingsManager (cfg_sm),
 *  whose members are Parameter objects with get()/set(). This GUI keeps the same
 *  state in cfg/cfg_cur (subjects) and params; this class gives the ported
 *  remote_server.cpp the same spelling over them.
 *
 *  Members the X6200 platform does not have (RF gain, the band's IF shift, the
 *  per-band VFO flag) keep their value in memory: the remote commands that use
 *  them answer without changing the radio.
 */

#pragma once

#include <cstdint>
#include <functional>

#include "subjects.h"

class Param {
public:
    Param() = default;
    Param(std::function<int32_t()> get, std::function<void(int32_t)> set) : get_(get), set_(set) {}
    Param(Subject *subject, std::function<int32_t()> get, std::function<void(int32_t)> set)
        : subject_(subject), get_(get), set_(set) {}

    int32_t get() const { return get_ ? get_() : value_; }
    float   getf() const { return getf_ ? getf_() : (float) get(); }

    void set(int32_t v) {
        if (set_) {
            set_(v);
        } else {
            value_ = v;
        }
    }

    /* For the parameters that carry a fraction (the power, 0.1 W steps) */
    void set(float v) {
        if (setf_) {
            setf_(v);
        } else {
            set((int32_t) v);
        }
    }

    /* The ported panel subscribes with a delay (the settings manager's own
     * scheduling); our subjects offer the same through subscribe_delayed() */
    void subscribe_delayed(void (*fn)(Subject *, void *), void *user_data) {
        if (subject_) {
            subject_add_delayed_observer(subject_, fn, user_data);
        }
    }

    void subscribe_delayed_and_notify(void (*fn)(Subject *, void *), void *user_data) {
        if (subject_) {
            subject_add_delayed_observer(subject_, fn, user_data);
            fn(subject_, user_data);
        }
    }

    void tief(std::function<float()> g, std::function<void(float)> s) {
        getf_ = g;
        setf_ = s;
    }

private:
    Subject                       *subject_ = nullptr;
    std::function<int32_t()>       get_;
    std::function<void(int32_t)>   set_;
    std::function<float()>         getf_;
    std::function<void(float)>     setf_;
    int32_t                        value_ = 0;
};

class SettingsManager {
public:
    SettingsManager();

    /* Los parámetros de estado vivo (cp_*) apuntan a subjects cfg_cur.* que se crean al
     * cargar la configuración, después de este constructor: como cfg_sm es un global, el
     * mapeo del constructor los deja en NULL. Se rehace cuando ya existen. */
    void init_live(void);

    /* Avisa si algún parámetro de estado vivo quedó apuntando a un subject NULL: eso hacía
     * que el control remoto no hiciera nada, en silencio. */
    void check_live(void);

    /* Persisted parameters */
    Param p_volume, p_squelch, p_rfgain, p_pwr, p_nb, p_nb_level, p_nb_width;
    Param p_nr, p_nr_level, p_band_id, p_band_current_vfo, p_band_if_shift;
    Param p_ft8_protocol, p_cw_decoder;

    /* Live (computed) state */
    Param cp_fg_freq, cp_bg_freq, cp_cur_mode, cp_cur_agc, cp_cur_att, cp_cur_pre;
    Param cp_cur_filter_bw, cp_cur_filter_low, cp_cur_filter_high, cp_mode_lo_offset;

    /* The remote server clamps the frequency commands with these */
    static constexpr int32_t HF_MIN_FREQ = 100000;
    static constexpr int32_t HF_MAX_FREQ = 500000000;

    int32_t  current_band_id() const;
    bool     is_valid_hw_freq(int32_t f) const;

private:
    /* Values for the parameters the platform does not have */
    mutable int32_t rfgain_ = 0;
    mutable int32_t band_vfo_ = 0;
    mutable int32_t band_if_shift_ = 0;
};

extern SettingsManager cfg_sm;
