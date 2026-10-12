/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "keyboard.h"

#include "pubsub_ids.h"
#include "usb_devices.h"

#include "kbd_rollover.h"
#include "lv_drivers/indev/evdev.h"
#include "lv_drivers/indev/xkb.h"

#include <linux/input.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <glob.h>


lv_group_t *keyboard_group;

static lv_indev_drv_t       indev_drv_2;
static bool                 ready = false;
static kbd_rollover_t       rollover;
static dev_t                kbd_rdev;  /* the keyboard opened: a new one (unplugged and back) opens again */
static ino_t                kbd_ino;
static lv_timer_t          *rescan_timer;
static int                  rescans_left;

/* After a USB device comes or goes, look for the keyboard this often, this many times: udev's
 * "add" comes before the keyboard's /dev/input node exists (randal007, x6100-js8 beta 4.5) */
#define KBD_RESCAN_MS    500
#define KBD_RESCAN_TIMES 10

extern int evdev_fd; /* lv_drivers/indev/evdev.c, set by evdev_set_file() */

/* The next key event from the USB keyboard, through xkb (layout, Shift). */
static bool kbd_next(void *ctx, uint16_t *scancode, uint32_t *key, int *value) {
    (void)ctx;
    struct input_event in;
    while (read(evdev_fd, &in, sizeof(in)) > 0) {
        if (in.type != EV_KEY) continue;
        *scancode = in.code;
        *value    = in.value;
        *key      = xkb_process_key(in.code, in.value); /* also keeps Shift state */
        return true;
    }
    return false;
}

static void kbd_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    (void)drv;
    kbd_rollover_read(&rollover, kbd_next, NULL, data);
}

/* A device with the keys of a keyboard (letters, space, enter): the radio's
 * own buttons and knobs have none of these */
static bool has_letters(const char *path) {
    unsigned long bits[KEY_MAX / (8 * sizeof(long)) + 1] = {0};
    int           fd = open(path, O_RDONLY | O_NONBLOCK);
    bool          ok;

    if (fd < 0) {
        return false;
    }
    ok = ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), bits) >= 0;
    close(fd);
    for (int k = KEY_Q; ok && k <= KEY_P; k++) {    /* the top letter row */
        ok = bits[k / (8 * sizeof(long))] & (1UL << (k % (8 * sizeof(long))));
    }
    return ok && (bits[KEY_SPACE / (8 * sizeof(long))] & (1UL << (KEY_SPACE % (8 * sizeof(long)))));
}

/* USB keyboards by path; Bluetooth ones have no path link: by their keys */
static char* search_kbd_device() {
    static char path[256];   /* a by-path link behind a hub is longer than 64 */
    glob_t      globbuf;
    bool        found = false;

    if (glob("/dev/input/by-path/*-kbd", 0, NULL, &globbuf) == 0) {
        snprintf(path, sizeof(path), "%s", globbuf.gl_pathv[0]);
        found = true;
    }
    globfree(&globbuf);
    if (!found && glob("/dev/input/event*", 0, NULL, &globbuf) == 0) {
        for (size_t i = 0; i < globbuf.gl_pathc && !found; i++) {
            if (has_letters(globbuf.gl_pathv[i])) {
                snprintf(path, sizeof(path), "%s", globbuf.gl_pathv[i]);
                found = true;
            }
        }
        globfree(&globbuf);
    }
    return found ? path : NULL;
}

static void setup_kbd(char *path) {
    struct stat st;

    if (stat(path, &st) != 0 || !evdev_set_file(path)) {
        return;
    }
    kbd_rdev = st.st_rdev;
    kbd_ino  = st.st_ino;
    rollover = (kbd_rollover_t){0};

    if (indev_drv_2.type == LV_INDEV_TYPE_NONE) {
        lv_indev_drv_init(&indev_drv_2);
        indev_drv_2.type = LV_INDEV_TYPE_KEYPAD;
        indev_drv_2.read_cb = kbd_read;

        lv_indev_t *keyboard_indev = lv_indev_drv_register(&indev_drv_2);

        lv_indev_set_group(keyboard_indev, keyboard_group);
    }

    ready = true;
}

/* A keyboard there and not open (or a different one: unplugged and back): open it; none: not ready */
static void check_kbd(void) {
    char       *dev_path = search_kbd_device();
    struct stat st;

    if (dev_path && stat(dev_path, &st) == 0) {
        if (!ready || st.st_rdev != kbd_rdev || st.st_ino != kbd_ino) setup_kbd(dev_path);
        return;
    }
    ready = false;
}

/* All the checks, also once one finds it: an unplugged keyboard's link can outlive the "remove" */
static void rescan_cb(lv_timer_t *t) {
    check_kbd();
    if (--rescans_left <= 0) {
        lv_timer_del(t);
        rescan_timer = NULL;
    }
}

static void on_usb_device_change(void * s, lv_msg_t * msg) {
    check_kbd();
    rescans_left = KBD_RESCAN_TIMES;
    if (!rescan_timer) rescan_timer = lv_timer_create(rescan_cb, KBD_RESCAN_MS, NULL);
}

void keyboard_init() {
    keyboard_group = lv_group_create();
    /* The MFK stops at the first and last item of a page instead of jumping round */
    lv_group_set_wrap(keyboard_group, false);

    lv_msg_subscribe(MSG_USB_DEVICE_CHANGED, on_usb_device_change, NULL);

    char *path = search_kbd_device();
    if (!path)
        return;

    setup_kbd(path);
}

bool keyboard_ready() {
    return ready;
}
