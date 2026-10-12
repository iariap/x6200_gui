/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "lvgl/lvgl.h"
#include "input_dev.h"
#include "gpio_safe.h"
#include "lv_drivers/display/fbdev.h"
#include "lv_drivers/display/drm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

#include "main.h"
#include "main_screen.h"
#include "styles.h"
#include "theme.h"
#include "remote_server.h"
#include "fonts/font_select.h"
#include "radio.h"
#include "dsp.h"
#include "util.h"
#include "keyboard.h"
#include "spectrum.h"
#include "waterfall.h"
#include "keypad.h"
#include "params/params.h"
#include "audio.h"
#include "audio_enhancement.h"
#include "update_check.h"
#include "screen_rec.h"
#include "wfview_server.h"
#include "msg.h"
#include "cw.h"
#include "pannel.h"
#include "cat.h"
#include "rtty.h"
#include "backlight.h"
#include "events.h"
#include "mfk.h"
#include "vol.h"
#include "qso_log.h"
#include "scheduler.h"
#include "wifi.h"
#include "usb_devices.h"

#define DISP_BUF_SIZE (800 * 480 * 4)

rotary_t                    *vol;
encoder_t                   *mfk;
rotary_t                    *mfk_inner;

static lv_color_t           buf[DISP_BUF_SIZE];
static lv_disp_draw_buf_t   disp_buf;
static lv_disp_drv_t        disp_drv;

void * tick_thread (void *args);

/* Runs on the update-check thread; msg_schedule_* queues it for the UI loop. */
static void update_check_notify(const char *version) {
    msg_schedule_text_fmt("Firmware %s available", version);
}

int main(void) {
    lv_init();
    // lv_png_init();

    /* The kernel has DRM (CONFIG_DRM_SUN4I plus the panel driver) and lv_drivers
     * brings the backend, but an unverified display engine can leave the screen
     * black: DRM is used only when it is asked for, by creating /mnt/use_drm or
     * with X6200_DRM=1. Otherwise fbdev, as always. docs/NEW_FEATURES.md §32. */
    bool use_drm = access("/mnt/use_drm", F_OK) == 0;

    if (!use_drm) {
        const char *env = getenv("X6200_DRM");

        use_drm = env && env[0] && strcmp(env, "0") != 0;
    }

    if (use_drm && drm_init()) {
        LV_LOG_USER("Display: DRM/KMS");
        screen_rec_out = drm_flush;             /* the recorder keeps capturing */
        fclose(fopen("/tmp/x6200_gui.drm", "w"));
    } else {
        unlink("/tmp/x6200_gui.drm");
        LV_LOG_USER("Display: fbdev");
        fbdev_init();
    }
    audio_init();
    event_init();
    usb_devices_monitor_init();

    lv_disp_draw_buf_init(&disp_buf, buf, NULL, DISP_BUF_SIZE);
    lv_disp_drv_init(&disp_drv);

    disp_drv.draw_buf   = &disp_buf;
    /* screen_rec_flush copies the frame while recording and then draws it
     * through screen_rec_out (fbdev_flush by default) */
    disp_drv.flush_cb   = screen_rec_flush;
    disp_drv.hor_res    = 480;
    disp_drv.ver_res    = 800;
    disp_drv.sw_rotate  = 1;
    disp_drv.rotated    = LV_DISP_ROT_90;

    lv_disp_drv_register(&disp_drv);

    lv_disp_set_bg_color(lv_disp_get_default(), lv_color_black());
    lv_disp_set_bg_opa(lv_disp_get_default(), LV_OPA_COVER);

    keyboard_init();

    keypad_init("/dev/input/event0");
    keypad_init("/dev/input/event4");

    /* El device tree nombra los cuatro encoders rotary0..rotary3 y esos nombres son
     * estables, pero el número de eventN depende del orden de sondeo (un USB que se
     * enumere antes los corre). Se buscan por nombre y, si no aparecen, se usa la ruta de
     * siempre. rotary0 = perilla principal, rotary1 = volumen, rotary2 = MFK,
     * rotary3 = MFK interno (los dos últimos son los de steps-per-period = 2). */
    rotary_main_init(input_dev_by_name("rotary0", "/dev/input/event1"));

    vol       = rotary_init(input_dev_by_name("rotary1", "/dev/input/event2"));
    mfk       = encoder_init(input_dev_by_name("rotary2", "/dev/input/event3"));
    mfk_inner = rotary_init(input_dev_by_name("rotary3", "/dev/input/event4"));

    /* Los tres pueden devolver NULL: rotary_init() y encoder_init() lo hacen si no
     * pueden abrir el dispositivo (y el equipo puede numerarlos distinto). Antes se los
     * desreferenciaba igual, y la GUI se moría acá mismo, antes de params_init y sin
     * haber dibujado nunca: era el logo eterno en la pantalla del equipo. Sin perilla se
     * sigue andando; las teclas del panel siguen funcionando. */
    if (vol != NULL) {
        vol->left[ROT_VOL_EDIT_MODE] = KEY_VOL_LEFT_EDIT;
        vol->right[ROT_VOL_EDIT_MODE] = KEY_VOL_RIGHT_EDIT;

        vol->left[ROT_VOL_SELECT_MODE] = KEY_VOL_LEFT_SELECT;
        vol->right[ROT_VOL_SELECT_MODE] = KEY_VOL_RIGHT_SELECT;
    } else {
        LV_LOG_ERROR("Sin encoder de volumen (/dev/input/event2): se sigue sin el");
    }

    if (mfk_inner != NULL) {
        // Usual navigation - left -> up key
        mfk_inner->left[ROT_MFK_INNER_DEFAULT_MODE] = LV_KEY_UP;
        mfk_inner->right[ROT_MFK_INNER_DEFAULT_MODE] = LV_KEY_DOWN;

        // EQ navigation - left -> down key (decrease)
        mfk_inner->left[ROT_MFK_INNER_INVERSE_MODE] = LV_KEY_DOWN;
        mfk_inner->right[ROT_MFK_INNER_INVERSE_MODE] = LV_KEY_UP;
    } else {
        LV_LOG_ERROR("Sin encoder interno del MFK (/dev/input/event4): se sigue sin el");
    }

    params_init();
    audio_enh_preset_load((audio_enh_preset_t) params.audio_enh_preset.x);
    audio_set_play_vol(params.play_gain_db_f.x);
    audio_set_rec_vol(params.rec_gain_db_f.x);
    mfk_change_mode(0);
    vol_change_mode(0);
    /* Bundled themes to the DATA partition once; one already there is never
     * overwritten (same as the X6100) */
    if (system("mkdir -p /mnt/themes; for t in /usr/share/x6200/themes/*; do "
               "[ -e \"/mnt/themes/${t##*/}\" ] || cp -r \"$t\" /mnt/themes/; done") != 0) {
        LV_LOG_WARN("Bundled themes not copied to /mnt/themes");
    }

    theme_init(params.theme_name.x);    /* an unknown one falls back to IU1WXY */
    fonts_apply(theme_shape.font >= 0 ? theme_shape.font : (params.font.x <= 3 ? params.font.x : 0));

    if (theme_shape.vision) {
        fonts_scale(VISION_FONT_STEPS);
    }

    styles_init(params.theme.x);

    dsp_init();
    lv_obj_t *main_obj = main_screen();
    waterfall_set_battery_mode(params.waterfall_battery.x);

    cw_init();
    rtty_init();
    /* Antes de todo lo que toca el GPIO (backlight, wifi, CAT): si esto falla, los usos
     * se saltean en lugar de desreferenciar NULL y matar la GUI. */
    gpio_safe_prepare();

    radio_init(
        &main_screen_notify_tx,
        &main_screen_notify_rx
    );
    wifi_power_setup();
    backlight_init();
    cat_init();
    // pannel_visible();
    if (!qso_log_init()) {
        LV_LOG_ERROR("Can't init QSO log");
    }
    qso_log_import_adif("/mnt/incoming_log.adi");

    pthread_t thread;
    pthread_create(&thread, NULL, tick_thread, NULL);
    pthread_detach(thread);

#if 0
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_0, 0);
    lv_scr_load_anim(main_obj, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
#else
    lv_scr_load(main_obj);
#endif

    /* New release alert: waits for the WiFi in its own thread and then posts a
     * message if the published version is newer than VERSION. */
    update_check_start(VERSION, update_check_notify);

    /* Icom LAN (wfview) server: no-op unless enabled in /mnt/wfview.conf */
    wfview_server_init();

    /* Web remote: control socket for the /remote page (UNIX socket) */
    remote_server_init();

    int64_t next_loop_time, sleep_time, loop_start_time;
    while (1) {
        loop_start_time = get_time();
        observer_delayed_notify_all();
        event_obj_check();
        scheduler_work();
        next_loop_time = lv_timer_handler() + loop_start_time;
        sleep_time = next_loop_time - get_time();
        if (sleep_time > 0) {
            usleep(sleep_time * 1000);
        }
    }
    return 0;
}

void * tick_thread (void *args)
{
      while(1) {
        usleep(5 * 1000);    /*Sleep for 5 millisecond*/
        lv_tick_inc(5);      /*Tell LVGL that 5 milliseconds were elapsed*/
    }
}
