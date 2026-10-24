/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include "radio.h"
#include "main.h"
#include "btaudio.h"
#include "cb.h"
#include "gpio_safe.h"

#include "cfg/atu.h"
#include "cfg/transverter.h"
#include "util.h"
#include "dsp.h"
#include "params/params.h"
#include "hkey.h"
#include "tx_info.h"
#include "msg.h"
#include "info.h"
#include "dialog_swrscan.h"
#include "cw.h"
#include "pubsub_ids.h"
#include "events.h"
#include "scheduler.h"
#include "visual_policy.h"
#include "sql_policy.h"

#include <aether_radio/x6200_control/low/flow.h>
#include <aether_radio/x6200_control/low/gpio.h>

#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <math.h>

#define FLOW_RESTART_TIMEOUT 300
#define IDLE_TIMEOUT (1000)
#define POWER_KEY_LONG_TIME 2000

static radio_state_change_t notify_tx;
static radio_state_change_t notify_rx;

static pthread_mutex_t  control_mux;

static x6200_flow_t     *pack;

static radio_state_t    state = RADIO_RX;
/* El control de la radio queda inicializado al final de radio_init(). Si la BASE
 * no responde, radio_init() sale antes y el lock tambien queda sin inicializar:
 * los hooks de CB no pueden llamar al control en ese caso (el banco QEMU, que
 * corre sin radio, se colgaba al entrar a CB). */
static bool             radio_control_ok = false;
static uint64_t         now_time;
static uint64_t         prev_time;
static uint64_t         idle_time;
static bool             mute = false;

/* Aviso de bateria baja (issue #121): solo informativo, con hysteresis sobre
 * los umbrales EXISTENTES del perfil CRITICAL de visual_policy.h. No bloquea
 * TX, no apaga nada y no cambia el presupuesto de RF. */
static bool             battery_low_warned = false;

/* Perfil visual del pack (UI-08 incr. 2). El estado lo toca solo este hilo. */
static visual_policy_t  visual_policy;

#define WITH_RADIO_LOCK(fn) radio_lock(); fn; radio_unlock();

#define CHANGE_PARAM(new_val, val, dirty, radio_fn) \
    if (new_val != val) { \
        params_lock(); \
        val = new_val; \
        params_unlock(&dirty); \
        radio_lock(); \
        radio_fn(val); \
        radio_unlock(); \
        lv_msg_send(MSG_PARAM_CHANGED, NULL); \
    }

static void radio_lock() {
    pthread_mutex_lock(&control_mux);
}

static void radio_unlock() {
    idle_time = get_time();
    pthread_mutex_unlock(&control_mux);
}

static void process_power_key(bool val, uint64_t now) {
    static event_keypad_t event = { .key = KEYPAD_POWER, .state = KEYPAD_RELEASE };
    static bool prev_val;
    static uint64_t power_press_time;

    if (val != prev_val) {
        if (val) {
            event.state = KEYPAD_PRESS;
            power_press_time = now;
        } else if (event.state == KEYPAD_PRESS) {
            event.state = KEYPAD_RELEASE;
        } else {
            event.state = KEYPAD_LONG_RELEASE;
        }
        event_keypad_t *ev = malloc(sizeof(event_keypad_t));
        *ev = event;
        event_send(NULL, EVENT_KEYPAD, (void*) ev);
    }
    if ((event.state == KEYPAD_PRESS) && (now - power_press_time > POWER_KEY_LONG_TIME)) {
        event.state = KEYPAD_LONG;
        event_keypad_t *ev = malloc(sizeof(event_keypad_t));
        *ev = event;
        event_send(NULL, EVENT_KEYPAD, (void*) ev);
    }
    prev_val = val;
}

/* Aplica el perfil ya evaluado al waterfall, al espectro y al brillo del
 * backlight. Corre en el hilo de LVGL (lo encola radio_tick por el scheduler, el
 * mismo patron que tx_info_update en src/tx_info.c, porque el waterfall no es de
 * este hilo). Los settings del usuario se leen aca, del lado de LVGL, y el perfil
 * solo los respeta o los recorta: los parametros no se escriben nunca. */
static void apply_visual_profile(void *arg) {
    visual_profile_t profile = *(visual_profile_t *) arg;

    visual_policy_waterfall_apply(profile, params.waterfall_battery.x);
    visual_policy_spectrum_apply(profile);
    visual_policy_brightness_apply(profile, params.brightness_normal);
}

/* Aplica el override persistido antes de la primera muestra (UI-08 incr. 5).
 * El byte de `params.visual_profile` (cargado por params_init) es AUTOMATICO por
 * defecto; si el usuario dejo un perfil fijado, el cap del brillo y el modo del
 * waterfall valen desde el arranque, sin esperar al primer paquete del pack. Con
 * AUTO no se toca nada: lo decide la bateria en radio_tick. Corre en el hilo
 * principal, el mismo que arma la GUI y que despues corre la politica. */
static void apply_visual_override_at_boot(void) {
    visual_override_t override = visual_policy_override_from_param(params.visual_profile.x);

    if (!override.manual) {
        return;
    }

    visual_policy_result_t boot = visual_policy_eval(&visual_policy, 0, 0, 0, false, 0, 0, override);

    visual_policy_waterfall_apply(boot.profile, params.waterfall_battery.x);
    visual_policy_spectrum_apply(boot.profile);
    visual_policy_brightness_apply(boot.profile, params.brightness_normal);
}

bool radio_tick() {

    if (now_time < prev_time) {
        prev_time = now_time;
    }

    int32_t d = now_time - prev_time;

    if (x6200_flow_read(pack)) {
        prev_time = now_time;

        // printf("power_key=%d resync=%d flag12=%d flag13=%d flag15=%d, hkey=%d, vext=%d charding=%d\n",
        //     pack->flag.power_key, pack->flag.resync, pack->flag.flag12, pack->flag.flag13, pack->flag.flag15,
        //     pack->hkey, pack->flag.vext, pack->flag.charging);

        static uint8_t delay = 0;

        if (delay++ > 10) {
            delay = 0;
            clock_update_power(pack->vext * 0.1f, pack->vbat*0.1f, pack->batcap, pack->flag.charging);

            /* UI-08 incr. 2 (y 3): el mismo pack que alimenta el reloj alimenta
             * el perfil visual. La politica espera mV y los campos vienen en
             * decimas de voltio (clock.c dibuja v_bat*10 sobre el mismo campo
             * que aca se convierte con *0.1), asi que mV = campo * 100: 121 ->
             * 12.1 V -> 12100 mV. La muestra acaba de llegar (x6200_flow_read
             * devolvio este paquete), asi que last_valid_ms = now_time y la
             * antiguedad es 0: el fallback por datos viejos no aplica a este
             * punto de conexion. Solo la transicion del perfil toca el waterfall
             * y el brillo (el mismo apply_visual_profile); el valor de arranque
             * lo pusieron main.c y Ajustes con los settings del usuario.
             *
             * El override sale del parametro persistido (UI-08 incr. 5): con
             * AUTO lo decide la bateria; con un perfil fijado, ese perfil manda
             * (y el parametro NO se escribe: la politica lo lee, nunca lo toca). */
            uint16_t vbat_mv = (uint16_t) pack->vbat * 100u;
            uint16_t vext_mv = (uint16_t) pack->vext * 100u;
            bool     bat_valid = (vbat_mv >= VP_VBAT_MIN_MV && vbat_mv <= VP_VBAT_MAX_MV);

            visual_policy_result_t visual = visual_policy_eval(&visual_policy,
                                                               vbat_mv,
                                                               vext_mv,
                                                               pack->batcap,
                                                               pack->flag.charging,
                                                               (uint32_t) now_time,
                                                               (uint32_t) now_time,
                                                               visual_policy_override_from_param(params.visual_profile.x));

            if (visual.changed) {
                scheduler_put(apply_visual_profile, &visual.profile, sizeof(visual.profile));
            }

            /* Aviso de bateria baja (issue #121), SOLO visible: un mensaje en la
             * GUI y una linea en el log. El umbral es el que ya usa la politica
             * visual para entrar a CRITICAL (VP_CRITICAL_ENTER_MV, 10.8 V en la
             * tabla de fabrica); no se inventa un numero nuevo. La calibracion
             * contra el pack real del owner queda pendiente en la radio. La
             * salida usa VP_CRITICAL_EXIT_MV (11.3 V) para no repetir el aviso.
             * Con fuente externa cargando no se avisa (es esperable), pero se
             * limpia el estado si la lectura se recupera. */
            bool low = visual_policy_low_battery_warn(vbat_mv, pack->flag.charging,
                                                      battery_low_warned);

            if (low && !battery_low_warned) {
                LV_LOG_WARN("BATTERY-LOW %.1f V <= %.1f V (umbral existente de CRITICAL; calibrar en radio)",
                            vbat_mv / 1000.0f, VP_CRITICAL_ENTER_MV / 1000.0f);
                msg_schedule_text_fmt("Battery low: %.1f V", vbat_mv / 1000.0f);
            } else if (!low && battery_low_warned && bat_valid) {
                msg_schedule_text_fmt("Battery recovered: %.1f V", vbat_mv / 1000.0f);
            }

            battery_low_warned = low;
        }
        // printf("sql_mute=%d sql_fm_mute=%d\n", pack->flag.sql_mute, pack->flag.sql_fm_mute);
        // printf("flags %08x\n", pack->flag);
        float *samples = (float*)((char *)pack + offsetof(x6200_flow_t, samples));
        x6200_mode_t mode = subject_get_int(cfg_cur.mode);

        dsp_samples(samples, RADIO_SAMPLES, pack->flag.tx, -(int16_t)pack->dbm);
        // printf("als=%f\n", pack->alc_level * 0.1f);

        process_power_key(pack->flag.power_key, now_time);

        if (subject_get_int(cfg.key_train.val) && ((mode == x6200_mode_cw) || (mode == x6200_mode_cwr))) {
            // Ignore TX from BASE
            pack->flag.tx = false;
        }

        switch (state) {
            case RADIO_RX:
                if (pack->flag.tx) {
                    state = RADIO_TX;
                    notify_tx();
                }
                break;

            case RADIO_TX:
                if (!pack->flag.tx) {
                    state = RADIO_RX;
                    notify_rx();
                } else {
                    tx_info_update(pack->tx_power * 0.1f, pack->vswr * 0.1f, pack->alc_level * 0.1f);
                }
                break;

            case RADIO_ATU_START:
                WITH_RADIO_LOCK(x6200_control_atu_tune(true));
                state = RADIO_ATU_WAIT;
                break;

            case RADIO_ATU_WAIT:
                if (pack->flag.tx) {
                    notify_tx();
                    state = RADIO_ATU_RUN;
                }
                break;

            case RADIO_ATU_RUN:
                if (pack->flag.atu_status && !pack->flag.tx) {
                    cfg_atu_save_network(pack->atu_params);
                    WITH_RADIO_LOCK(x6200_control_atu_tune(false));
                    subject_set_int(cfg.atu_enabled.val, true);
                    notify_rx();

                    // TODO: change with observer on atu->loaded change
                    WITH_RADIO_LOCK(x6200_control_cmd(x6200_atu_network, pack->atu_params));
                    state = RADIO_RX;
                } else if (pack->flag.tx) {
                    tx_info_update(pack->tx_power * 0.1f, pack->vswr * 0.1f, pack->alc_level * 0.1f);
                }
                break;

            case RADIO_SWRSCAN:
                dialog_swrscan_update(pack->vswr * 0.1f);
                break;

            case RADIO_POWEROFF:
                x6200_control_poweroff();
                state = RADIO_OFF;
                break;

            case RADIO_OFF:
                break;
        }

        hkey_put(pack->hkey);
    } else {
        if (d > FLOW_RESTART_TIMEOUT) {
            LV_LOG_WARN("Flow reset");
            prev_time = now_time;
            x6200_flow_restart();
            dsp_reset();
        }

        /* Watchdog del barrido de ROE (issue #125): si el flow deja de mandar paquetes, el
         * barrido no se queda transmitiendo: el motor aborta por timeout y apaga el TX. */
        if (state == RADIO_SWRSCAN) {
            dialog_swrscan_poll();
        }

        return true;
    }
    return false;
}

static void * radio_thread(void *arg) {
    while (true) {
        now_time = get_time();

        /* El sondeo del flow duerme SIEMPRE, sin mirar lo que devuelve radio_tick()
         * (issue #219, F-08): antes el sleep dependia de ese valor de retorno, asi
         * que la rama que no dormia giraba en vacio consumiendo CPU (medido en
         * QEMU_USER: ~99.8% de un core host y ~630k read()/s). Con el sleep fijo
         * el lazo corre a ~67 iteraciones/s (15000 us) en todas las rutas, el mismo
         * ritmo que ya tenia la ruta que dormia, y el sleep va FUERA del lock de
         * radio (radio_tick no lo retiene al volver). */
        radio_tick();
        usleep(15000);

        int32_t idle = now_time - idle_time;

        if (idle > IDLE_TIMEOUT) {
            WITH_RADIO_LOCK(x6200_control_idle());

            idle_time = now_time;
        }
    }
}

/* Audio de recepcion (#244): ver radio_vol_apply() y on_cur_mode_change_vol() mas abajo. */
static void on_volume_change(Subject *subj, void *user_data);
static void on_cur_mode_change_vol(Subject *subj, void *user_data);

static void on_change_bool(Subject *subj, void *user_data) {
    int32_t new_val = subject_get_int(subj);
    void (*fn)(bool) = (void (*)(bool))user_data;
    WITH_RADIO_LOCK(fn(new_val));
}

static void on_change_int8(Subject *subj, void *user_data) {
    int32_t new_val = subject_get_int(subj);
    void (*fn)(int8_t) = (void (*)(int8_t))user_data;
    WITH_RADIO_LOCK(fn(new_val));
}

static void on_change_uint8(Subject *subj, void *user_data) {
    int32_t new_val = subject_get_int(subj);
    void (*fn)(uint8_t) = (void (*)(uint8_t))user_data;
    WITH_RADIO_LOCK(fn(new_val));
}

static void on_change_uint16(Subject *subj, void *user_data) {
    int32_t new_val = subject_get_int(subj);
    void (*fn)(uint16_t) = (void (*)(uint16_t))user_data;
    WITH_RADIO_LOCK(fn(new_val));
}

static void on_change_uint32(Subject *subj, void *user_data) {
    int32_t new_val = subject_get_int(subj);
    void (*fn)(uint32_t) = (void (*)(uint32_t))user_data;
    WITH_RADIO_LOCK(fn(new_val));
}

static void on_change_float(Subject *subj, void *user_data) {
    float new_val = subject_get_float(subj);
    void (*fn)(float) = (void (*)(float))user_data;
    WITH_RADIO_LOCK(fn(new_val));
}

static void on_vfo_freq_change(Subject *subj, void *user_data) {
    x6200_vfo_t vfo = (x6200_vfo_t )user_data;
    int32_t new_val = subject_get_int(subj);
    int32_t shift = cfg_transverter_get_shift(new_val);
    WITH_RADIO_LOCK(x6200_control_vfo_freq_set(vfo, new_val - shift));
    LV_LOG_USER("Radio set vfo %i freq=%i (%i)", vfo, new_val, new_val - shift);
}

/* #244: el BASE apaga el audio de RX cuando se le escribe el modo de un VFO
 * inactivo (medido: entrar a CW con el modo del VFO B escrito mutea el RX hasta
 * el power-cycle; sin ese write el audio sigue sano). La RX es el VFO A: se
 * escribe siempre; el B solo con split habilitado. */
static bool mode_write_allowed(x6200_vfo_t vfo) {
    if (vfo == X6200_VFO_A) {
        return true;
    }

    return subject_get_int(cfg_cur.band->split.val) != 0;
}

static void on_vfo_mode_change(Subject *subj, void *user_data) {
    x6200_vfo_t vfo = (x6200_vfo_t )user_data;
    int32_t new_val = subject_get_int(subj);

    if (!mode_write_allowed(vfo)) {
        LV_LOG_USER("Radio vfo %i mode=%i sin escribir (inactivo, #244)", vfo, new_val);
        return;
    }

    WITH_RADIO_LOCK(x6200_control_vfo_mode_set(vfo, new_val));
    LV_LOG_USER("Radio set vfo %i mode=%i", vfo, new_val);
    radio_i2c_write_pack();
}

static void on_split_mode_sync(Subject *subj, void *user_data) {
    (void) subj;
    (void) user_data;

    if (subject_get_int(cfg_cur.band->split.val)) {
        int32_t b_mode = subject_get_int(cfg_cur.band->vfo_b.mode.val);
        WITH_RADIO_LOCK(x6200_control_vfo_mode_set(X6200_VFO_B, b_mode));
    }
}

static void on_vfo_agc_change(Subject *subj, void *user_data) {
    x6200_vfo_t vfo = (x6200_vfo_t )user_data;
    int32_t new_val = subject_get_int(subj);
    WITH_RADIO_LOCK(x6200_control_vfo_agc_set(vfo, new_val));
    LV_LOG_USER("Radio set vfo %i agc=%i", vfo, new_val);
}

static void on_vfo_att_change(Subject *subj, void *user_data) {
    x6200_vfo_t vfo = (x6200_vfo_t )user_data;
    int32_t new_val = subject_get_int(subj);
    WITH_RADIO_LOCK(x6200_control_vfo_att_set(vfo, new_val));
    LV_LOG_USER("Radio set vfo %i att=%i", vfo, new_val);
}

static void on_vfo_pre_change(Subject *subj, void *user_data) {
    x6200_vfo_t vfo = (x6200_vfo_t )user_data;
    int32_t new_val = subject_get_int(subj);
    WITH_RADIO_LOCK(x6200_control_vfo_pre_set(vfo, new_val));
    LV_LOG_USER("Radio set vfo %i pre=%i", vfo, new_val);
}

static void on_atu_network_change(Subject *subj, void *user_data) {
    uint32_t new_val = subject_get_int(subj);
    WITH_RADIO_LOCK(x6200_control_cmd(x6200_atu_network, new_val));
    LV_LOG_USER("Radio set atu network=%u", new_val);
}

static void on_low_filter_change(Subject *subj, void *user_data) {
    int32_t low = subject_get_int(subj);
    switch (subject_get_int(cfg_cur.mode)) {
        case x6200_mode_cw:
        case x6200_mode_cwr:
        case x6200_mode_am:
        case x6200_mode_sam:
        case x6200_mode_nfm:
        case x6200_mode_wfm:
            break;

        default:
            LV_LOG_USER("Radio set filter_low=%i", low);
            radio_lock();
            x6200_control_rx_filter_set_low(low);
            radio_unlock();
            break;
    }
}

static void on_high_filter_change(Subject *subj, void *user_data) {
    int32_t high = subject_get_int(subj);
    radio_lock();
    switch (subject_get_int(cfg_cur.mode)) {
        case x6200_mode_cw:
        case x6200_mode_cwr:
        case x6200_mode_am:
        case x6200_mode_sam:
        case x6200_mode_nfm:
        case x6200_mode_wfm:
            LV_LOG_USER("Radio set filter_low=%i", -high);
            LV_LOG_USER("Radio set filter_high=%i", high);
            x6200_control_rx_filter_set(-high, high);
            break;

            default:
            LV_LOG_USER("Radio set filter_high=%i", high);
            x6200_control_rx_filter_set_high(high);
            break;
    }
    radio_unlock();
}

/* Un cambio de squelch aplica el nivel y deja el enable del DSP coherente con
 * la convencion 0 = abierto / >0 = habilitado (issue #166). Corre una sola vez
 * por cambio: lo dispara el subject cfg_cur.sql_level, nunca radio_tick(). */
static void on_change_cur_sql_level(Subject *subj, void *user_data) {
    x6200_mode_t mode = subject_get_int(cfg_cur.mode);
    uint8_t val = subject_get_int(subj);
    radio_lock();
    sql_policy_apply(mode, val);
    radio_unlock();
}

void radio_bb_reset() {
    gpio_safe_set(x6200_pin_bb_reset, 1);
    usleep(100000);
    gpio_safe_set(x6200_pin_bb_reset, 0);
}

/* TEST #292: el GUI vendor manda por I2C (0x72, bus 0) el bloque completo de
 * 54 registros (216 B, reg 0x0000). Nuestro port escribe los registros de a uno
 * por serial; el BASE parece aplicar el modo solo con el bloque completo. */
static void radio_i2c_write_pack(void) {
    int fd = open("/dev/i2c-0", O_RDWR);

    if (fd < 0) {
        LV_LOG_ERROR("TEST292 i2c: no puedo abrir /dev/i2c-0");
        return;
    }

    uint8_t buf[2 + 216];

    buf[0] = 0x00;   /* reg 0x0000, 2 bytes big-endian */
    buf[1] = 0x00;

    for (int i = 0; i < 54; i++) {
        uint32_t v = (uint32_t) x6200_control_get(i);

        buf[2 + i * 4 + 0] = v & 0xff;
        buf[2 + i * 4 + 1] = (v >> 8) & 0xff;
        buf[2 + i * 4 + 2] = (v >> 16) & 0xff;
        buf[2 + i * 4 + 3] = (v >> 24) & 0xff;
    }

    struct i2c_msg msg = { .addr = 0x72, .flags = 0, .len = sizeof(buf), .buf = buf };
    struct i2c_rdwr_ioctl_data data = { .msgs = &msg, .nmsgs = 1 };

    if (ioctl(fd, I2C_RDWR, &data) < 0) {
        LV_LOG_ERROR("TEST292 i2c: ioctl fallo");
    } else {
        LV_LOG_USER("TEST292 i2c: pack 216B escrito (modo=%i)", (int) x6200_control_get(x6200_vfoa_mode));
    }

    close(fd);
}

void radio_init(radio_state_change_t tx_cb, radio_state_change_t rx_cb) {
    /* La politica visual arranca en QUALITY: el modo Ahorro del waterfall solo
     * cambia si el pack lo pide (UI-08 incr. 2). Despues se aplica el override
     * persistido, si lo hay (UI-08 incr. 5). */
    visual_policy_init(&visual_policy);
    apply_visual_override_at_boot();

    /* El GPIO se inicializa antes que esto (gpio_safe_prepare, en main), así que acá no
     * se vuelve a pedir. Y el reintento del control tiene tope: antes era un bucle
     * infinito en el hilo principal, así que si la BASE no respondía la GUI no dibujaba
     * nunca. Ahora, si no aparece, se sigue sin radio y se avisa. */
    for (int intento = 0; intento < 30; intento++) {
        if (x6200_control_init()) {
            break;
        }

        if (intento == 29) {
            LV_LOG_ERROR("La BASE no respondió: la GUI sigue sin control de la radio");
            return;
        }

        usleep(500000);
    }

    if (!x6200_flow_init())
        return;

    gpio_safe_set(x6200_pin_morse_key, 1);     /* Morse key off */

    notify_tx = tx_cb;
    notify_rx = rx_cb;

    pack = malloc(sizeof(x6200_flow_t));

    subject_add_observer_and_call(cfg_cur.band->vfo_a.freq.val, on_vfo_freq_change, (void*)X6200_VFO_A);
    subject_add_observer_and_call(cfg_cur.band->vfo_b.freq.val, on_vfo_freq_change, (void*)X6200_VFO_B);

    subject_add_observer_and_call(cfg_cur.band->vfo_a.mode.val, on_vfo_mode_change, (void*)X6200_VFO_A);
    /* #244: el modo del VFO B no se manda al arrancar; solo con split. */
    subject_add_observer(cfg_cur.band->vfo_b.mode.val, on_vfo_mode_change, (void*)X6200_VFO_B);

    subject_add_observer_and_call(cfg_cur.band->vfo_a.agc.val, on_vfo_agc_change, (void*)X6200_VFO_A);
    subject_add_observer_and_call(cfg_cur.band->vfo_b.agc.val, on_vfo_agc_change, (void*)X6200_VFO_B);

    subject_add_observer_and_call(cfg_cur.band->vfo_a.att.val, on_vfo_att_change, (void*)X6200_VFO_A);
    subject_add_observer_and_call(cfg_cur.band->vfo_b.att.val, on_vfo_att_change, (void*)X6200_VFO_B);

    subject_add_observer_and_call(cfg_cur.band->vfo_a.pre.val, on_vfo_pre_change, (void*)X6200_VFO_A);
    subject_add_observer_and_call(cfg_cur.band->vfo_b.pre.val, on_vfo_pre_change, (void*)X6200_VFO_B);

    subject_add_observer_and_call(cfg_cur.band->vfo.val, on_change_uint32, x6200_control_vfo_set);
    subject_add_observer_and_call(cfg_cur.band->split.val, on_change_uint8, x6200_control_split_set);
    subject_add_observer(cfg_cur.band->split.val, on_split_mode_sync, NULL);
    subject_add_observer_and_call(cfg_cur.band->rfg.val, on_change_uint8, x6200_control_rfg_set);

    subject_add_observer_and_call(cfg_cur.filter.low, on_low_filter_change, NULL);
    subject_add_observer_and_call(cfg_cur.filter.high, on_high_filter_change, NULL);

    /* Sin and_call, por lo mismo que el squelch (#166): el observer toma radio_lock y el valor
     * persistido se aplica al final del init, con el mutex ya creado (#244). */
    subject_add_observer(cfg.vol.val, on_volume_change, NULL);
    // subject_add_observer_and_call(cfg.sql.val, on_change_uint8, x6200_control_sql_set);
    // subject_add_observer_and_call(cfg.sql_fm.val, on_change_uint8, x6200_control_sql_fm_set);
    /* Sin and_call: el valor persistido se aplica al final del init, cuando el
     * mutex del control ya existe (el observer toma radio_lock). Ver #166. */
    subject_add_observer(cfg_cur.sql_level, on_change_cur_sql_level, NULL);
    /* #244: al salir de CW/CWR se re-manda el nivel de audio (ver on_cur_mode_change_vol). */
    subject_add_observer(cfg_cur.mode, on_cur_mode_change_vol, NULL);
    subject_add_observer_and_call(cfg.pwr.val, on_change_float, x6200_control_txpwr_set);
    subject_add_observer_and_call(cfg.fft_dec.val, on_change_uint8, x6200_control_fft_dec_set);
    subject_add_observer_and_call(cfg.key_tone.val, on_change_uint16, x6200_control_key_tone_set);
    subject_add_observer_and_call(cfg.atu_enabled.val, on_change_uint8, x6200_control_atu_set);
    subject_add_observer_and_call(cfg_cur.atu->network, on_atu_network_change, NULL);

    subject_add_observer_and_call(cfg.key_speed.val, on_change_uint8, x6200_control_key_speed_set);
    subject_add_observer_and_call(cfg.key_mode.val, on_change_uint8, x6200_control_key_mode_set);
    subject_add_observer_and_call(cfg.iambic_mode.val, on_change_uint8, x6200_control_iambic_mode_set);
    subject_add_observer_and_call(cfg.key_vol.val, on_change_uint16, x6200_control_key_vol_set);
    subject_add_observer_and_call(cfg.key_train.val, on_change_uint8, x6200_control_key_train_set);
    subject_add_observer_and_call(cfg.qsk_time.val, on_change_uint16, x6200_control_qsk_time_set);
    subject_add_observer_and_call(cfg.key_ratio.val, on_change_float, x6200_control_key_ratio_set);

    subject_add_observer_and_call(cfg.agc_hang.val, on_change_uint8, x6200_control_agc_hang_set);
    subject_add_observer_and_call(cfg.agc_knee.val, on_change_int8, x6200_control_agc_knee_set);
    subject_add_observer_and_call(cfg.agc_slope.val, on_change_uint8, x6200_control_agc_slope_set);

    subject_add_observer_and_call(cfg.comp.val, on_change_bool, x6200_control_comp_set);

    subject_add_observer_and_call(cfg.dnf.val, on_change_uint8, x6200_control_dnf_set);
    subject_add_observer_and_call(cfg.dnf_center.val, on_change_uint16, x6200_control_dnf_center_set);
    subject_add_observer_and_call(cfg.dnf_width.val, on_change_uint16, x6200_control_dnf_width_set);
    subject_add_observer_and_call(cfg.nb.val, on_change_uint8, x6200_control_nb_set);
    subject_add_observer_and_call(cfg.nb_level.val, on_change_uint8, x6200_control_nb_level_set);
    subject_add_observer_and_call(cfg.nb_width.val, on_change_uint8, x6200_control_nb_width_set);
    subject_add_observer_and_call(cfg.nr.val, on_change_uint8, x6200_control_nr_set);
    subject_add_observer_and_call(cfg.nr_level.val, on_change_uint8, x6200_control_nr_level_set);

    subject_add_observer_and_call(cfg.eq.rx.en.val, on_change_bool, x6200_control_rx_eq_set);
    subject_add_observer_and_call(cfg.eq.rx.p1.val, on_change_int8, x6200_control_rx_eq_p1_set);
    subject_add_observer_and_call(cfg.eq.rx.p2.val, on_change_int8, x6200_control_rx_eq_p2_set);
    subject_add_observer_and_call(cfg.eq.rx.p3.val, on_change_int8, x6200_control_rx_eq_p3_set);
    subject_add_observer_and_call(cfg.eq.rx.p4.val, on_change_int8, x6200_control_rx_eq_p4_set);
    subject_add_observer_and_call(cfg.eq.rx.p5.val, on_change_int8, x6200_control_rx_eq_p5_set);

    subject_add_observer_and_call(cfg.eq.rx_wfm.en.val, on_change_bool, x6200_control_rx_eq_wfm_set);
    subject_add_observer_and_call(cfg.eq.rx_wfm.p1.val, on_change_int8, x6200_control_rx_eq_wfm_p1_set);
    subject_add_observer_and_call(cfg.eq.rx_wfm.p2.val, on_change_int8, x6200_control_rx_eq_wfm_p2_set);
    subject_add_observer_and_call(cfg.eq.rx_wfm.p3.val, on_change_int8, x6200_control_rx_eq_wfm_p3_set);
    subject_add_observer_and_call(cfg.eq.rx_wfm.p4.val, on_change_int8, x6200_control_rx_eq_wfm_p4_set);
    subject_add_observer_and_call(cfg.eq.rx_wfm.p5.val, on_change_int8, x6200_control_rx_eq_wfm_p5_set);

    subject_add_observer_and_call(cfg.eq.mic.en.val, on_change_bool, x6200_control_mic_eq_set);
    subject_add_observer_and_call(cfg.eq.mic.p1.val, on_change_int8, x6200_control_mic_eq_p1_set);
    subject_add_observer_and_call(cfg.eq.mic.p2.val, on_change_int8, x6200_control_mic_eq_p2_set);
    subject_add_observer_and_call(cfg.eq.mic.p3.val, on_change_int8, x6200_control_mic_eq_p3_set);
    subject_add_observer_and_call(cfg.eq.mic.p4.val, on_change_int8, x6200_control_mic_eq_p4_set);
    subject_add_observer_and_call(cfg.eq.mic.p5.val, on_change_int8, x6200_control_mic_eq_p5_set);

    x6200_control_charger_set(params.charger == RADIO_CHARGER_ON);
    x6200_control_bias_drive_set(params.bias_drive);
    x6200_control_bias_final_set(params.bias_final);

    x6200_control_mic_set(params.mic);
    x6200_control_hmic_set(params.hmic);
    x6200_control_imic_set(params.imic);
    /* #244: el ruteo de audio del MCU (record/mic) puede quedar latcheado en modo
     * record si una reproduccion no alcanzo a restaurarlo (crash/cierre durante el
     * audio). Restaurarlo en cada arranque de la GUI devuelve el audio de RX sin
     * power-cycle (antes solo lo sacaba un corte de energia). */
    x6200_control_record_set(false);

    radio_i2c_write_pack();
    x6200_control_spmode_set(params.spmode.x);

    x6200_control_vox_set(params.vox);
    x6200_control_vox_ag_set(params.vox_ag);
    x6200_control_vox_delay_set(params.vox_delay);
    x6200_control_vox_gain_set(params.vox_gain);

    x6200_control_rit_set(params.rit);
    x6200_control_xit_set(params.xit);

    x6200_control_linein_set(params.line_in);
    x6200_control_lineout_set(params.line_out);
    x6200_control_monitor_level_set(params.moni);

    // x6200_control_comp_set(false);
    // x6200_control_comp_level_set(x6200_comp_1_8);

    prev_time = get_time();
    idle_time = prev_time;

    pthread_mutex_init(&control_mux, NULL);
    radio_control_ok = true;

    /* El squelch del DSP arranca segun el nivel persistido (issue #166): 0 deja
     * el squelch abierto (enable off) y >0 lo habilita. Va aca, despues de los
     * demas setters del registro compartido (spmode/mic) y con el mutex ya
     * inicializado, para que el ultimo valor escrito sea el del squelch. */
    radio_lock();
    sql_policy_apply(subject_get_int(cfg_cur.mode), subject_get_int(cfg_cur.sql_level));
    radio_unlock();

    /* Nivel de audio de recepcion con el estado persistido (#244): es el mismo write que antes
     * hacia el and_call del observer de cfg.vol.val, pero ahora va despues de crear el mutex
     * (el observer toma radio_lock) y pasa por la funcion que compone los mutes. */
    radio_vol_apply();

    pthread_t thread;

    pthread_create(&thread, NULL, radio_thread, NULL);
    pthread_detach(thread);
}

radio_state_t radio_get_state() {
    return state;
}

void radio_set_freq(int32_t freq) {
    if (!radio_check_freq(freq)) {
        LV_LOG_ERROR("Freq %i incorrect", freq);
        return;
    }
    x6200_vfo_t vfo = subject_get_int(cfg_cur.band->vfo.val);
    int32_t shift = cfg_transverter_get_shift(freq);
    WITH_RADIO_LOCK(x6200_control_vfo_freq_set(vfo, freq - shift));
}

bool radio_check_freq(int32_t freq) {
    if (freq >= 500000 && freq <= 55000000) {
        return true;
    }
    return cfg_transverter_get_shift(freq) != 0;
}

/* Nivel de audio de recepcion que va al registro 14 del BASE (issue #244).
 *
 * Es la UNICA funcion que decide ese valor, y lo hace a partir del estado y no de una
 * transicion: `mute` (el del usuario) y el mute por auriculares A2DP se componen, y el
 * nivel guardado es el que manda cuando ninguno de los dos esta activo. Antes el valor
 * se calculaba con un toggle (`mute = !mute`) y la perilla limpiaba `mute` sin volver a
 * mandar el registro cuando el nivel no podia moverse (tope 0/100), asi que la GUI podia
 * quedar creyendo que NO estaba muda con el equipo en 0 (silencio sin causa visible).
 * El X6100 de referencia ya lo hace asi: radio_vol_refresh() en .x6100_ref/src/radio.c.
 *
 * Cualquier camino que cambie `mute`, el mute de BT o cfg.vol.val tiene que terminar en
 * radio_vol_apply(); es barato (un write de 6 bytes por I2C). */
static uint16_t radio_vol_value(void) {
    if (mute || btaudio_speaker_muted()) {
        return 0;
    }

    return (uint16_t) limit(subject_get_int(cfg.vol.val), VOL_MIN, VOL_MAX);
}

void radio_vol_apply(void) {
    WITH_RADIO_LOCK(x6200_control_rxvol_set(radio_vol_value()));
}

static void on_volume_change(Subject *subj, void *user_data) {
    (void) subj;
    (void) user_data;

    radio_vol_apply();
}

uint16_t radio_change_vol(int16_t df) {
    int32_t vol = subject_get_int(cfg.vol.val);
    if (df == 0) {
        return vol;
    }

    mute = false;

    uint16_t new_val = limit(vol + df, VOL_MIN, VOL_MAX);

    if (new_val != vol) {
        subject_set_int(cfg.vol.val, new_val);

        /* Con auriculares A2DP conectados la perilla tambien mueve el volumen de los auriculares
         * (el sink de PulseAudio, via btaudio-ctl volume): si no, no habia forma de cambiarlo
         * (docs/HALLAZGOS.md 23). */
        btaudio_headphone_volume(new_val);
    } else {
        /* El nivel no se movio (tope del rango) pero el mute del usuario si se limpio: hay que
         * re-mandar el registro igual o el equipo queda en 0 y la GUI dice que no esta muda
         * (issue #244). */
        radio_vol_apply();
    };

    return new_val;
}

bool radio_is_muted() {
    return mute;
}

void radio_change_mute() {
    mute = !mute;
    radio_vol_apply();
}

/* #244: ir a CW y volver deja el audio de RX mudo hasta un power-cycle (medido por el
 * dueño dos veces el 22/10; el estado del BASE no lo revierte ningun otro camino). Lo
 * unico que la GUI tiene con formato verificado es el write del registro 14 —el que usa
 * la perilla y que se sabe que cambia el nivel—, asi que al SALIR de CW/CWR se re-manda
 * el nivel. Es una mitigacion barata (un write de 6 bytes) y NO un arreglo probado: si el
 * BASE quedo en un estado mas profundo que el registro de volumen, esto no lo saca (ver
 * docs/HALLAZGOS.md 64 y el anexo A). No toca el mute del usuario ni el de BT porque el
 * valor sale de radio_vol_value(). */
static void on_cur_mode_change_vol(Subject *subj, void *user_data) {
    static x6200_mode_t prev;
    static bool         prev_set = false;

    (void) user_data;

    x6200_mode_t mode = subject_get_int(subj);
    bool de_cw  = prev_set && (prev == x6200_mode_cw || prev == x6200_mode_cwr);
    bool a_cw   = (mode == x6200_mode_cw || mode == x6200_mode_cwr);

    prev     = mode;
    prev_set = true;

    if (de_cw && !a_cw) {
        LV_LOG_USER("Radio vol re-sync al salir de CW (#244)");
        radio_vol_apply();
    }
}

uint16_t radio_change_moni(int16_t df) {
    if (df == 0) {
        return params.moni;
    }

    int16_t new_val = limit(params.moni + df, 0, 100);
    if (new_val != params.moni) {
        params_lock();
        params.moni = new_val;
        params_unlock(&params.dirty.moni);
        WITH_RADIO_LOCK(x6200_control_monitor_level_set(params.moni));
        lv_msg_send(MSG_PARAM_CHANGED, NULL);
    }

    return params.moni;
}

bool radio_change_spmode(int16_t df) {
    if (df == 0) {
        return params.spmode.x;
    }

    params_bool_set(&params.spmode, df > 0);
    lv_msg_send(MSG_PARAM_CHANGED, NULL);

    WITH_RADIO_LOCK(x6200_control_spmode_set(params.spmode.x));

    return params.spmode.x;
}

/* Interlock de CB (#146 seccion 7.2, #160): true si el software puede transmitir
 * en la frecuencia actual con la politica cb_tx_scope. Trabaja por VENTANA de
 * frecuencia, no exige canal exacto. Fuera de 26-28 MHz siempre permite, asi que
 * las bandas de aficionado siguen iguales. */
static bool radio_tx_allowed(void) {
    return cb_tx_allowed(subject_get_int(cfg_cur.fg_freq), params.cb_tx_scope.x);
}

void radio_start_atu() {
    if (state == RADIO_RX) {
        /* El tune del ATU transmite a potencia plena (portadora de sintonia):
         * es un camino de TX que el software controla, asi que pasa por el mismo
         * interlock que el PTT (issue #160). Fuera de 26-28 MHz no cambia nada.
         * El barrido de ROE (radio_start_swrscan) queda afuera a proposito: es
         * un diagnostico a 0,1 W y el unico camino de TX que el owner necesita
         * en CB (ver el #160). */
        if (!radio_tx_allowed()) {
            msg_update_text_fmt("TX blocked: not a CB channel");
            voice_say_text_fmt("TX blocked, not a C B channel");
            return;
        }
        state = RADIO_ATU_START;
    }
}

/* El barrido de ROE fuerza LSB (es una portadora) y transmite al PISO de potencia del equipo
 * (issue #113: "la potencia de transmision durante el barrido debe ser la minima posible,
 * simulando el nanovna"; SWRSCAN_TX_POWER_DW = 0.1 W, 14.8 dB menos que los 3 W que ponia antes):
 * al pararlo hay que devolver el equipo como estaba. La potencia se restaura desde cfg.pwr.val;
 * el modo hay que guardarlo al arrancar (issue #84: sin esto la radio quedaba en LSB).
 *
 * La potencia se pone aca y no en el dialogo a proposito: tiene que estar en el piso ANTES de
 * habilitar el barrido (x6200_control_swrscan_set(true) es lo que arranca el TX) y las dos
 * escrituras van juntas bajo el lock de radio. */
static int32_t  swrscan_prev_mode;
static bool     swrscan_mode_saved = false;

bool radio_start_swrscan() {
    if (state != RADIO_RX) {
        return false;
    }

    swrscan_prev_mode  = subject_get_int(cfg_cur.mode);
    swrscan_mode_saved = true;

    subject_set_int(cfg_cur.mode, x6200_mode_lsb);
    radio_lock();
    x6200_control_txpwr_set(SWRSCAN_TX_POWER_W);
    x6200_control_swrscan_set(true);
    radio_unlock();
    state = RADIO_SWRSCAN;

    return true;
}

void radio_stop_swrscan() {
    if (state == RADIO_SWRSCAN) {
        state = RADIO_RX;
        radio_lock();
        x6200_control_swrscan_set(false);
        x6200_control_txpwr_set(subject_get_float(cfg.pwr.val));
        radio_unlock();

        /* El cambio de modo dispara observadores que toman el lock de radio, asi que va afuera
         * del radio_lock (mismo orden que el set de LSB al arrancar). */
        if (swrscan_mode_saved) {
            subject_set_int(cfg_cur.mode, swrscan_prev_mode);
            swrscan_mode_saved = false;
        }
    }
}

/* Snapshot del estado del EQUIPO (dial, modo, agc/pre/att y potencia) para las apps que lo mutan
 * al abrirse: al cerrarlas hay que devolverlo como estaba (issue #94, la familia del barrido de
 * ROE del #84/#96). Es el mismo patron de radio_start_swrscan() pero generico: lo guarda y lo
 * devuelve el dispatcher de apps, asi ninguna app depende de su propio dtor ni del slot SQLite de
 * memorias (que se usa para las memorias del usuario). La frecuencia y el modo se mueven por los
 * sujetos de cfg_cur, que son los que hablan con el hardware a traves de sus observadores
 * (on_vfo_freq_change / on_vfo_mode_change); el cambio de modo va fuera del radio_lock porque
 * dispara observadores que toman ese lock. */
typedef struct {
    int32_t freq;
    int32_t mode;
    int32_t agc;
    int32_t pre;
    int32_t att;
    float   pwr;
    bool    valid;
} radio_state_backup_t;

static radio_state_backup_t radio_backup;

void radio_state_save() {
    radio_backup.freq  = subject_get_int(cfg_cur.fg_freq);
    radio_backup.mode  = subject_get_int(cfg_cur.mode);
    radio_backup.agc   = subject_get_int(cfg_cur.agc);
    radio_backup.pre   = subject_get_int(cfg_cur.pre);
    radio_backup.att   = subject_get_int(cfg_cur.att);
    radio_backup.pwr   = subject_get_float(cfg.pwr.val);
    radio_backup.valid = true;
}

void radio_state_restore() {
    if (!radio_backup.valid) {
        return;
    }
    radio_backup.valid = false;

    /* Fuera del radio_lock: setear estos sujetos dispara observadores que toman el lock */
    subject_set_int(cfg_cur.fg_freq, radio_backup.freq);
    subject_set_int(cfg_cur.mode, radio_backup.mode);
    subject_set_int(cfg_cur.agc, radio_backup.agc);
    subject_set_int(cfg_cur.pre, radio_backup.pre);
    subject_set_int(cfg_cur.att, radio_backup.att);
    subject_set_float(cfg.pwr.val, radio_backup.pwr);
}

void radio_set_pwr(float d) {
    WITH_RADIO_LOCK(x6200_control_txpwr_set(d));
}

/* --- CB 27 MHz (issue #146): entrada/salida de la banda ---------------------
 *
 * Dentro de CB el split se apaga (para que el PTT no termine en otro VFO/banda),
 * el modo cae a AM si no es AM/SAM/NFM, la potencia se recorta a params.cb_pwr_max
 * y el XIT se fuerza a 0 (mueve el TX +-1,5 kHz y saca del canal sin que el
 * operador lo vea). Al salir se restauran el split y la potencia que habia antes.
 *
 * El split de la banda anterior se cachea mientras se esta FUERA de CB: el
 * observer de cfg.band_id de src/cfg/band.c corre antes que el nuestro (se
 * registro primero) y ya reemplazo el split por el de la banda nueva cuando
 * llega este callback. La potencia es global (cfg.pwr), asi que se guarda al
 * entrar.
 *
 * Sin timers ni polling: solo observers de subjects, que es el mecanismo que ya
 * usa la pantalla principal. */

#define CB_BAND_FIRST 201
#define CB_BAND_LAST  204

static bool  cb_active;        /* la banda actual es un grid CB */
static bool  cb_boot_done;     /* ya paso el arranque: el control de la radio existe */
static int   cb_saved_split;   /* split de la ultima banda fuera de CB */
static float cb_saved_pwr;     /* potencia de la banda anterior, para restaurar */
static int32_t cb_last_band_id; /* banda vista por ultimo, para detectar el cambio en curso */

static bool cb_in_band(void) {
    int32_t id = subject_get_int(cfg.band_id.val);

    return id >= CB_BAND_FIRST && id <= CB_BAND_LAST;
}

static float cb_pwr_limit(void) {
    /* Un valor no positivo (base editada a mano) cae al default conservador. */
    return params.cb_pwr_max.x > 0.0f ? params.cb_pwr_max.x : 4.0f;
}

static void cb_split_cache_cb(Subject *subj, void *user_data) {
    /* Mientras cfg/band.c carga la banda nueva, el subject del split cambia al
     * valor de la banda DESTINO. Ese cambio no es del usuario y no tiene que
     * pisar el split que hay que restaurar. Se reconoce porque band.c ya movio
     * el pk de la banda (cfg_cur.band->vfo.pk) al destino, mientras que
     * cb_last_band_id sigue siendo la banda que este modulo vio por ultima vez;
     * en un cambio del usuario (o al arrancar) los dos coinciden. */
    if (!cb_active && cfg_cur.band->vfo.pk == cb_last_band_id) {
        cb_saved_split = subject_get_int(subj);
    }
}

/* Tope de potencia mientras se esta en CB. Este observer se registra ANTES que
 * el de radio_init() que manda cfg.pwr al control, asi que el recorte ocurre
 * antes de que la potencia llegue al equipo (no hay overshoot). */
static void cb_pwr_limit_cb(Subject *subj, void *user_data) {
    if (!cb_active) {
        return;
    }

    float max = cb_pwr_limit();

    if (subject_get_float(subj) > max + 0.001f) {
        subject_set_float(cfg.pwr.val, max);
        msg_update_text_fmt("Power limited to %.1f W in CB", max);
        voice_say_text_fmt("Power limited to %i watts", (int) (max + 0.5f));
    }
}

static void cb_apply_entry(bool save_state) {
    cb_active = true;

    if (save_state) {
        cb_saved_pwr = subject_get_float(cfg.pwr.val);
    }

    /* Split off: se restaura al salir. */
    subject_set_int(cfg_cur.band->split.val, 0);

    /* Modo: AM salvo que ya sea AM/SAM/NFM (un FM elegido por el usuario se
     * respeta). Al salir, la banda destino recarga el suyo. */
    x6200_mode_t mode = subject_get_int(cfg_cur.mode);

    if (mode != x6200_mode_am && mode != x6200_mode_sam && mode != x6200_mode_nfm) {
        subject_set_int(cfg_cur.mode, x6200_mode_am);
    }

    /* Tope de potencia. */
    float max = cb_pwr_limit();

    if (subject_get_float(cfg.pwr.val) > max) {
        subject_set_float(cfg.pwr.val, max);
    }

    /* XIT a 0. En el arranque la radio todavia no esta inicializada (radio_init()
     * corre despues): se deja el parametro en 0 y radio_init() lo manda al
     * equipo. En un cambio de banda en runtime se manda aca mismo. */
    if (params.xit != 0) {
        params_lock();
        params.xit = 0;
        params_unlock(&params.dirty.xit);
        lv_msg_send(MSG_PARAM_CHANGED, NULL);
    }

    if (cb_boot_done && radio_control_ok) {
        WITH_RADIO_LOCK(x6200_control_xit_set(0));
    }
}

static void cb_leave(void) {
    cb_active = false;

    subject_set_float(cfg.pwr.val, cb_saved_pwr);
    subject_set_int(cfg_cur.band->split.val, cb_saved_split);
}

static void cb_band_cb(Subject *subj, void *user_data) {
    int32_t id = subject_get_int(cfg.band_id.val);
    bool    in = cb_in_band();

    if (in) {
        /* Cada cambio a un grid CB aplica la politica de la banda (el modo y el
         * split que acaba de cargar cfg/band.c pueden ser los guardados de ese
         * grid). De A a B no se vuelve a guardar la potencia: ya esta recortada
         * y hay que restaurar la de ANTES de entrar a CB, no la recortada. */
        cb_apply_entry(!cb_active);
    } else if (cb_active) {
        cb_leave();
    }

    cb_last_band_id = id;

    /* Fuera de CB, el split vigente es el de esta banda: es el que hay que
     * devolver si se entra a CB desde aca. */
    if (!in) {
        cb_saved_split = subject_get_int(cfg_cur.band->split.val);
    }
}

void cb_hooks_init(void) {
    /* Ultimo split conocido de una banda fuera de CB (al arrancar, el de la banda
     * guardada). */
    cb_last_band_id = subject_get_int(cfg.band_id.val);
    cb_saved_split = subject_get_int(cfg_cur.band->split.val);

    subject_add_observer(cfg_cur.band->split.val, cb_split_cache_cb, NULL);
    subject_add_observer(cfg.pwr.val, cb_pwr_limit_cb, NULL);
    subject_add_observer(cfg.band_id.val, cb_band_cb, NULL);

    /* En el arranque el observer de cfg.band_id no dispara: si la ultima banda
     * guardada es CB, se aplica la entrada una vez. */
    if (cb_in_band()) {
        cb_apply_entry(true);
    }

    cb_boot_done = true;
}

x6200_mic_sel_t radio_change_mic(int16_t d) {
    if (d == 0) {
        return params.mic;
    }

    params_lock();

    switch (params.mic) {
        case x6200_mic_builtin:
            params.mic = d > 0 ? x6200_mic_handle : x6200_mic_auto;
            break;

        case x6200_mic_handle:
            params.mic = d > 0 ? x6200_mic_auto : x6200_mic_builtin;
            break;

        case x6200_mic_auto:
            params.mic = d > 0 ? x6200_mic_builtin : x6200_mic_handle;
            break;
    }

    params_unlock(&params.dirty.mic);
    lv_msg_send(MSG_PARAM_CHANGED, NULL);

    WITH_RADIO_LOCK(x6200_control_mic_set(params.mic));

    return params.mic;
}

uint8_t radio_change_hmic(int16_t d) {
    if (d == 0) {
        return params.hmic;
    }
    int32_t new_val = limit(params.hmic + d, 0, 30);
    CHANGE_PARAM(new_val, params.hmic, params.dirty.hmic, x6200_control_hmic_set);

    return params.hmic;
}

uint8_t radio_change_imic(int16_t d) {
    if (d == 0) {
        return params.imic;
    }

    int32_t new_val = limit(params.imic + d, 0, 30);
    CHANGE_PARAM(new_val, params.imic, params.dirty.imic, x6200_control_imic_set);

    return params.imic;
}

x6200_vfo_t radio_toggle_vfo() {
    x6200_vfo_t new_vfo = (subject_get_int(cfg_cur.band->vfo.val) == X6200_VFO_A) ? X6200_VFO_B : X6200_VFO_A;

    subject_set_int(cfg_cur.band->vfo.val, new_vfo);
    // TODO: move to another file
    voice_say_text_fmt("V F O %s", (new_vfo == X6200_VFO_A) ? "A" : "B");

    return new_vfo;
}

void radio_set_charger(radio_charger_t v) {
    WITH_RADIO_LOCK(x6200_control_charger_set(v == RADIO_CHARGER_ON));
}

void radio_poweroff() {
    /* Apagado por software = salida limpia: el proximo arranque no tiene que
     * reportar LASTBREATH (issue #121). */
    lastbreath_mark_clean_now();

    if (params.charger == RADIO_CHARGER_SHADOW) {
        WITH_RADIO_LOCK(x6200_control_charger_set(true));
    }

    state = RADIO_POWEROFF;
}

radio_charger_t radio_change_charger(int16_t d) {
    if (d == 0) {
        return params.charger;
    }

    params_lock();

    switch (params.charger) {
        case RADIO_CHARGER_OFF:
            params.charger = d > 0 ? RADIO_CHARGER_ON : RADIO_CHARGER_SHADOW;
            break;

        case RADIO_CHARGER_ON:
            params.charger = d > 0 ? RADIO_CHARGER_SHADOW : RADIO_CHARGER_OFF;
            break;

        case RADIO_CHARGER_SHADOW:
            params.charger = d > 0 ? RADIO_CHARGER_OFF : RADIO_CHARGER_ON;
            break;
    }

    params_unlock(&params.dirty.charger);
    lv_msg_send(MSG_PARAM_CHANGED, NULL);

    WITH_RADIO_LOCK(x6200_control_charger_set(params.charger == RADIO_CHARGER_ON));

    return params.charger;
}

void radio_set_ptt(bool tx) {
    if (tx && !radio_tx_allowed()) {
        msg_update_text_fmt("TX blocked: not a CB channel");
        voice_say_text_fmt("TX blocked, not a C B channel");

        /* Nunca se deja el PTT prendido: se apaga explicito. */
        WITH_RADIO_LOCK(x6200_control_ptt_set(false));
        return;
    }

    WITH_RADIO_LOCK(x6200_control_ptt_set(tx));
}

/* Accepted for the ported JS8 app; see the note in radio.h */
void radio_set_tx_filter(uint16_t low, uint16_t high) {
    (void) low;
    (void) high;
}

void radio_set_rx_dsp_off(bool off) {
    (void) off;
}

void radio_speaker_play(bool on) {
    (void) on;
}

/* Digital modes: modem mode while a frame is transmitted */
void radio_set_modem(bool tx) {
    /* El modo modem es lo que keyea el TX de SSTV/WeFax/JS8 (tx_player), otro
     * camino de software que no pasa por radio_set_ptt(): mismo interlock
     * (issue #160). Apagarlo siempre pasa. Fuera de 26-28 MHz no cambia nada. */
    if (tx && !radio_tx_allowed()) {
        msg_update_text_fmt("TX blocked: not a CB channel");
        voice_say_text_fmt("TX blocked, not a C B channel");
        return;
    }

    WITH_RADIO_LOCK(x6200_control_modem_set(tx));
}

int16_t radio_change_rit(int16_t d) {
    if (d == 0) {
        return params.rit;
    }

    int16_t new_val = limit(align_int(params.rit + d * 10, 10), -1500, +1500);
    if (new_val != params.rit) {
        params_lock();
        params.rit = new_val;
        params_unlock(&params.dirty.rit);
        lv_msg_send(MSG_PARAM_CHANGED, NULL);
        WITH_RADIO_LOCK(x6200_control_rit_set(params.rit));
    }

    return params.rit;
}

int16_t radio_change_xit(int16_t d) {
    /* En CB el XIT esta bloqueado (issue #146, seccion 5.5): mueve el TX
     * +-1,5 kHz y saca del canal sin que el operador lo vea. El RIT sigue
     * disponible. */
    if (cb_in_band()) {
        if (params.xit != 0) {
            params_lock();
            params.xit = 0;
            params_unlock(&params.dirty.xit);
            lv_msg_send(MSG_PARAM_CHANGED, NULL);
            if (radio_control_ok) {
                WITH_RADIO_LOCK(x6200_control_xit_set(0));
            }
        }
        return 0;
    }

    if (d == 0) {
        return params.xit;
    }

    int16_t new_val = limit(align_int(params.xit + d * 10, 10), -1500, +1500);
    if (new_val != params.xit) {
        params_lock();
        params.xit = new_val;
        params_unlock(&params.dirty.xit);
        lv_msg_send(MSG_PARAM_CHANGED, NULL);
        WITH_RADIO_LOCK(x6200_control_xit_set(params.xit));
    }

    return params.xit;
}

void radio_set_line_in(uint8_t d) {
    CHANGE_PARAM(d, params.line_in, params.dirty.line_in, x6200_control_linein_set);
}

void radio_set_line_out(uint8_t d) {
    CHANGE_PARAM(d, params.line_out, params.dirty.line_out, x6200_control_lineout_set);
}

void radio_set_morse_key(bool on) {
    /* El keyer CW cierra este GPIO SIN pasar por radio_set_ptt(), que es donde
     * vivia el interlock de CB (#160): cw_send transmitia fuera del SBC. Se
     * evalua solo al CERRAR la llave (soltarla siempre pasa: nunca se deja el
     * keyer pegado). Es un choke point de seguridad ademas del aviso que da el
     * keyer en cw_encoder.c; por eso aca no repite mensaje. */
    if (on && !radio_tx_allowed()) {
        return;
    }

    gpio_safe_set(x6200_pin_morse_key, on ? 0 : 1);
}
