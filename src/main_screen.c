/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "main_screen.h"

#include "styles.h"
#include "ui_set.h"         /* guardas de igualdad (OPT-4 #150) */
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
#include "dialog_propagation.h"
#include "dialog_logs.h"
#include "dialog_wifi.h"
#include "dialog_eq.h"
#include "dfl_inline.h"
#include "dfl_presets.h"
#include "dfl_context.h"
#include "backlight.h"
#include "buttons.h"
#include "recorder.h"
#include "voice.h"
#include "pubsub_ids.h"
#include "cfg/mode.h"
#include "cfg/memory.h"
#include "adaptive_tuning.h"
#include "rotary.h"
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

/* Aviso del dial trabado (issue #220): girar el dial con `freq_lock` consume los
 * detents, asi que el aviso se emite a lo sumo una vez por ventana de 2 s de
 * reloj monotono en vez de una vez por detent. 0 = todavia no se aviso en esta
 * sesion bloqueada, para que el PRIMER intento despues de activar el lock avise
 * enseguida; volver a 0 al cambiar el lock reinicia el throttle. */
#define FREQ_LOCK_NOTICE_MS 2000
static uint64_t     freq_lock_notice_ms = 0;

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

    /* El editor DFL inline no es un dialogo: cambiar de app/pagina lo contrae
     * limpiamente (issue #179) y devuelve la barra a su pagina normal. */
    dfl_inline_collapse();

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
        case ACTION_APP_PROPAGATION: return "Propagation";
        case ACTION_APP_DFL_EDITOR:  return "DFLEditor";
        case ACTION_APP_LOGS:        return "Logs";
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

        case ACTION_APP_LOGS:
            dialog_construct(dialog_logs, obj);
            voice_say_text_fmt("Log uploads window");
            break;

        case ACTION_APP_PROPAGATION:
            dialog_construct(dialog_propagation, obj);
            voice_say_text_fmt("Propagation window");
            break;

        /* Editor DFL inline (issue #179). No es una app del menu de apps ni un
         * dialogo: se abre como panel inline sobre el waterfall, desde la quinta
         * celda de la pagina DFL y desde el long press fisico DFL. Se despacha
         * aca (con `case`) para que el control remoto (banco/webserver) tambien
         * lo pueda abrir; `apps_close()` ya contrajo cualquier DFL previo. */
        case ACTION_APP_DFL_EDITOR:
            dfl_inline_expand();
            voice_say_text_fmt("DFL editor");
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
        case ACTION_APP_LOGS:
        case ACTION_APP_PROPAGATION:
            main_screen_start_app(action);
            break;

        /* Editor DFL inline (issue #179). Se abre desde la quinta celda de la
         * pagina DFL y desde el long press fisico DFL. `main_screen_start_app()`
         * cierra cualquier app/dialogo abierto (no deja ventanas huerfanas) y el
         * case de arriba expande el panel inline. */
        case ACTION_APP_DFL_EDITOR:
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

/* Feedback de una accion bloqueada (issue #220): una sola frase corta en la
 * barra de mensajes transitoria que ya existe, sin voz y sin tocar el estado
 * bloqueado. En TX no se emite: los avisos de error/interlock que la radio pone
 * en la misma barra son mas criticos y un control trabado no los debe pisar. */
static void locked_notice(const char *text) {
    if (radio_get_state() == RADIO_TX) {
        return;
    }

    msg_update_text_fmt("%s", text);
}

/* Aviso del dial principal trabado: a lo sumo uno cada FREQ_LOCK_NOTICE_MS.
 * El timestamp se reinicia al cambiar el lock (freq_lock_notice_reset()), asi el
 * primer intento de una sesion bloqueada avisa enseguida. */
static void freq_lock_notice(void) {
    uint64_t now = get_time();

    if (freq_lock_notice_ms == 0 || (now - freq_lock_notice_ms) >= FREQ_LOCK_NOTICE_MS) {
        freq_lock_notice_ms = now;
        locked_notice("Frequency locked");
    }
}

static void freq_lock_notice_reset(void) {
    freq_lock_notice_ms = 0;
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
                    /* Activar PRE apaga ATT (issue #220): el aviso nombra las dos
                     * cosas para que el operador vea que el atenuador se apago.
                     * Si ATT ya estaba off se conserva el mensaje de siempre. */
                    if (pre && att) {
                        msg_tiny_set_text_fmt("Preamplifier On / Attenuator Off");
                    } else {
                        msg_tiny_set_text_fmt("Pre: %s", pre ? "On" : "Off");
                    }
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
                    /* Activar ATT apaga PRE (issue #220): mismo criterio que PRE
                     * sobre ATT. Si PRE ya estaba off se conserva el mensaje. */
                    if (att && pre) {
                        msg_tiny_set_text_fmt("Attenuator On / Preamplifier Off");
                    } else {
                        msg_tiny_set_text_fmt("Att: %s", att ? "On" : "Off");
                    }
                }
            }
            break;

        case KEYPAD_BAND_UP:
            if (keypad->state == KEYPAD_RELEASE) {
                if (band_lock) {
                    /* La banda trabada no cambia en silencio (issue #220). El
                     * dialog_send preexistente se conserva. */
                    locked_notice("Band locked");
                } else {
                    cfg_band_load_next(true);
                }
                dialog_send(EVENT_BAND_UP, NULL);
            }
            break;

        case KEYPAD_BAND_DOWN:
            if (keypad->state == KEYPAD_RELEASE) {
                if (band_lock) {
                    locked_notice("Band locked");
                } else {
                    cfg_band_load_next(false);
                }
                dialog_send(EVENT_BAND_DOWN, NULL);
            }
            break;

        case KEYPAD_MODE_AM:
        case KEYPAD_MODE_CW:
        case KEYPAD_MODE_SSB:
            if (mode_lock) {
                /* Solo el release y el long press son intenciones efectivas
                 * (change_mode los acepta); el press/repeat no avisa, que es el
                 * mismo filtro que aplica change_mode (issue #220). */
                if (keypad->state == KEYPAD_RELEASE || keypad->state == KEYPAD_LONG) {
                    locked_notice("Mode locked");
                }
            } else {
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
                freq_lock_notice_reset();
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
                /* Pulsacion corta de DFL (issue #183): con el DFL ya abierto
                 * cancela lo pendiente y vuelve a Favoritos (contraendo el
                 * grafico si estaba en C); desde fuera abre Favoritos (o Ajustar
                 * si el modo no tiene catalogo). NO aplica ningun preset. */
                if (buttons_dfl_active()) {
                    buttons_dfl_short();
                } else {
                    apps_disable();
                    buttons_dfl_open();
                }
                voice_say_text_fmt("DFL parameters");
            } else if (keypad->state == KEYPAD_LONG) {
                /* DFL largo: si la accion configurada es el editor DFL, alterna
                 * (abre/contrae); si no, ejecuta la accion persistida. */
                if (params.long_dfl == ACTION_APP_DFL_EDITOR) {
                    dfl_inline_toggle();
                } else {
                    main_screen_action(params.long_dfl);
                }
            }
            break;

        case KEYPAD_AB:
            if (ab_lock) {
                /* VFO A/B trabado (issue #220): release y long press avisan, el
                 * press/repeat no. El toggle/clone preexistente no corre. */
                if (keypad->state == KEYPAD_RELEASE || keypad->state == KEYPAD_LONG) {
                    locked_notice("VFO locked");
                }
            } else if (keypad->state == KEYPAD_RELEASE) {
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
                    /* Antes de activar TX (todavia en RX, mismo contexto): Auto
                     * Tune cancela y restaura si es dueño del dial provisional
                     * (issue #190, seccion 5.3). Nunca se difiere un restore que
                     * pudiera correr estando en TX. */
                    autotune_ui_cancel_before_tx();
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
                freq_lock_notice_reset();
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

/* Cambio de modo con el DFL abierto (issue #183): misma familia no toca nada;
 * una familia nueva cancela E/F sin persistir ni aplicar y carga la pagina A (o
 * B si no admite presets). Nunca aplica un preset por si solo. */
static void on_dfl_mode_change(Subject *subj, void *user_data) {
    (void) subj;
    (void) user_data;

    buttons_dfl_on_mode_change();
}

static void main_screen_radio_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);

    /* Con TX ya activo Auto Tune solo puede abortar SIN sintonizar (issue #190,
     * seccion 5.3): jamas se escribe fg_freq desde aca. El cancel previo al TX
     * lo hace el handler de PTT, que todavia esta en RX. */
    if (code == EVENT_RADIO_TX) {
        autotune_ui_abort_on_tx();
    }

    lv_event_send(meter, code, NULL);
    lv_event_send(tx_info, code, NULL);
    lv_event_send(spectrum, code, NULL);

    dialog_send(code, NULL);

    /* El editor DFL inline no es un dialogo: se le avisa TX/RX por separado. */
    dfl_inline_on_radio_event(code);

    /* Con una pagina DFL abierta, TX deshabilita los presets y aplicar Mem
     * (issue #183); la navegacion y Volver siguen. */
    buttons_dfl_on_radio(code == EVENT_RADIO_TX);
}

static void main_screen_update_cb(lv_event_t * e) {

    spectrum_clear();
}

/* --- Contexto de sintonia adaptativa (issue #186, seccion 4) ----------------
 *
 * El motor vive en src/rotary.c (hilo de la GUI, consumidor de la cola). Aca
 * solo se pide el reset cuando cambia el contexto, para que el PRIMER detent
 * posterior sea x1:
 *   - lock/unlock, paso, VFO A/B, banda, modo, perfil y umbral;
 *   - cambio EXTERNO de frecuencia (CAT, memoria, app, scan).
 * El cambio externo se detecta comparando con `expected_freq` (la ultima
 * frecuencia que dejo el dial o el teclado): los observers corren antes de que
 * LVGL lea el encoder, asi que el reset llega a tiempo. */

static struct {
    bool        valid;
    int32_t     expected_freq;
    int32_t     step;
    int32_t     mode;
    int32_t     band;
    int32_t     vfo;
    bool        lock;
    bool        cb;
    uint8_t     profile;
    uint8_t     threshold;
} tuning_ctx;

static void on_tuning_freq_change(Subject *subj, void *user_data);
static void on_tuning_context_change(Subject *subj, void *user_data);

void main_screen_tuning_reset(void) {
    tuning_ctx.valid = false;
    rotary_tuning_reset();
}

/* Cambio externo de frecuencia: si no es la que dejo el dial, el motor arranca
 * de cero en el proximo detent. */
static void on_tuning_freq_change(Subject *subj, void *user_data) {
    (void) subj;
    (void) user_data;

    if (tuning_ctx.valid && subject_get_int(cfg_cur.fg_freq) != tuning_ctx.expected_freq) {
        main_screen_tuning_reset();
    }
}

/* Lock, paso, modo, banda o VFO: reinicio inmediato. */
static void on_tuning_context_change(Subject *subj, void *user_data) {
    (void) subj;
    (void) user_data;

    main_screen_tuning_reset();
}

/* Un unico paso EXACTO, sin aceleracion (teclado/HKEY, seccion 4). Misma suma
 * segura en int64 y misma validacion que el dial. */
static void freq_shift(int16_t diff) {
    /* Una orden manual de dial aborta ANTES de aplicarse y sin escribir: lo que
     * elige el operador nunca se pisa (issue #190, seccion 5.1). */
    autotune_ui_abort_manual();

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

    int32_t target;

    if (!adaptive_tuning_target_freq(freq, diff, subject_get_int(cfg_cur.freq_step), &target)) {
        return;
    }

    if (!radio_check_freq(target)) {
        return;
    }

    subject_set_int(cfg_cur.fg_freq, target);
    tuning_ctx.expected_freq = subject_get_int(cfg_cur.fg_freq);

    voice_say_freq(target);
}

/* Indicador `xN` (issue #186): muestra el multiplicador ENTERO efectivo que
 * devolvio el motor, nunca velocidad ni fracciones. Mantiene los estilos y el
 * timeout; ocultarlo NO cambia la frecuencia (el timer solo esconde). */
static void update_tuning_multiplier_display(uint8_t multiplier) {
    if (!tuning_multiplier_label) return;

    if (multiplier == 0) multiplier = 1;

    // Update text (OPT-4 #150: guarda de igualdad; el encoder reescribe el
    // mismo "xN" al moverse dentro del mismo multiplo)
    char buf[16];

    snprintf(buf, sizeof(buf), "x%u", (unsigned) multiplier);
    ui_label_set_text(tuning_multiplier_label, buf);

    // Update color based on range
    lv_color_t color;
    if (multiplier <= 1) {
        color = lv_color_white();                    // White - normal
    } else if (multiplier < 10) {
        color = lv_color_hex(theme_hex(TC_warn));    // Yellow - fast
    } else {
        color = lv_color_hex(theme_hex(TC_err));     // Red - scan
    }
    ui_obj_set_text_color(tuning_multiplier_label, color);
    lv_obj_clear_flag(tuning_multiplier_label, LV_OBJ_FLAG_HIDDEN);

    // Reset hide timer
    if (multiplier_hide_timer) {
        lv_timer_reset(multiplier_hide_timer);
        lv_timer_resume(multiplier_hide_timer);
    }
}

static void hide_tuning_multiplier(void) {
    if (tuning_multiplier_label) {
        lv_obj_add_flag(tuning_multiplier_label, LV_OBJ_FLAG_HIDDEN);
    }
}

static void hide_multiplier_cb(lv_timer_t *timer) {
    if (tuning_multiplier_label) {
        lv_obj_add_flag(tuning_multiplier_label, LV_OBJ_FLAG_HIDDEN);
    }
}

/* Layout equivalence between the persisted selector and the pure engine
 * (seccion 2.1). */
_Static_assert(FREQ_ACCEL_NONE == ADAPTIVE_ACCEL_NONE &&
               FREQ_ACCEL_LITE == ADAPTIVE_ACCEL_LITE &&
               FREQ_ACCEL_STRONG == ADAPTIVE_ACCEL_STRONG,
               "freq_accel_t must match adaptive_accel_t");

/* Receptor del dial (seccion 4). El payload es SINCRONICO y vive en el stack
 * del emisor (src/rotary.c, que llama lv_event_send): no se libera aca.
 *
 * `weighted_detents` ya viene con el multiplicador efectivo por detent, en
 * int64; `raw_diff` es el conteo crudo del tramo de igual signo y se usa tal
 * cual para CB y para dialog_rotary(). Nunca se netean signos contrarios. */
static void main_screen_rotary_enhanced_cb(lv_event_t * e) {
    rotary_event_enhanced_t *enhanced = (rotary_event_enhanced_t *)lv_event_get_param(e);
    int32_t freq = subject_get_int(cfg_cur.fg_freq);

    /* El giro del dial (con o sin aceleracion) es una orden manual: aborta la
     * sesion de Auto Tune ANTES de escribir el VFO y sin restaurar nada
     * (issue #190, seccion 5.1). La aceleracion del #186 no se toca. */
    autotune_ui_abort_manual();

    cb_plan_t plan;
    bool      cb      = cb_plan_of_freq(freq, &plan);
    bool      lock    = subject_get_int(freq_lock);
    int32_t   step    = subject_get_int(cfg_cur.freq_step);
    int32_t   mode    = subject_get_int(cfg_cur.mode);
    int32_t   band    = subject_get_int(cfg.band_id.val);
    int32_t   vfo     = subject_get_int(cfg_cur.band->vfo.val);
    uint8_t   profile = params.freq_accel.x;
    uint8_t   thresh  = params.freq_accel_threshold.x;

    /* Checks de contexto (por si un cambio no paso por su observer). Cualquier
     * diferencia invalida el motor: el proximo detent arranca en x1. En el
     * primer evento (valid == false) NO se pide otro reset: el motor ya viene
     * reseteado por rotary_main_init()/por el observer que invalido el contexto;
     * pedirlo aca consumiria un segundo detent en x1. */
    if (tuning_ctx.valid &&
        (tuning_ctx.lock != lock ||
         tuning_ctx.step != step ||
         tuning_ctx.mode != mode ||
         tuning_ctx.band != band ||
         tuning_ctx.vfo != vfo ||
         tuning_ctx.cb != cb ||
         tuning_ctx.profile != profile ||
         tuning_ctx.threshold != thresh ||
         tuning_ctx.expected_freq != freq)) {
        main_screen_tuning_reset();
    }

    tuning_ctx.valid         = true;
    tuning_ctx.expected_freq = freq;
    tuning_ctx.step          = step;
    tuning_ctx.mode          = mode;
    tuning_ctx.band          = band;
    tuning_ctx.vfo           = vfo;
    tuning_ctx.lock          = lock;
    tuning_ctx.cb            = cb;
    tuning_ctx.profile       = profile;
    tuning_ctx.threshold     = thresh;

    /* En CB el dial cuenta canales: 1 detent = 1 canal, SIN multiplicador
     * adaptativo (issue #146, seccion 5.3). Se ignora `weighted_detents`, se
     * oculta el indicador y el motor ya se reinicio al entrar/salir de CB. */
    if (cb) {
        if (!lock) {
            int ch = cb_step_channel(plan, freq, enhanced->raw_diff);

            subject_set_int(cfg_cur.fg_freq, cb_freq_of_channel(plan, ch));
            voice_say_text_fmt("Channel %i", ch);

            tuning_ctx.expected_freq = subject_get_int(cfg_cur.fg_freq);
        } else {
            /* Tambien el dial CB trabado avisa (issue #220), con el mismo
             * throttle: los detents se consumen sin mover el canal. */
            freq_lock_notice();
        }

        hide_tuning_multiplier();
        dialog_rotary(enhanced->raw_diff);
        return;
    }

    if (lock) {
        /* Con el dial trabado se consumen los pulsos sin mover el VFO y se
         * reinicia la aceleracion; el movimiento CRUDO sigue llegando al
         * dialogo abierto, como antes. El aviso "Frequency locked" sale a lo
         * sumo una vez cada 2 s (issue #220). */
        main_screen_tuning_reset();
        hide_tuning_multiplier();
        freq_lock_notice();
        dialog_rotary(enhanced->raw_diff);
        return;
    }

    /* delta_hz = weighted_detents x freq_step, TODO en int64. Sin align_int() y
     * sin redondear al multiplicador: se conserva cualquier offset legitimo de
     * la frecuencia inicial respecto de la grilla (14.200.003 + 10 = 14.200.013). */
    int32_t target;

    if (adaptive_tuning_target_freq(freq, enhanced->weighted_detents, step, &target) &&
        radio_check_freq(target)) {
        subject_set_int(cfg_cur.fg_freq, target);
        tuning_ctx.expected_freq = subject_get_int(cfg_cur.fg_freq);
    }

    update_tuning_multiplier_display(enhanced->last_multiplier);

    dialog_rotary(enhanced->raw_diff);
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
            /* Con el editor DFL inline expandido, el encoder VOL (rotary1) edita
             * el filtro: NO llega a vol_update(), asi que no cambia volumen ni
             * VFO. */
            if (key == KEY_VOL_LEFT_EDIT && dfl_inline_on_vol_key(KEY_VOL_LEFT_EDIT)) {
                break;
            }
            vol_update(-1, false);
            break;

        case KEY_VOL_RIGHT_EDIT:
        case ']':
            if (key == KEY_VOL_RIGHT_EDIT && dfl_inline_on_vol_key(KEY_VOL_RIGHT_EDIT)) {
                break;
            }
            vol_update(+1, false);
            break;

        case KEY_VOL_LEFT_SELECT:
        case '{':
            if (key == KEY_VOL_LEFT_SELECT && dfl_inline_on_vol_key(KEY_VOL_LEFT_SELECT)) {
                break;
            }
            vol_change_mode(-1);
            break;

        case KEY_VOL_RIGHT_SELECT:
        case '}':
            if (key == KEY_VOL_RIGHT_SELECT && dfl_inline_on_vol_key(KEY_VOL_RIGHT_SELECT)) {
                break;
            }
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
            /* Con una pagina DFL abierta (issue #183), ESC navega el DFL: A
             * restaura la barra anterior, B/D/F vuelven a A, C contrae a B y E
             * vuelve a D. Sin el DFL, conserva el comportamiento previo: alterna
             * EDIT/SELECT del encoder de volumen. */
            if (buttons_dfl_active()) {
                buttons_dfl_escape();
                break;
            }
            if (dfl_inline_is_expanded()) {
                dfl_inline_collapse();
                break;
            }
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
            freq_lock_notice_reset();
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
    freq_lock_notice_reset();
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

    /* Franja de spots del cluster (X6100): son overlays independientes (lineas y
     * etiquetas) que spot_marks_init() crea DESPUES del waterfall, asi que se
     * dibujan encima. No se reserva su alto: cuando el cluster no tiene spots
     * quedaba una franja negra de 44 px entre las etiquetas y el waterfall (#298). */
    lv_coord_t spots_y = y;

    waterfall = waterfall_init(obj);

    waterfall_min_max_reset();

    lv_obj_set_y(waterfall, y);
    waterfall_set_height(480 - y);

    /* Editor DFL inline (issue #179): panel nativo dentro del rectangulo del
     * waterfall, creado despues de la imagen para dibujarse encima. Deriva su
     * geometria del objeto real (no hardcodea 240). */
    dfl_inline_init(waterfall);

    /* Presets de filtro del DFL (issue #183): la banda contextual de las paginas
     * de confirmacion/asignacion y la cache de memorias/favoritos (se leen UNA
     * vez de params.db; nunca hay SQL desde un callback de dibujo). */
    dfl_context_init(waterfall);
    dfl_presets_init();

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
    lv_label_set_text(tuning_multiplier_label, "x1");
    lv_obj_add_flag(tuning_multiplier_label, LV_OBJ_FLAG_HIDDEN);
    
    // Hide timer for multiplier display
    multiplier_hide_timer = lv_timer_create(hide_multiplier_cb, 2000, NULL);
    lv_timer_pause(multiplier_hide_timer);
    

    cw_tune_init(obj);

    msg_schedule_text_fmt("X6200 de R1CBU " VERSION);

    subject_add_delayed_observer(freq_lock, on_fg_freq_change, NULL);
    subject_add_delayed_observer(cfg_cur.band->split.val, on_fg_freq_change, NULL);
    subject_add_delayed_observer(cfg_cur.fg_freq, on_fg_freq_change, NULL);

    /* Cambio de modo con el DFL abierto (issue #183): cancela suboperaciones
     * pendientes y reubica la pagina segun la familia, sin aplicar nada. */
    subject_add_delayed_observer(cfg_cur.mode, on_dfl_mode_change, NULL);

    subject_add_delayed_observer(freq_lock, update_freq_boundaries, NULL);
    // update boundaries on TX with split
    subject_add_delayed_observer(cfg_cur.fg_freq, update_freq_boundaries, NULL);
    subject_add_delayed_observer(cfg_cur.fft_width, update_freq_boundaries, NULL);
    update_freq_boundaries(cfg_cur.fft_width, NULL);

    subject_add_delayed_observer(cfg_cur.bg_freq, on_fg_freq_change, NULL);
    on_fg_freq_change(cfg_cur.bg_freq, NULL);

    /* Resets del motor adaptativo (issue #186): el cambio EXTERNO de frecuencia
     * y los cambios de contexto invalidan la aceleracion antes de que LVGL lea
     * el encoder (los delayed observers corren primero en el loop, src/main.c). */
    subject_add_delayed_observer(cfg_cur.fg_freq, on_tuning_freq_change, NULL);
    subject_add_delayed_observer(freq_lock, on_tuning_context_change, NULL);
    subject_add_delayed_observer(cfg_cur.freq_step, on_tuning_context_change, NULL);
    subject_add_delayed_observer(cfg_cur.mode, on_tuning_context_change, NULL);
    subject_add_delayed_observer(cfg.band_id.val, on_tuning_context_change, NULL);
    subject_add_delayed_observer(cfg_cur.band->vfo.val, on_tuning_context_change, NULL);

    tuning_ctx.valid = false;

    /* Asistente de sintonia fina CW/CWR (issue #190): registra su observador de
     * cambio externo de frecuencia. El asistente queda en reposo hasta que el
     * operador pulse Auto Tune. */
    autotune_ui_init();

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

    ui_label_set_text(freq_shadow, ghost);
}

/* Fila de frecuencia en CB (issue #169): el canal manda. `true` si `f` cae en un
 * grid CB; `*plan` queda con el grid y `*ch` con el canal 1..40, o CB_CHANNEL_NONE
 * si es off-channel (hueco o frecuencia intermedia). */
static bool cb_row_state(int32_t f, cb_plan_t *plan, int *ch) {
    if (!cb_plan_of_freq(f, plan)) return false;
    *ch = cb_channel_of_freq(*plan, f);   /* CB_CHANNEL_NONE = off-channel */
    return true;
}

/* Fuente normal (no la segmentada del tema) y fantasma oculto mientras la fila
 * muestra el canal: la familia `font_digits` es solo digitos y no tiene `CH`,
 * `CB` ni `--`. Se aplica SOLO en la transicion CB <-> no-CB (`cb_row_on`), no en
 * cada cambio de frecuencia. Al salir, quitar la fuente local devuelve la del
 * estilo (`font_digit(...)`) y el fantasma se vuelve a llenar en el camino normal. */
static bool cb_row_on = false;

static void cb_row_fonts(bool on) {
    if (on == cb_row_on) return;
    cb_row_on = on;
    for (int i = 0; i < 3; i++) {
        if (on) {
            lv_obj_set_style_text_font(freq[i], i == 1 ? &sony_38 : &sony_30, 0);
        } else {
            lv_obj_remove_local_style_prop(freq[i], LV_STYLE_TEXT_FONT, 0);
        }
    }
    if (freq_shadow != NULL) {
        if (on) lv_obj_add_flag(freq_shadow, LV_OBJ_FLAG_HIDDEN);
        else    lv_obj_clear_flag(freq_shadow, LV_OBJ_FLAG_HIDDEN);
    }
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

    /* Fila en CB (issue #169): 27.185 · CH 19 · CB C. El canal va en grande con
     * la familia NORMAL (cb_row_fonts), asi que `CH`/`CB`/`--` se ven tambien en
     * los temas con font_digits; el fantasma se oculta y no se toca msg_tiny (el
     * overlay taparia la fila). Al salir vuelve el camino de abajo. */
    cb_plan_t plan;
    int       ch = CB_CHANNEL_NONE;
    bool      cb = cb_row_state(f, &plan, &ch);

    cb_row_fonts(cb);

    if (cb) {
        char     text[32], marked[48];
        uint16_t mhz_cb, khz_cb, hz_cb;

        split_freq(f, &mhz_cb, &khz_cb, &hz_cb);

        snprintf(marked, sizeof(marked), "#%06X %i.%03i", color, mhz_cb, khz_cb);
        ui_label_set_text(freq[0], marked);

        if (ch != CB_CHANNEL_NONE) snprintf(text, sizeof(text), "CH %d", ch);
        else                       snprintf(text, sizeof(text), "CH --");
        snprintf(marked, sizeof(marked), "#%06X %s", color, text);
        ui_label_set_text(freq[1], marked);

        snprintf(marked, sizeof(marked), "#%06X %s", color, cb_plan_name(plan));
        ui_label_set_text(freq[2], marked);
        return;   /* nada de msg_tiny ni de freq_shadow_update */
    }

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
    char marked[64];

    snprintf(marked, sizeof(marked), "#%06X %s", color, value);
    ui_label_set_text(freq[1], marked);
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

    char boundary[32];

    /* La franja del plan de banda sigue el span SIEMPRE, tambien en CB (issue #169):
     * el grid se pinta igual que los segmentos de aficionado. */
    band_plan_update(f, (int32_t) half_width);

    /* En CB los tres labels los escribe on_fg_freq_change() (27.185 / CH 19 / CB C),
     * que no depende del span: aca no se tocan los extremos. */
    cb_plan_t plan;

    if (cb_plan_of_freq(f, &plan)) {
        return;
    }

    split_freq(f - half_width, &mhz, &khz, &hz);
    snprintf(boundary, sizeof(boundary), "#%06X %i.%03i", color, mhz, khz);
    ui_label_set_text(freq[0], boundary);

    split_freq(f + half_width, &mhz, &khz, &hz);
    snprintf(boundary, sizeof(boundary), "#%06X %i.%03i", color, mhz, khz);
    ui_label_set_text(freq[2], boundary);
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
