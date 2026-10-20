/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <sys/ioctl.h>

#include "rotary.h"
#include "keyboard.h"
#include "backlight.h"
#include "adaptive_tuning.h"
#include "params/params.h"

#include <poll.h>
#include <pthread.h>
#include <time.h>

/* -------------------------------------------------------------------------
 * VOL / MFK / MFK interno: teclas, estado por instancia (issue #186)
 *
 * Este camino NO alimenta el motor adaptativo ni manda EVENT_ROTARY_ENHANCED:
 * el dial principal tiene su propia cola y su propio motor. Los estados de
 * tecla/remanente viven en rotary_t, no en `static` de archivo, asi que un
 * encoder no consume ni altera los del otro.
 * ------------------------------------------------------------------------- */

static void rotary_input_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    struct input_event  in;
    rotary_t            *rotary = (rotary_t*) drv->user_data;
    int32_t             diff = 0;
    bool                send = false;

    /* Inicializacion EXPLICITA de la salida (antes dependia de lo que dejara la
     * lectura anterior en el struct de LVGL). */
    data->state             = LV_INDEV_STATE_RELEASED;
    data->key               = 0;
    data->continue_reading  = 0;

    if (rotary->remain_diff == 0) {
        while (read(rotary->fd, &in, sizeof(struct input_event)) > 0) {
            if (in.type == EV_REL) {
                diff += in.value;
                send = true;
            }
        }

        if (send) {
            backlight_tick();

            if (rotary->left[0] != 0 || rotary->right[0] != 0) {
                data->continue_reading = 1;
                rotary->remain_diff    = diff;
            }
            /* Sin teclas configuradas no hay nada que emitir: este encoder no
             * mueve el VFO (ni comparte aceleracion con el dial). */
        }
    } else if (rotary->prev_state == LV_INDEV_STATE_PRESSED) {
        data->state = LV_INDEV_STATE_RELEASED;
        if (rotary->remain_diff != 0) {
            data->continue_reading = 1;
        }
    } else {
        if (rotary->remain_diff > 0) {
            diff = 1;
        } else {
            diff = -1;
        }
        rotary->remain_diff -= diff;
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = diff > 0 ? rotary->left[rotary->mode] : rotary->right[rotary->mode];
        data->continue_reading = 1;
    }

    rotary->prev_state = data->state;
}

rotary_t * rotary_init(char *dev_name) {
    int fd = open(dev_name, O_RDWR | O_NOCTTY | O_NDELAY);

    if (fd == -1) {
        LV_LOG_ERROR("unable to open rotary interface:");

        return NULL;
    }

    fcntl(fd, F_SETFL, O_ASYNC | O_NONBLOCK);

    rotary_t *rotary = malloc(sizeof(rotary_t));

    memset(rotary, 0, sizeof(rotary_t));
    rotary->fd         = fd;
    rotary->mode       = ROT_VOL_EDIT_MODE;
    rotary->remain_diff = 0;
    rotary->prev_state = LV_INDEV_STATE_RELEASED;

    lv_indev_drv_init(&rotary->indev_drv);

    rotary->indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    rotary->indev_drv.read_cb = rotary_input_read;
    rotary->indev_drv.user_data = rotary;

    rotary->indev = lv_indev_drv_register(&rotary->indev_drv);

    lv_indev_set_group(rotary->indev, keyboard_group);

    return rotary;
}

/* -------------------------------------------------------------------------
 * El dial principal, en su propio hilo (issue #186, seccion 3)
 *
 * El hilo lector SOLO lee eventos y los encola con SU timestamp del kernel
 * (struct input_event.time en microsegundos, CLOCK_MONOTONIC cuando el driver
 * lo acepta). No toca LVGL, ni Subjects, ni la radio, y no asigna memoria por
 * evento. La GUI drena la cola EN ORDEN; la velocidad y el multiplicador se
 * calculan por detent con el timestamp real, nunca con el tamano del lote ni
 * con lv_tick_get() reconstruido por frame.
 *
 * La cola tiene backpressure (512 entradas): si el GUI se atrasa, el lector
 * espera espacio en lugar de descartar. `diff == 0` se reserva como marca de
 * discontinuidad del reloj.
 * ------------------------------------------------------------------------- */

/* Estado compartido con el hilo lector: el struct de la cola (protegido por su
 * mutex) y el fd. El resto lo usa SOLO el hilo de la GUI. */
static struct {
    int                     fd;
    rotary_queue_t          q;
    lv_indev_drv_t          indev_drv;

    /* Solo hilo GUI */
    adaptive_tuning_state_t tuning;
    uint8_t                 last_profile;
    uint8_t                 last_threshold;
    bool                    reset_pending;
    uint32_t                stats_log_ms;
    uint32_t                stats_high_watermark;
    uint32_t                stats_full_waits;
} main_knob = {
    .last_profile   = 0xFF,
    .last_threshold = 0xFF,
};

/* Solo hilo lector */
static uint64_t knob_last_ts;
static bool     knob_have_ts;

void rotary_tuning_reset(void) {
    main_knob.reset_pending = true;
}

void rotary_tuning_queue_stats(uint32_t *high_watermark, uint32_t *queue_full_waits) {
    rotary_queue_stats(&main_knob.q, high_watermark, queue_full_waits);
}

static uint64_t monotonic_us(void) {
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0;
    }

    return (uint64_t) now.tv_sec * 1000000ULL + (uint64_t) now.tv_nsec / 1000ULL;
}

/* Timestamp del evento. Devuelve 0 si el kernel no entrego uno utilizable (el
 * hilo lector cae a clock_gettime). */
static uint64_t event_timestamp_us(const struct input_event *in) {
    uint64_t ts;

    if (in->time.tv_sec == 0 && in->time.tv_usec == 0) {
        return 0;
    }

    ts = (uint64_t) in->time.tv_sec * 1000000ULL + (uint64_t) in->time.tv_usec;

    return ts;
}

static void queue_event(int32_t diff, uint64_t timestamp_us) {
    /* Marca no creciente: se avisa la discontinuidad (diff 0) y se sigue con un
     * reloj monotono local, para no inventar velocidades con marcas repetidas. */
    if (knob_have_ts && timestamp_us <= knob_last_ts) {
        uint64_t fallback = monotonic_us();

        rotary_queue_push(&main_knob.q, 0, knob_last_ts);

        if (fallback > knob_last_ts) {
            timestamp_us = fallback;
        } else {
            timestamp_us = knob_last_ts + ADAPTIVE_MIN_DT_US;
        }
    }

    knob_last_ts = timestamp_us;
    knob_have_ts = true;

    rotary_queue_push(&main_knob.q, diff, timestamp_us);
}

static void * main_knob_thread(void *arg) {
    struct pollfd      pfd = { .fd = main_knob.fd, .events = POLLIN };
    struct input_event in;

    (void) arg;

    while (true) {
        if (poll(&pfd, 1, -1) <= 0) {
            continue;
        }

        while (read(main_knob.fd, &in, sizeof(in)) == (ssize_t) sizeof(in)) {
            /* Se acepta exactamente el mismo codigo REL que antes; EV_SYN y
             * EV_KEY no se reinterpretan. */
            if (in.type != EV_REL || in.value == 0) {
                continue;
            }

            uint64_t ts = event_timestamp_us(&in);

            if (ts == 0) {
                ts = monotonic_us();
            }

            queue_event(in.value, ts);
        }
    }

    return NULL;
}

/* Drenaje + motor + despacho de tramos. Corre en el hilo de la GUI. */
static void main_knob_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    static rotary_data_t    batch[ROTARY_QUEUE_SIZE];
    static int32_t          diffs[ROTARY_QUEUE_SIZE];
    static uint64_t         timestamps[ROTARY_QUEUE_SIZE];
    static rotary_event_enhanced_t segments[ROTARY_QUEUE_SIZE];

    (void) drv;

    data->state            = LV_INDEV_STATE_RELEASED;
    data->key              = 0;
    data->continue_reading = 0;

    size_t n = rotary_queue_drain(&main_knob.q, batch, ROTARY_QUEUE_SIZE);

    if (n == 0) {
        return;
    }

    backlight_tick();

    /* Configuracion vigente AL PROCESAR: si cambio el perfil o el umbral desde
     * el ultimo drenaje se reinicia UNA sola vez (el primer detent es x1). */
    uint8_t profile   = params.freq_accel.x;
    uint8_t threshold = params.freq_accel_threshold.x;

    if (profile != main_knob.last_profile || threshold != main_knob.last_threshold) {
        adaptive_tuning_reset(&main_knob.tuning);
        main_knob.last_profile   = profile;
        main_knob.last_threshold = threshold;
    }

    if (main_knob.reset_pending) {
        adaptive_tuning_reset(&main_knob.tuning);
        main_knob.reset_pending = false;
    }

    for (size_t i = 0; i < n; i++) {
        diffs[i]      = batch[i].diff;
        timestamps[i] = batch[i].timestamp_us;
    }

    size_t segments_n = adaptive_tuning_segments(&main_knob.tuning, diffs, timestamps, n,
                                                 (adaptive_accel_t) profile, threshold,
                                                 segments, ROTARY_QUEUE_SIZE);

    /* lv_event_send() es SINCRONICO: el payload vive en este stack y el receptor
     * no lo libera ni lo guarda. */
    for (size_t i = 0; i < segments_n; i++) {
        lv_event_send(lv_scr_act(), EVENT_ROTARY_ENHANCED, &segments[i]);
    }

    /* Diagnostico de la cola: solo si cambio y como maximo una linea por minuto
     * (nunca una por detent). */
    uint32_t now_ms = lv_tick_get();

    if ((uint32_t) (now_ms - main_knob.stats_log_ms) >= 60000u) {
        uint32_t high = 0, waits = 0;

        main_knob.stats_log_ms = now_ms;
        rotary_queue_stats(&main_knob.q, &high, &waits);

        if (high != main_knob.stats_high_watermark || waits != main_knob.stats_full_waits) {
            main_knob.stats_high_watermark = high;
            main_knob.stats_full_waits     = waits;
            LV_LOG_USER("rotary0 queue: high_watermark=%u full_waits=%u", high, waits);
        }
    }
}

void rotary_main_init(char *dev_name) {
    pthread_t thread;
    int       clock_id = CLOCK_MONOTONIC;

    main_knob.fd = open(dev_name, O_RDWR | O_NOCTTY | O_NDELAY);

    if (main_knob.fd == -1) {
        LV_LOG_ERROR("unable to open main knob");
        return;
    }

    fcntl(main_knob.fd, F_SETFL, O_NONBLOCK);

    /* Los timestamps de los EV_REL tienen que ser comparables entre si y con
     * CLOCK_MONOTONIC. Si el driver no lo acepta se usan los que entregue el
     * kernel y el productor cae a clock_gettime ante marcas no crecientes. */
    if (ioctl(main_knob.fd, EVIOCSCLOCKID, &clock_id) < 0) {
        LV_LOG_WARN("rotary0: EVIOCSCLOCKID(CLOCK_MONOTONIC) no disponible (%s); "
                    "se usan los timestamps del kernel con fallback monotono", strerror(errno));
    }

    rotary_queue_init(&main_knob.q);
    adaptive_tuning_init(&main_knob.tuning);
    main_knob.last_profile   = 0xFF;
    main_knob.last_threshold = 0xFF;
    main_knob.reset_pending  = false;
    main_knob.stats_log_ms   = lv_tick_get();

    knob_last_ts = 0;
    knob_have_ts = false;

    pthread_create(&thread, NULL, main_knob_thread, NULL);
    pthread_detach(thread);

    lv_indev_drv_init(&main_knob.indev_drv);
    main_knob.indev_drv.type    = LV_INDEV_TYPE_KEYPAD;
    main_knob.indev_drv.read_cb = main_knob_read;
    lv_indev_set_group(lv_indev_drv_register(&main_knob.indev_drv), keyboard_group);
}
