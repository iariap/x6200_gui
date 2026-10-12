/*
 * Unit tests: Bluetooth device list parsing (bt_devices.c).
 *
 * The dialog shows what btaudio-ctl prints. The parsing is pure C and must cope
 * with the shapes that script really produces: devices without a name yet, the
 * headphones/audio state lines, newly discovered devices, and an error message
 * from the script (which must not invent devices).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <string.h>

#include "bt_devices.h"
#include "test_framework.h"

/* A paired keyboard, a paired headset and the audio state */
static void test_list(void) {
    static const char *out =
        "DEV\tAA:BB:CC:DD:EE:01\t1\thp\t1\tWH-1000XM3\n"
        "DEV\tAA:BB:CC:DD:EE:02\t0\tkbd\t0\tKeyboard K380\n"
        "DEV\tAA:BB:CC:DD:EE:03\t0\tcat\t0\t\n"          /* no name yet */
        "HP\t1\t65\n"
        "RX\ton\n"
        "MUTE\ton\n";

    bt_list_t l;

    CHECK(bt_devices_parse(out, &l) == 3);
    CHECK(l.n == 3);

    CHECK(strcmp(l.dev[0].mac, "AA:BB:CC:DD:EE:01") == 0);
    CHECK(strcmp(l.dev[0].name, "WH-1000XM3") == 0);
    CHECK(strcmp(l.dev[0].role, "hp") == 0);
    CHECK(l.dev[0].connected);
    CHECK(l.dev[0].autoconnect);
    CHECK(l.dev[0].paired);

    CHECK(!l.dev[1].connected);
    CHECK(!l.dev[1].autoconnect);
    CHECK(strcmp(l.dev[1].role, "kbd") == 0);

    /* A device with no name is still a device: the row shows the MAC */
    CHECK(l.dev[2].name[0] == '\0');
    CHECK(strcmp(l.dev[2].mac, "AA:BB:CC:DD:EE:03") == 0);
    CHECK(l.dev[2].paired);

    CHECK(l.hp);
    CHECK(l.hp_volume == 65);
    CHECK(l.rx);
    CHECK(l.mute);
}

/* Newly discovered devices (from scan) are not paired yet */
static void test_scan(void) {
    static const char *out =
        "NEW\t11:22:33:44:55:66\tMDR-1000X\thp\n"
        "NEW\t11:22:33:44:55:67\t\tkbd\n";

    bt_list_t l;

    CHECK(bt_devices_parse(out, &l) == 2);
    CHECK(!l.dev[0].paired);
    CHECK(strcmp(l.dev[0].name, "MDR-1000X") == 0);
    CHECK(strcmp(l.dev[0].role, "hp") == 0);

    /* No name: the row will show the MAC instead */
    CHECK(l.dev[1].name[0] == '\0');
    CHECK(strcmp(l.dev[1].role, "kbd") == 0);

    /* Nothing said about the headphones: unknown, not "off" */
    CHECK(!l.hp);
    CHECK(l.hp_volume == -1);
}

/* An error from the script must not look like a device */
static void test_garbage(void) {
    static const char *out =
        "btcat-ctl: bluetoothd not running\n"
        "\n"
        "HP\t0\t-1\n"
        "DEV\tmissing fields\n";

    bt_list_t l;

    CHECK(bt_devices_parse(out, &l) == 0);
    CHECK(l.n == 0);
    CHECK(!l.hp);
    CHECK(l.hp_volume == -1);
    CHECK(!l.rx);
}

/* The state word the dialog shows */
static void test_state(void) {
    bt_device_t d = { .paired = false };

    CHECK(strcmp(bt_device_state(&d), "new") == 0);

    d.paired    = true;
    d.connected = true;
    CHECK(strcmp(bt_device_state(&d), "connected") == 0);

    d.connected   = false;
    d.autoconnect = true;
    CHECK(strcmp(bt_device_state(&d), "not connected, auto") == 0);

    d.autoconnect = false;
    CHECK(strcmp(bt_device_state(&d), "not connected") == 0);
}

/* More devices than the list holds: the extras are dropped, not written out */
static void test_overflow(void) {
    char out[4096] = "";
    char line[80];

    for (int i = 0; i < BT_MAX_DEV + 5; i++) {
        snprintf(line, sizeof(line), "DEV\tAA:BB:CC:DD:EE:%02X\t0\tcat\t0\tdev%d\n", i, i);
        strcat(out, line);
    }

    bt_list_t l;

    CHECK(bt_devices_parse(out, &l) == BT_MAX_DEV);
}

void test_bt_devices(void) {
    test_list();
    test_scan();
    test_garbage();
    test_state();
    test_overflow();
}
