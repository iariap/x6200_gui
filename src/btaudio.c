/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Los auriculares Bluetooth: la app (dialog_bt.c) maneja scan/list/pair/connect/disconnect y
 *  esto es lo que el resto de la GUI necesita del script (btaudio-ctl): el volumen de los
 *  auriculares y el mute del altavoz. Antes el script tenia comandos para las dos cosas
 *  (volume, mute) y nadie los llamaba: el volumen de los auriculares no se podia cambiar y el
 *  flag de mute quedaba escrito sin efecto (docs/HALLAZGOS.md 23).
 */

#include "btaudio.h"

#include "bt_state.h"
#include "radio.h"

#include "util.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BT_CTL  "/usr/sbin/btaudio-ctl"

#define BT_LOG  "/mnt/app_logs/bt.log"

void btaudio_log(const char *fmt, ...) {
    char    linea[256];
    char    hora[32];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(linea, sizeof(linea), fmt, ap);
    va_end(ap);

    get_time_str(hora, sizeof(hora));

    FILE *f = fopen(BT_LOG, "a");

    if (f == NULL) {
        return;
    }

    fprintf(f, "%s %s\n", hora, linea);
    fclose(f);
}

static void *run_thread(void *arg) {
    char *cmd = arg;

    int rc = system(cmd);

    (void) rc;
    free(cmd);

    return NULL;
}

void btaudio_run(const char *fmt, ...) {
    char    args[128];
    char    cmd[200];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(args, sizeof(args), fmt, ap);
    va_end(ap);

    snprintf(cmd, sizeof(cmd), BT_CTL " %s >/dev/null 2>&1", args);

    btaudio_log("btaudio-ctl %s", args);

    char *line = strdup(cmd);

    if (line == NULL) {
        return;
    }

    pthread_t t;

    if (pthread_create(&t, NULL, run_thread, line) == 0) {
        pthread_detach(t);
    } else {
        free(line);
    }
}

void btaudio_headphone_volume(int pct) {
    if (!bt_state_connected()) {
        return;
    }

    btaudio_run("volume %i", pct);
}

/* El mute que puso esta politica (no el del usuario). Se compone con el del usuario en
 * radio_vol_apply() (#244): antes los dos compartian el toggle radio_change_mute(), asi que
 * "auriculares desconectados" solo devolvia el audio si el usuario no lo habia muteado a mano
 * y el flag no se desincronizara. */
static bool mudo_por_bt = false;

bool btaudio_speaker_muted(void) {
    return mudo_por_bt;
}

void btaudio_follow_state(void) {
    static bool con_antes = false;

    bool con  = bt_state_connected();
    bool mudo = bt_state_mute();

    if (con == con_antes) {
        return;
    }

    con_antes = con;

    if (con && mudo) {
        if (!mudo_por_bt) {
            mudo_por_bt = true;
            radio_vol_apply();
            btaudio_log("auriculares conectados: altavoz a 0 (BTAUDIO_MUTE=yes)");
        }
    } else if (!con && mudo_por_bt) {
        mudo_por_bt = false;
        radio_vol_apply();
        btaudio_log("auriculares desconectados: altavoz de vuelta");
    }
}

void btaudio_speaker_mute(bool on) {
    /* El equipo ya sabe mutearse: radio_vol_apply() deja el audio de recepcion en 0 sin tocar
     * el mute del usuario (docs/HALLAZGOS.md 23, 64). */
    if (on != mudo_por_bt) {
        mudo_por_bt = on;
        radio_vol_apply();
        btaudio_log("altavoz %s (mute del equipo)", on ? "apagado" : "encendido");
    }
}
