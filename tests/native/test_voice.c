/*
 * Unit tests: lo que la voz dice (voice_text.c).
 *
 * La lógica que importa y se puede probar sin motor: cuándo hay que hablar según el ajuste
 * Off / When LCD off / Always, cómo se deletrea una frecuencia y la traducción de los textos.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <string.h>

#include "test_framework.h"
#include "voice_text.h"

void test_voice(void) {
    /* Off nunca habla */
    CHECK(!voice_should_speak(VOICE_MODE_OFF, true));
    CHECK(!voice_should_speak(VOICE_MODE_OFF, false));

    /* "When LCD off" habla sólo con la pantalla apagada */
    CHECK(!voice_should_speak(VOICE_MODE_LCD, true));
    CHECK(voice_should_speak(VOICE_MODE_LCD, false));

    /* "Always" siempre */
    CHECK(voice_should_speak(VOICE_MODE_ALWAYS, true));
    CHECK(voice_should_speak(VOICE_MODE_ALWAYS, false));

    /* un modo desconocido no habla */
    CHECK(!voice_should_speak(99, false));

    /* la frecuencia se deletrea dígito por dígito */
    char buf[64];

    voice_freq_text(7170000, buf, sizeof(buf));
    CHECK(strstr(buf, "7") != NULL);
    CHECK(strstr(buf, "punto") != NULL);
    CHECK(strstr(buf, "megahertz") != NULL);

    voice_freq_text(14074000, buf, sizeof(buf));
    CHECK(strstr(buf, "1") != NULL);
    CHECK(strstr(buf, "megahertz") != NULL);

    /* la traducción cambia las palabras del diccionario y deja el resto */
    char out[64];

    voice_translate("Volume: 5", out, sizeof(out));
    CHECK(strstr(out, "Volumen") != NULL);
    CHECK(strstr(out, "5") != NULL);

    voice_translate("Battery voltage", out, sizeof(out));
    CHECK(strstr(out, "Bateria") != NULL);

    voice_translate("algo raro", out, sizeof(out));
    CHECK(strcmp(out, "algo raro") == 0);

    /* un WAV de 16 bits: se encuentra el bloque de datos */
    static unsigned char wav[44 + 8];
    memcpy(wav, "RIFF", 4); memcpy(wav + 8, "WAVE", 4);
    memcpy(wav + 12, "fmt ", 4);
    wav[16] = 16;                                   /* tamaño del bloque fmt */
    wav[20] = 1; wav[22] = 1;                       /* PCM, 1 canal */
    wav[24] = 0x22; wav[25] = 0x56;                 /* 22050 Hz = 0x5622 */
    wav[34] = 16;                                   /* 16 bits */
    memcpy(wav + 36, "data", 4);
    wav[40] = 8;                                    /* 8 bytes de datos = 4 muestras */

    size_t   off = 0, len = 0;
    unsigned rate = 0;

    CHECK(voice_wav_pcm(wav, sizeof(wav), &off, &len, &rate));
    CHECK(rate == 22050);
    CHECK(off == 44);
    CHECK(len == 8);

    /* no es un WAV */
    CHECK(!voice_wav_pcm((const unsigned char *) "no soy wav", 10, &off, &len, &rate));

    /* el remuestreo 22050 -> 44100 duplica cada muestra */
    int16_t in[4] = {100, -200, 300, 4};
    int16_t out2[16];

    CHECK(voice_resample(in, 4, 22050, out2, 16, 44100) == 8);
    CHECK(out2[0] == 100 && out2[1] == 100);
    CHECK(out2[2] == -200 && out2[3] == -200);
    CHECK(out2[4] == 300 && out2[5] == 300);

    /* y no se pasa del buffer */
    CHECK(voice_resample(in, 4, 22050, out2, 3, 44100) == 3);
}
