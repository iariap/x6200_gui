/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Bluetooth device list parsing (see bt_devices.h).
 */

#include "bt_devices.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FIELDS  7

static void copy(char *dst, size_t size, const char *src) {
    snprintf(dst, size, "%s", src ? src : "");
}

/* Split a tab separated line keeping the empty fields (a device with no name
 * yet is normal: bluetoothctl only sets the Alias once it knows the device). */
static int split(const char *line, char *buf, size_t size, char **fields, int max) {
    size_t len = strlen(line);
    int    n   = 0;

    if (len >= size) {
        len = size - 1;
    }

    memcpy(buf, line, len);
    buf[len] = '\0';

    fields[n++] = buf;

    for (size_t i = 0; i < len && n < max; i++) {
        if (buf[i] == '\t') {
            buf[i]        = '\0';
            fields[n++]   = buf + i + 1;
        }
    }

    return n;
}

int bt_devices_parse(const char *text, bt_list_t *out) {
    memset(out, 0, sizeof(*out));
    out->hp_volume = -1;

    const char *p = text;

    while (*p) {
        const char *eol = strchr(p, '\n');
        size_t      len = eol ? (size_t) (eol - p) : strlen(p);
        char        line[256];

        if (len >= sizeof(line)) {
            len = sizeof(line) - 1;
        }

        memcpy(line, p, len);
        line[len] = '\0';
        p         = eol ? eol + 1 : p + strlen(p);

        char  buf[256];
        char *f[MAX_FIELDS] = {0};
        int   n             = split(line, buf, sizeof(buf), f, MAX_FIELDS);

        if (n < 1) {
            continue;
        }

        if (!strcmp(f[0], "HP") && n >= 3) {
            out->hp        = f[1][0] == '1';
            out->hp_volume = atoi(f[2]);
        } else if (!strcmp(f[0], "RX") && n >= 2) {
            out->rx = !strcmp(f[1], "on");
        } else if (!strcmp(f[0], "MUTE") && n >= 2) {
            out->mute = !strcmp(f[1], "on");
        } else if ((!strcmp(f[0], "DEV") && n >= 6) || (!strcmp(f[0], "NEW") && n >= 4)) {
            if (out->n >= BT_MAX_DEV) {
                continue;
            }

            bt_device_t *dev = &out->dev[out->n];

            memset(dev, 0, sizeof(*dev));
            copy(dev->mac, sizeof(dev->mac), f[1]);

            if (!strcmp(f[0], "DEV")) {
                dev->connected   = f[2][0] == '1';
                copy(dev->role, sizeof(dev->role), f[3]);
                dev->autoconnect = f[4][0] == '1';
                copy(dev->name, sizeof(dev->name), f[5]);
                dev->paired      = true;
            } else {
                copy(dev->name, sizeof(dev->name), f[2]);
                copy(dev->role, sizeof(dev->role), f[3]);
                dev->paired = false;
            }

            out->n++;
        }
    }

    return out->n;
}

const char *bt_device_state(const bt_device_t *d) {
    if (!d->paired) {
        return "new";
    }

    if (d->connected) {
        return "connected";
    }

    return d->autoconnect ? "not connected, auto" : "not connected";
}
