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
}
