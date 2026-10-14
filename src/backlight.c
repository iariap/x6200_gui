/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <aether_radio/x6200_control/low/gpio.h>

#include "lvgl/lvgl.h"
#include "backlight.h"
#include "gpio_safe.h"
#include "util.h"
#include "voice.h"
#include "visual_policy.h"

static int          power;
static int          brightness;
static bool         on = true;

static lv_timer_t   *timer = NULL;

static void backlight_timer(lv_timer_t *t) {
    /* Nivel de reposo (timeout idle): pasa por el cap vigente del perfil
     * (UI-08 incr. 4); el setting de params queda intacto. */
    visual_policy_brightness_rewrite(params.brightness_idle);
    gpio_safe_set(x6200_pin_light, params.brightness_buttons == BUTTONS_LIGHT ? 1 : 0);
    timer = NULL;
}

static void set_brightness(int16_t value) {
    if (brightness > 0) {
        char    str[8];
        int     len = snprintf(str, sizeof(str), "%i\n", 10 - value);

        ssize_t n = write(brightness, str, len);
    }
}

static void set_power(bool value) {
    if (power > 0) {
        char    str[8];
        int     len = snprintf(str, sizeof(str), "%i\n", value ? 0 : 1);

        ssize_t n = write(power, str, len);
    }
}

void backlight_init() {
    power = open("/sys/class/backlight/backlight/bl_power", O_WRONLY);
    brightness = open("/sys/class/backlight/backlight/brightness", O_WRONLY);
    on = true;

    backlight_tick();
}

void backlight_tick() {
    if (timer) {
        lv_timer_set_period(timer, params.brightness_timeout * 1000);
        lv_timer_reset(timer);
    } else {
        timer = lv_timer_create(backlight_timer, params.brightness_timeout * 1000, NULL);
        lv_timer_set_repeat_count(timer, 1);

        /* Nivel normal (rewrite por input): pasa por el cap vigente del perfil
         * (UI-08 incr. 4) para que el recorte del perfil no se pierda en el
         * proximo input; el setting de params queda intacto. */
        visual_policy_brightness_rewrite(params.brightness_normal);
        gpio_safe_set(x6200_pin_light, params.brightness_buttons == BUTTONS_DARK ? 0 : 1);
    }
}

void backlight_set_brightness(int16_t value) {
    if (on) {
        if (value < 0) {
            set_power(false);

            /* Setting max PWM for reduce noice */
            set_brightness(9);
        } else {
            set_brightness(value);
            set_power(true);
        }
    }
}

void backlight_set_buttons(buttons_light_t value) {
    params_lock();
    params.brightness_buttons = value;
    params_unlock(&params.dirty.brightness_buttons);

    gpio_safe_set(x6200_pin_light, value == BUTTONS_DARK ? 0 : 1);
}

void backlight_switch() {
    if (on) {
        set_power(false);
        set_brightness(9);
        gpio_safe_set(x6200_pin_light, 0);

        on = false;
        voice_say_text_fmt("Display off");
        lv_disp_enable_invalidation(lv_disp_get_default(), false);
    } else {
        lv_disp_enable_invalidation(lv_disp_get_default(), true);
        set_power(true);
        set_brightness(params.brightness_normal);
        gpio_safe_set(x6200_pin_light, params.brightness_buttons == BUTTONS_DARK ? 0 : 1);

        voice_say_text_fmt("Display on");
        on = true;
    }
}

bool backlight_is_on() {
    return on;
}
