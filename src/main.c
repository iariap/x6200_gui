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
#include <time.h>
#include <sys/time.h>

#include "main.h"
#include "main_screen.h"
#include "styles.h"
#include "theme.h"
#include "voice.h"
#include "remote_server.h"
#include "fonts/font_select.h"
#include "radio.h"
#include "cb.h"
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
#include "crashlog_modules.h"
#include "lastbreath.h"
#include "clock.h"

/* Draw buffer, in lv_color_t (issue #147). `size_in_px_cnt` is a *pixel* count,
 * not bytes: the old `(800 * 480 * 4)` was meant as "4 bytes per pixel" but
 * landed on the pixel count, so it reserved 4 full screens (5.86 MiB) where a
 * line-sized buffer does the job.
 *
 * The display is logical landscape 800x480 and LVGL rotates it in software to
 * the 480x800 panel (disp_drv.sw_rotate + LV_DISP_ROT_90), so the buffer only
 * has to hold whole logical lines: LVGL splits the invalidated area with
 * get_max_row() and flushes it in parts. A partial buffer renders the *same*
 * physical framebuffer as a full one (proven byte-for-byte by
 * tools/ui_harness/probe/disp_buf_probe.c, tests/check_drawbuf_partial.sh).
 *
 * Measured candidates (docs/lvgl-opt1-drawbuf.md): 480 lines = one full screen
 * (-75%, zero change in flushes), 128 lines (-93.3%, 400 KiB), 64 (-96.7%),
 * 32 (-98.3%). 128 is the chosen candidate: byte-identical framebuffer, the
 * flush contract preserved, host render time flat and the same flush count as
 * the old buffer on the spectrum strip (the hottest region). */
#define DISP_BUF_LINES 128
#define DISP_BUF_SIZE  (800 * DISP_BUF_LINES)

/* A partial buffer must hold at least one full logical line (800 px): with less
 * than one line get_max_row() returns 0 and LVGL renders nothing. */
_Static_assert(DISP_BUF_SIZE >= 800, "draw buffer smaller than one logical line");

rotary_t                    *vol;
encoder_t                   *mfk;
rotary_t                    *mfk_inner;

static lv_color_t           buf[DISP_BUF_SIZE];
static lv_disp_draw_buf_t   disp_buf;
static lv_disp_drv_t        disp_drv;

/* Runs on the update-check thread; msg_schedule_* queues it for the UI loop. */
static void update_check_notify(const char *version) {
    msg_schedule_text_fmt("Firmware %s available", version);
}

/* Tiempo de arranque: cada etapa deja su marca en /mnt/app_logs/boot.log (particion DATA, se
 * puede leer desde la PC). Sirve para saber QUE parte del arranque se lleva los segundos. */
void boot_mark(const char *etapa) {
    double up = 0.0;
    FILE  *f  = fopen("/proc/uptime", "r");

    if (f != NULL) {
        if (fscanf(f, "%lf", &up) != 1) {
            up = 0.0;
        }
        fclose(f);
    }

    /* Ademas del uptime, cuanto duro la etapa: es el numero que dice QUE trozo del arranque
     * se lleva los segundos. El reloj monotono funciona tambien en el banco (chroot), donde
     * no hay /proc/uptime. */
    static uint64_t marca_anterior_ms = 0;
    uint64_t        ahora_ms          = get_time();

    if (marca_anterior_ms == 0) {
        marca_anterior_ms = ahora_ms;
    }

    double etapa_s = (double) (ahora_ms - marca_anterior_ms) / 1000.0;
    marca_anterior_ms = ahora_ms;

    FILE *b = fopen("/mnt/app_logs/boot.log", "a");

    if (b != NULL) {
        fprintf(b, "  %-14s %.1f s de uptime  (+%.2f s)\n", etapa, up, etapa_s);
        fclose(b);
    }
}

/* Identidad del build (issue #165): UNA linea al arranque con el hash de las
 * fuentes que se compilaron. El valor lo embebe CMake en tiempo de build
 * (X6200_FUENTE_MD5, ver CMakeLists.txt) y es el mismo sello .fuente.md5 que deja
 * tools/compila-gui.sh, asi que el QA puede comparar el sello del build con el que
 * corre la radio SIN depender del md5 del binario (que no distingue commits). Va
 * al log de la GUI y a boot.log (la particion DATA), que es lo que se puede leer
 * desde la PC. Si el build no lo definio, dice "desconocido". */
#ifndef X6200_FUENTE_MD5
#define X6200_FUENTE_MD5 "desconocido"
#endif

static void boot_identity(void) {
    LV_LOG_USER("fuente: %s (x6200_gui)", X6200_FUENTE_MD5);

    FILE *b = fopen("/mnt/app_logs/boot.log", "a");

    if (b != NULL) {
        fprintf(b, "fuente: %s (x6200_gui)\n", X6200_FUENTE_MD5);
        fclose(b);
    }
}

/* --- Ultima respiracion (issue #121) -----------------------------------------
 * Cuatro caidas del equipo entero no dejaron evidencia: el log de la GUI vive
 * en tmpfs y solo se archiva en una salida ordenada, asi que un corte duro se
 * lo lleva. Cada `lastbreath_period_ms` se escribe una linea chica (tmp+rename)
 * con uptime, estado de radio, app, TX, bateria y ultimo evento; al arrancar,
 * si la anterior no fue una salida limpia, se loguea `LASTBREATH ...`.
 *
 * El camino de escritura NO usa heap: la linea se arma en un buffer de pila y
 * se escribe con open/write/fdatasync/rename (src/lastbreath.c). Si el path no
 * existe (sin particion DATA, banco sin /mnt) se avisa una vez y se sigue.
 * Configurable para el banco: X6200_LASTBREATH_SECS (1..3600, default 10) y
 * X6200_LASTBREATH_PATH (default /mnt/lastbreath). */

static const char      *lastbreath_path = "/mnt/lastbreath";
static uint32_t         lastbreath_period_ms = 10 * 1000;
static uint64_t         lastbreath_next_ms = 0;
static lastbreath_state_t lastbreath_last;
static uint32_t         lastbreath_prev_write_us = 0;
static bool             lastbreath_enabled = false;
static bool             lastbreath_write_warned = false;

static uint64_t lastbreath_now_us(void) {
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return get_time() * 1000ULL;
    }

    return (uint64_t) ts.tv_sec * 1000000ULL + (uint64_t) ts.tv_nsec / 1000ULL;
}

static const char *radio_state_name(radio_state_t state) {
    switch (state) {
        case RADIO_RX:        return "RX";
        case RADIO_TX:        return "TX";
        case RADIO_ATU_START: return "ATU_START";
        case RADIO_ATU_WAIT:  return "ATU_WAIT";
        case RADIO_ATU_RUN:   return "ATU_RUN";
        case RADIO_SWRSCAN:   return "SWRSCAN";
        case RADIO_POWEROFF:  return "POWEROFF";
        case RADIO_OFF:       return "OFF";
    }

    return "unknown";
}

static bool radio_state_is_tx(radio_state_t state) {
    return state == RADIO_TX || state == RADIO_ATU_START || state == RADIO_ATU_WAIT ||
           state == RADIO_ATU_RUN || state == RADIO_SWRSCAN;
}

static void lastbreath_state_fill(lastbreath_state_t *s) {
    lastbreath_state_init(s);

    s->uptime_ms = lastbreath_read_uptime_ms();
    lastbreath_read_boot_id(s->boot, sizeof(s->boot));

    radio_state_t radio = radio_get_state();

    snprintf(s->radio, sizeof(s->radio), "%s", radio_state_name(radio));
    s->tx = radio_state_is_tx(radio);

    snprintf(s->app, sizeof(s->app), "%s", main_screen_current_app());
    snprintf(s->last_event, sizeof(s->last_event), "%s", event_last_name());
    snprintf(s->msg, sizeof(s->msg), "%s", msg_last_text());

    clock_power_t power;

    if (clock_get_power(&power) && power.valid) {
        s->bat_valid = true;
        s->bat_mv    = (int32_t) (power.bat_v * 1000.0f + 0.5f);
        s->ext_mv    = (int32_t) (power.ext_v * 1000.0f + 0.5f);
        s->bat_pct   = power.cap;
        s->charging  = power.charging;
    }

    event_queue_stats_t events;

    event_queue_stats_get(&events);
    s->ev_pending = events.pending;
    s->ev_peak    = events.peak;
    s->ev_dropped = events.dropped;

    /* Costo auto-medido: el registro muestra la duracion del write anterior.
     * La variable es aparte porque state_init() acaba de borrar el struct. */
    s->prev_write_us = lastbreath_prev_write_us;
}

static void lastbreath_tick(void) {
    if (!lastbreath_enabled) {
        return;
    }

    uint64_t now = get_time();

    if (now < lastbreath_next_ms) {
        return;
    }

    lastbreath_state_fill(&lastbreath_last);

    char   line[LASTBREATH_LINE_MAX];
    size_t len = lastbreath_format(line, sizeof(line), &lastbreath_last);

    uint64_t start = lastbreath_now_us();
    int      rc    = lastbreath_write_atomic(lastbreath_path, line, len);
    uint64_t took  = lastbreath_now_us() - start;

    if (rc == 0) {
        lastbreath_prev_write_us = took > UINT32_MAX ? UINT32_MAX : (uint32_t) took;
        lastbreath_last.prev_write_us = lastbreath_prev_write_us;
        lastbreath_write_warned = false;
    } else if (!lastbreath_write_warned) {
        lastbreath_write_warned = true;
        LV_LOG_WARN("LASTBREATH sin escritura en %s: se sigue sin registro (issue #121)", lastbreath_path);
    }

    lastbreath_next_ms = now + lastbreath_period_ms;
}

/* Salida limpia: el proximo arranque no reporta LASTBREATH. Se apaga el registro
 * para que un tick tardio no vuelva a escribir clean=0 antes del corte. */
void lastbreath_mark_clean_now(void) {
    if (!lastbreath_enabled) {
        return;
    }

    lastbreath_state_fill(&lastbreath_last);
    (void) lastbreath_mark_clean(lastbreath_path, &lastbreath_last);
    lastbreath_enabled = false;
}

static void lastbreath_init_from_env(void) {
    const char *path = getenv("X6200_LASTBREATH_PATH");

    if (path != NULL && path[0] != '\0') {
        lastbreath_path = path;
    }

    const char *secs = getenv("X6200_LASTBREATH_SECS");

    if (secs != NULL && secs[0] != '\0') {
        long value = strtol(secs, NULL, 10);

        if (value >= 1 && value <= 3600) {
            lastbreath_period_ms = (uint32_t) value * 1000u;
        } else {
            LV_LOG_WARN("X6200_LASTBREATH_SECS=%s fuera de 1..3600: se usa 10", secs);
        }
    }

    char report[LASTBREATH_LINE_MAX * 2];
    lastbreath_boot_t boot = lastbreath_boot(lastbreath_path, report, sizeof(report));

    if (boot == LASTBREATH_BOOT_UNCLEAN) {
        size_t len = strlen(report);

        if (len > 0 && report[len - 1] == '\n') {
            report[len - 1] = '\0';
        }

        /* Una sola linea para el triage: que habia cuando el equipo dejo de
         * responder. */
        LV_LOG_WARN("%s", report);
    } else if (boot == LASTBREATH_BOOT_CLEAN) {
        LV_LOG_USER("LASTBREATH salida limpia del run anterior (%s)", lastbreath_path);
    } else if (boot == LASTBREATH_BOOT_ERROR) {
        LV_LOG_WARN("LASTBREATH no se pudo leer %s (issue #121)", lastbreath_path);
    }
}

static void lastbreath_start(void) {
    lastbreath_enabled = true;
    lastbreath_next_ms = 0;
    lastbreath_tick();

    atexit(lastbreath_mark_clean_now);
}

/* Tiempo de arranque: se anota cuando la GUI ya esta dibujando, para poder medir (boot.log) */
static void boot_time_log(void) {
    double up = 0.0;
    FILE  *f  = fopen("/proc/uptime", "r");

    if (f != NULL) {
        if (fscanf(f, "%lf", &up) != 1) {
            up = 0.0;
        }
        fclose(f);
    }

    LV_LOG_USER("GUI lista: %.1f s de uptime", up);

    FILE *b = fopen("/mnt/app_logs/boot.log", "a");

    if (b != NULL) {
        fprintf(b, "GUI lista: %.1f s\n", up);
        fclose(b);
    }
}


/* --- Registro de crashes ---------------------------------------------------------------------
 * Un SIGSEGV sin handler deja el log cortado y no dice donde: esto registra la senal, la direccion
 * que fallo, el PC/LR/SP y la base del binario (con &main_screen, para resolver sin depender del
 * ASLR) y termina. docs/HALLAZGOS.md 38. */
#include <signal.h>
#include <ucontext.h>

int main(int argc, char **argv);   /* para tomar su direccion como base del binario */

static void crash_log_handler(int sig, siginfo_t *info, void *ctx) {
    ucontext_t *uc = (ucontext_t *) ctx;

    fprintf(stderr, "CRASHLOG sig=%d fallo=%p pc=%p lr=%p sp=%p base=%p\n",
            sig, info != NULL ? info->si_addr : NULL,
#if defined(__arm__) && defined(__GLIBC__)
            (void *) uc->uc_mcontext.arm_pc, (void *) uc->uc_mcontext.arm_lr, (void *) uc->uc_mcontext.arm_sp,
#else
            NULL, NULL, NULL,
#endif
            (void *) &main);
    fflush(stderr);

    /* Volcado de la pila: para saber quien llamo cuando el PC cae en una libreria (ahi el lr no
     * sirve). Despues se buscan las direcciones dentro del texto del binario y se resuelven con
     * addr2line restando la base. docs/HALLAZGOS.md 38. */
#if defined(__arm__) && defined(__GLIBC__)
    {
        /* Antes de la pila, los modulos cargados y su md5: sin eso, un PC en
         * 0xb6xxxxxx no se puede resolver (issue #131). El bloque ya esta
         * formateado desde el arranque; aca solo se escribe, no se aloca. */
        size_t      mods_len = 0;
        const char *mods     = crashlog_modules_block(&mods_len);

        if (mods != NULL && mods_len > 0) {
            ssize_t written = write(STDERR_FILENO, mods, mods_len);

            (void) written;
        }

        uint32_t *sp = (uint32_t *) uc->uc_mcontext.arm_sp;

        fprintf(stderr, "CRASHLOG-STACK");
        for (int i = 0; i < 160; i++) {
            fprintf(stderr, " %08x", sp[i]);
        }
        fprintf(stderr, "\n");
        fflush(stderr);
    }
#endif

    _exit(128 + sig);
}

static void crash_log_install(void) {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crash_log_handler;
    sa.sa_flags     = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
}

int main(int argc, char **argv) {
    (void) argc;

    /* El hilo de GUI queda identificado antes de crear cualquier hilo: los
     * modulos que tocan LVGL (buttons, cw_tune_ui, ...) lo consultan para
     * diferirse cuando los llama un hilo ajeno (issue #131). */
    scheduler_thread_init();

    crash_log_install();
    boot_mark("GUI inicio");
    boot_identity();

    /* Tabla de modulos del CRASHLOG (issue #131): se arma una sola vez, antes
     * de que la GUI pueda crashear. El md5 de las .so se lee aca; el handler
     * despues solo escribe el bloque ya formateado. */
    uint64_t crashlog_start = get_time();
    crashlog_modules_init(argv[0]);
    uint64_t crashlog_ms = get_time() - crashlog_start;
    boot_mark("crashlog modulos");

    lv_init();
    boot_mark("lv_init");

    const crashlog_mod_t *exe = crashlog_modules_first();
    if (exe != NULL) {
        LV_LOG_USER("CRASHLOG-MOD-INIT modulos=%zu exe_base=0x%08lx exe_md5=%s exe=%s (%llu ms)",
                    crashlog_modules_count(), (unsigned long) exe->base, exe->md5, exe->path,
                    (unsigned long long) crashlog_ms);
    } else {
        LV_LOG_WARN("CRASHLOG-MOD-INIT sin modulos (no se pudo leer /proc/self/maps)");
    }

    /* Registro de ultima respiracion (issue #121): leer el del run anterior
     * antes de que el primer tick lo pise. Si no fue una salida limpia, queda
     * una linea LASTBREATH en este log. */
    lastbreath_init_from_env();
    boot_mark("lastbreath init");
    // lv_png_init();

    /* Diagnostico de voz (X6200_VOICE_TEST="texto"): dice la frase y termina. Va aca, antes de
     * la pantalla, la entrada, la radio y el WiFi, para poder correrlo con la GUI andando sin
     * pelearle el framebuffer ni la red. */
    audio_init();
    boot_mark("audio_init");

    if (voice_selftest()) {
        exit(0);
    }

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
    boot_mark("display init");
    event_init();
    usb_devices_monitor_init();
    boot_mark("eventos/usb");

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
    boot_mark("display");

    keyboard_init();
    boot_mark("keyboard");

    /* Igual que las perillas: los nombres del device tree son estables (matrix_keypad es el
     * teclado del panel, rotary3 el encoder interno del MFK, que la GUI lee como teclado) y
     * el numero de eventN depende del orden de sondeo. La busqueda por nombre lee sysfs, no
     * abre el dispositivo, asi que no cuesta arranque. */
    keypad_init(input_dev_by_name("matrix_keypad", "/dev/input/event0"));
    keypad_init(input_dev_by_name("rotary3", "/dev/input/event4"));

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

    boot_mark("entradas");
    params_init();
    boot_mark("params_init");
    /* CB 27 MHz (issue #146): despues de params_init() para que el observer de
     * cfg.band_id corra despues del de src/cfg/band.c, y antes de radio_init()
     * para que el tope de potencia se recorte antes de mandar cfg.pwr al equipo.
     * Con la ultima banda guardada en CB aplica la entrada una vez. */
    cb_hooks_init();
    boot_mark("cb_hooks");
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


    theme_init(params.theme_name.x);    /* un nombre retirado o desconocido cae al default (Aurora) */
    /* El desplegable de Ajustes y `params.font` hablan el idioma de fonts_apply():
     * su lista de items se arma de `font_names[]` en orden FONT_*, asi que la fila
     * elegida ES el valor FONT_* (antes era una tabla paralela escrita a mano y
     * elegir "Roboto" aplicaba DejaVu: la copia de esa tabla mataba la GUI al
     * reiniciar y el equipo quedaba en el logo). FONT_STOCK (0) no toca nada: es la
     * familia propia del equipo. El tema, si nombra una, manda. */
    if (theme_shape.font >= 0) {
        fonts_apply((uint8_t) theme_shape.font);
    } else if (params.font.x > 0) {
        fonts_apply((uint8_t) params.font.x);
    }

    if (theme_shape.vision) {
        fonts_scale(VISION_FONT_STEPS);
    }

    /* Los paneles de dígitos (fila de frecuencia y reloj) pueden dibujar con una
     * familia segmentada; styles_init() resuelve font_digit() al crear los estilos,
     * asi que la eleccion tiene que estar hecha antes. */
    fonts_set_digit_family((uint8_t) theme_shape.font_digits);

    styles_init(params.theme.x);
    boot_mark("fuentes y estilos");

    dsp_init();
    lv_obj_t *main_obj = main_screen();
    boot_mark("pantalla principal");
    waterfall_set_battery_mode(params.waterfall_battery.x);

    cw_init();
    rtty_init();
    boot_mark("cw/rtty");
    /* Antes de todo lo que toca el GPIO (backlight, wifi, CAT): si esto falla, los usos
     * se saltean en lugar de desreferenciar NULL y matar la GUI. */
    gpio_safe_prepare();

    radio_init(
        &main_screen_notify_tx,
        &main_screen_notify_rx
    );
    boot_mark("radio");
    wifi_power_setup();
    backlight_init();
    cat_init();
    // pannel_visible();
    if (!qso_log_init()) {
        LV_LOG_ERROR("Can't init QSO log");
    }
    qso_log_import_adif("/mnt/incoming_log.adi");
    boot_mark("cat/qso_log");

    /* LV_TICK_CUSTOM (lv_conf.h del proyecto) mide el tick con CLOCK_MONOTONIC:
     * el hilo que hacia `usleep(5*1000) + lv_tick_inc(5)` ya no existe (era el
     * 93.5% de los wakeups del hilo de GUI, issue #13). */

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
    boot_mark("wfview");

    /* Web remote: control socket for the /remote page (UNIX socket) */
    remote_server_init();
    boot_mark("control remoto");

    int64_t next_loop_time, sleep_time, loop_start_time;
    voice_ready();        /* la GUI ya esta en pie: desde aca la voz anuncia */
    static bool boot_time_anotado = false;

    /* Primer registro de esta corrida (clean=0) y latido cada
     * lastbreath_period_ms. */
    lastbreath_start();

    while (1) {
        loop_start_time = get_time();
        observer_delayed_notify_all();
        event_obj_check();
        scheduler_work();
        lastbreath_tick();
        next_loop_time = lv_timer_handler() + loop_start_time;

        if (!boot_time_anotado) {
            boot_time_anotado = true;
            boot_time_log();
        }
        sleep_time = next_loop_time - get_time();
        if (sleep_time > 0) {
            usleep(sleep_time * 1000);
        }
    }
    return 0;
}
