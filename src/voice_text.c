/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - lo que la voz dice (ver voice_text.h).
 */

#include "voice_text.h"

#include <stdio.h>
#include <string.h>

/* Los WAV de espeak son PCM 16 bits; se busca el bloque "data" y se listo. */
bool voice_wav_pcm(const unsigned char *wav, size_t n, size_t *off, size_t *len, unsigned *rate) {
    if (wav == NULL || n < 44 || off == NULL || len == NULL || rate == NULL) {
        return false;
    }

    if (memcmp(wav, "RIFF", 4) != 0 || memcmp(wav + 8, "WAVE", 4) != 0) {
        return false;
    }

    size_t   pos = 12;
    unsigned canales = 1, bits = 16, sr = 22050;
    bool     hay_formato = false;

    while (pos + 8 <= n) {
        const unsigned char *id  = wav + pos;
        uint32_t             tam = (uint32_t) wav[pos + 4] | ((uint32_t) wav[pos + 5] << 8) |
                                   ((uint32_t) wav[pos + 6] << 16) | ((uint32_t) wav[pos + 7] << 24);

        if (memcmp(id, "fmt ", 4) == 0 && tam >= 16 && pos + 8 + 16 <= n) {
            const unsigned char *f = wav + pos + 8;

            canales = (unsigned) (f[2] | (f[3] << 8));
            sr      = (unsigned) (f[4] | (f[5] << 8) | ((unsigned) f[6] << 16) | ((unsigned) f[7] << 24));
            bits    = (unsigned) (f[14] | (f[15] << 8));
            hay_formato = true;
        } else if (memcmp(id, "data", 4) == 0) {
            if (!hay_formato || bits != 16) {
                return false;       /* sólo se maneja PCM de 16 bits */
            }

            size_t disponible = n - (pos + 8);

            *off  = pos + 8;
            *len  = tam < disponible ? (size_t) tam : disponible;
            *rate = sr;
            (void) canales;
            return *len > 0;
        }

        pos += 8 + tam + (tam & 1);     /* los bloques van alineados a 2 */
    }

    return false;
}

size_t voice_resample(const int16_t *in, size_t n_in, unsigned rate_in, int16_t *out, size_t out_max, unsigned rate_out) {
    size_t escritas = 0;

    if (in == NULL || out == NULL || n_in == 0 || rate_in == 0 || rate_out == 0) {
        return 0;
    }

    for (size_t i = 0; i < n_in && escritas < out_max; i++) {
        /* Cuántas muestras de salida caen en esta de entrada */
        size_t desde = ((uint64_t) i * rate_out) / rate_in;
        size_t hasta = ((uint64_t) (i + 1) * rate_out) / rate_in;

        for (size_t k = desde; k < hasta && escritas < out_max; k++) {
            out[escritas++] = in[i];
        }
    }

    return escritas;
}

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
