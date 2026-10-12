/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - anuncios por voz (ver voice.h).
 *
 *  Cómo habla: se arma el texto, se pasa por el diccionario al español y un hilo propio corre
 *  `espeak --stdout` (sin shell: fork + execv, el texto va por stdin) escribiendo un WAV, que
 *  después reproduce `aplay`. El hilo duerme en una condición cuando no hay nada que decir
 *  (nada de polling), así que el costo en reposo es cero.
 */

#include "voice.h"
#include "voice_text.h"

#include "backlight.h"
#include "params/params.h"
#include "radio.h"

#include "lvgl/lvgl.h"

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define VOICE_QUEUE     8
#define VOICE_TEXT_MAX  160

static char            cola[VOICE_QUEUE][VOICE_TEXT_MAX];
static int             cola_cabeza;
static int             cola_cola;
static pthread_mutex_t mux = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  hay = PTHREAD_COND_INITIALIZER;
static pthread_t       hilo;
static bool            hilo_listo;
static const char     *idioma = "es";

/* ¿Corresponde hablar ahora? (ajuste + pantalla + transmisión) */
static bool puede_hablar(void) {
    if (!voice_should_speak(params.voice_mode.x, backlight_is_on())) {
        return false;
    }

    /* Al aire no: el anuncio se oiría en la transmisión */
    if (radio_get_state() == RADIO_TX) {
        return false;
    }

    return true;
}

/* Corre espeak y después aplay. Sin shell: el texto va por stdin. */
static void sintetizar_y_reproducir(const char *texto, bool forzar) {
    char  wav[] = "/tmp/voice_XXXXXX";
    int   fd    = mkstemp(wav);
    char *argv_espeak[] = {"espeak", "-v", (char *) idioma, "--stdout", NULL};
    pid_t pid;

    if (fd < 0) {
        return;
    }

    if (!forzar && !puede_hablar()) {
        close(fd);
        unlink(wav);
        return;
    }

    pid = fork();

    if (pid == 0) {
        int entrada[2];

        if (pipe(entrada) != 0) {
            _exit(1);
        }

        dup2(fd, STDOUT_FILENO);
        dup2(entrada[0], STDIN_FILENO);
        close(fd);
        close(entrada[0]);
        close(entrada[1]);
        execv("/usr/bin/espeak", argv_espeak);
        _exit(127);
    }

    if (pid > 0) {
        /* el texto va por stdin: no hay shell ni comillas que escapar */
        FILE *f = fdopen(fd, "w");

        if (f != NULL) {
            fputs(texto, f);
            fclose(f);
        } else {
            close(fd);
        }

        waitpid(pid, NULL, 0);
        close(fd);

        pid_t play = fork();

        if (play == 0) {
            char *argv_aplay[] = {"aplay", "-q", wav, NULL};

            execv("/usr/bin/aplay", argv_aplay);
            _exit(127);
        }

        if (play > 0) {
            waitpid(play, NULL, 0);
        }
    } else {
        close(fd);
    }

    unlink(wav);
}

static void *hilo_voz(void *arg) {
    (void) arg;

    while (1) {
        char texto[VOICE_TEXT_MAX];
        char traducido[VOICE_TEXT_MAX * 2];

        pthread_mutex_lock(&mux);

        while (cola_cabeza == cola_cola) {
            pthread_cond_wait(&hay, &mux);      /* duerme: costo cero en reposo */
        }

        snprintf(texto, sizeof(texto), "%s", cola[cola_cabeza]);
        cola_cabeza = (cola_cabeza + 1) % VOICE_QUEUE;
        pthread_mutex_unlock(&mux);

        voice_translate(texto, traducido, sizeof(traducido));
        sintetizar_y_reproducir(traducido, false);
    }

    return NULL;
}

/* Encola un texto ya armado */
static void encolar(const char *texto) {
    if (texto == NULL || *texto == 0) {
        return;
    }

    if (!puede_hablar()) {
        return;
    }

    if (!hilo_listo) {
        if (pthread_create(&hilo, NULL, hilo_voz, NULL) != 0) {
            return;
        }

        pthread_detach(hilo);
        hilo_listo = true;
    }

    pthread_mutex_lock(&mux);
    snprintf(cola[cola_cola], VOICE_TEXT_MAX, "%s", texto);
    cola_cola = (cola_cola + 1) % VOICE_QUEUE;
    pthread_cond_signal(&hay);
    pthread_mutex_unlock(&mux);
}

/* Prueba de voz sin pasar por la UI: con X6200_VOICE_TEST="texto" dice esa frase al arrancar.
 * La usan los scripts de diagnóstico y sirve para comprobar el TTS (y el audio) en el equipo. */
void voice_selftest(void) {
    const char *texto = getenv("X6200_VOICE_TEST");

    if (texto != NULL && *texto != 0) {
        LV_LOG_USER("Prueba de voz: \"%s\"", texto);
        sintetizar_y_reproducir(texto, true);
    }
}

void voice_sure(void) {
}

void voice_change_mode(void) {
    static const char *nombres[] = {"Off", "When LCD off", "Always"};
    int modo = params.voice_mode.x;

    modo = (modo + 1) % 3;
    params.voice_mode.x = modo;

    /* El aviso se dice siempre, aunque el modo nuevo sea Off: si no, no se enteraría */
    sintetizar_y_reproducir(nombres[modo], true);
}

const char * voice_change(int16_t diff) {
    static const char *nombres[] = {"Off", "When LCD off", "Always"};
    int modo = params.voice_mode.x;

    modo = (modo + (diff > 0 ? 1 : diff < 0 ? 2 : 0)) % 3;
    params.voice_mode.x = modo;

    return nombres[modo];
}

void voice_say_text_fmt(const char * fmt, ...) {
    char    texto[VOICE_TEXT_MAX];
    va_list ap;

    if (fmt == NULL) {
        return;
    }

    va_start(ap, fmt);
    vsnprintf(texto, sizeof(texto), fmt, ap);
    va_end(ap);
    encolar(texto);
}

void voice_delay_say_text_fmt(const char * fmt, ...) {
    char    texto[VOICE_TEXT_MAX];
    va_list ap;

    if (fmt == NULL) {
        return;
    }

    va_start(ap, fmt);
    vsnprintf(texto, sizeof(texto), fmt, ap);
    va_end(ap);

    /* La cola es serie: "con retardo" es simplemente encolar */
    encolar(texto);
}

void voice_say_freq(uint64_t freq) {
    char texto[VOICE_TEXT_MAX];

    voice_freq_text(freq, texto, sizeof(texto));
    encolar(texto);
}

void voice_say_bool(const char *prompt, bool x) {
    voice_say_text_fmt("%s: %s", prompt ? prompt : "", x ? "on" : "off");
}

void voice_say_int(const char *prompt, int32_t x) {
    voice_say_text_fmt("%s: %i", prompt ? prompt : "", x);
}

void voice_say_float(const char *prompt, float x) {
    voice_say_text_fmt("%s: %.1f", prompt ? prompt : "", (double) x);
}

void voice_say_float2(const char *prompt, float x) {
    voice_say_text_fmt("%s: %.2f", prompt ? prompt : "", (double) x);
}

void voice_say_text(const char *prompt, const char *x) {
    if (prompt != NULL && x != NULL) {
        voice_say_text_fmt("%s: %s", prompt, x);
    } else if (x != NULL) {
        voice_say_text_fmt("%s", x);
    }
}

void voice_say_lang(void) {
    /* El motor arranca en español; cambiarlo en caliente complica más de lo que aporta */
    idioma = "en";
    voice_say_text_fmt("Voice language: english");
}
