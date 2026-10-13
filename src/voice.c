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

#include "audio.h"
#include "backlight.h"
#include "params/params.h"
#include "radio.h"

#include "lvgl/lvgl.h"

#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
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
static bool            despierta;      /* la GUI termino de arrancar: recien ahi se habla */
static const char     *idioma = "es";

/* ¿Corresponde hablar ahora? (ajuste + pantalla + transmisión) */
/* Diagnostico de la voz: se anota por que habla o por que no, y el resultado de mandarla al
 * audio. Sin esto, "no se escucha nada" no dice donde se corta. Se lee en
 * /mnt/app_logs/voice.log (queda en la particion DATA, se puede mirar desde la PC). */
static void voz_log(const char *fmt, ...) {
    FILE *f = fopen("/mnt/app_logs/voice.log", "a");

    if (f == NULL) {
        return;
    }

    va_list ap;

    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

static bool puede_hablar(void) {
    if (!despierta) {
        /* Al arrancar la pantalla principal anuncia varias cosas (las llamadas ya estaban en el
         * codigo, mudas por el stub): no se dicen hasta que la GUI esta en pie. */
        return false;
    }

    if (!voice_should_speak(params.voice_mode.x, backlight_is_on())) {
        return false;
    }

    /* Al aire no: el anuncio se oiría en la transmisión */
    if (radio_get_state() == RADIO_TX) {
        return false;
    }

    return true;
}

/* Sintetiza y lo manda al MISMO stream de la GUI (audio_play), que es el que ya usa el audio
 * de FT8/JS8 y anda en la radio. Nada de aplay: la GUI tiene tomada la placa.
 *
 * espeak escribe un WAV de 16 bits a 22050 Hz; se saca el bloque de datos, se remuestrea a la
 * frecuencia del stream y se escribe en trozos: audio_play() espera a que haya lugar y con
 * bloques grandes nunca lo encuentra (ver el comentario de dialog_js8.c). */
static void sintetizar_y_reproducir(const char *texto, bool forzar) {
    char  archivo[] = "/tmp/voice_txt_XXXXXX";
    int   fd;
    int   tubo[2];
    pid_t pid;
    char  arg_lang[8];
    char *argv_espeak[] = {"espeak", "-v", arg_lang, "-f", archivo, "--stdout", NULL};

    if (!forzar && !puede_hablar()) {
        return;
    }

    fd = mkstemp(archivo);

    if (fd < 0) {
        return;
    }

    if (write(fd, texto, strlen(texto)) < 0) {
        close(fd);
        unlink(archivo);
        return;
    }

    close(fd);

    if (pipe(tubo) != 0) {
        unlink(archivo);
        return;
    }

    snprintf(arg_lang, sizeof(arg_lang), "%s", idioma);

    pid = fork();

    if (pid == 0) {
        dup2(tubo[1], STDOUT_FILENO);
        close(tubo[0]);
        close(tubo[1]);
        execv("/usr/bin/espeak", argv_espeak);
        _exit(127);
    }

    close(tubo[1]);

    /* Leer todo el WAV. El texto va por un archivo, asi que no hay baile con stdin. */
    size_t         cap = 1 << 20;
    size_t         n   = 0;
    unsigned char *buf = malloc(cap);

    if (buf != NULL) {
        ssize_t r;

        while ((r = read(tubo[0], buf + n, cap - n)) > 0) {
            n += (size_t) r;

            if (n == cap) {
                unsigned char *nuevo = realloc(buf, cap * 2);

                if (nuevo == NULL) {
                    break;
                }

                buf = nuevo;
                cap *= 2;
            }
        }
    }

    close(tubo[0]);
    waitpid(pid, NULL, 0);
    unlink(archivo);

    if (buf != NULL && n > 44) {
        size_t   off = 0, len = 0;
        unsigned rate = 0;

        if (voice_wav_pcm(buf, n, &off, &len, &rate) && rate > 0) {
            size_t   muestras    = len / 2;
            size_t   salida_max  = (size_t) ((uint64_t) muestras * AUDIO_PLAY_RATE / rate) + 8;
            int16_t *pcm         = malloc(salida_max * sizeof(int16_t));

            if (pcm != NULL) {
                size_t escritas = voice_resample((const int16_t *) (buf + off), muestras, rate,
                                                 pcm, salida_max, AUDIO_PLAY_RATE);

                /* En trozos: un bloque grande no encuentra lugar en el stream */
                for (size_t at = 0; at < escritas; at += 2048) {
                    size_t parte = escritas - at > 2048 ? 2048 : escritas - at;

                    if (audio_play(pcm + at, parte) < 0) {
                        break;
                    }
                }

                if (escritas > 0) {
                    audio_play_wait();
                }

                voz_log("sintetizado: %zu muestras de %u Hz (reloj de la GUI: %i Hz)%s",
                        escritas, rate, AUDIO_PLAY_RATE,
                        escritas == 0 ? " - NO SE ESCRIBIO NADA AL AUDIO" : "");

                LV_LOG_USER("Voz: %zu muestras a %i Hz (%u -> %i)", escritas, AUDIO_PLAY_RATE, rate, AUDIO_PLAY_RATE);
                free(pcm);
            }
        }
    }

    free(buf);
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
        voz_log("descartado: modo=%i lcd=%i despierta=%i tx=%i [%s]",
                params.voice_mode.x, backlight_is_on(), despierta,
                radio_get_state() == RADIO_TX, texto);
        return;
    }

    voz_log("encolado: modo=%i lcd=%i [%s]", params.voice_mode.x, backlight_is_on(), texto);

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

void voice_ready(void) {
    despierta = true;
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
