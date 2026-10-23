/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "clock.h"

#include "styles.h"
#include "theme.h"
#include "backlight.h"
#include "voice.h"
#include "msg.h"

#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>

/* Reloj permanente + banda de estado compacta (issue #250, epic #249).
 *
 * El panel de arriba a la derecha era un reloj que ALTERNABA hora y
 * alimentacion (`params.clock_view` + tres timeouts): en TX, o cuando vencia un
 * timeout, la hora desaparecia. Ahora la hora es permanente (`HH:MM:SS`, 24h) y
 * la alimentacion/bateria vive en una linea de estado subordinada debajo, como
 * los indicadores separados de un movil (referencia IC-705; la alternancia tipo
 * VFO-B del KX3 se descarto, ver el issue).
 *
 * Estructura (contrato UX/geometry del issue):
 *
 *   contenedor (clock_style: el bitmap del panel, UNA vez, sin input)
 *     fila del reloj  y=0..40    fantasma "88:88:88" (familia segmentada) + valor
 *     fila de estado  y=41..60   [plug + %.1f V] (opcional)   [icono + NN%]
 *
 * Nada mas se mueve: el panel sigue midiendo 206/209 x 61 y su x es la del borde
 * derecho (`clock_style`), el S-meter y la fila de frecuencia no se tocan.
 *
 * Los parametros persistidos (`params.clock_view` y los tres timeouts) y las
 * funciones exportadas se conservan por compatibilidad de DB/API, pero el reloj
 * ya no los lee: `clock_set_view()` y compania solo escriben el parametro.
 */

/* --- Geometria ------------------------------------------------------------------
 *
 * Fila del reloj: la caja del valor y de su fantasma, al ancho del panel
 * (`lv_pct(100)`) y con el alto del contenido (la line height de la familia, que
 * cambia con el tema: 30/31 px en las de texto, 19/20 en las segmentadas). El
 * fantasma y el valor miden exactamente lo mismo porque piden la misma fuente y
 * la misma caja, asi que el fantasma cae digito a digito sobre el valor. Los 5 px
 * de arriba la centran en la fila y=0..40 del contrato, sin el `pad_ver 18` del
 * reloj viejo (que dejaba el texto en y=18..43 y solapaba cualquier linea
 * inferior).
 *
 * Fila de estado: pegada al borde inferior, dentro de la banda y=41..60, con el
 * mismo margen lateral. El icono y el porcentaje se separan STATUS_GAP px: el
 * porcentaje fija el borde derecho (LV_ALIGN_BOTTOM_RIGHT) y el icono se re-ancla
 * a su borde izquierdo en CADA actualizacion de texto (`align_battery_icon()`),
 * porque su ancho cambia ("--%" -> "100%"). Un ancla de una sola vez (el bug de
 * #269/#270) dejaba el icono en la posicion del ancho viejo y el primer digito
 * del porcentaje caia encima. */
#define CLOCK_LINE_Y     5
#define STATUS_PAD       8
#define STATUS_GAP       4
#define STATUS_Y         (-2)

/* Umbral de la fuente externa: el mismo del reloj de potencia de main (3.0 V),
 * por debajo de eso el `v_ext` del pack es ruido de un pin sin fuente. */
#define EXT_MIN_V        3.0f

/* Niveles del icono (contrato 4): 0..10 vacio, 11..35 nivel 1, 36..60 nivel 2,
 * 61..85 nivel 3, 86..100 completo. El SoC NO se recalcula de la tension: el
 * porcentaje es el que informa el hardware. */
#define BAT_EMPTY_MAX    10
#define BAT_LEVEL1_MAX   35
#define BAT_LEVEL2_MAX   60
#define BAT_LEVEL3_MAX   85
#define BAT_CAP_MAX      100

/* El "8 fantasma" de la linea del reloj (issue #9, decision 1): el valor es
 * SIEMPRE "%02i:%02i:%02i" (ocho caracteres, digitos y dos puntos), asi que el
 * fantasma es la constante "88:88:88" y no hace falta transformar el texto. */
#define CLOCK_GHOST      "88:88:88"

/* Sin telemetria no se inventa un 0%: se dice que no hay muestra. */
#define BAT_PCT_UNKNOWN  "--%"

static lv_obj_t         *obj;

/* Ultima muestra del pack publicada por radio.c (un solo mutex: el registro de
 * ultima respiracion la lee desde el arranque, clock_get_power). */
static pthread_mutex_t  power_mux = PTHREAD_MUTEX_INITIALIZER;

static float            v_ext;
static float            v_bat;
static uint8_t          cap_bat;
static bool             charging;

/* Las caras del panel, por rol (ver la estructura del encabezado). */
static lv_obj_t         *shadow;
static lv_obj_t         *lb_time;
static lv_obj_t         *lb_ext;
static lv_obj_t         *lb_bat_icon;
static lv_obj_t         *lb_bat_pct;

static lv_timer_t       *timer_time_sync;
static time_t           last_time_sync=0;

static void check_time_sync_cb(lv_timer_t * t);
static time_t get_last_sync_time();

/* Una toma de `power_mux` por actualizacion (contrato 5): el snapshot viaja por
 * valor y todas las operaciones LVGL corren fuera del mutex, en el hilo LVGL
 * (show_time es el callback del timer de 500 ms). */
typedef struct {
    float   ext_v;
    float   bat_v;
    uint8_t cap;
    bool    charging;
} power_sample_t;

static power_sample_t power_snapshot(void) {
    power_sample_t sample;

    pthread_mutex_lock(&power_mux);
    sample.ext_v    = v_ext;
    sample.bat_v    = v_bat;
    sample.cap      = cap_bat;
    sample.charging = charging;
    pthread_mutex_unlock(&power_mux);

    return sample;
}

/* Una muestra es valida solo con tension positiva y finita y capacidad en
 * rango; en otro caso la linea de estado lo dice (`--%`, icono dim) en vez de
 * inventar un 0.0 V / 0% (el defecto que se ve en el home de main). */
static bool sample_valid(const power_sample_t * s) {
    return s->bat_v > 0.0f && isfinite(s->bat_v) && s->cap <= BAT_CAP_MAX;
}

static const char * battery_symbol(const power_sample_t * s) {
    if (!sample_valid(s))           return LV_SYMBOL_BATTERY_EMPTY;
    if (s->cap <= BAT_EMPTY_MAX)    return LV_SYMBOL_BATTERY_EMPTY;
    if (s->cap <= BAT_LEVEL1_MAX)   return LV_SYMBOL_BATTERY_1;
    if (s->cap <= BAT_LEVEL2_MAX)   return LV_SYMBOL_BATTERY_2;
    if (s->cap <= BAT_LEVEL3_MAX)   return LV_SYMBOL_BATTERY_3;

    return LV_SYMBOL_BATTERY_FULL;
}

/* Escribe solo si el texto cambia (OPT-4 #150): lv_label_set_text invalida el
 * objeto aunque el texto sea el mismo, y show_time corre cada 500 ms mientras la
 * hora cambia una vez por segundo. La comparacion es contra el texto VIVO de la
 * etiqueta: no hay cache que un cambio de tema/fuente pueda dejar vieja. */
static void set_text(lv_obj_t * label, const char * text) {
    if (strcmp(lv_label_get_text(label), text) != 0) {
        lv_label_set_text(label, text);
    }
}

/* Misma guarda para el color: se lee el estilo resuelto del objeto (no un cache)
 * para que un cambio de tema siga aplicandose. */
static void set_text_color(lv_obj_t * label, lv_color_t color) {
    if (lv_color_to32(lv_obj_get_style_text_color(label, 0)) != lv_color_to32(color)) {
        lv_obj_set_style_text_color(label, color, 0);
    }
}

/* La fuente externa se omite sin fuente: la etiqueta se OCULTA (no se le deja
 * una caja vacia pintando ni un texto de relleno). */
static void set_hidden(lv_obj_t * label, bool hidden) {
    if (lv_obj_has_flag(label, LV_OBJ_FLAG_HIDDEN) != hidden) {
        if (hidden) {
            lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/* El icono se ancla al borde IZQUIERDO de la etiqueta del porcentaje (que es la
 * que fija el borde derecho de la barra): la separacion es SIEMPRE STATUS_GAP,
 * con cualquier ancho de texto.
 *
 * `lv_obj_align_to` es de UNA sola vez: calcula la posicion ABSOLUTA a partir
 * del ancho que la base tiene en ese instante. Si la etiqueta del porcentaje
 * cambia de ancho despues ("--%" -> "100%", el mas ancho) y el icono no se
 * vuelve a anclar, queda clavado en la posicion del ancho viejo y el primer
 * digito cae encima del icono (auditoria de UI #269/#270). Por eso se re-ancla
 * en cada actualizacion del texto, DESPUES de escribirlo: `lv_obj_align_to`
 * corre el layout antes de leer las cajas, asi que ve el ancho nuevo. */
static void align_battery_icon(void) {
    lv_obj_align_to(lb_bat_icon, lb_bat_pct, LV_ALIGN_OUT_LEFT_BOTTOM, -STATUS_GAP, 0);
}

/* Una etiqueta del reloj: sin el tema por defecto de LVGL (lv_hal_disp.c lo
 * inicializa al registrar el display y le pone color, radio, padding y borde),
 * sin input y sin scroll. Todo lo que dibuja lo pide explicitamente. */
static lv_obj_t * clock_label(lv_obj_t * parent) {
    lv_obj_t * label = lv_label_create(parent);

    lv_obj_remove_style_all(label);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE);

    return label;
}

/* La fila del reloj: el valor y su fantasma son dos etiquetas sin estilo del
 * tema, con la MISMA caja (todo el ancho del panel, el alto del contenido) y el
 * mismo align, asi que el fantasma cae digito a digito sobre el valor. */
static lv_obj_t * clock_line_label(lv_obj_t * parent) {
    lv_obj_t * label = clock_label(parent);

    lv_obj_set_size(label, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, CLOCK_LINE_Y);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label, font_digit(&sony_38), 0);
    lv_obj_set_style_text_color(label, TC(text), 0);

    return label;
}

static void update_time(void) {
    time_t      now = time(NULL);
    struct tm   *t = localtime(&now);
    char        str[16];

    if (t == NULL) {
        /* Reloj del sistema en fallo: una hora ficticia no; y nunca un crash. */
        snprintf(str, sizeof(str), "--:--:--");
    } else {
        snprintf(str, sizeof(str), "%02i:%02i:%02i", t->tm_hour, t->tm_min, t->tm_sec);
    }

    /* El fantasma es la constante "88:88:88" (el valor siempre tiene ocho
     * caracteres): se escribe al construir y no se reescribe en cada tick. */
    set_text(lb_time, str);
}

static void update_status(const power_sample_t * s) {
    char        str[32];
    lv_color_t  color;
    bool        valid = sample_valid(s);

    /* Alimentacion externa, a la izquierda (opcional). */
    if (isfinite(s->ext_v) && s->ext_v >= EXT_MIN_V) {
        /* El simbolo de carga es un estado de la bateria: solo con muestra
         * valida (sin parpadeo: `charging` llega en el mismo pack). */
        const char * sym = (valid && s->charging) ? SYMBOL_PLUG_CHARGE : SYMBOL_PLUG;

        snprintf(str, sizeof(str), "%s %.1f V", sym, s->ext_v);
        set_text(lb_ext, str);
        set_hidden(lb_ext, false);
    } else {
        set_hidden(lb_ext, true);
    }

    /* Bateria, a la derecha: icono (sony_20, el unico slot chico con los glifos
     * del bloque U+F240..U+F244) y porcentaje exacto. */
    if (valid) {
        snprintf(str, sizeof(str), "%u%%", (unsigned) s->cap);
        color = TC(text);
    } else {
        snprintf(str, sizeof(str), "%s", BAT_PCT_UNKNOWN);
        color = TC(text_dim);
    }

    set_text(lb_bat_pct, str);
    set_text(lb_bat_icon, battery_symbol(s));
    set_text_color(lb_bat_pct, color);
    set_text_color(lb_bat_icon, color);

    /* El ancho del porcentaje acaba de (poder) cambiar: el icono se re-ancla al
     * ancho nuevo para que su caja nunca pise la del texto. */
    align_battery_icon();
}

static void show_time() {
    /* La hora va SIEMPRE, sin depender de radio_get_state(), de
     * params.clock_view ni de ningun timeout. */
    update_time();

    /* La linea de estado se refresca con el proximo tick, desde el ultimo pack
     * (una sola toma del mutex por actualizacion). */
    power_sample_t sample = power_snapshot();

    update_status(&sample);
}

lv_obj_t * clock_init(lv_obj_t * parent) {
    /* power_mux se inicializa estaticamente (PTHREAD_MUTEX_INITIALIZER): el
     * registro de ultima respiracion puede leer la muestra desde el arranque,
     * antes de que exista la pantalla. */

    /* El panel: UN contenedor con el fondo del reloj (clock_style trae el
     * bitmap, el tamano 206/209x61 y el align al borde derecho) y sin capturar
     * input. Antes el fondo lo dibujaba la etiqueta del valor (o su fantasma), lo
     * que con dos lineas habria dejado el bitmap repetido o a medias. */
    obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_add_style(obj, &clock_style, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    /* El "8 fantasma" se crea primero, asi que queda DEBAJO del valor: sin
     * familia segmentada no hay capa (con la familia por defecto el 8 seria un
     * digito mas). */
    if (font_digit_active()) {
        shadow = clock_line_label(obj);
        lv_obj_set_style_text_opa(shadow, UI_DIGIT_SHADOW_OPA, 0);
        lv_label_set_text(shadow, CLOCK_GHOST);
    }

    lb_time = clock_line_label(obj);

    /* La etiqueta del valor conserva la MISMA imagen del panel como
     * `bg_img_src`, con la opacidad en TRANSP: no dibuja un segundo bitmap, y
     * `_opaque` de la regla de solapamiento (tools/ab_testing, UI-017) sigue
     * leyendo el apilado fantasma/valor como diseno. */
    lv_obj_set_style_bg_img_src(lb_time, lv_obj_get_style_bg_img_src(obj, 0), 0);
    lv_obj_set_style_bg_img_opa(lb_time, LV_OPA_TRANSP, 0);

    /* La linea de estado: fuente externa a la izquierda, bateria a la derecha.
     * Las tres arrancan con el texto vacio: la de la fuente externa nace oculta
     * (hasta que el pack traiga una fuente valida) y no tiene que quedar con el
     * "Text" con el que LVGL crea una etiqueta. */
    lb_ext = clock_label(obj);
    lv_obj_set_style_text_font(lb_ext, &sony_20, 0);
    lv_obj_set_style_text_color(lb_ext, TC(text), 0);
    lv_obj_align(lb_ext, LV_ALIGN_BOTTOM_LEFT, STATUS_PAD, STATUS_Y);
    lv_label_set_text(lb_ext, "");

    lb_bat_pct = clock_label(obj);
    lv_obj_set_style_text_font(lb_bat_pct, &sony_16, 0);
    lv_obj_set_style_text_color(lb_bat_pct, TC(text), 0);
    lv_obj_align(lb_bat_pct, LV_ALIGN_BOTTOM_RIGHT, -STATUS_PAD, STATUS_Y);
    lv_label_set_text(lb_bat_pct, "");

    lb_bat_icon = clock_label(obj);
    lv_obj_set_style_text_font(lb_bat_icon, &sony_20, 0);
    lv_obj_set_style_text_color(lb_bat_icon, TC(text), 0);
    lv_label_set_text(lb_bat_icon, "");
    /* La posicion inicial (vacia) del icono; `show_time()` de abajo la re-ancla
     * con el texto real de la primera muestra. */
    align_battery_icon();

    /* Primer dibujo y los dos timers de siempre: la hora cada 500 ms y el aviso
     * de sincronizacion NTP cada 200 ms. Ninguno nuevo. */
    show_time();
    lv_timer_create(show_time, 500, NULL);

    last_time_sync = get_last_sync_time();
    timer_time_sync = lv_timer_create(check_time_sync_cb, 200, NULL);

    return obj;
}

void clock_update_power(float ext, float bat, uint8_t cap, bool charge_flag) {
    pthread_mutex_lock(&power_mux);
    v_ext = ext;
    v_bat = bat;
    cap_bat = cap;
    charging = charge_flag;
    pthread_mutex_unlock(&power_mux);
}

bool clock_get_power(clock_power_t *out) {
    if (out == NULL) {
        return false;
    }

    pthread_mutex_lock(&power_mux);
    out->ext_v    = v_ext;
    out->bat_v    = v_bat;
    out->cap      = cap_bat;
    out->charging = charging;
    out->valid    = v_bat > 0.0f;
    pthread_mutex_unlock(&power_mux);

    return out->valid;
}

/* Los setters de la vista y los timeouts siguen existiendo (API y DB
 * compatibles) pero el reloj ya no los lee: la hora es permanente. */

void clock_set_view(clock_view_t x) {
    params_lock();
    params.clock_view = x;
    params_unlock(&params.dirty.clock_view);
}

void clock_set_time_timeout(uint8_t sec) {
    params_lock();
    params.clock_time_timeout = sec;
    params_unlock(&params.dirty.clock_time_timeout);
}

void clock_set_power_timeout(uint8_t sec) {
    params_lock();
    params.clock_power_timeout = sec;
    params_unlock(&params.dirty.clock_power_timeout);
}

void clock_set_tx_timeout(uint8_t sec) {
    params_lock();
    params.clock_tx_timeout = sec;
    params_unlock(&params.dirty.clock_tx_timeout);
}

void clock_say_bat_info() {
    voice_sure();
    voice_say_float("Battery voltage|", v_bat);
}

static void check_time_sync_cb(lv_timer_t * t) {
    time_t new_time = get_last_sync_time();
    if (last_time_sync != new_time) {
        last_time_sync = new_time;
        msg_schedule_text_fmt("Time is synchronized with NTP");
    }
}

static time_t get_last_sync_time() {
    struct stat attr;
    if (stat("/var/run/time-sync", &attr) == 0) {
        return attr.st_atime;
    }
    return 0;
}

bool clock_time_synced(void) {
    return get_last_sync_time() != 0;
}
