/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "main_screen.h"

#include "styles.h"
#include "ui_tokens.h"      /* la geometria de la fila de frecuencia (issue #10, UI-05) */
#include "spectrum.h"
#include "waterfall.h"
#include "util.h"
#include "radio.h"
#include "events.h"
#include "msg.h"
#include "msg_tiny.h"
#include "dsp.h"
#include "clock.h"
#include "cw_tune_ui.h"
#include "info.h"
#include "meter.h"
#include "band_info.h"
#include "tx_info.h"
#include "mfk.h"
#include "vol.h"
#include "main.h"
#include "pannel.h"
#include "rtty.h"
#include "screenshot.h"
#include "keyboard.h"
#include "dialog.h"
#include "dialog_settings.h"
#include "dialog_freq.h"
#include "dialog_msg_cw.h"
#include "dialog_msg_voice.h"
#include "dialog_swrscan.h"
#include "dialog_ft8.h"
#include "dialog_qth.h"
#include "dialog_recorder.h"
#include "dialog_callsign.h"
#include "dialog_wifi.h"
#include "dialog_eq.h"
#include "backlight.h"
#include "buttons.h"
#include "recorder.h"
#include "voice.h"
#include "pubsub_ids.h"
#include "cfg/mode.h"
#include "cfg/memory.h"
#include "adaptive_tuning.h"
#include "autotune_ui.h"
#include "cb.h"
#include "band_plan.h"
#include "screen_rec.h"
#include "dialog_wspr.h"
#include "dialog_navtex.h"
#include "dialog_sstv.h"
#include "dialog_wefax.h"
#include "dialog_cluster.h"
#include "dialog_field.h"
#include "dialog_js8.h"
#include "dialog_cw_kbd.h"
#include "dialog_cw_trainer.h"
#include "dialog_bt.h"
#include "dialog_status.h"
#include "controls.h"
#include "dialog_gps.h"
#include "dialog_bands.h"
#include "spot_marks.h"
#include "cluster_column.h"
#include "theme.h"
#include "fonts/font_select.h"

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


static uint16_t     spectrum_height = (480 / 3);
static uint16_t     freq_height = UI_FREQ_ROW_H;
static lv_obj_t     *obj;
static Subject      *freq_lock;
static bool         mode_lock = false;
static bool         ab_lock = false;
static bool         band_lock = false;

static lv_obj_t     *spectrum;
static lv_obj_t     *freq[3];
/* El "8 fantasma" del centro de la fila (ver freq_shadow_update): NULL mientras no
 * haya familia segmentada. */
static lv_obj_t     *freq_shadow;
static lv_obj_t     *waterfall;
static lv_obj_t     *msg;
static lv_obj_t     *msg_tiny;
static lv_obj_t     *meter;
static lv_obj_t     *tx_info;
static lv_obj_t     *tuning_multiplier_label;
static lv_timer_t   *multiplier_hide_timer;

static void freq_shift(int16_t diff);
static void next_freq_step(bool up);
static void toggle_atu_enabled();

// Observers functions

static void on_fg_freq_change(Subject *subj, void *user_data);
static void update_freq_boundaries(Subject *subj, void *user_data);


void mem_load(uint16_t id) {
    if (!cfg_memory_load(id)) {
        msg_update_text_fmt("Nothing to load for memory %i", id);
    }
    if (id != MEM_BACKUP_ID) {
        msg_update_text_fmt("Loaded from memory %i", id);
    }
}

void mem_save(uint16_t id) {
    cfg_memory_save(id);

    if (id <= MEM_HKEY_MAX_ID) {
        msg_update_text_fmt("Saved in memory %i", id);
    }
}

static void toggle_atu_enabled() {
    bool new_atu_enabled = !subject_get_int(cfg.atu_enabled.val);
    subject_set_int(cfg.atu_enabled.val, new_atu_enabled);
    voice_say_text_fmt("Auto tuner %s", new_atu_enabled ? "On" : "Off");
}

static void next_freq_step(bool up) {
    uint16_t new_step = cfg_mode_change_freq_step(up);
    msg_update_text_fmt("Freq step: %i Hz", new_step);
    voice_say_text_fmt("Frequency step %i herz", new_step);
}

/* The theme applies when the windows are built: restart so it is taken */
static void restart_cb(lv_timer_t *t) {
    (void) t;

    exit(0);
}

/* La pagina de la barra que estaba antes de abrir una app que NO es dialogo
 * (hoy, RTTY): cerrar la app la devuelve en vez de dejar pegada la suya. Un
 * dialogo guarda su pagina previa en dialog_t.prev_page; RTTY no tiene
 * descriptor, asi que la suya vive aca (menu-hygiene 5 y 6). */
static buttons_page_t *app_prev_page = NULL;

/* Nombre corto de la app abierta (issue #121): sin dialogos abiertos es "home".
 * Lo escribe el dispatcher, que es el unico camino de apertura, y lo lee el
 * registro de ultima respiracion. */
static const char *current_app = "home";

/* Apaga la app abierta: cierra su dialogo (si lo hay) y RTTY. */
static void apps_stop() {
    dialog_destruct();

    rtty_set_state(RTTY_OFF);
    pannel_visible();
    current_app = "home";
}

/* Etiqueta corta para el registro de ultima respiracion. Las apps se nombran
 * igual que en el menu, sin espacios (el record es de una linea). */
static const char *app_label(press_action_t action) {
    switch (action) {
        case ACTION_APP_RTTY:      return "RTTY";
        case ACTION_APP_FT8:       return "FT8";
        case ACTION_APP_SWRSCAN:   return "SWRScan";
        case ACTION_APP_SETTINGS:  return "Settings";
        case ACTION_APP_WSPR:      return "WSPR";
        case ACTION_APP_NAVTEX:    return "NavTex";
        case ACTION_APP_SSTV:      return "SSTV";
        case ACTION_APP_WEFAX:     return "WeFax";
        case ACTION_APP_CLUSTER:   return "Cluster";
        case ACTION_APP_FIELD:     return "Field";
        case ACTION_APP_JS8:       return "JS8";
        case ACTION_APP_CW_KBD:    return "CW-Kbd";
        case ACTION_APP_CWTRAINER: return "CWTrainer";
        case ACTION_APP_BT:        return "BT";
        case ACTION_APP_GPS:       return "GPS";
        case ACTION_APP_STATUS:    return "Status";
        case ACTION_APP_RECORDER:  return "Recorder";
        case ACTION_APP_WIFI:      return "WiFi";
        case ACTION_APP_EQ:        return "EQ";
        case ACTION_APP_BANDS:     return "Bands";
        case ACTION_APP_QTH:       return "QTH";
        case ACTION_APP_CALLSIGN:  return "Callsign";
        default:                   return "home";
    }
}

/* Cierra la app abierta porque el usuario pasa a una pagina de la barra de casa
 * (teclas GEN/APP/KEY/DFN/DFL): se suelta la pagina previa de la app, que ya no
 * aplica, porque lo que sigue carga la del grupo pedido. */
static void apps_disable() {
    apps_stop();

    app_prev_page = NULL;
}

/* Cierra la app abierta y devuelve la barra a la pagina previa a su apertura.
 * Es el camino del "off" del control remoto (main_screen_close_app) y el de
 * abrir una app, que cierra la anterior -y su barra- antes de que la nueva tome
 * la pagina: asi el dialogo que se abra encima guarda la de casa y no la de la
 * app que se cierra (menu-hygiene 5 y 6). */
static void apps_close() {
    buttons_page_t *prev = app_prev_page;

    apps_stop();

    app_prev_page = NULL;
    if (prev) {
        buttons_load_page(prev);
    }
}

void main_screen_start_app(press_action_t app_action) {
    apps_close();

    current_app = app_label(app_action);

    /* Abrir una app corta el asistente de sintonia fina (issue #100) y devuelve el
     * dial al punto de arranque: el asistente corre sobre la pantalla principal y no
     * debe seguir moviendo la frecuencia por detras de una ventana. Cubre tambien a
     * RTTY, que no es un dialogo (no pasa por dialog_construct()). */
    autotune_ui_cancel();

    switch (app_action) {
        case ACTION_APP_RTTY:
            /* La barra antes de RTTY: cerrarla la devuelve. Si la barra ya es la
             * de RTTY (se reabre teniendo RTTY abierto) se conserva la guardada
             * en la primera apertura. */
            if (buttons_get_cur_page() != &buttons_page_rtty) {
                app_prev_page = buttons_get_cur_page();
            }
            buttons_load_page(&buttons_page_rtty);
            rtty_set_state(RTTY_RX);
            pannel_visible();
            voice_say_text_fmt("Teletype window");
            break;

        case ACTION_APP_SETTINGS:
            dialog_construct(dialog_settings, obj);
            voice_say_text_fmt("Settings window");
            break;

        case ACTION_APP_SWRSCAN:
            dialog_construct(dialog_swrscan, obj);
            voice_say_text_fmt("SWR scan window");
            break;

        case ACTION_APP_FT8:
            dialog_construct(dialog_ft8, obj);
            voice_say_text_fmt("FT8 window");
            break;

        case ACTION_APP_WSPR:
            dialog_construct(dialog_wspr, obj);
            break;

        case ACTION_APP_NAVTEX:
            dialog_construct(dialog_navtex, obj);
            break;

        case ACTION_APP_SSTV:
            dialog_construct(dialog_sstv, obj);
            break;

        case ACTION_APP_WEFAX:
            dialog_construct(dialog_wefax, obj);
            break;

        case ACTION_APP_CLUSTER:
            dialog_construct(dialog_cluster, obj);
            break;

        case ACTION_APP_FIELD:
            dialog_construct(dialog_field, obj);
            break;

        case ACTION_APP_JS8:
            dialog_construct(dialog_js8, obj);
            break;

        case ACTION_APP_CW_KBD:
            dialog_construct(dialog_cw_kbd, obj);
            break;

        case ACTION_APP_CWTRAINER:
            dialog_construct(dialog_cw_trainer, obj);
            break;

        case ACTION_APP_BT:
            dialog_construct(dialog_bt, obj);
            break;

        case ACTION_APP_GPS:
            dialog_construct(dialog_gps, obj);
            break;

        case ACTION_APP_STATUS:
            dialog_construct(dialog_status, obj);
            break;

        case ACTION_APP_RECORDER:
            dialog_construct(dialog_recorder, obj);
            voice_say_text_fmt("Audio recorder window");
            break;

        case ACTION_APP_WIFI:
            dialog_construct(dialog_wifi, obj);
            voice_say_text_fmt("Wi-Fi window");
            break;

        case ACTION_APP_EQ:
            dialog_construct(dialog_eq, obj);
            voice_say_text_fmt("EQ window");
            break;

        case ACTION_APP_BANDS:
            /* La app Bands entra por aca, como el resto: el boton del menu de apps
             * (buttons.cpp, make_app_btn) y el long press de Ajustes llegan los dos
             * a main_screen_start_app(), despues de apps_disable(). Antes esta
             * accion se despachaba solo en main_screen_action() construyendo el
             * dialogo sobre lo que estuviera abierto: el boton del menu (que llama
             * a esta funcion) no hacia nada y por el long press la ventana anterior
             * quedaba abierta y huerfana. */
            dialog_construct(dialog_bands, obj);
            break;

        case ACTION_APP_QTH:
            dialog_construct(dialog_qth, obj);
            voice_say_text_fmt("QTH window");
            break;

        case ACTION_APP_CALLSIGN:
            dialog_construct(dialog_callsign, obj);
            voice_say_text_fmt("Callsign window");
            break;

        default:
            break;
    }
}

void main_screen_action(press_action_t action) {
    bool b;
    switch (action) {
        case ACTION_NONE:
            break;

        case ACTION_SCREENSHOT:
            screenshot_take();
            break;

        case ACTION_SCREEN_REC:
            screen_rec_toggle();
            break;

        case ACTION_RECORDER:
            if (recorder_is_on()) {
                recorder_set_on(false);
                voice_say_text_fmt("Audio recorder off");
            } else {
                voice_say_text_fmt("Audio recorder on");
                recorder_set_on(true);
            }
            break;

        case ACTION_MUTE:
            radio_change_mute();
            break;

        case ACTION_VOICE_MODE:
            voice_change_mode();
            break;

        /* Las funciones ya existian; faltaba poder asignarlas a un boton (como en el X6100) */
        case ACTION_ATT_TOGGLE:
            controls_toggle_att(NULL);
            msg_update_text_fmt("#%06X ATT: %s", theme_hex(TC_text),
                                subject_get_int(cfg_cur.att) ? "On" : "Off");
            break;

        case ACTION_PRE_TOGGLE:
            controls_toggle_pre(NULL);
            msg_update_text_fmt("#%06X PRE: %s", theme_hex(TC_text),
                                subject_get_int(cfg_cur.pre) ? "On" : "Off");
            break;

        case ACTION_ATU_TOGGLE:
            toggle_atu_enabled();
            msg_update_text_fmt("#%06X ATU: %s", theme_hex(TC_text),
                                subject_get_int(cfg.atu_enabled.val) ? "On" : "Off");
            break;

        case ACTION_ATU_TUNE:
            radio_start_atu();
            break;

        case ACTION_BAT_INFO:
            clock_say_bat_info();
            break;

        case ACTION_STEP_UP:
            next_freq_step(true);
            break;

        case ACTION_STEP_DOWN:
            next_freq_step(false);
            break;

        case ACTION_NR_TOGGLE:
            b = subject_get_int(cfg.nr.val);
            b = !b;
            subject_set_int(cfg.nr.val, b);
            msg_update_text_fmt("#FFFFFF NR: %s", b ? "On" : "Off");
            break;

        case ACTION_THEME: {
            char names[16][32];
            int  n = theme_list(names, 16);
            int  cur = 0;

            for (int i = 0; i < n; i++) {
                if (strcmp(names[i], params.theme_name.x) == 0) {
                    cur = i;
                }
            }

            if (n > 0) {
                const char *next = names[(cur + 1) % n];

                params_str_set(&params.theme_name, next);
                params_flush();

                /* Every window takes its colours when it is built, so the GUI
                 * restarts to apply the theme, exactly as the X6100 does (the
                 * x6200_daemon starts it again). */
                msg_update_text_fmt("Theme: %s - restarting", next);

                lv_timer_t *restart = lv_timer_create(restart_cb, 1200, NULL);

                if (restart) {
                    lv_timer_set_repeat_count(restart, 1);
                } else {
                    exit(0);
                }
            }
            break;
        }

        case ACTION_NB_TOGGLE:
            b = subject_get_int(cfg.nb.val);
            b = !b;
            subject_set_int(cfg.nb.val, b);
            msg_update_text_fmt("#FFFFFF NB: %s", b ? "On" : "Off");
            break;

        /* Asistente de sintonia fina (issue #100): arranca el barrido alrededor del
         * dial, o lo cancela y restaura si ya estaba corriendo. RX only. */
        case ACTION_ATUNE:
            autotune_ui_toggle();
            break;

        case ACTION_APP_RTTY:
        case ACTION_APP_FT8:
        case ACTION_APP_SWRSCAN:
        case ACTION_APP_SETTINGS:
        case ACTION_APP_WSPR:
        case ACTION_APP_NAVTEX:
        case ACTION_APP_SSTV:
        case ACTION_APP_WEFAX:
        case ACTION_APP_CLUSTER:
        case ACTION_APP_FIELD:
        case ACTION_APP_JS8:
        case ACTION_APP_CW_KBD:
        case ACTION_APP_CWTRAINER:
        case ACTION_APP_BT:
        case ACTION_APP_STATUS:
        case ACTION_APP_RECORDER:
        case ACTION_APP_WIFI:
        case ACTION_APP_BANDS:
        case ACTION_APP_QTH:
        case ACTION_APP_CALLSIGN:
            main_screen_start_app(action);
            break;
    }
}

static x6200_mode_t get_next_mode_am_fm(bool long_press) {
    x6200_mode_t    mode = subject_get_int(cfg_cur.mode);
    switch (mode) {
        case x6200_mode_am:
            mode = x6200_mode_sam;
            break;
        case x6200_mode_sam:
            mode = x6200_mode_nfm;
            break;
        case x6200_mode_nfm:
            mode = x6200_mode_wfm;
            break;
        case x6200_mode_wfm:
        default:
            mode = x6200_mode_am;
            break;
    }
    return mode;
}

static x6200_mode_t get_next_mode_cw(bool long_press) {
    x6200_mode_t    mode = subject_get_int(cfg_cur.mode);
    switch (mode) {
        case x6200_mode_cw:
            mode = x6200_mode_cwr;
            break;
        case x6200_mode_cwr:
        default:
            mode = x6200_mode_cw;
            break;
    }
    return mode;
}

static x6200_mode_t get_next_mode_ssb(bool long_press) {
    x6200_mode_t    mode = subject_get_int(cfg_cur.mode);
    switch (mode) {
        case x6200_mode_lsb_dig:
            if (long_press) {
                mode = x6200_mode_lsb;
            } else {
                mode = x6200_mode_usb_dig;
            }
            break;
        case x6200_mode_usb_dig:
            if (long_press) {
                mode = x6200_mode_usb;
            } else {
                mode = x6200_mode_lsb_dig;
            }
            break;
        case x6200_mode_lsb:
            if (long_press) {
                mode = x6200_mode_lsb_dig;
            } else {
                mode = x6200_mode_usb;
            }
            break;
        case x6200_mode_usb:
            if (long_press) {
                mode = x6200_mode_usb_dig;
            } else {
                mode = x6200_mode_lsb;
            }
            break;
        default:
            mode = x6200_mode_lsb;
            break;
    }
    return mode;
}

static void change_mode(keypad_key_t key, keypad_state_t state) {
    switch (state) {
        case KEYPAD_LONG:
        case KEYPAD_RELEASE:
            break;
        default:
            return;
    }

    // Define mode->text struct
    typedef struct {
        x6200_mode_t    mode;
        const char*     msg;
    } mode_text_t;

    mode_text_t modes_text[] = {
        {.mode=x6200_mode_wfm, .msg="W F M modulation"},
        {.mode=x6200_mode_nfm, .msg="N F M modulation"},
        {.mode=x6200_mode_sam, .msg="S A M modulation"},
        {.mode=x6200_mode_am, .msg="A M modulation"},
        {.mode=x6200_mode_cwr, .msg="CWR modulation"},
        {.mode=x6200_mode_cw, .msg="CW modulation"},
        {.mode=x6200_mode_lsb_dig, .msg="LSB digital modulation"},
        {.mode=x6200_mode_lsb, .msg="LSB modulation"},
        {.mode=x6200_mode_usb_dig, .msg="USB digital modulation"},
        {.mode=x6200_mode_usb, .msg="USB modulation"},
    };

    // find next mode
    x6200_mode_t    next_mode;
    switch (key) {
        case KEYPAD_MODE_AM:
            next_mode = get_next_mode_am_fm(state == KEYPAD_LONG);
            break;
        case KEYPAD_MODE_CW:
            next_mode = get_next_mode_cw(state == KEYPAD_LONG);
            break;
        case KEYPAD_MODE_SSB:
            next_mode = get_next_mode_ssb(state == KEYPAD_LONG);
            break;
        default:
            break;
    }

    for (size_t i = 0; i < sizeof(modes_text)/sizeof(modes_text[0]); i++) {
        if (modes_text[i].mode == next_mode) {
            voice_say_text_fmt(modes_text[i].msg);
            break;
        }
    }
    subject_set_int(cfg_cur.mode, next_mode);
}

static void main_screen_keypad_cb(lv_event_t * e) {
    event_keypad_t *keypad = lv_event_get_param(e);

    switch (keypad->key) {
        case KEYPAD_PRE: ;
            int32_t pre = subject_get_int(cfg_cur.pre);
            int32_t att = subject_get_int(cfg_cur.att);
            if (keypad->state == KEYPAD_RELEASE) {
                pre = !pre;
                if (pre && att) {
                    subject_set_int(cfg_cur.att, x6200_att_off);
                }
                subject_set_int(cfg_cur.pre, pre);
                voice_say_text_fmt("Preamplifier %s", pre ? "On" : "Off");
                // radio_change_pre();
                //

                if (params.mag_info.x) {
                    msg_tiny_set_text_fmt("Pre: %s", pre ? "On" : "Off");
                }
            } else if (keypad->state == KEYPAD_LONG) {
                att = !att;
                if (pre && att) {
                    subject_set_int(cfg_cur.pre, x6200_pre_off);
                }
                subject_set_int(cfg_cur.att, att);
                voice_say_text_fmt("Attenuator %s", att ? "On" : "Off");
                // radio_change_att();
                //

                if (params.mag_info.x) {
                    msg_tiny_set_text_fmt("Att: %s", att ? "On" : "Off");
                }
            }
            break;

        case KEYPAD_BAND_UP:
            if (keypad->state == KEYPAD_RELEASE) {
                if (!band_lock) {
                    cfg_band_load_next(true);
                }
                dialog_send(EVENT_BAND_UP, NULL);
            }
            break;

        case KEYPAD_BAND_DOWN:
            if (keypad->state == KEYPAD_RELEASE) {
                if (!band_lock) {
                    cfg_band_load_next(false);
                }
                dialog_send(EVENT_BAND_DOWN, NULL);
            }
            break;

        case KEYPAD_MODE_AM:
        case KEYPAD_MODE_CW:
        case KEYPAD_MODE_SSB:
            if (!mode_lock) {
                change_mode(keypad->key, keypad->state);
            }
            break;

        case KEYPAD_AGC:
            if (keypad->state == KEYPAD_RELEASE) {
                x6200_agc_t agc = subject_get_int(cfg_cur.agc);
                switch (agc) {
                    case x6200_agc_off:
                        agc = x6200_agc_slow;
                        voice_say_text_fmt("Auto gain slow mode");
                        break;

                    case x6200_agc_slow:
                        agc = x6200_agc_fast;
                        voice_say_text_fmt("Auto gain fast mode");
                        break;

                    case x6200_agc_fast:
                        agc = x6200_agc_auto;
                        voice_say_text_fmt("Auto gain auto mode");
                        break;

                    case x6200_agc_auto:
                        agc = x6200_agc_off;
                        voice_say_text_fmt("Auto gain off");
                        break;
                }
                subject_set_int(cfg_cur.agc, agc);
                // radio_change_agc();
                //

                if (params.mag_info.x) {
                    msg_tiny_set_text_fmt("AGC: %s", cfg_mode_agc_label(subject_get_int(cfg_cur.agc)));
                }
            } else if (keypad->state == KEYPAD_LONG) {
                bool new_split = !subject_get_int(cfg_cur.band->split.val);
                subject_set_int(cfg_cur.band->split.val, new_split);
                voice_say_text_fmt("Split %s", new_split ? "On" : "Off");

                spectrum_clear();

                if (params.mag_info.x) {
                    msg_tiny_set_text_fmt("%s", info_params_vfo_label_get());
                }
            }
            break;

        case KEYPAD_FST:
            if (keypad->state == KEYPAD_RELEASE) {
                next_freq_step(true);
            } else if (keypad->state == KEYPAD_LONG) {
                subject_set_int(freq_lock, !subject_get_int(freq_lock));
                voice_say_text_fmt("Frequency %s", subject_get_int(freq_lock) ? "locked" : "unlocked");
            }
            break;

        case KEYPAD_ATU:
            if (keypad->state == KEYPAD_RELEASE) {
                toggle_atu_enabled();

                if (params.mag_info.x) {
                    msg_tiny_set_text_fmt("ATU: %s", subject_get_int(cfg.atu_enabled.val) ? "On" : "Off");
                }
            } else if (keypad->state == KEYPAD_LONG) {
                radio_start_atu();
            }
            break;

        case KEYPAD_F1:
            if (keypad->state == KEYPAD_RELEASE) {
                buttons_press(0, false);
            } else if (keypad->state == KEYPAD_LONG) {
                buttons_press(0, true);
            }
            break;

        case KEYPAD_F2:
            if (keypad->state == KEYPAD_RELEASE) {
                buttons_press(1, false);
            } else if (keypad->state == KEYPAD_LONG) {
                buttons_press(1, true);
            }
            break;

        case KEYPAD_F3:
            if (keypad->state == KEYPAD_RELEASE) {
                buttons_press(2, false);
            } else if (keypad->state == KEYPAD_LONG) {
                buttons_press(2, true);
            }
            break;

        case KEYPAD_F4:
            if (keypad->state == KEYPAD_RELEASE) {
                buttons_press(3, false);
            } else if (keypad->state == KEYPAD_LONG) {
                buttons_press(3, true);
            }
            break;

        case KEYPAD_F5:
            if (keypad->state == KEYPAD_RELEASE) {
                buttons_press(4, false);
            } else if (keypad->state == KEYPAD_LONG) {
                buttons_press(4, true);
            }
            break;

        case KEYPAD_GEN:
            if (keypad->state == KEYPAD_RELEASE) {
                apps_disable();
                buttons_load_page_group(buttons_group_gen);
            } else if (keypad->state == KEYPAD_LONG) {
                main_screen_action(params.long_gen);
            }
            break;

        case KEYPAD_APP:
            if (keypad->state == KEYPAD_RELEASE) {
                apps_disable();
                buttons_load_page_group(buttons_group_app);
            } else if (keypad->state == KEYPAD_LONG) {
                main_screen_action(params.long_app);
            }
            break;

        case KEYPAD_KEY:
            if (keypad->state == KEYPAD_RELEASE) {
                apps_disable();
                buttons_load_page_group(buttons_group_key);
            } else if (keypad->state == KEYPAD_LONG) {
                main_screen_action(params.long_key);
            }
            break;

        case KEYPAD_MSG:
            if (keypad->state == KEYPAD_RELEASE) {
                switch (subject_get_int(cfg_cur.mode)) {
                    case x6200_mode_cw:
                    case x6200_mode_cwr:
                        if (!dialog_type_is_run(dialog_msg_cw)) {
                            apps_close();
                        }

                        pannel_hide();
                        dialog_construct(dialog_msg_cw, obj);
                        voice_say_text_fmt("CW messages window");
                        break;

                    case x6200_mode_lsb:
                    case x6200_mode_usb:
                    case x6200_mode_am:
                    case x6200_mode_nfm:
                        if (!dialog_type_is_run(dialog_msg_voice)) {
                            apps_close();
                        }

                        pannel_hide();
                        dialog_construct(dialog_msg_voice, obj);
                        voice_say_text_fmt("Voice messages window");
                        break;

                    default:
                        msg_tiny_set_text_fmt("Not used in this mode");
                        break;
                }
            } else if (keypad->state == KEYPAD_LONG) {
                main_screen_action(params.long_msg);
            }
            break;

        case KEYPAD_DFN:
            if (keypad->state == KEYPAD_RELEASE) {
                apps_disable();
                buttons_load_page_group(buttons_group_dfn);
            } else if (keypad->state == KEYPAD_LONG) {
                main_screen_action(params.long_dfn);
            }
            break;

        case KEYPAD_DFL:
            if (keypad->state == KEYPAD_RELEASE) {
                apps_disable();
                buttons_load_page_group(buttons_group_dfl);
                voice_say_text_fmt("DFL parameters");
            } else if (keypad->state == KEYPAD_LONG) {
                main_screen_action(params.long_dfl);
            }
            break;

        case KEYPAD_AB:
            if (!ab_lock) {
                if (keypad->state == KEYPAD_RELEASE) {
                    radio_toggle_vfo();

                    spectrum_clear();

                    if (params.mag_info.x) {
                        msg_tiny_set_text_fmt("%s", info_params_vfo_label_get());
                    }
                } else if (keypad->state == KEYPAD_LONG) {
                    x6200_vfo_t cur_vfo = subject_get_int(cfg_cur.band->vfo.val);
                    cfg_band_vfo_copy();
                    // radio_vfo_set();
                    msg_update_text_fmt("Clone VFO %s", cur_vfo == X6200_VFO_A ? "A->B" : "B->A");
                    voice_say_text_fmt("V F O cloned %s", cur_vfo == X6200_VFO_A ? "from A to B" : "from B to A");
                }
            }
            break;

        case KEYPAD_POWER:
            if (keypad->state == KEYPAD_RELEASE) {
                backlight_switch();
            } else if (keypad->state == KEYPAD_LONG) {
                voice_say_text_fmt("Power off");
                msg_update_text_fmt("Power off");
                radio_poweroff();
            }
            break;

        case KEYPAD_PTT:
            switch (keypad->state) {
                case KEYPAD_PRESS:
                    radio_set_ptt(true);

                    switch (subject_get_int(cfg_cur.mode)) {
                        case x6200_mode_cw:
                        case x6200_mode_cwr:
                            radio_set_morse_key(true);
                            break;
                    }
                    break;

                case KEYPAD_RELEASE:
                case KEYPAD_LONG_RELEASE:
                    switch (subject_get_int(cfg_cur.mode)) {
                        case x6200_mode_cw:
                        case x6200_mode_cwr:
                            radio_set_morse_key(false);
                            break;
                    }

                    radio_set_ptt(false);
                    break;
                default:
                    break;
            }
            break;

        default:
            LV_LOG_WARN("Unsuported key: %u", keypad->key);
            break;
    }
}

static void main_screen_hkey_cb(lv_event_t * e) {
    event_hkey_t *hkey = lv_event_get_param(e);
    switch (hkey->key) {
        case HKEY_1:
        case HKEY_2:
        case HKEY_3:
        case HKEY_4:
        case HKEY_5:
        case HKEY_6:
        case HKEY_7:
        case HKEY_8:
        case HKEY_9:
            if (hkey->state == HKEY_RELEASE) {
                mem_load(hkey->key - HKEY_1 + 1);
                voice_say_text_fmt("Memory %i loaded", hkey->key - HKEY_1 + 1);
            } else if (hkey->state == HKEY_LONG) {
                mem_save(hkey->key - HKEY_1 + 1);
                voice_say_text_fmt("Memory %i stored", hkey->key - HKEY_1 + 1);
            }
            break;

        case HKEY_SPCH:
            if (hkey->state == HKEY_RELEASE) {
                subject_set_int(freq_lock, !subject_get_int(freq_lock));
                voice_say_text_fmt("Frequency %s", subject_get_int(freq_lock) ? "locked" : "unlocked");
            }
            break;

        case HKEY_TUNER:
            if (hkey->state == HKEY_RELEASE) {
                toggle_atu_enabled();

            } else if (hkey->state == HKEY_LONG) {
                radio_start_atu();
            }
            break;

        case HKEY_XFC:
            if (hkey->state == HKEY_RELEASE) {
                radio_toggle_vfo();

                spectrum_clear();
            }
            break;

        case HKEY_UP:
            if (hkey->state == HKEY_RELEASE) {
                if (!subject_get_int(freq_lock)) {
                    freq_shift(+1);
                }
            } else if (hkey->state == HKEY_LONG) {
                if (!band_lock) {
                    cfg_band_load_next(true);
                }
                dialog_send(EVENT_BAND_UP, NULL);
            }
            break;

        case HKEY_DOWN:
            if (hkey->state == HKEY_RELEASE) {
                if (!subject_get_int(freq_lock)) {
                    freq_shift(-1);
                }
            } else if (hkey->state == HKEY_LONG) {
                if (!band_lock) {
                    cfg_band_load_next(false);
                }
                dialog_send(EVENT_BAND_DOWN, NULL);
            }
            break;

        case HKEY_F1:
            if (hkey->state == HKEY_RELEASE) {
                main_screen_action(params.press_f1);
            } else if (hkey->state == HKEY_LONG) {
                main_screen_action(params.long_f1);
            }
            break;

        case HKEY_F2:
            if (hkey->state == HKEY_RELEASE) {
                main_screen_action(params.press_f2);
            } else if (hkey->state == HKEY_LONG) {
                main_screen_action(params.long_f2);
            }
            break;

        default:
            LV_LOG_WARN("Unsuported key: %u", hkey->key);
            break;
    }
}

static void main_screen_radio_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);

    lv_event_send(meter, code, NULL);
    lv_event_send(tx_info, code, NULL);
    lv_event_send(spectrum, code, NULL);

    dialog_send(code, NULL);
}

static void main_screen_update_cb(lv_event_t * e) {

    spectrum_clear();
}

static uint16_t freq_accel(uint16_t diff) {
    if (diff < 3) {
        return 1;
    }

    switch (params.freq_accel.x) {
        case FREQ_ACCEL_NONE:
            return 1;

        case FREQ_ACCEL_LITE:
            return (diff < 6) ? 5 : 10;

        case FREQ_ACCEL_STRONG:
            return (diff < 6) ? 10 : 30;
    }
    return 1;
}

static void freq_shift(int16_t diff) {
    if (subject_get_int(freq_lock)) {
        return;
    }

    int32_t freq = subject_get_int(cfg_cur.fg_freq);

    /* En CB (27 MHz) el dial cuenta CANALES de 40, no Hz: 1 detent = 1 canal, sin
     * aceleracion y sin wrap (issue #146, seccion 5.3). Fuera de canal la primera
     * vuelta engancha el primer canal en la direccion del giro. */
    cb_plan_t plan;
    if (cb_plan_of_freq(freq, &plan)) {
        int ch = cb_step_channel(plan, freq, diff);
        subject_set_int(cfg_cur.fg_freq, cb_freq_of_channel(plan, ch));
        voice_say_text_fmt("Channel %i", ch);
        return;
    }

    int32_t df = diff * subject_get_int(cfg_cur.freq_step) * freq_accel(abs(diff));
    freq = align_int(freq + df, abs(df));
    subject_set_int(cfg_cur.fg_freq, freq);

    voice_say_freq(freq);
}

static void update_tuning_multiplier_display(float multiplier) {
    if (!tuning_multiplier_label) return;
    
    // Update text
    lv_label_set_text_fmt(tuning_multiplier_label, "x%.1f", multiplier);
    
    // Update color based on range
    lv_color_t color;
    if (multiplier < 1.0f) {
        color = lv_color_hex(theme_hex(TC_info));  // Blue - fine
    } else if (multiplier < 2.0f) {
        color = lv_color_white();         // White - normal
    } else if (multiplier < 10.0f) {
        color = lv_color_hex(theme_hex(TC_warn));   // Yellow - fast
    } else {
        color = lv_color_hex(theme_hex(TC_err));   // Red - scan
    }
    lv_obj_set_style_text_color(tuning_multiplier_label, color, 0);
    lv_obj_clear_flag(tuning_multiplier_label, LV_OBJ_FLAG_HIDDEN);
    
    // Reset hide timer
    if (multiplier_hide_timer) {
        lv_timer_reset(multiplier_hide_timer);
        lv_timer_resume(multiplier_hide_timer);
    }
}

static void hide_multiplier_cb(lv_timer_t *timer) {
    if (tuning_multiplier_label) {
        lv_obj_add_flag(tuning_multiplier_label, LV_OBJ_FLAG_HIDDEN);
    }
}

/* The adaptive multiplier is gated by the user "Freq acceleration" setting so
 * that it keeps its original meaning: none = 1x (no acceleration),
 * lite = up to 10x, strong = up to 100x (band scanning). */
_Static_assert(FREQ_ACCEL_NONE == ADAPTIVE_ACCEL_NONE &&
               FREQ_ACCEL_LITE == ADAPTIVE_ACCEL_LITE &&
               FREQ_ACCEL_STRONG == ADAPTIVE_ACCEL_STRONG,
               "freq_accel_t must match adaptive_accel_t");

static void main_screen_rotary_enhanced_cb(lv_event_t * e) {
    rotary_event_enhanced_t *enhanced = (rotary_event_enhanced_t *)lv_event_get_param(e);
    float multiplier = adaptive_multiplier_gate(enhanced->multiplier,
                                                (adaptive_accel_t) params.freq_accel.x);

    int32_t freq = subject_get_int(cfg_cur.fg_freq);

    /* En CB el dial cuenta canales: 1 detent = 1 canal, SIN multiplicador adaptativo
     * (issue #146, seccion 5.3). El indicador "xN" se omite porque no hay
     * aceleracion que mostrar. */
    cb_plan_t plan;
    if (cb_plan_of_freq(freq, &plan)) {
        if (!subject_get_int(freq_lock)) {
            int ch = cb_step_channel(plan, freq, enhanced->raw_diff);
            subject_set_int(cfg_cur.fg_freq, cb_freq_of_channel(plan, ch));
            voice_say_text_fmt("Channel %i", ch);
        }
        dialog_rotary(enhanced->raw_diff);
        free(enhanced);
        return;
    }

    if (!subject_get_int(freq_lock)) {
        int32_t base_step = subject_get_int(cfg_cur.freq_step);
        
        // Calculate adaptive step using the multiplier
        int32_t adaptive_step = (int32_t)(base_step * multiplier);

        if (adaptive_step < 1) {
            adaptive_step = 1;
        }
        
        int32_t df = enhanced->raw_diff * adaptive_step;

        /* align_int() takes a uint16_t grid, so for very large scan steps keep
         * the normal step grid instead of overflowing a cast. */
        uint16_t grid = (adaptive_step <= 65535) ? (uint16_t)adaptive_step : (uint16_t)base_step;
        
        // Apply frequency change (range limits are handled by the cfg subjects)
        freq = align_int(freq + df, grid);
        
        subject_set_int(cfg_cur.fg_freq, freq);
    }
    
    // Update multiplier display
    update_tuning_multiplier_display(multiplier);
    
    dialog_rotary(enhanced->raw_diff);
    free(enhanced);
}

static void spectrum_key_cb(lv_event_t * e) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));

    switch (key) {
        case '-':
            if (!subject_get_int(freq_lock)) {
                freq_shift(-1);
            }
            break;

        case '=':
            if (!subject_get_int(freq_lock)) {
                freq_shift(+1);
            }
            break;

        case '_':
            next_freq_step(false);
            break;

        case '+':
            next_freq_step(true);
            break;

        case KEY_VOL_LEFT_EDIT:
        case '[':
            vol_update(-1, false);
            break;

        case KEY_VOL_RIGHT_EDIT:
        case ']':
            vol_update(+1, false);
            break;

        case KEY_VOL_LEFT_SELECT:
        case '{':
            vol_change_mode(-1);
            break;

        case KEY_VOL_RIGHT_SELECT:
        case '}':
            vol_change_mode(+1);
            break;

        case KEYBOARD_F9:
            dialog_construct(dialog_settings, obj);
            break;

        case LV_KEY_LEFT:
            switch (mfk_state) {
                case MFK_STATE_EDIT:
                    mfk_update(-1, false);
                    break;

                case MFK_STATE_SELECT:
                    mfk_change_mode(-1);
                    break;
            }
            break;

        case LV_KEY_RIGHT:
            switch (mfk_state) {
                case MFK_STATE_EDIT:
                    mfk_update(+1, false);
                    break;

                case MFK_STATE_SELECT:
                    mfk_change_mode(+1);
                    break;
            }
            break;

        case LV_KEY_UP:
            mfk_change_mode(+1);
            break;

        case LV_KEY_DOWN:
            mfk_change_mode(-1);
            break;

        case LV_KEY_ESC:
            if (!dialog_is_run() && vol != NULL) {
                switch (vol->mode) {
                    case ROT_VOL_EDIT_MODE:
                        vol->mode = ROT_VOL_SELECT_MODE;
                        voice_say_text_fmt("Selection mode");
                        break;

                    case ROT_VOL_SELECT_MODE:
                        vol->mode = ROT_VOL_EDIT_MODE;
                        voice_say_text_fmt("Edit mode");
                        break;
                }
                vol_update(0, false);
            }
            break;

        case KEYBOARD_PRINT:
        case KEYBOARD_PRINT_SCR:
            screenshot_take();
            break;

        case KEYBOARD_SCRL_LOCK:
            subject_set_int(freq_lock, !subject_get_int(freq_lock));
            break;

        case KEYBOARD_PGUP:
            if (!band_lock) {
                cfg_band_load_next(true);
            }
            dialog_send(EVENT_BAND_UP, NULL);
            break;

        case KEYBOARD_PGDN:
            if (!band_lock) {
                cfg_band_load_next(false);
            }
            dialog_send(EVENT_BAND_DOWN, NULL);
            break;

        case HKEY_FINP:
            if (!subject_get_int(freq_lock)) {
                voice_say_text_fmt("Enter frequency");
                dialog_construct(dialog_freq, obj);
            }
            break;

        default:
            break;
    }
}

static void spectrum_pressed_cb(lv_event_t * e) {
    switch (mfk_state) {
        case MFK_STATE_EDIT:
            mfk_state = MFK_STATE_SELECT;
            voice_say_text_fmt("Selection mode");
            break;

        case MFK_STATE_SELECT:
            mfk_state = MFK_STATE_EDIT;
            voice_say_text_fmt("Edit mode");
            break;
    }
    mfk_update(0, false);
}

static void keys_enable_cb(lv_timer_t *t) {
    lv_group_add_obj(keyboard_group, spectrum);
    lv_group_set_editing(keyboard_group, true);
}

void main_screen_keys_enable(bool value) {
    if (value) {
        lv_timer_t *timer = lv_timer_create(keys_enable_cb, 100, NULL);
        lv_timer_set_repeat_count(timer, 1);
    } else {
        lv_group_remove_obj(spectrum);
        lv_group_set_editing(keyboard_group, false);
    }
}

void main_screen_lock_freq(bool lock) {
    subject_set_int(freq_lock, lock);
}

void main_screen_lock_band(bool lock) {
    band_lock = lock;
}

void main_screen_lock_mode(bool lock) {
    mode_lock = lock;
    info_lock_mode(lock);
}

void main_screen_lock_ab(bool lock) {
    ab_lock = lock;
}

void main_screen_set_freq(uint64_t freq) {
    subject_set_int(cfg_cur.fg_freq, freq);
    event_send(lv_scr_act(), EVENT_SCREEN_UPDATE, NULL);
}

lv_obj_t * main_screen() {
    uint16_t y = 0;

    freq_lock = subject_create_int(false);

    obj = lv_obj_create(NULL);

    lv_obj_add_event_cb(obj, main_screen_rotary_enhanced_cb, EVENT_ROTARY_ENHANCED, NULL);
    lv_obj_add_event_cb(obj, main_screen_keypad_cb, EVENT_KEYPAD, NULL);
    lv_obj_add_event_cb(obj, main_screen_hkey_cb, EVENT_HKEY, NULL);
    lv_obj_add_event_cb(obj, main_screen_radio_cb, EVENT_RADIO_TX, NULL);
    lv_obj_add_event_cb(obj, main_screen_radio_cb, EVENT_RADIO_RX, NULL);
    lv_obj_add_event_cb(obj, main_screen_update_cb, EVENT_SCREEN_UPDATE, NULL);

    lv_obj_add_style(obj, &background_style, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    spectrum = spectrum_init(obj);
    main_screen_keys_enable(true);

    lv_obj_add_event_cb(spectrum, spectrum_key_cb, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(spectrum, spectrum_pressed_cb, LV_EVENT_PRESSED, NULL);

    spectrum_min_max_reset();

    lv_obj_set_y(spectrum, y);
    lv_obj_set_height(spectrum, spectrum_height);

    y += spectrum_height;

    /* Fila de frecuencia: un contenedor flex (LV_FLEX_ROW) del ancho del panel
     * (UI_FREQ_SCREEN_W) cuyos tres labels se colocan por flex, en vez de la x
     * calculada a mano por label (incremento 3). Mismo idioma que la barra de
     * info (incremento 2): el flex gobierna SOLO el eje X. Las anchuras siguen
     * saliendo de freq_style / freq_main_style (150/500/150) y los hijos no
     * llevan flex_grow: flex coloca, no redimensiona, asi que la tesela
     * 150+500+150 = 800 se mantiene sola. El Y no lo toca el flex: el
     * contenedor se ancla en el MISMO cursor `y` del layout apilado (debajo del
     * espectro, `y += spectrum_height`) y, con pad/borde 0, deja a los labels en
     * el mismo cursor que tenian (el espectro y el flujo vertical no se tocan). */
    /* El "8 fantasma" del label central, creado ANTES de freq_row para quedar
     * debajo en el z-order (el hermano posterior se dibuja encima). Va como hijo de
     * la pantalla y NO del flex: un cuarto hijo correria las teselas 150+500+150.
     * La geometria la da freq_main_style (500x36, texto centrado y mismo pad_ver
     * que el label del valor), asi que el fantasma cae digito a digito sobre la
     * frecuencia. */
    if (font_digit_active()) {
        freq_shadow = lv_label_create(obj);
        lv_obj_add_style(freq_shadow, &freq_main_style, 0);
        lv_obj_set_style_text_opa(freq_shadow, UI_DIGIT_SHADOW_OPA, 0);
        lv_obj_set_pos(freq_shadow, UI_FREQ_CENTER_X, y);
    }

    lv_obj_t *freq_row = lv_obj_create(obj);

    lv_obj_clear_flag(freq_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(freq_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(freq_row, 0, 0);
    lv_obj_set_style_border_width(freq_row, 0, 0);
    lv_obj_set_style_pad_all(freq_row, 0, 0);
    lv_obj_set_style_pad_column(freq_row, 0, 0);
    lv_obj_set_size(freq_row, UI_FREQ_SCREEN_W, freq_height);
    lv_obj_set_pos(freq_row, UI_FREQ_LEFT_X, y);
    lv_obj_set_layout(freq_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(freq_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(freq_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *f;

    f = lv_label_create(freq_row);
    lv_obj_add_style(f, &freq_style, 0);
    lv_label_set_recolor(f, true);
    freq[0] = f;

    f = lv_label_create(freq_row);
    lv_obj_add_style(f, &freq_main_style, 0);
    lv_label_set_recolor(f, true);
    freq[1] = f;

    f = lv_label_create(freq_row);
    lv_obj_add_style(f, &freq_style, 0);
    lv_label_set_recolor(f, true);
    freq[2] = f;

    y += freq_height;

    /* Franja de spots del cluster arriba del waterfall (X6100): se reserva su alto */
    lv_coord_t spots_y = y;

    y += SPOT_MARKS_H;

    waterfall = waterfall_init(obj);

    waterfall_min_max_reset();

    lv_obj_set_y(waterfall, y);
    waterfall_set_height(480 - y);

    spot_marks_init(obj, spots_y, spots_y + 4);

    /* IARU band plan strip: at the bottom of the spectrum, lined up with the
     * frequency row (created after the waterfall so it stays on top). */
    band_plan_init(obj, spectrum_height - BAND_PLAN_H);

    /* DX cluster spots down the right edge (Settings -> Cluster column) */
    cluster_column_init(obj, y);
    cluster_column_layout(y);

    buttons_init(obj);
    buttons_load_page(&buttons_page_vol_1);

    pannel_init(obj);
    msg = msg_init(obj);
    msg_tiny = msg_tiny_init(obj);

    clock_init(obj);
    info_init(obj);

    meter = meter_init(obj);
    tx_info = tx_info_init(obj);
    
    // Tuning multiplier display (adaptive tuning)
    tuning_multiplier_label = lv_label_create(obj);
    lv_obj_set_style_text_font(tuning_multiplier_label, &sony_16, 0);
    lv_obj_align(tuning_multiplier_label, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_label_set_text(tuning_multiplier_label, "x1.0");
    lv_obj_add_flag(tuning_multiplier_label, LV_OBJ_FLAG_HIDDEN);
    
    // Hide timer for multiplier display
    multiplier_hide_timer = lv_timer_create(hide_multiplier_cb, 2000, NULL);
    lv_timer_pause(multiplier_hide_timer);
    
    // Initialize adaptive tuning
    adaptive_tuning_init(&g_adaptive_tuning);

    cw_tune_init(obj);

    msg_schedule_text_fmt("X6200 de R1CBU " VERSION);

    subject_add_delayed_observer(freq_lock, on_fg_freq_change, NULL);
    subject_add_delayed_observer(cfg_cur.band->split.val, on_fg_freq_change, NULL);
    subject_add_delayed_observer(cfg_cur.fg_freq, on_fg_freq_change, NULL);

    subject_add_delayed_observer(freq_lock, update_freq_boundaries, NULL);
    // update boundaries on TX with split
    subject_add_delayed_observer(cfg_cur.fg_freq, update_freq_boundaries, NULL);
    subject_add_delayed_observer(cfg_cur.fft_width, update_freq_boundaries, NULL);
    update_freq_boundaries(cfg_cur.fft_width, NULL);

    subject_add_delayed_observer(cfg_cur.bg_freq, on_fg_freq_change, NULL);
    on_fg_freq_change(cfg_cur.bg_freq, NULL);

    return obj;
}

void main_screen_notify_tx()
{
    event_send(obj, EVENT_RADIO_TX, NULL);
}

void main_screen_notify_rx()
{
    event_send(obj, EVENT_RADIO_RX, NULL);
}

/* El "8 fantasma": la capa de segmentos apagados del look Casio (issue #9, decision
 * 1). La familia segmentada trae los siete segmentos de cada digito, asi que un `8`
 * dibujado con su glifo enciende TODOS: el fantasma es el texto del valor con cada
 * digito cambiado por `8`, dibujado a alpha bajo y por DEBAJO del label del valor
 * (`freq_shadow` se crea antes que `freq_row`). Los separadores y el largo se
 * conservan, y eso es lo que hace que el fantasma caiga sobre los mismos digitos.
 *
 * Se llama SIEMPRE junto con el texto del valor: si el valor cambia de forma (split,
 * el dial trabado) los dos textos tienen que decir lo mismo o el fantasma queda
 * corrido. Sin familia segmentada la etiqueta no existe y esto no hace nada. */
static void freq_shadow_update(const char *value) {
    if (freq_shadow == NULL) return;

    /* El buffer del valor mas largo: "%i.%03i.%03i / %i.%03i.%03i" son 26 caracteres. */
    char    ghost[48];
    size_t  i = 0;

    for (; value[i] != '\0' && i < sizeof(ghost) - 1; i++) {
        ghost[i] = (value[i] >= '0' && value[i] <= '9') ? '8' : value[i];
    }
    ghost[i] = '\0';

    lv_label_set_text(freq_shadow, ghost);
}

static void on_fg_freq_change(Subject *subj, void *user_data) {
    int32_t    f;
    char       value[48];
    /* El color viaja en el markup de recolor del label (`#RRGGBB `), y ese markup PISA el
     * text_color del estilo: un blanco cableado aca deja la fila de frecuencia blanco sobre
     * blanco en el tema claro Outdoor (UI-010, region `frecuencia` con content 0.0000). Los
     * roles del tema son el color correcto: `text` normalmente y `text_soft` ("inactive knob /
     * lock", su comentario en theme.h) con el dial trabado, que en IU1WXY vale exactamente el
     * 0xBBBBBB de antes. `%06X` porque LVGL solo acepta el parametro si mide 6 digitos. */
    uint32_t    color = theme_hex(subject_get_int(freq_lock) ? TC_text_soft : TC_text);

    bool split = subject_get_int(cfg_cur.band->split.val);
    Subject *freq_fg_subj = cfg_cur.fg_freq;
    Subject *freq_bg_subj = cfg_cur.bg_freq;

    if (split && radio_get_state() == RADIO_TX) {
        freq_fg_subj = cfg_cur.bg_freq;
        freq_bg_subj = cfg_cur.fg_freq;
    }

    f = subject_get_int(freq_fg_subj);

    uint16_t    mhz, khz, hz;

    split_freq(f, &mhz, &khz, &hz);

    if (params.mag_freq.x) {
        if (mhz < 100) {
            msg_tiny_set_text_fmt("%i.%03i.%03i", mhz, khz, hz);
        } else {
            msg_tiny_set_text_fmt("%i.%03i", mhz, khz);
        }
    }

    if (split) {
        uint16_t    mhz2, khz2, hz2;
        int32_t    f2 = subject_get_int(freq_bg_subj);

        split_freq(f2, &mhz2, &khz2, &hz2);

        snprintf(value, sizeof(value), "%i.%03i.%03i / %i.%03i.%03i", mhz, khz, hz, mhz2, khz2, hz2);
    } else {
        snprintf(value, sizeof(value), "%i.%03i.%03i", mhz, khz, hz);
    }

    /* El color va en el markup del valor, que pisa el text_color del estilo; el
     * fantasma va sin markup, para que la unica diferencia entre los dos sea la
     * alpha (y en el tema claro Outdoor el fantasma no quede invisible). */
    lv_label_set_text_fmt(freq[1], "#%06X %s", color, value);
    freq_shadow_update(value);
}

static void update_freq_boundaries(Subject *subj, void *user_data) {
    bool split = subject_get_int(cfg_cur.band->split.val);
    Subject *freq_fg_subj = cfg_cur.fg_freq;

    if (split && radio_get_state() == RADIO_TX) {
        freq_fg_subj = cfg_cur.bg_freq;
    }
    int32_t  f = subject_get_int(freq_fg_subj);

    uint16_t    mhz, khz, hz;
    uint32_t    half_width = subject_get_int(cfg_cur.fft_width) / 2;
    /* Los extremos de la fila usan los mismos roles del tema que la frecuencia central (UI-010) */
    uint32_t    color = theme_hex(subject_get_int(freq_lock) ? TC_text_soft : TC_text);

    split_freq(f - half_width, &mhz, &khz, &hz);
    lv_label_set_text_fmt(freq[0], "#%06X %i.%03i", color, mhz, khz);

    split_freq(f + half_width, &mhz, &khz, &hz);
    lv_label_set_text_fmt(freq[2], "#%06X %i.%03i", color, mhz, khz);

    /* The band plan strip follows the same span as the frequency labels */
    band_plan_update(f, (int32_t) half_width);
}

/* Accepted for the ported app dialogs: without the channels dialog there is no
 * overlay to draw, so the name is shown as a message. */
void channel_overlay_show(const char *name) {
    if (name && *name) {
        msg_schedule_text_fmt("%s", name);
    }
}

/* The ported app dialogs call this to close themselves */
const char *main_screen_current_app(void) {
    return current_app;
}

void main_screen_close_app(void) {
    /* Cierra la app abierta de verdad: su dialogo (si lo hay) y RTTY, que no es
     * un dialogo, y devuelve la barra a la pagina previa a la apertura. Es el
     * camino de {"cmd":"app","text":"off"} del control remoto: con la sola
     * dialog_destruct() RTTY seguia encendida y "app" no quedaba en "" (#50,
     * menu-hygiene 5). */
    apps_close();
}

bool main_screen_freq_locked(void) {
    return subject_get_int(freq_lock);
}

bool main_screen_mode_locked(void) {
    return mode_lock;
}

bool main_screen_band_locked(void) {
    return false;
}

void main_screen_agc_next(void) {
    x6200_agc_t agc = (x6200_agc_t) subject_get_int(cfg_cur.agc);

    switch (agc) {
        case x6200_agc_off:  agc = x6200_agc_slow; break;
        case x6200_agc_slow: agc = x6200_agc_fast; break;
        case x6200_agc_fast: agc = x6200_agc_auto; break;
        default:             agc = x6200_agc_off;  break;
    }

    subject_set_int(cfg_cur.agc, agc);
    msg_tiny_set_text_fmt("AGC: %s", cfg_mode_agc_label(agc));
}

/* The X6100 has an encoder state machine (edit/select per control); this GUI's
 * vol/mfk modules keep the mode and apply the turning directly, so these map to
 * the closest behaviour available. */
void main_screen_mfk(int dir) {
    if (dir) {
        mfk_change_mode(dir);
    } else {
        mfk_update(0, false);
    }
}

void main_screen_vol_press(void) {
    vol_change_mode(1);
}
