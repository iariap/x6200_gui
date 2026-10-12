/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - lo que la voz dice (ver voice_text.h).
 */

#include "voice_text.h"

#include <stdio.h>
#include <string.h>

bool voice_should_speak(int mode, bool lcd_on) {
    switch (mode) {
        case VOICE_MODE_ALWAYS:
            return true;

        case VOICE_MODE_LCD:
            /* "When LCD off": se habla justo cuando no se ve la pantalla */
            return !lcd_on;

        default:
            return false;
    }
}

void voice_freq_text(uint64_t hz, char *buf, size_t n) {
    char digitos[32];
    int  k = 0;

    if (buf == NULL || n == 0) {
        return;
    }

    /* La parte entera en MHz y tres decimales, dígito por dígito */
    unsigned long mhz  = (unsigned long) (hz / 1000000ULL);
    unsigned long frac = (unsigned long) ((hz % 1000000ULL) / 100);      /* kHz */

    if (mhz > 99999UL) {
        mhz = 99999UL;
    }

    snprintf(digitos, sizeof(digitos), "%lu", mhz);

    k = snprintf(buf, n, "%s", "");
    for (int i = 0; digitos[i] != 0 && (size_t) k < n; i++) {
        k += snprintf(buf + k, n - k, "%s%c", i ? " " : "", digitos[i]);
    }

    if ((size_t) k < n) {
        k += snprintf(buf + k, n - k, " punto");
    }

    /* tres dígitos de la parte decimal, uno por uno */
    char dec[8];

    snprintf(dec, sizeof(dec), "%03lu", frac);
    for (int i = 0; i < 3 && (size_t) k < n; i++) {
        k += snprintf(buf + k, n - k, " %c", dec[i]);
    }

    if ((size_t) k < n) {
        snprintf(buf + k, n - k, ", megahertz");
    }
}

/* Diccionario de palabras: alcanza con traducir las palabras de los 119 textos de la GUI, no
 * cada frase entera. Lo que no está, se dice como vino. */
static const struct {
    const char *en;
    const char *es;
} palabras[] = {
    {"Volume", "Volumen"},        {"volume", "volumen"},
    {"Band", "Banda"},            {"band", "banda"},
    {"Mode", "Modo"},             {"mode", "modo"},
    {"Power", "Potencia"},        {"power", "potencia"},
    {"Battery", "Bateria"},       {"voltage", "voltaje"},
    {"Antenna", "Antena"},        {"antenna", "antena"},
    {"Filter", "Filtro"},         {"filter", "filtro"},
    {"Spectrum", "Espectro"},     {"spectrum", "espectro"},
    {"Attenuator", "Atenuador"},  {"att", "atenuador"},
    {"Auto", "Auto"},             {"gain", "ganancia"},
    {"hang", "retension"},        {"knee", "codo"},
    {"slope", "pendiente"},       {"level", "nivel"},
    {"switcher", "conmutador"},   {"selector", "selector"},
    {"Window", "Ventana"},        {"window", "ventana"},
    {"Off", "apagado"},           {"On", "encendido"},
    {"off", "apagado"},           {"on", "encendido"},
    {"Tuner", "Sintonizador"},    {"tuner", "sintonizador"},
    {"Charger", "Cargador"},      {"charger", "cargador"},
    {"Decoder", "Decodificador"}, {"decoder", "decodificador"},
    {"Recorder", "Grabadora"},    {"recorder", "grabadora"},
    {"Audio", "Audio"},           {"Callsign", "Indicativo"},
    {"Compressor", "Compresor"},  {"Comp", "Compresor"},
    {"Clock", "Reloj"},           {"clone", "clonado"},
    {"Cloned", "Clonado"},        {"Slow", "lento"},
    {"Fast", "rapido"},           {"Auto gain", "Ganancia automatica"},
    {"Notch", "Notch"},           {"Text", "Texto"},
    {"Selection", "Seleccion"},   {"Edit", "Edicion"},
    {"Min", "Minimo"},            {"Max", "Maximo"},
    {"dB", "decibeles"},          {"Hz", "hertz"},
};

void voice_translate(const char *in, char *buf, size_t n) {
    size_t k = 0;

    if (buf == NULL || n == 0) {
        return;
    }

    if (in == NULL) {
        buf[0] = 0;
        return;
    }

    while (*in != 0 && k + 1 < n) {
        /* ¿empieza alguna palabra del diccionario acá? */
        size_t i;

        for (i = 0; i < sizeof(palabras) / sizeof(*palabras); i++) {
            size_t len = strlen(palabras[i].en);

            if (strncmp(in, palabras[i].en, len) == 0) {
                int escrito = snprintf(buf + k, n - k, "%s", palabras[i].es);

                if (escrito > 0) {
                    k += (size_t) escrito;
                }

                in += len;
                break;
            }
        }

        if (i == sizeof(palabras) / sizeof(*palabras)) {
            buf[k++] = *in++;
        }
    }

    buf[k < n ? k : n - 1] = 0;
}
