/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>

#include "rotary.h"
#include "keyboard.h"
#include "backlight.h"
#include "adaptive_tuning.h"

#include <poll.h>
#include <pthread.h>
#include <time.h>

static int32_t remain_diff = 0;
static lv_indev_state_t prev_state;


static void rotary_input_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    struct input_event  in;
    rotary_t            *rotary = (rotary_t*) drv->user_data;
    int32_t             diff = 0;
    bool                send = false;

    if (remain_diff == 0) {
        while (read(rotary->fd, &in, sizeof(struct input_event)) > 0) {
            if (in.type == EV_REL) {
                diff += in.value;
                send = true;
                
                // Adaptive tuning: record event with timestamp
                uint32_t now = lv_tick_get();
                adaptive_tuning_add_event(&g_adaptive_tuning, in.value, now);
            }
        }

        if (send) {
            backlight_tick();
            
            // Update adaptive tuning calculations
            adaptive_tuning_update(&g_adaptive_tuning);

            if (rotary->left[0] == 0 && rotary->right[0] == 0) {
                // Send enhanced rotary event with multiplier info
                rotary_event_enhanced_t *enhanced = (rotary_event_enhanced_t *)malloc(sizeof(rotary_event_enhanced_t));
                enhanced->raw_diff = diff;
                enhanced->multiplier = g_adaptive_tuning.current_multiplier;
                // effective_step will be calculated by receiver based on current freq step
                enhanced->effective_step = 0; 
                lv_event_send(lv_scr_act(), EVENT_ROTARY_ENHANCED, (void *)enhanced);
            } else {
                data->continue_reading = 1;
                remain_diff = diff;
            }
        }
    } else if (prev_state == LV_INDEV_STATE_PRESSED) {
        data->state = LV_INDEV_STATE_RELEASED;
        if (remain_diff != 0) {
            data->continue_reading = 1;
        }
    } else {
        if (remain_diff > 0) {
            diff = 1;
        } else {
            diff = -1;
        }
        remain_diff -= diff;
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = diff > 0 ? rotary->left[rotary->mode] : rotary->right[rotary->mode];
        data->continue_reading = 1;

    }
    prev_state = data->state;

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
    rotary->fd = fd;

    lv_indev_drv_init(&rotary->indev_drv);

    rotary->indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    rotary->indev_drv.read_cb = rotary_input_read;
    rotary->indev_drv.user_data = rotary;

    rotary->indev = lv_indev_drv_register(&rotary->indev_drv);

    lv_indev_set_group(rotary->indev, keyboard_group);

    return rotary;
}

/* --- The main dial, in its own thread ------------------------------------
 *
 * Their fix (upstream ae2af22): if the input is read from the GUI loop, the time
 * between detents includes whatever the loop was doing, so the speed -and with it
 * the adaptive tuning- is wrong when the GUI is busy. This thread timestamps each
 * detent and queues it; the GUI takes everything queued as one event per frame, the
 * detents summed and the time per detent averaged over them, and a change of
 * direction ends the batch. */

#define MAIN_QUEUE  16

static struct {
    int             fd;
    rotary_data_t   q[MAIN_QUEUE];
    uint8_t         r, w;
    pthread_mutex_t mux;
    lv_indev_drv_t  indev_drv;
} main_knob = { .mux = PTHREAD_MUTEX_INITIALIZER };

static void * main_knob_thread(void *arg) {
    struct pollfd      pfd = { .fd = main_knob.fd, .events = POLLIN };
    struct input_event in;
    struct timespec    prev = {0, 0}, now;

    while (true) {
        int16_t diff = 0;

        if (poll(&pfd, 1, -1) <= 0) {
            continue;
        }

        while (read(main_knob.fd, &in, sizeof(in)) > 0) {
            if (in.type == EV_REL) {
                diff += in.value;
            }
        }

        if (!diff) {
            continue;
        }

        clock_gettime(CLOCK_MONOTONIC, &now);

        float ms = (now.tv_sec - prev.tv_sec) * 1000.0f + (now.tv_nsec - prev.tv_nsec) * 1e-6f;

        prev = now;
        ms   = LV_MIN(ms, 500.0f) / LV_ABS(diff);

        pthread_mutex_lock(&main_knob.mux);

        if ((main_knob.w + 1) % MAIN_QUEUE != main_knob.r) {    /* full: dropped */
            main_knob.q[main_knob.w] = (rotary_data_t) { .diff = diff, .dt = LV_MAX((uint16_t) ms, 1) };
            main_knob.w = (main_knob.w + 1) % MAIN_QUEUE;
        }

        pthread_mutex_unlock(&main_knob.mux);
    }

    return NULL;
}

static void main_knob_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    int32_t  diff = 0;
    uint32_t n = 0, ms = 0;

    pthread_mutex_lock(&main_knob.mux);

    while (main_knob.r != main_knob.w) {
        rotary_data_t *d = &main_knob.q[main_knob.r];

        if (diff && (d->diff > 0) != (diff > 0)) {
            data->continue_reading = 1;
            break;
        }

        diff += d->diff;
        n    += LV_ABS(d->diff);
        ms   += d->dt * LV_ABS(d->diff);
        main_knob.r = (main_knob.r + 1) % MAIN_QUEUE;
    }

    pthread_mutex_unlock(&main_knob.mux);

    if (diff) {
        uint32_t dt = LV_MAX(ms / n, 1);

        backlight_tick();

        /* Adaptive tuning wants one entry per detent, as it had when the input was
         * read here: the batch is unpacked, dated backwards with the time per detent
         * the thread measured, so the speed is the real one */
        uint32_t now   = lv_tick_get();
        int16_t  step  = diff > 0 ? 1 : -1;
        int16_t  ticks = LV_ABS(diff);

        for (int16_t k = 0; k < ticks; k++) {
            adaptive_tuning_add_event(&g_adaptive_tuning, step,
                                      now - (uint32_t) dt * (uint32_t) (ticks - 1 - k));
        }

        adaptive_tuning_update(&g_adaptive_tuning);

        rotary_event_enhanced_t *enhanced = malloc(sizeof(rotary_event_enhanced_t));

        enhanced->raw_diff       = diff;
        enhanced->multiplier     = g_adaptive_tuning.current_multiplier;
        enhanced->effective_step = 0;
        lv_event_send(lv_scr_act(), EVENT_ROTARY_ENHANCED, (void *) enhanced);
    }
}

void rotary_main_init(char *dev_name) {
    pthread_t thread;

    main_knob.fd = open(dev_name, O_RDWR | O_NOCTTY | O_NDELAY);

    if (main_knob.fd == -1) {
        LV_LOG_ERROR("unable to open main knob");
        return;
    }

    fcntl(main_knob.fd, F_SETFL, O_NONBLOCK);

    pthread_create(&thread, NULL, main_knob_thread, NULL);
    pthread_detach(thread);

    lv_indev_drv_init(&main_knob.indev_drv);
    main_knob.indev_drv.type    = LV_INDEV_TYPE_KEYPAD;
    main_knob.indev_drv.read_cb = main_knob_read;
    lv_indev_set_group(lv_indev_drv_register(&main_knob.indev_drv), keyboard_group);
}
