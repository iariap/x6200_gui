/*
 * Verificacion estatica de Settings y del cableado de #186.
 *
 * No necesita LVGL: lee las fuentes y comprueba
 *   - que TODO string literal de src/dialog_settings.c sea ASCII/ingles (la
 *     regla del owner: Settings solo en ingles, tambien las locuciones);
 *   - la fila `Accel threshold` inmediatamente despues de `Freq acceleration`,
 *     con spinbox 3..15 y sufijo `detents/s`;
 *   - el parametro `freq_accel_threshold` (default 7, rango, saneamiento,
 *     load/save) y la voz `Acceleration starts at`;
 *   - que el lector VOL/MFK no toque el motor adaptativo ni emita el evento del
 *     dial, y que el dial use cola de 512 con timestamps del kernel;
 *   - que la aplicacion al VFO sea int64 con `radio_check_freq()` y que CB use
 *     `raw_diff` sin aceleracion.
 *
 * Las rutas se pasan por entorno desde run_native_tests.sh; los defaults son
 * relativos al worktree para poder correr el binario a mano desde la raiz.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "test_framework.h"

static char * read_file(const char *path) {
    FILE *f = fopen(path, "rb");

    if (f == NULL) {
        printf("    no se pudo abrir %s\n", path);
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }

    long n = ftell(f);

    if (n < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    char *buf = malloc((size_t) n + 1);

    if (buf == NULL) {
        fclose(f);
        return NULL;
    }

    size_t got = fread(buf, 1, (size_t) n, f);

    fclose(f);
    buf[got] = 0;

    return buf;
}

static const char * src_path(const char *env, const char *fallback) {
    const char *p = getenv(env);

    return (p != NULL && *p != 0) ? p : fallback;
}

/* Reemplaza comentarios por espacios (conservando los \n) y deja intactos los
 * literales de cadena/caracter. Asi las palabras espanolas de los comentarios
 * no dan falsos positivos y las busquedas de codigo no matchean comentarios. */
static char * strip_comments(const char *src) {
    size_t n = strlen(src);
    char  *out = malloc(n + 1);

    if (out == NULL) {
        return NULL;
    }

    size_t i = 0, o = 0;

    while (i < n) {
        char c = src[i];

        if (c == '"' || c == '\'') {
            char quote = c;

            out[o++] = src[i++];

            while (i < n) {
                if (src[i] == '\\' && i + 1 < n) {
                    out[o++] = src[i++];
                    out[o++] = src[i++];
                    continue;
                }

                out[o++] = src[i];

                if (src[i++] == quote) {
                    break;
                }
            }

            continue;
        }

        if (c == '/' && i + 1 < n && src[i + 1] == '*') {
            i += 2;

            while (i + 1 < n && !(src[i] == '*' && src[i + 1] == '/')) {
                if (src[i] == '\n') {
                    out[o++] = '\n';
                } else {
                    out[o++] = ' ';
                }

                i++;
            }

            i = (i + 1 < n) ? i + 2 : n;
            continue;
        }

        if (c == '/' && i + 1 < n && src[i + 1] == '/') {
            i += 2;

            while (i < n && src[i] != '\n') {
                out[o++] = ' ';
                i++;
            }

            continue;
        }

        out[o++] = src[i++];
    }

    out[o] = 0;

    return out;
}

static bool is_word_char(char c) {
    return isalnum((unsigned char) c) || c == '_';
}

static bool has_word_ci(const char *hay, const char *word) {
    size_t wl = strlen(word);
    size_t hl = strlen(hay);

    if (wl == 0 || wl > hl) {
        return false;
    }

    for (size_t i = 0; i + wl <= hl; i++) {
        if ((i == 0 || !is_word_char(hay[i - 1])) &&
            strncasecmp(hay + i, word, wl) == 0 &&
            (i + wl == hl || !is_word_char(hay[i + wl]))) {
            return true;
        }
    }

    return false;
}

/* Lista de palabras inequivocamente espanolas (evita "Audio", "Radio", "Color",
 * "No", etc., que son iguales en ingles). */
static const char *spanish_words[] = {
    "ajuste", "ajustes", "frecuencia", "operacion", "pantalla", "banda",
    "memoria", "configuracion", "seleccion", "habilitar", "deshabilitar",
    "silencio", "volumen", "brillo", "tema", "idioma", "velocidad", "umbral",
    "anunciar", "guardar", "cancelar", "aceptar", "cerrar", "ninguno",
    "fuerte", "suave", "encendido", "apagado", "reloj", "potencia", "antena",
    "tono", "ganancia", "filtro", "barrido", "escala", "fuente", "hora",
    "fecha", "zona", "formato", "unidad", "metros", "pies", "grupo", "hijos",
    "navegables", "apertura", "creados", "pulsar", "mantener", "ajustar",
    "siguiente", "anterior", "activar", "desactivar", "reiniciar", "cambiar",
};

static bool literal_spanish(const char *lit) {
    for (const unsigned char *p = (const unsigned char *) lit; *p != 0; p++) {
        if (*p >= 0x80) {
            return true;        /* acentos/ñ/¿¡ en UTF-8 */
        }
    }

    for (size_t i = 0; i < sizeof(spanish_words) / sizeof(spanish_words[0]); i++) {
        if (has_word_ci(lit, spanish_words[i])) {
            return true;
        }
    }

    return false;
}

static void test_settings_strings_english(const char *src) {
    char *clean = strip_comments(src);

    CHECK(clean != NULL);

    if (clean == NULL) {
        return;
    }

    size_t n = strlen(clean);
    size_t i = 0;
    int    strings = 0, bad = 0;

    while (i < n) {
        if (clean[i] != '"') {
            i++;
            continue;
        }

        char   lit[512];
        size_t k = 0;

        i++;

        while (i < n && clean[i] != '"') {
            if (clean[i] == '\\' && i + 1 < n) {
                if (k + 2 < sizeof(lit)) {
                    lit[k++] = clean[i];
                    lit[k++] = clean[i + 1];
                }

                i += 2;
                continue;
            }

            if (k + 1 < sizeof(lit)) {
                lit[k++] = clean[i];
            }

            i++;
        }

        lit[k] = 0;
        strings++;

        if (literal_spanish(lit)) {
            bad++;
            printf("    string en espanol en Settings: \"%s\"\n", lit);
        }

        if (i < n) {
            i++;
        }
    }

    CHECK(strings > 100);       /* el archivo se leyo completo */
    CHECK(bad == 0);
    free(clean);
}

static void test_settings_row_order(const char *src) {
    char *clean = strip_comments(src);

    CHECK(clean != NULL);

    if (clean == NULL) {
        return;
    }

    const char *freq = strstr(clean, "\"Freq acceleration\"");
    const char *accel = strstr(clean, "\"Accel threshold\"");
    const char *unit = strstr(clean, "\"detents/s\"");
    const char *def = strstr(clean, "make_freq_accel_threshold(uint8_t row)");
    const char *call_freq = strstr(clean, "make_freq_accel(row)");
    const char *call_accel = strstr(clean, "make_freq_accel_threshold(row)");
    const char *spin = strstr(clean, "spinbox_uint8(grid, &params.freq_accel_threshold)");
    const char *digits = strstr(clean, "lv_spinbox_set_digit_format(obj, 2, 0)");
    const char *reset = strstr(clean, "main_screen_tuning_reset();");
    const char *voice = strstr(clean, "voice_say_settings_fmt");
    const char *vbegin = strstr(clean, "voice_settings_begin()");
    const char *vend = strstr(clean, "voice_settings_end()");

    CHECK(freq != NULL);
    CHECK(accel != NULL);
    CHECK(unit != NULL);
    CHECK(def != NULL);
    CHECK(call_freq != NULL);
    CHECK(call_accel != NULL);
    CHECK(spin != NULL);
    CHECK(digits != NULL);
    CHECK(reset != NULL);
    CHECK(voice != NULL);
    CHECK(vbegin != NULL);
    CHECK(vend != NULL);
    CHECK(strstr(clean, ".destruct_cb = settings_destruct_cb") != NULL);

    if (freq != NULL && accel != NULL) {
        CHECK(freq < accel);
    }

    if (call_freq != NULL && call_accel != NULL) {
        CHECK(call_freq < call_accel);

        /* Inmediatamente despues: entre las dos llamadas solo queda el cierre de
         * la sentencia y la asignacion `row =` de la fila siguiente. */
        bool only_glue = true;

        for (const char *p = call_freq + strlen("make_freq_accel(row)"); p < call_accel; p++) {
            char c = *p;

            if (!isspace((unsigned char) c) && c != ';' && c != 'r' && c != 'o' &&
                c != 'w' && c != '=') {
                only_glue = false;
                break;
            }
        }

        CHECK(only_glue);
    }

    /* El callback del spinbox usa params_uint8_set(var, ...) y el valor llega
     * desde lv_spinbox_get_value(): no hay un guardado paralelo. */
    CHECK(strstr(clean, "params_uint8_set(var, lv_spinbox_get_value(obj))") != NULL);
    CHECK(strstr(clean, "params_uint8_set(var, lv_spinbox_get_value(obj))") < reset);

    free(clean);
}

static void test_params_wiring(void) {
    const char *h_path = src_path("X6200_PARAMS_H", "src/params/params.h");
    const char *c_path = src_path("X6200_PARAMS_SRC", "src/params/params.c");
    char       *h = read_file(h_path);
    char       *c = read_file(c_path);

    CHECK(h != NULL);
    CHECK(c != NULL);

    if (h == NULL || c == NULL) {
        free(h);
        free(c);
        return;
    }

    char *hc = strip_comments(h);
    char *cc = strip_comments(c);

    CHECK(hc != NULL);
    CHECK(cc != NULL);

    if (hc != NULL && cc != NULL) {
        const char *field = strstr(hc, "freq_accel_threshold;");
        const char *prev = strstr(hc, "freq_accel;");

        CHECK(field != NULL);
        CHECK(prev != NULL);

        if (field != NULL && prev != NULL) {
            CHECK(prev < field);
        }

        CHECK(strstr(cc, ".min = 3") != NULL);
        CHECK(strstr(cc, ".max = 15") != NULL);
        CHECK(strstr(cc, ".name = \"freq_accel_threshold\"") != NULL);
        CHECK(strstr(cc, ".voice = \"Acceleration starts at\"") != NULL);
        CHECK(strstr(cc, "params_load_uint8(&params.freq_accel_threshold") != NULL);
        CHECK(strstr(cc, "params_save_uint8(&params.freq_accel_threshold)") != NULL);
        CHECK(strstr(cc, "params_freq_accel_threshold_sanitize()") != NULL);
        CHECK(strstr(cc, "adaptive_tuning_threshold_sanitize(params.freq_accel_threshold.x)") != NULL);
        CHECK(strstr(cc, "params_unlock(&params.freq_accel_threshold.dirty)") != NULL);
    }

    free(hc);
    free(cc);
    free(h);
    free(c);
}

static void test_rotary_wiring(void) {
    const char *c_path = src_path("X6200_ROTARY_SRC", "src/rotary.c");
    const char *h_path = src_path("X6200_ROTARY_H", "src/rotary.h");
    const char *q_path = src_path("X6200_ROTARY_QUEUE_H", "src/rotary_queue.h");
    char       *c = read_file(c_path);
    char       *h = read_file(h_path);
    char       *q = read_file(q_path);

    CHECK(c != NULL);
    CHECK(h != NULL);
    CHECK(q != NULL);

    if (c == NULL || h == NULL || q == NULL) {
        free(c);
        free(h);
        free(q);
        return;
    }

    char *cc = strip_comments(c);
    char *hc = strip_comments(h);
    char *qc = strip_comments(q);

    CHECK(cc != NULL);
    CHECK(hc != NULL);
    CHECK(qc != NULL);

    if (cc != NULL && hc != NULL && qc != NULL) {
        /* Estado por instancia, no static de archivo. */
        CHECK(strstr(hc, "int32_t             remain_diff;") != NULL);
        CHECK(strstr(hc, "lv_indev_state_t    prev_state;") != NULL);
        CHECK(strstr(qc, "#define ROTARY_QUEUE_SIZE   512") != NULL);
        CHECK(strstr(qc, "int32_t     diff;") != NULL);
        CHECK(strstr(qc, "uint64_t    timestamp_us;") != NULL);

        /* El lector VOL/MFK no toca el motor ni el evento del dial. */
        const char *reader = strstr(cc, "static void rotary_input_read");
        const char *init = strstr(cc, "rotary_t * rotary_init");

        CHECK(reader != NULL);
        CHECK(init != NULL);

        if (reader != NULL && init != NULL && reader < init) {
            char saved = *init;

            *(char *) init = 0;

            CHECK(strstr(reader, "adaptive_tuning_") == NULL);
            CHECK(strstr(reader, "EVENT_ROTARY_ENHANCED") == NULL);
            CHECK(strstr(reader, "lv_event_send") == NULL);

            *(char *) init = saved;
        }

        /* El dial principal: cola, clock del kernel y evento con payload. */
        CHECK(strstr(cc, "EVIOCSCLOCKID") != NULL);
        CHECK(strstr(cc, "CLOCK_MONOTONIC") != NULL);
        CHECK(strstr(cc, "rotary_queue_drain") != NULL);
        CHECK(strstr(cc, "adaptive_tuning_segments") != NULL);
        CHECK(strstr(cc, "EVENT_ROTARY_ENHANCED") != NULL);
        CHECK(strstr(cc, "lv_event_send") != NULL);
    }

    free(cc);
    free(hc);
    free(qc);
    free(c);
    free(h);
    free(q);
}

static void test_vfo_and_cb_wiring(void) {
    const char *path = src_path("X6200_MAIN_SCREEN_SRC", "src/main_screen.c");
    const char *h_path = src_path("X6200_MAIN_SCREEN_H", "src/main_screen.h");
    char       *src = read_file(path);
    char       *h = read_file(h_path);

    CHECK(src != NULL);
    CHECK(h != NULL);

    if (src == NULL || h == NULL) {
        free(src);
        free(h);
        return;
    }

    char *clean = strip_comments(src);
    char *hc = strip_comments(h);

    CHECK(clean != NULL);
    CHECK(hc != NULL);

    if (clean != NULL && hc != NULL) {
        CHECK(strstr(clean, "adaptive_tuning_target_freq(freq, enhanced->weighted_detents, step, &target)") != NULL);
        CHECK(strstr(clean, "radio_check_freq(target)") != NULL);
        CHECK(strstr(clean, "cb_step_channel(plan, freq, enhanced->raw_diff)") != NULL);
        CHECK(strstr(clean, "dialog_rotary(enhanced->raw_diff)") != NULL);
        CHECK(strstr(clean, "adaptive_tuning_target_freq(freq, diff,") != NULL);
        CHECK(strstr(clean, "align_int") == NULL);
        CHECK(strstr(clean, "effective_step") == NULL);
        CHECK(strstr(clean, "freq_accel(") == NULL);
        CHECK(strstr(clean, "hide_tuning_multiplier()") != NULL);
        CHECK(strstr(clean, "main_screen_tuning_reset") != NULL);
        CHECK(strstr(hc, "void main_screen_tuning_reset(void);") != NULL);
    }

    free(clean);
    free(hc);
    free(src);
    free(h);
}

static void test_voice_wiring(void) {
    const char *c_path = src_path("X6200_VOICE_C", "src/voice.c");
    const char *h_path = src_path("X6200_VOICE_H", "src/voice.h");
    char       *c = read_file(c_path);
    char       *h = read_file(h_path);

    CHECK(c != NULL);
    CHECK(h != NULL);

    if (c != NULL && h != NULL) {
        CHECK(strstr(h, "voice_say_settings_fmt") != NULL);
        CHECK(strstr(h, "voice_settings_begin") != NULL);
        CHECK(strstr(h, "voice_settings_end") != NULL);
        CHECK(strstr(c, "cola_en") != NULL);
        CHECK(strstr(c, "sintetizar_y_reproducir_lang(texto, false, \"en\")") != NULL);
        CHECK(strstr(c, "settings_abierto") != NULL);
    }

    free(c);
    free(h);
}

void test_settings_language(void) {
    const char *settings_path = src_path("X6200_SETTINGS_SRC", "src/dialog_settings.c");
    char       *settings = read_file(settings_path);

    CHECK(settings != NULL);

    if (settings != NULL) {
        test_settings_strings_english(settings);
        test_settings_row_order(settings);
        free(settings);
    }

    test_params_wiring();
    test_rotary_wiring();
    test_vfo_and_cb_wiring();
    test_voice_wiring();
}
