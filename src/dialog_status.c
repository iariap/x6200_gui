/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - System status.
 *
 *  One place to see what the radio thinks it has: the Bluetooth adapter and how it
 *  is attached, the TX audio level, the DATA partition, wfview, the web server and
 *  the update check. It exists because those are the things that cannot be checked
 *  without the hardware: here they can at least be read.
 */

#include "dialog_status.h"

#include "buttons.h"
#include "fonts/font_select.h"
#include "keyboard.h"
#include "main.h"
#include "styles.h"
#include "theme.h"
#include "tx_player.h"
#include "wifi.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <sys/utsname.h>
#include <unistd.h>

#define STATUS_ROWS     8

#define VF(n) (theme_shape.vision ? font_down(&sony_##n, VISION_FONT_STEPS - 1) : &sony_##n)

static dialog_t  dialog;
static lv_obj_t *rows[STATUS_ROWS];
static int       n_rows;

/* key=value (the quotes the shell configs use are optional) */
static bool conf_get(const char *path, const char *key, char *out, size_t size) {
    FILE *f = fopen(path, "r");

    out[0] = '\0';

    if (!f) {
        return false;
    }

    size_t klen = strlen(key);
    char   line[256];

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            const char *v = line + klen + 1;

            if (*v == '"') {
                v++;
            }

            snprintf(out, size, "%s", v);
            out[strcspn(out, "\"\r\n")] = '\0';
            fclose(f);

            return true;
        }
    }

    fclose(f);

    return false;
}

static void add_row(const char *fmt, ...) {
    if (n_rows >= STATUS_ROWS) {
        return;
    }

    char text[160];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);

    lv_obj_t *l = lv_label_create(dialog.obj);

    lv_obj_set_style_text_font(l, VF(20), 0);
    lv_obj_set_style_text_color(l, TC(text), 0);
    lv_label_set_recolor(l, true);
    lv_label_set_text(l, text);
    lv_obj_set_pos(l, 24, 62 + n_rows * 30);

    rows[n_rows++] = l;
}

/* How the Bluetooth adapter is attached: whatever hci module is loaded */
static const char *bt_transport(void) {
    FILE *f = fopen("/proc/modules", "r");

    if (!f) {
        return "unknown";
    }

    char line[256];
    const char *found = "not loaded";

    while (fgets(line, sizeof(line), f)) {
        if (!strncmp(line, "btusb ", 6)) {
            found = "USB";
            break;
        }

        if (!strncmp(line, "hci_uart ", 9)) {
            found = "UART";
            break;
        }

        if (!strncmp(line, "hci_vhci ", 9)) {
            found = "virtual";
            break;
        }
    }

    fclose(f);

    return found;
}

static void construct_cb(lv_obj_t *parent) {
    dialog.obj = dialog_init(parent);
    n_rows     = 0;

    lv_obj_t *title = lv_label_create(dialog.obj);

    lv_obj_set_style_text_font(title, VF(28), 0);
    lv_obj_set_style_text_color(title, TC(text_head), 0);
    lv_label_set_text(title, "Status");
    lv_obj_set_pos(title, 24, 12);

    /* Firmware */
    add_row("#%06x GUI# %s", theme_hex(TC_text_dim), VERSION);

    char rel[64] = "?";

    FILE *f = fopen("/proc/sys/kernel/osrelease", "r");

    if (f) {
        if (fgets(rel, sizeof(rel), f)) {
            rel[strcspn(rel, "\n")] = '\0';
        }

        fclose(f);
    }

    add_row("#%06x Kernel# %s", theme_hex(TC_text_dim), rel);

    /* Bluetooth: present, how it is attached, and whether the pairings are saved */
    bool bt = access("/sys/class/bluetooth/hci0", F_OK) == 0;

    add_row("#%06x Bluetooth# %s (%s)", theme_hex(TC_text_dim),
            bt ? "adapter present" : "no adapter", bt ? bt_transport() : "-");

    struct stat st;

    if (stat("/mnt/backups/bluetooth.tgz", &st) == 0) {
        char when[32];
        struct tm tm;

        localtime_r(&st.st_mtime, &tm);
        strftime(when, sizeof(when), "%Y-%m-%d %H:%M", &tm);
        add_row("#%06x Pairings# saved %s", theme_hex(TC_text_dim), when);
    } else {
        add_row("#%06x Pairings# none saved yet", theme_hex(TC_text_dim));
    }

    /* TX audio level: the value the digital modes start from */
    add_row("#%06x TX level# %.1f dB (digital modes)", theme_hex(TC_text_dim),
            tx_player_base_gain_offset());

    /* DATA partition */
    struct statvfs vfs;

    if (statvfs("/mnt", &vfs) == 0) {
        unsigned long free_mb  = (unsigned long) ((double) vfs.f_bavail * vfs.f_frsize / 1048576.0);
        unsigned long total_mb = (unsigned long) ((double) vfs.f_blocks * vfs.f_frsize / 1048576.0);

        add_row("#%06x /mnt# %lu MB free of %lu MB", theme_hex(TC_text_dim), free_mb, total_mb);
    } else {
        add_row("#%06x /mnt# not mounted", theme_hex(TC_text_dim));
    }

    /* wfview */
    char enabled[16];
    bool wf = conf_get("/mnt/wfview.conf", "enabled", enabled, sizeof(enabled)) &&
              (!strcmp(enabled, "on") || !strcmp(enabled, "true") || !strcmp(enabled, "1") ||
               !strcmp(enabled, "yes"));

    add_row("#%06x wfview# %s (port 50001)", theme_hex(TC_text_dim), wf ? "on" : "off");

    /* Web configurator */
    char *ip = NULL;
    char *gw = NULL;

    if (wifi_get_ipaddr(&ip, &gw) && ip) {
        add_row("#%06x Web# http://%s/ %s", theme_hex(TC_text_dim), ip,
                access("/mnt/https/cert.pem", F_OK) == 0 ? "+ HTTPS" : "");
    } else {
        add_row("#%06x Web# no IP address", theme_hex(TC_text_dim));
    }

    /* Update check */
    char src[96];

    if (conf_get("/mnt/update_check.conf", "enabled", enabled, sizeof(enabled))) {
        add_row("#%06x Update# %s (check-source)", theme_hex(TC_text_dim),
                strcmp(enabled, "off") ? "on" : "off");
    } else {
        add_row("#%06x Update# on (default)", theme_hex(TC_text_dim));
    }

    (void) src;
}

static void destruct_cb(void) {
    n_rows = 0;
}

static void key_cb(lv_event_t *e) {
    if (lv_event_get_key(e) == LV_KEY_ESC) {
        dialog_destruct();
    }
}

static void close_cb(button_data_t *data) {
    (void) data;

    dialog_destruct();
}

static button_data_t btn_close = { .type = BTN_TEXT, .label = "Close", .press = close_cb };

static buttons_page_t btn_page = {
    .items = {NULL, NULL, NULL, NULL, &btn_close},
};

static dialog_t dialog = {
    .run          = false,
    .construct_cb = construct_cb,
    .destruct_cb  = destruct_cb,
    .key_cb       = key_cb,
    .btn_page     = &btn_page,
};

dialog_t *dialog_status = &dialog;
