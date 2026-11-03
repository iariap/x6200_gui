/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */
#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#ifdef X6200_RX_SPEAKER_QA
#include <stdarg.h>
#include <stdatomic.h>
#include <inttypes.h>
#include <time.h>
#include "radio.h"
#endif

#include <pulse/pulseaudio.h>
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>

#include "lvgl/lvgl.h"
#include "audio.h"
#include "meter.h"
#include "dsp.h"
#include "params/params.h"
#include "audio_enhancement.h"
#include "screen_rec.h"
#include "rx_audio_live.h"

#define AUDIO_RATE_MS   100

static bool                  audio_up = false;     /* pulseaudio en condiciones */
static pa_threaded_mainloop *mloop;
static pa_mainloop_api      *mlapi;
static pa_context           *ctx;

static pa_stream            *play_stm;
static char                 *play_device = "alsa_output.platform-sound.stereo-fallback";

static pa_stream            *capture_stm;
static char                 *capture_device = "alsa_input.platform-sound.stereo-fallback";

static pa_stream            *monitor_stm = NULL;

/* Router RX live (#364): stream propio, separado de play_stm (TTS/audio generado).
 * El backend MUX queda fail-closed hasta que #363 acredite A/B/C en hardware. */
#ifndef X6200_AUDIO_CALLBACK_TEST
static pa_stream            *rx_live_stm = NULL;
#endif

#ifdef X6200_RX_SPEAKER_QA
/* QA6: contadores atomicos para no bloquear el callback de captura. */
static atomic_uint_fast64_t qa_cap_samples;
static atomic_uint_fast64_t qa_cap_real_samples;
static atomic_uint_fast64_t qa_cap_calls;
static atomic_uint_fast64_t qa_cap_holes;
static atomic_uint_fast64_t qa_cap_last_ms;
static atomic_uint          qa_cap_max_interval_ms;
static atomic_uint          qa_cap_gaps;

static atomic_uint          qa_pa_write_calls;
static atomic_uint          qa_pa_write_ok;
static atomic_uint          qa_pa_write_nospace;
static atomic_uint          qa_pa_write_errors;
static atomic_uint          qa_pa_writable_min;
static atomic_uint          qa_pa_writable_max;
/* QA10: fase de prueba de eco. 0=off, 1=ZERO, 2=RAW, 3=PROCESSED. */
static atomic_int           qa_echo_phase;
static atomic_int           qa_echo_single_phase;
static atomic_int           qa_echo_preset_off;
static atomic_bool          qa_hw_running;
static atomic_bool          qa_hw_abort;
static atomic_int           qa_hw_only_phase;
#endif

/* Cuando el stream de reproducción no acepta datos (el sink real no consume: ver HALLAZGOS),
 * se anota hasta cuándo no insistir. Sin esto, cada bloque de audio de FT8/JS8/voz pagaría su
 * propio tope de 2 s y el que manda audio se quedaría esperando minutos. */
static uint64_t              play_sin_lugar_hasta = 0;

static float                peak_db = -60.0f;

static void record_monitor_setup();

#ifndef X6200_AUDIO_CALLBACK_TEST
static bool rx_live_pa_open(void *arg) {
    (void) arg;
    if (!audio_up || mloop == NULL || ctx == NULL) {
        return false;
    }
    if (rx_live_stm != NULL) {
        return pa_stream_get_state(rx_live_stm) == PA_STREAM_READY;
    }

    pa_sample_spec spec = {
        .format = PA_SAMPLE_S16NE,
        .rate = AUDIO_PLAY_RATE,
        .channels = 1,
    };
    pa_buffer_attr attr;
    memset(&attr, 0xff, sizeof(attr));
    attr.fragsize = pa_usec_to_bytes(AUDIO_RATE_MS * PA_USEC_PER_MSEC, &spec);
    /* Produccion: objetivo inicial de 200 ms (#364), sin acumular segundos de
     * audio viejo. La build QA usa 600 ms para cubrir la latencia real del sink
     * durante las pruebas supervisadas; no se traslada a produccion. */
#ifdef X6200_RX_SPEAKER_QA
    attr.tlength = pa_usec_to_bytes(3000 * PA_USEC_PER_MSEC, &spec);
    /* QA6: el servidor negocio prebuf ~= tlength (132302/134064 B) y el stream
     * quedaba en prebuffer sin avanzar read_index. Forzar prebuf=0 inicia la
     * reproduccion con el primer bloque; produccion no cambia. */
    attr.prebuf = 0;
#else
    attr.tlength = pa_usec_to_bytes(RX_AUDIO_LIVE_TARGET_BUFFER_MS * PA_USEC_PER_MSEC, &spec);
#endif

    pa_threaded_mainloop_lock(mloop);
    rx_live_stm = pa_stream_new(ctx, "X6200 RX Live", &spec, NULL);
    if (rx_live_stm != NULL) {
        if (pa_stream_connect_playback(rx_live_stm, play_device, &attr,
                                       PA_STREAM_ADJUST_LATENCY
#ifdef X6200_RX_SPEAKER_QA
                                       | PA_STREAM_AUTO_TIMING_UPDATE
#endif
                                       , NULL, NULL) < 0) {
            pa_stream_unref(rx_live_stm);
            rx_live_stm = NULL;
        }
    }
    pa_threaded_mainloop_unlock(mloop);

    return rx_live_stm != NULL;
}

static void rx_live_pa_close(void *arg) {
    (void) arg;
    if (rx_live_stm == NULL || mloop == NULL) {
        return;
    }
    pa_threaded_mainloop_lock(mloop);
    pa_stream_disconnect(rx_live_stm);
    pa_stream_unref(rx_live_stm);
    rx_live_stm = NULL;
    pa_threaded_mainloop_unlock(mloop);
}

static bool rx_live_pa_ready(void *arg) {
    bool ready = false;
    pa_stream_state_t st;

    (void) arg;
    if (!audio_up || mloop == NULL || rx_live_stm == NULL) {
        return false;
    }
    pa_threaded_mainloop_lock(mloop);
    st = pa_stream_get_state(rx_live_stm);
    ready = st == PA_STREAM_READY;
    pa_threaded_mainloop_unlock(mloop);
    return ready;
}

static int rx_live_pa_write(void *arg, const int16_t *samples, size_t count) {
    int rc = -1;
    int avail;
    pa_stream_state_t st;

    (void) arg;
    if (!audio_up || mloop == NULL || rx_live_stm == NULL ||
        samples == NULL || count == 0) {
        return RX_AUDIO_LIVE_WRITE_ERROR;
    }
    if (count > (size_t) INT_MAX / sizeof(int16_t)) {
        return RX_AUDIO_LIVE_WRITE_ERROR;
    }

#ifdef X6200_RX_SPEAKER_QA
    atomic_fetch_add(&qa_pa_write_calls, 1u);
#endif
    pa_threaded_mainloop_lock(mloop);
    st = pa_stream_get_state(rx_live_stm);
    avail = pa_stream_writable_size(rx_live_stm);
#ifdef X6200_RX_SPEAKER_QA
    if (avail >= 0) {
        unsigned a = (unsigned) avail;
        unsigned old_min = atomic_load(&qa_pa_writable_min);
        while (old_min > a && !atomic_compare_exchange_weak(&qa_pa_writable_min, &old_min, a)) {
        }
        unsigned old_max = atomic_load(&qa_pa_writable_max);
        while (old_max < a && !atomic_compare_exchange_weak(&qa_pa_writable_max, &old_max, a)) {
        }
    }
#endif
    if (st != PA_STREAM_READY || avail < 0) {
        pa_threaded_mainloop_unlock(mloop);
#ifdef X6200_RX_SPEAKER_QA
        atomic_fetch_add(&qa_pa_write_errors, 1u);
#endif
        return RX_AUDIO_LIVE_WRITE_ERROR;
    }
    if ((size_t) avail < count * sizeof(int16_t)) {
        pa_threaded_mainloop_unlock(mloop);
#ifdef X6200_RX_SPEAKER_QA
        atomic_fetch_add(&qa_pa_write_nospace, 1u);
#endif
        return RX_AUDIO_LIVE_WRITE_NO_SPACE;
    }
    rc = pa_stream_write(rx_live_stm, samples, count * sizeof(int16_t),
                         NULL, 0, PA_SEEK_RELATIVE);
    pa_threaded_mainloop_unlock(mloop);
#ifdef X6200_RX_SPEAKER_QA
    atomic_fetch_add(rc < 0 ? &qa_pa_write_errors : &qa_pa_write_ok, 1u);
#endif

    return (rc < 0) ? RX_AUDIO_LIVE_WRITE_ERROR : RX_AUDIO_LIVE_WRITE_OK;
}

#ifdef X6200_RX_SPEAKER_QA
/* SOLO build QA: autorizacion atomica de entrada a DAC. La restauracion false
 * NUNCA se gatea por esta bandera. */
static atomic_bool rx_qa_authorized;

static bool rx_live_mux_ready(void *arg) {
    (void) arg;
    return atomic_load(&rx_qa_authorized);
}

static int rx_live_mux_request(void *arg, bool dac) {
    (void) arg;
    if (!dac) {
        return radio_audio_route_set(false) ? RX_AUDIO_LIVE_MUX_OK
                                            : RX_AUDIO_LIVE_MUX_UNKNOWN;
    }
    if (!atomic_load(&rx_qa_authorized)) {
        return RX_AUDIO_LIVE_MUX_NOT_ENTERED;
    }
    return radio_audio_route_set(true) ? RX_AUDIO_LIVE_MUX_OK
                                       : RX_AUDIO_LIVE_MUX_NOT_ENTERED;
}
#else
/* Gate de #367: sin readback/restauracion independiente acreditados, el backend
 * real deniega la entrada a DAC. La obligacion de volver a BASE no se gatea:
 * si alguna vez se selecciona DAC, mux_request(false) debe poder ejecutarse. */
static bool rx_live_mux_ready(void *arg) {
    (void) arg;
    return false;
}

static int rx_live_mux_request(void *arg, bool dac) {
    (void) arg;
    (void) dac;
    return RX_AUDIO_LIVE_MUX_NOT_ENTERED;
}
#endif

static const rx_audio_live_backend_t rx_live_backend = {
    .ctx = NULL,
    .open = rx_live_pa_open,
    .close = rx_live_pa_close,
    .ready = rx_live_pa_ready,
    .write = rx_live_pa_write,
    .mux_ready = rx_live_mux_ready,
    .mux_request = rx_live_mux_request,
};
#endif

static void on_state_change(pa_context *c, void *userdata) {
    pa_threaded_mainloop_signal(mloop, 0);
}

/* Scratch del callback: 10 ms de la tasa de captura (4410 muestras) en el STACK.
 * El callback de PulseAudio no reserva memoria dinamica: cualquier nbytes positivo se
 * recorre en chunks de a lo sumo este tamano, asi un fragmento grande no puede ni fallar
 * por heap ni pasarse de la capacidad de dsp.cpp::audio (44100 floats). */
#define CAPTURE_SCRATCH_SAMPLES (AUDIO_CAPTURE_RATE / 10)

#ifdef X6200_RX_SPEAKER_QA
static int16_t qa_zero_block[CAPTURE_SCRATCH_SAMPLES];
#endif

/* El fragmento es de PulseAudio: se consume UNA vez y se anota el error sin reintentar
 * desde el callback (un retry aca solo puede perder el hilo del mainloop). */
static void capture_drop(pa_stream *s) {
    if (pa_stream_drop(s) < 0) {
        LV_LOG_ERROR("pa_stream_drop() fallo: %s", pa_strerror(pa_context_errno(ctx)));
    }
}

/* Entrega un chunk ya copiado en el scratch a los dos consumidores, EN ORDEN y una sola
 * vez: `process` distingue PCM real (filtros/NR antes de entregar) de un hole (ceros que
 * nunca pasan por la cadena, para no avanzar notch/nivelador/NR con silencio inventado). */
#ifdef X6200_RX_SPEAKER_QA
static void rx_qa_capture_observe(size_t samples, bool process);
#endif

static void capture_deliver(int16_t *scratch, size_t samples, bool process) {
#ifdef X6200_RX_SPEAKER_QA
    rx_qa_capture_observe(samples, process);
    int phase = atomic_load(&qa_echo_phase);
    int16_t raw[CAPTURE_SCRATCH_SAMPLES];

    if (samples > CAPTURE_SCRATCH_SAMPLES) {
        samples = CAPTURE_SCRATCH_SAMPLES;
    }
    if (process) {
        memcpy(raw, scratch, samples * sizeof(int16_t));
        audio_enh_process(scratch, samples);
        if (phase == 1) {
            rx_audio_live_submit(qa_zero_block, samples);
        } else if (phase == 2) {
            rx_audio_live_submit(raw, samples);
        } else {
            rx_audio_live_submit(scratch, samples);
        }
    } else if (phase != 0) {
        /* Hole durante la prueba de eco: mantener el stream continuo con cero,
         * sin inventar PCM para los otros consumidores. */
        rx_audio_live_submit(qa_zero_block, samples);
    }
#else
    if (process) {
        audio_enh_process(scratch, samples);
        /* Misma PCM post-Speex para el router RX live (default OFF; DAC gate #367). */
        rx_audio_live_submit(scratch, samples);
    }
#endif

    /* Screen recorder: keeps the RX audio next to the video frames */
    screen_rec_put_audio_samples(samples, scratch);

    dsp_put_audio_samples(samples, scratch);
}

static void read_callback(pa_stream *s, size_t nbytes, void *udata) {
    /* `data` es const a proposito: la memoria del fragmento es de PulseAudio y no se
     * escribe nunca; lo que se procesa es la copia en el scratch. */
    const void *data = NULL;
    int16_t     scratch[CAPTURE_SCRATCH_SAMPLES];
    size_t      samples;
    size_t      done = 0;
    int         rc;

    (void) udata;

    /* El retorno se mira ANTES de tocar cualquier buffer: un error puede dejar `data`
     * y `nbytes` sin definir. */
    rc = pa_stream_peek(s, &data, &nbytes);

    if (rc < 0) {
        LV_LOG_ERROR("pa_stream_peek() fallo: %s", pa_strerror(pa_context_errno(ctx)));
        rx_audio_live_notify_capture_error();
        return;     /* sin drop: no hay fragmento que consumir */
    }

    if (data == NULL && nbytes == 0) {
        return;     /* empty: ni fragmento ni hole, no se dropea nada */
    }

    /* Un nbytes impar no es PCM S16 completo: reinterpretar el byte suelto es leer fuera
     * de la muestra y "completarla" seria inventar audio. Se descarta el fragmento. */
    if (nbytes & 1u) {
        LV_LOG_WARN("Fragmento de captura con %zu bytes (impar): se descarta", nbytes);
        capture_drop(s);
        return;
    }

    samples = nbytes / 2;

    if (data == NULL) {
        /* Hole: PulseAudio no tiene muestras para este tramo (underrun/ajuste de latencia).
         * Se entrega silencio por la duracion representada y se descarta el estado pendiente
         * del NR para que la trama en curso no reproduzca audio viejo despues del hueco. */
        audio_enh_nr_reset_pending();

        while (done < samples) {
            size_t chunk = samples - done;

            if (chunk > CAPTURE_SCRATCH_SAMPLES) {
                chunk = CAPTURE_SCRATCH_SAMPLES;
            }

            memset(scratch, 0, chunk * sizeof(int16_t));
            capture_deliver(scratch, chunk, false);
            done += chunk;
        }

        capture_drop(s);
        return;
    }

    while (done < samples) {
        size_t chunk = samples - done;

        if (chunk > CAPTURE_SCRATCH_SAMPLES) {
            chunk = CAPTURE_SCRATCH_SAMPLES;
        }

        memcpy(scratch, (const int16_t *) data + done, chunk * sizeof(int16_t));
        capture_deliver(scratch, chunk, true);
        done += chunk;
    }

    capture_drop(s);
}

static void mixer_setup() {
    // overall level
    (void) system("amixer sset 'Headphone',0 58,58");
    // Play level from app to radio (for FT8)
    (void) system("amixer sset 'AIF1 DA0',0 160,160");

    // capture audio from radio
    (void) system("amixer sset 'Mic1',0 0,0 cap");
    // mic boost
    (void) system("amixer sset 'Mic1 Boost',0 0");
    // disable capturing from mixer
    (void) system("amixer sset 'Mixer',0 nocap");
    (void) system("amixer sset 'ADC Gain',0 3");
    (void) system("amixer sset 'AIF1 AD0',0 160,160");
    (void) system("amixer sset 'AIF1 AD0 Stereo',0 'Mix Mono'");
    (void) system("amixer sset 'AIF1 Data Digital ADC',0 cap");

    audio_set_rec_vol(0.0f);
    audio_set_play_vol(0.0f);
}

static void * mixer_setup_thread(void *arg) {
    mixer_setup();
    return NULL;
}

/* La conexion con PulseAudio -y la espera a que el contexto este listo- tardaba 1,7 s medidos
 * en la radio, y todo eso pasaba ANTES del primer dibujo: la pantalla se quedaba en el logo.
 * Ahora va en su propio hilo y la GUI dibuja de inmediato. Mientras el audio no este listo,
 * audio_play() devuelve -1 (no entro nada) y no pasa nada: se pierden los primeros anuncios de
 * voz, que es bastante mejor que mirar el logo. Se devuelve -1 y no 0 porque el que llama cuenta
 * lo que de verdad entro al stream (ver la voz): un 0 se leia como "se mando". */
void audio_init_sync(void);      /* el cuerpo que ahora corre en un hilo */

static void *audio_init_thread(void *arg) {
    (void) arg;

    audio_init_sync();
    return NULL;
}

void audio_init() {
    pthread_t t;

    /* La cadena de capture filters se inicializa ACA, de forma SINCRONA y una sola vez,
     * ANTES de lanzar el hilo de PulseAudio: antes vivia dentro de audio_init_sync() y
     * podia terminar despues de que params_init()/dsp_init() aplicaran el preset elegido
     * por el usuario, borrandolo. PA y los `amixer` siguen arrancando en background. */
    audio_enh_init();
#ifndef X6200_AUDIO_CALLBACK_TEST
    rx_audio_live_init(&rx_live_backend);
#endif

    if (pthread_create(&t, NULL, audio_init_thread, NULL) == 0) {
        pthread_detach(t);
    } else {
        LV_LOG_WARN("No se pudo lanzar el hilo del audio; se inicializa en el hilo principal");
        audio_init_sync();
    }
}

bool audio_wait_ready(unsigned timeout_ms) {
    uint64_t limite = pa_rtclock_now() + (uint64_t) timeout_ms * 1000;

    /* No alcanza con que el contexto este arriba: hay que esperar a que el STREAM este READY.
     * Medido el 13/10: con audio_up en true pero el stream todavia sin aceptar, la primera frase
     * se descartaba entera ("al audio 0 de 73204"), porque audio_play() aguanta 2 s y despues
     * tira el bloque. */
    while (pa_rtclock_now() < limite) {
        bool listo;

        if (mloop != NULL) {
            pa_threaded_mainloop_lock(mloop);
        }

        listo = audio_up && play_stm != NULL && pa_stream_get_state(play_stm) == PA_STREAM_READY;

        if (mloop != NULL) {
            pa_threaded_mainloop_unlock(mloop);
        }

        if (listo) {
            return true;
        }

        usleep(20000);      /* 20 ms */
    }

    return false;
}

void audio_init_sync(void) {
    /* Los diez `amixer` y el mixer de ALSA de mixer_setup() pueden tardar, trabarse, o
     * abortar el proceso si el elemento no existe -y todo eso pasaba antes del primer
     * dibujo, con la pantalla quedándose en el logo del equipo-. Va en su propio hilo:
     * la GUI dibuja siempre y los niveles se aplican cuando se pueden. */
    pthread_t mixer_thread;

    if (pthread_create(&mixer_thread, NULL, mixer_setup_thread, NULL) == 0) {
        pthread_detach(mixer_thread);
    } else {
        LV_LOG_WARN("No se pudo lanzar el hilo del mixer");
    }

    mloop = pa_threaded_mainloop_new();
    pa_threaded_mainloop_start(mloop);

    mlapi = pa_threaded_mainloop_get_api(mloop);
    ctx = pa_context_new(mlapi, "X6200 GUI");

    pa_threaded_mainloop_lock(mloop);
    pa_context_set_state_callback(ctx, on_state_change, NULL);
    pa_context_connect(ctx, NULL, 0, NULL);
    pa_threaded_mainloop_unlock(mloop);

    /* Si pulseaudio no está -por ejemplo arrancado en modo sistema sin el usuario
     * `pulse`, que es lo que pasaba en la radio- el contexto pasa a FAILED. Esta
     * espera no terminaba nunca: la GUI se quedaba en el logo del equipo sin dibujar.
     * Ahora se corta, avisa y la GUI sigue andando, sin audio. */
    while (true) {
        pa_context_state_t st = pa_context_get_state(ctx);

        if (st == PA_CONTEXT_READY) {
            break;
        }
        if (!PA_CONTEXT_IS_GOOD(st)) {
            LV_LOG_WARN("pulseaudio no disponible (%s): la GUI sigue, pero sin audio",
                        pa_strerror(pa_context_errno(ctx)));
            return;
        }
        pa_threaded_mainloop_wait(mloop);
    }

    audio_up = true;

    LV_LOG_USER("Conected");

    pa_buffer_attr  attr;

    pa_sample_spec  spec = {
        .format = PA_SAMPLE_S16NE,
        .channels = 1
    };

    memset(&attr, 0xff, sizeof(attr));

    /* Play */

    spec.rate = AUDIO_PLAY_RATE,
    attr.fragsize = pa_usec_to_bytes(AUDIO_RATE_MS * PA_USEC_PER_MSEC, &spec);
    attr.tlength = attr.fragsize * 8;

    play_stm = pa_stream_new(ctx, "X6200 GUI Play", &spec, NULL);

    pa_threaded_mainloop_lock(mloop);
    pa_stream_connect_playback(play_stm, play_device, &attr, PA_STREAM_ADJUST_LATENCY, NULL, NULL);
    pa_threaded_mainloop_unlock(mloop);

    /* Capture */

    spec.rate = AUDIO_CAPTURE_RATE,
    attr.fragsize = attr.tlength = pa_usec_to_bytes(AUDIO_RATE_MS * PA_USEC_PER_MSEC, &spec);

    capture_stm = pa_stream_new(ctx, "X6200 GUI Capture", &spec, NULL);

    pa_threaded_mainloop_lock(mloop);
    pa_stream_set_read_callback(capture_stm, read_callback, NULL);
    pa_stream_connect_record(capture_stm, capture_device, &attr, PA_STREAM_ADJUST_LATENCY);
    pa_threaded_mainloop_unlock(mloop);

    record_monitor_setup();
}

int audio_play(int16_t *samples_buf, size_t samples) {
    if (!audio_up || play_stm == NULL) {
        /* Todavia no hay stream (el audio arranca en un hilo): no entro nada, y se dice. */
        return -1;
    }

    /* Ya se comprobó hace poco que no acepta: no hacer esperar a nadie (ver arriba). */
    if (play_sin_lugar_hasta != 0 && pa_rtclock_now() < play_sin_lugar_hasta) {
        return -1;
    }

    /* Espera a que haya lugar. NO se puede esperar al mainloop sin tope: si el stream no se
     * destraba (pasa cuando el sink real no consume, ver HALLAZGOS), `pa_threaded_mainloop_wait()`
     * no vuelve nunca y el tope de abajo jamás se evalúa: la GUI quedaba colgada en futex_wait.
     * Se duerme 1 ms y se vuelve a preguntar, así el tope es de verdad. */
    bool     hay_lugar = false;
    uint64_t limite    = pa_rtclock_now() + 2000000;      /* 2 s */

    pa_threaded_mainloop_lock(mloop);
    while (pa_rtclock_now() < limite) {
        if (pa_stream_writable_size(play_stm) >= (samples * 2)) {
            hay_lugar = true;
            break;
        }

        pa_threaded_mainloop_unlock(mloop);
        usleep(1000);
        pa_threaded_mainloop_lock(mloop);
    }
    pa_threaded_mainloop_unlock(mloop);

    if (!hay_lugar) {
        /* -1 y no 0: el que llama (la voz, por ejemplo) tiene que poder cortar en vez de
         * mandar bloque tras bloque que se descartan uno por uno. Y se recuerda por 5 s para
         * que el resto de los bloques de la misma frase/transmisión no paguen el tope. */
        play_sin_lugar_hasta = pa_rtclock_now() + 5000000;
        LV_LOG_WARN("El stream de reproducción no acepta datos: se descarta el bloque");
        return -1;
    }

    play_sin_lugar_hasta = 0;

    pa_threaded_mainloop_lock(mloop);
    int res = pa_stream_write(play_stm, samples_buf, samples * 2, NULL, 0, PA_SEEK_RELATIVE);
    pa_threaded_mainloop_unlock(mloop);

    if (res < 0) {
        LV_LOG_ERROR("pa_stream_write() failed: %s", pa_strerror(pa_context_errno(ctx)));
    } else {
        /* Audio generado/TTS/mensajes: prioridad sobre el RX live. */
        rx_audio_live_set_generated_audio(true);
    }

    return res;
}

void audio_play_wait() {
    if (!audio_up || play_stm == NULL) {
        return;
    }


    pa_operation *op;
    int r;

    pa_threaded_mainloop_lock(mloop);
    op = pa_stream_drain(play_stm, NULL, NULL);
    pa_threaded_mainloop_unlock(mloop);

    if (op == NULL) {
        LV_LOG_WARN("No se pudo drenar el stream de reproducción");
        return;
    }

    /* Lo mismo: el drenaje se espera con un tope real, no con un wait sin timeout. */
    uint64_t limite = pa_rtclock_now() + 2000000;         /* 2 s */

    pa_threaded_mainloop_lock(mloop);
    while (pa_rtclock_now() < limite) {
        r = pa_operation_get_state(op);

        if (r == PA_OPERATION_DONE || r == PA_OPERATION_CANCELLED) {
            break;
        }
        if (r == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_unlock(mloop);
            usleep(1000);
            pa_threaded_mainloop_lock(mloop);
        } else {
            break;
        }
    }
    pa_threaded_mainloop_unlock(mloop);

    pa_operation_unref(op);
    rx_audio_live_set_generated_audio(false);
}

void audio_gain_db(int16_t *buf, size_t samples, float gain, int16_t *out) {
    float scale = exp10f(gain / 20.0f);

    for (uint16_t i = 0; i < samples; i++) {
        int32_t x = buf[i] * scale;

        if (x > 32767) {
            x = 32767;
        }

        if (x < -32767) {
            x = -32767;
        }

        out[i] = x;
    }
}

void audio_gain_db_transition(int16_t *buf, size_t samples, float gain1, float gain2, int16_t *out) {
    float scale1 = exp10f(gain1 / 20.0f);
    float scale2 = exp10f(gain2 / 20.0f);
    float scale;
    for (uint16_t i = 0; i < samples; i++) {
        scale = scale1 + i * (scale2 - scale1) / samples;
        int32_t x = buf[i] * scale;

        if (x > 32767) {
            x = 32767;
        }

        if (x < -32767) {
            x = -32767;
        }

        out[i] = x;
    }
}

void audio_play_en(bool on) {
    if (on) {
        /* Para ENTRAR en modo reproduccion hace falta el stream: si PulseAudio no
         * esta listo no se toca el ruteo del MCU (no habria que restaurar despues). */
        if (!audio_up || play_stm == NULL) {
            return;
        }

        rx_audio_live_set_generated_audio(true);
        x6200_control_hmic_set(0);
        x6200_control_imic_set(0);
        x6200_control_record_set(true);
    } else {
        /* La restauracion va SIEMPRE, aunque el stream se haya caido entre el on y
         * el off (#244): si se saltea, el BASE queda en modo record/mic y el audio de
         * RX no vuelve hasta un power-cycle. */
        rx_audio_live_set_generated_audio(false);
        x6200_control_record_set(false);
        x6200_control_hmic_set(params.hmic);
        x6200_control_imic_set(params.imic);
    }
}

float audio_set_play_vol(float db) {
    snd_mixer_t *handle;
    snd_mixer_selem_id_t *sid;
    const char *card = "default";
    const char *selem_name = "AIF1 DA0";

    /* Los retornos se checan: si la tarjeta no está (o el mixer no se puede abrir),
     * seguir con el handle es usar memoria sin inicializar. Antes se llamaba a
     * snd_mixer_attach con lo que hubiera quedado en `handle`. */
    if (snd_mixer_open(&handle, 0) < 0 || handle == NULL) {
        LV_LOG_WARN("No se pudo abrir el mixer (%s)", card);
        return db;
    }
    if (snd_mixer_attach(handle, card) < 0 ||
        snd_mixer_selem_register(handle, NULL, NULL) < 0 ||
        snd_mixer_load(handle) < 0) {
        LV_LOG_WARN("No se pudo preparar el mixer de '%s'", card);
        snd_mixer_close(handle);
        return db;
    }

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, selem_name);
    snd_mixer_elem_t* elem = snd_mixer_find_selem(handle, sid);

    /* Si el elemento no existe (p.ej. el mixer de "default" es el de pulseaudio y no
     * expone los del codec), ALSA aborta el proceso con una aserción. */
    if (elem == NULL) {
        LV_LOG_WARN("No hay elemento '%s' en el mixer: se deja el volumen como estaba", selem_name);
        snd_mixer_close(handle);
        return db;
    }

    snd_mixer_selem_set_playback_dB_all(elem, (long)(db * 100.0f), 0);
    long db_long;
    snd_mixer_selem_get_playback_dB(elem, SND_MIXER_SCHN_MONO, &db_long);
    snd_mixer_close(handle);
    return (float)db_long / 100.0f;
}

float audio_set_rec_vol(float db) {
    snd_mixer_t *handle;
    snd_mixer_selem_id_t *sid;
    const char *card = "default";
    const char *selem_name = "ADC";

    /* Los retornos se checan: si la tarjeta no está (o el mixer no se puede abrir),
     * seguir con el handle es usar memoria sin inicializar. Antes se llamaba a
     * snd_mixer_attach con lo que hubiera quedado en `handle`. */
    if (snd_mixer_open(&handle, 0) < 0 || handle == NULL) {
        LV_LOG_WARN("No se pudo abrir el mixer (%s)", card);
        return db;
    }
    if (snd_mixer_attach(handle, card) < 0 ||
        snd_mixer_selem_register(handle, NULL, NULL) < 0 ||
        snd_mixer_load(handle) < 0) {
        LV_LOG_WARN("No se pudo preparar el mixer de '%s'", card);
        snd_mixer_close(handle);
        return db;
    }

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, selem_name);
    snd_mixer_elem_t* elem = snd_mixer_find_selem(handle, sid);

    if (elem == NULL) {
        LV_LOG_WARN("No hay elemento '%s' en el mixer: se deja el volumen como estaba", selem_name);
        snd_mixer_close(handle);
        return db;
    }

    snd_mixer_selem_set_capture_dB_all(elem, (long)(db * 100.0f), 0);
    long db_long;
    snd_mixer_selem_get_capture_dB(elem, SND_MIXER_SCHN_MONO, &db_long);
    snd_mixer_close(handle);
    return (float) db_long / 100.0f;
}

#ifdef X6200_RX_SPEAKER_QA
/* ------------------------------------------------------------------------- */
/* SOLO QA: secuencias acotadas de RX procesado al parlante. Este codigo no   */
/* existe en la build de produccion (CMake option default OFF).               */
/* ------------------------------------------------------------------------- */

static uint64_t rx_qa_mono_ms(void) {
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t) ts.tv_sec * 1000ULL + (uint64_t) ts.tv_nsec / 1000000ULL;
}

static void rx_qa_capture_reset(void) {
    atomic_store(&qa_cap_samples, 0);
    atomic_store(&qa_cap_real_samples, 0);
    atomic_store(&qa_cap_calls, 0);
    atomic_store(&qa_cap_holes, 0);
    atomic_store(&qa_cap_last_ms, 0);
    atomic_store(&qa_cap_max_interval_ms, 0);
    atomic_store(&qa_cap_gaps, 0);
}

static void rx_qa_capture_observe(size_t samples, bool process) {
    uint64_t now = rx_qa_mono_ms();
    uint64_t prev = atomic_load(&qa_cap_last_ms);

    if (prev != 0 && now > prev) {
        unsigned interval = (unsigned) (now - prev);
        unsigned old = atomic_load(&qa_cap_max_interval_ms);
        while (interval > old && !atomic_compare_exchange_weak(&qa_cap_max_interval_ms, &old, interval)) {
        }
        if (interval > 150u) {
            atomic_fetch_add(&qa_cap_gaps, 1u);
        }
    }
    atomic_store(&qa_cap_last_ms, now);
    atomic_fetch_add(&qa_cap_samples, (uint64_t) samples);
    atomic_fetch_add(&qa_cap_calls, 1u);
    if (process) {
        atomic_fetch_add(&qa_cap_real_samples, (uint64_t) samples);
    } else {
        atomic_fetch_add(&qa_cap_holes, 1u);
    }
}

static void rx_qa_pa_counters_reset(void) {
    atomic_store(&qa_pa_write_calls, 0);
    atomic_store(&qa_pa_write_ok, 0);
    atomic_store(&qa_pa_write_nospace, 0);
    atomic_store(&qa_pa_write_errors, 0);
    atomic_store(&qa_pa_writable_min, 0xffffffffu);
    atomic_store(&qa_pa_writable_max, 0);
}

typedef struct {
    int      stream_state;
    int      stream_index;
    int      writable;
    uint32_t rate;
    uint32_t channels;
    int      format;
    uint32_t tlength;
    uint32_t maxlength;
    uint32_t prebuf;
    uint32_t minreq;
    uint32_t fragsize;
    int      corked;
    int      latency_valid;
    int      time_valid;
    int      timing_valid;
    int64_t  latency_us;
    int64_t  stream_time_us;
    int64_t  read_index;
    int64_t  write_index;
    int      read_corrupt;
    int      write_corrupt;
    int      playing;
    uint64_t sink_usec;
    uint64_t transport_usec;
    uint64_t configured_sink_usec;
    int64_t  since_underrun;
} rx_qa_pa_snapshot_t;

static void rx_qa_pa_snapshot(rx_qa_pa_snapshot_t *m) {
    memset(m, 0, sizeof(*m));

    if (!audio_up || mloop == NULL || rx_live_stm == NULL) {
        return;
    }

    pa_threaded_mainloop_lock(mloop);
    m->stream_state = (int) pa_stream_get_state(rx_live_stm);
    m->stream_index = (int) pa_stream_get_index(rx_live_stm);
    m->writable = pa_stream_writable_size(rx_live_stm);
    m->corked = pa_stream_is_corked(rx_live_stm);

    const pa_sample_spec *spec = pa_stream_get_sample_spec(rx_live_stm);
    if (spec != NULL) {
        m->rate = spec->rate;
        m->channels = spec->channels;
        m->format = (int) spec->format;
    }

    const pa_buffer_attr *attr = pa_stream_get_buffer_attr(rx_live_stm);
    if (attr != NULL) {
        m->tlength = attr->tlength;
        m->maxlength = attr->maxlength;
        m->prebuf = attr->prebuf;
        m->minreq = attr->minreq;
        m->fragsize = attr->fragsize;
    }

    pa_usec_t usec = 0;
    int negative = 0;
    if (pa_stream_get_latency(rx_live_stm, &usec, &negative) == 0) {
        m->latency_valid = 1;
        m->latency_us = (int64_t) usec * (negative ? -1 : 1);
    }
    if (pa_stream_get_time(rx_live_stm, &usec) == 0) {
        m->time_valid = 1;
        m->stream_time_us = (int64_t) usec;
    }

    const pa_timing_info *ti = pa_stream_get_timing_info(rx_live_stm);
    if (ti != NULL) {
        m->timing_valid = 1;
        m->read_index = ti->read_index;
        m->write_index = ti->write_index;
        m->read_corrupt = ti->read_index_corrupt;
        m->write_corrupt = ti->write_index_corrupt;
        m->playing = ti->playing;
        m->sink_usec = ti->sink_usec;
        m->transport_usec = ti->transport_usec;
        m->configured_sink_usec = ti->configured_sink_usec;
        m->since_underrun = ti->since_underrun;
    }
    pa_threaded_mainloop_unlock(mloop);
}

typedef struct {
    uint64_t t0_ms;
    rx_audio_live_stats_t base_stats;
    rx_audio_live_qa_metrics_t base_metrics;
    uint64_t base_cap_samples;
    uint64_t base_cap_real_samples;
    uint64_t base_cap_calls;
    uint64_t base_cap_holes;
    uint64_t base_cap_last_ms;
    uint32_t base_pa_calls;
    uint32_t base_pa_ok;
    uint32_t base_pa_nospace;
    uint32_t base_pa_errors;
} rx_qa_diag_base_t;

static FILE      *qa_metrics_fp;
static FILE      *qa_events_fp;
static rx_qa_diag_base_t qa_diag_base;
static const char *qa_diag_phase = "none";

static void rx_qa_write_events_locked(void) {
    rx_audio_live_qa_event_t ev[RX_AUDIO_LIVE_QA_EVENT_RING];
    size_t n;
    int guard = 0;

    if (qa_events_fp == NULL) {
        return;
    }
    while ((n = rx_audio_live_qa_drain_events(ev, RX_AUDIO_LIVE_QA_EVENT_RING)) > 0 && guard++ < 8) {
        for (size_t i = 0; i < n; i++) {
            fprintf(qa_events_fp, "%" PRIu64 ",%s,%d\n",
                    ev[i].t_ms - qa_diag_base.t0_ms,
                    rx_audio_live_qa_event_name(ev[i].type), ev[i].value);
        }
        fflush(qa_events_fp);
    }
}

static void rx_qa_diag_begin(const char *mode) {
    char path[128];

    rx_qa_capture_reset();
    rx_qa_pa_counters_reset();
    rx_audio_live_qa_reset_metrics();
    atomic_store(&qa_echo_phase, 0);
    (void) rx_audio_live_qa_drain_events(NULL, 0);

    qa_diag_phase = mode;
    qa_diag_base.t0_ms = rx_qa_mono_ms();
    rx_audio_live_get_stats(&qa_diag_base.base_stats);
    rx_audio_live_qa_get_metrics(&qa_diag_base.base_metrics);
    qa_diag_base.base_cap_samples = atomic_load(&qa_cap_samples);
    qa_diag_base.base_cap_real_samples = atomic_load(&qa_cap_real_samples);
    qa_diag_base.base_cap_calls = atomic_load(&qa_cap_calls);
    qa_diag_base.base_cap_holes = atomic_load(&qa_cap_holes);
    qa_diag_base.base_cap_last_ms = atomic_load(&qa_cap_last_ms);
    qa_diag_base.base_pa_calls = atomic_load(&qa_pa_write_calls);
    qa_diag_base.base_pa_ok = atomic_load(&qa_pa_write_ok);
    qa_diag_base.base_pa_nospace = atomic_load(&qa_pa_write_nospace);
    qa_diag_base.base_pa_errors = atomic_load(&qa_pa_write_errors);

    snprintf(path, sizeof(path), "/mnt/app_logs/rx_qa_metrics_%s.csv", mode);
    qa_metrics_fp = fopen(path, "w");
    snprintf(path, sizeof(path), "/mnt/app_logs/rx_qa_events_%s.csv", mode);
    qa_events_fp = fopen(path, "w");
    if (qa_events_fp != NULL) {
        fprintf(qa_events_fp, "t_ms,event,value\n");
        fflush(qa_events_fp);
    }
    if (qa_metrics_fp != NULL) {
        fprintf(qa_metrics_fp,
            "t_s,phase,cap_samples,cap_real_samples,cap_calls,cap_max_interval_ms,cap_gaps,cap_holes,"
            "submitted,writes,drops,underruns,backpressure,degraded,mux_on_ok,mux_off_ok,"
            "ring_occ,ring_hwm,pending_max_ms,pending_samples,state,"
            "pa_state,pa_writable,pa_stream_index,pa_rate,pa_channels,pa_format,"
            "pa_tlength,pa_maxlength,pa_prebuf,pa_minreq,pa_fragsize,pa_corked,"
            "req_tlength_us,req_fragsize_us,req_prebuf,req_minreq,req_maxlength,"
            "pa_latency_valid,pa_latency_us,pa_time_valid,pa_stream_time_us,"
            "pa_timing_valid,pa_read_index,pa_write_index,pa_read_corrupt,pa_write_corrupt,"
            "pa_playing,pa_sink_usec,pa_transport_usec,pa_configured_sink_usec,pa_since_underrun,"
            "pa_write_calls,pa_write_ok,pa_write_nospace,pa_write_errors,pa_writable_min,pa_writable_max\n");
        fflush(qa_metrics_fp);
    }
}

static void rx_qa_emit_events(void);

static void rx_qa_diag_sample(int sec) {
    rx_audio_live_stats_t st;
    rx_audio_live_qa_metrics_t qm;
    rx_qa_pa_snapshot_t pa;

    rx_audio_live_get_stats(&st);
    rx_audio_live_qa_get_metrics(&qm);
    rx_qa_pa_snapshot(&pa);
    rx_qa_emit_events();

    if (qa_metrics_fp == NULL) {
        return;
    }
    fprintf(qa_metrics_fp,
        "%d,%s,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%u,%u,%" PRIu64 ","
        "%u,%u,%u,%u,%u,%u,%u,%u,"
        "%u,%u,%u,%u,%u,"
        "%d,%d,%d,%u,%u,%d,"
        "%u,%u,%u,%u,%u,%d,"
        "%u,%u,%d,%d,%d,"
        "%d,%" PRId64 ",%d,%" PRId64 ","
        "%d,%" PRId64 ",%" PRId64 ",%d,%d,"
        "%d,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRId64 ","
        "%u,%u,%u,%u,%u,%u\n",
        sec, qa_diag_phase,
        atomic_load(&qa_cap_samples) - qa_diag_base.base_cap_samples,
        atomic_load(&qa_cap_real_samples) - qa_diag_base.base_cap_real_samples,
        atomic_load(&qa_cap_calls) - qa_diag_base.base_cap_calls,
        atomic_load(&qa_cap_max_interval_ms),
        atomic_load(&qa_cap_gaps),
        atomic_load(&qa_cap_holes) - qa_diag_base.base_cap_holes,
        qm.submitted_blocks - qa_diag_base.base_metrics.submitted_blocks,
        st.writes - qa_diag_base.base_stats.writes,
        st.dropped_blocks - qa_diag_base.base_stats.dropped_blocks,
        st.underruns - qa_diag_base.base_stats.underruns,
        st.backpressure - qa_diag_base.base_stats.backpressure,
        qm.degraded_transitions - qa_diag_base.base_metrics.degraded_transitions,
        qm.mux_on_ok - qa_diag_base.base_metrics.mux_on_ok,
        qm.mux_off_ok - qa_diag_base.base_metrics.mux_off_ok,
        qm.ring_occupancy, qm.ring_high_water, qm.pending_max_ms, qm.pending_samples, qm.state,
        pa.stream_state, pa.writable, pa.stream_index, pa.rate, pa.channels, pa.format,
        pa.tlength, pa.maxlength, pa.prebuf, pa.minreq, pa.fragsize, pa.corked,
        3000000u, 100000u, -1, -1, -1,
        pa.latency_valid, pa.latency_us, pa.time_valid, pa.stream_time_us,
        pa.timing_valid, pa.read_index, pa.write_index, pa.read_corrupt, pa.write_corrupt,
        pa.playing, pa.sink_usec, pa.transport_usec, pa.configured_sink_usec, pa.since_underrun,
        atomic_load(&qa_pa_write_calls) - qa_diag_base.base_pa_calls,
        atomic_load(&qa_pa_write_ok) - qa_diag_base.base_pa_ok,
        atomic_load(&qa_pa_write_nospace) - qa_diag_base.base_pa_nospace,
        atomic_load(&qa_pa_write_errors) - qa_diag_base.base_pa_errors,
        atomic_load(&qa_pa_writable_min), atomic_load(&qa_pa_writable_max));
    fflush(qa_metrics_fp);
}

static void rx_qa_emit_events(void) {
    rx_qa_write_events_locked();
}

static void rx_qa_diag_end(void) {
    rx_qa_emit_events();
    if (qa_metrics_fp != NULL) {
        fclose(qa_metrics_fp);
        qa_metrics_fp = NULL;
    }
    if (qa_events_fp != NULL) {
        fclose(qa_events_fp);
        qa_events_fp = NULL;
    }
}

typedef enum {
    RX_QA_SHORT = 0,
    RX_QA_SOAK,
    RX_QA_ECHO,
} rx_qa_mode_t;

static atomic_bool rx_qa_running;
static atomic_int  rx_qa_short_successes;

static void rx_qa_log(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    FILE *f;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    LV_LOG_USER("RX_QA: %s", buf);
    f = fopen("/mnt/app_logs/rx_qa.log", "a");
    if (f != NULL) {
        fprintf(f, "%s\n", buf);
        fclose(f);
    }
}

static bool rx_qa_preflight(void) {
    if (radio_get_state() != RADIO_RX) {
        rx_qa_log("REFUSE state!=RX");
        return false;
    }
    if (!audio_wait_ready(3000)) {
        rx_qa_log("REFUSE audio stream not ready");
        return false;
    }
    if (rx_audio_live_restore_unconfirmed()) {
        rx_qa_log("REFUSE restore unconfirmed");
        return false;
    }
    if (rx_audio_live_tx_active()) {
        rx_qa_log("REFUSE tx active");
        return false;
    }
    if (rx_audio_live_generated_active()) {
        rx_qa_log("REFUSE generated audio active");
        return false;
    }
    return true;
}

static bool rx_qa_wait_state(rx_audio_live_state_t want, int timeout_ms) {
    for (int i = 0; i < timeout_ms; i += 10) {
        if (rx_audio_live_get_state() == want) {
            return true;
        }
        usleep(10000);
    }
    return rx_audio_live_get_state() == want;
}

static bool rx_qa_stop(void) {
    (void) rx_audio_live_set_enabled(false);
    atomic_store(&rx_qa_authorized, false);

    bool base = rx_qa_wait_state(RX_AUDIO_LIVE_BASE, 1500);
    bool ok = base && !rx_audio_live_restore_unconfirmed();
    if (!ok) {
        rx_qa_log("RESTORE_UNCONFIRMED state=%s",
                  rx_audio_live_state_name(rx_audio_live_get_state()));
        return false;
    }

    /* Intent independiente de retorno nominal; no depende de la autorizacion. */
    if (!radio_audio_route_set(false)) {
        rx_qa_log("BASE_ROUTE_RESTORE_FAILED");
        return false;
    }
    return true;
}


static const char *rx_qa_mode_name(rx_qa_mode_t mode) {
    switch (mode) {
        case RX_QA_SOAK: return "soak";
        case RX_QA_ECHO: return "echo";
        default: return "short";
    }
}

static const char *rx_qa_echo_phase_name(int phase) {
    switch (phase) {
        case 1: return "ZERO";
        case 2: return "RAW";
        case 3: return "PROCESSED";
        default: return "OFF";
    }
}

static void rx_qa_log_pa(const char *phase) {
    rx_qa_pa_snapshot_t pa;
    rx_qa_pa_snapshot(&pa);
    rx_qa_log("ECHO_PA phase=%s state=%d writable=%d tlength=%u prebuf=%u fragsize=%u "
              "latency_valid=%d latency_us=%" PRId64 " read_idx=%" PRId64 " write_idx=%" PRId64
              " playing=%d sink_usec=%" PRIu64,
              phase, pa.stream_state, pa.writable, pa.tlength, pa.prebuf, pa.fragsize,
              pa.latency_valid, pa.latency_us, pa.read_index, pa.write_index,
              pa.playing, pa.sink_usec);
}

static void *rx_qa_thread(void *arg) {
    rx_qa_mode_t mode = (rx_qa_mode_t) (intptr_t) arg;
    audio_enh_preset_t preset_before = audio_enh_preset_get_current();
    uint32_t reg_before = x6200_control_get(x6200_sple_atue_trx);
    uint32_t rxvol = x6200_control_get(x6200_rxvol);
    uint8_t hmic_before = params.hmic;
    uint8_t imic_before = params.imic;
    bool ok = false;
    bool soak_aborted = false;

    rx_qa_log("START mode=%s state=%s preset=%s reg12_shadow=0x%08x rxvol=%u",
              rx_qa_mode_name(mode),
              rx_audio_live_state_name(rx_audio_live_get_state()),
              audio_enh_preset_get_name(preset_before), reg_before, rxvol);

    if (!rx_qa_preflight()) {
        goto done;
    }

    rx_qa_diag_begin(rx_qa_mode_name(mode));

    /* Retorno a BASE explicito antes de habilitar. */
    if (!radio_audio_route_set(false)) {
        rx_qa_log("FAIL pre-arm BASE");
        goto done;
    }

    audio_enh_preset_load(AUDIO_ENH_PRESET_OFF);
    atomic_store(&rx_qa_authorized, true);
    if (!rx_audio_live_set_enabled(true)) {
        rx_qa_log("FAIL router enable");
        goto cleanup;
    }
    if (!rx_qa_wait_state(RX_AUDIO_LIVE_RX_DAC, 2500)) {
        rx_qa_log("FAIL no RX_DAC state=%s",
                  rx_audio_live_state_name(rx_audio_live_get_state()));
        goto cleanup;
    }

    if (mode == RX_QA_SHORT) {
        rx_qa_log("PHASE OFF");
        usleep(700000);
        audio_enh_preset_load(AUDIO_ENH_PRESET_SSB_VOICE);
        rx_qa_log("PHASE SSB_VOICE");
        usleep(700000);
    } else if (mode == RX_QA_ECHO) {
        static const int all_phases[] = { 1, 2, 3 };
        int only = atomic_load(&qa_echo_single_phase);
        int phases[3] = { 1, 2, 3 };
        size_t phase_count = 3;
        if (only >= 1 && only <= 3) {
            phases[0] = only;
            phase_count = 1;
            qa_diag_phase = rx_qa_echo_phase_name(only);
        } else {
            (void) all_phases;
        }
        audio_enh_preset_load(atomic_load(&qa_echo_preset_off)
                                  ? AUDIO_ENH_PRESET_OFF
                                  : AUDIO_ENH_PRESET_SSB_VOICE);
        for (size_t p = 0; p < phase_count; p++) {
            int phase = phases[p];
            qa_diag_phase = rx_qa_echo_phase_name(phase);
            atomic_store(&qa_echo_phase, phase);
            rx_qa_log("ECHO_PHASE %s START t_ms=%" PRIu64,
                      qa_diag_phase, rx_qa_mono_ms() - qa_diag_base.t0_ms);
            rx_qa_log_pa(qa_diag_phase);
            for (int s = 0; s < 5; s++) {
                usleep(1000000);
                rx_qa_diag_sample((int) (p * 5 + s + 1));
                if (radio_get_state() != RADIO_RX ||
                    rx_audio_live_get_state() != RX_AUDIO_LIVE_RX_DAC) {
                    rx_qa_log("ECHO_PHASE %s aborted state", qa_diag_phase);
                    soak_aborted = true;
                    break;
                }
            }
            rx_qa_log_pa(qa_diag_phase);
            rx_qa_log("ECHO_PHASE %s END", qa_diag_phase);
            if (soak_aborted) {
                break;
            }
        }
        atomic_store(&qa_echo_phase, 0);
        atomic_store(&qa_echo_single_phase, 0);
        atomic_store(&qa_echo_preset_off, 0);
    } else {
        audio_enh_preset_load(AUDIO_ENH_PRESET_SSB_VOICE);
        rx_qa_log("SOAK 30000ms");
        for (int i = 0; i < 300; i++) {
            if (radio_get_state() != RADIO_RX) {
                rx_qa_log("SOAK aborted state!=RX");
                soak_aborted = true;
                break;
            }
            usleep(100000);
            if ((i + 1) % 10 == 0) {
                rx_qa_diag_sample((i + 1) / 10);
            }
        }
    }
    ok = !soak_aborted;

cleanup:
    {
        atomic_store(&qa_echo_phase, 0);
        bool restored = rx_qa_stop();
        rx_audio_live_stats_t st;
        rx_audio_live_get_stats(&st);
        rx_qa_diag_sample(mode == RX_QA_SOAK ? 31 : 1);
        rx_qa_diag_end();
        audio_enh_preset_load(preset_before);

        uint32_t reg_after = x6200_control_get(x6200_sple_atue_trx);
        rx_qa_log("AFTER reg12_shadow=0x%08x rxvol=%u hmic=%u imic=%u state=%s preset=%s",
                  reg_after, x6200_control_get(x6200_rxvol), params.hmic, params.imic,
                  rx_audio_live_state_name(rx_audio_live_get_state()),
                  audio_enh_preset_get_name(audio_enh_preset_get_current()));
        rx_qa_log("STATS writes=%u drops=%u underruns=%u restore_fail=%u backpressure=%u",
                  st.writes, st.dropped_blocks, st.underruns, st.restore_failures,
                  st.backpressure);

#ifdef X6200_RX_SPEAKER_QA
        uint32_t rxvol_after = x6200_control_get(x6200_rxvol);
        if (rxvol_after != rxvol) {
            rx_qa_log("VOLUME_CHANGED_EXTERNAL before=%u after=%u", rxvol, rxvol_after);
        }
        bool clean = restored &&
                     reg_after == reg_before &&
                     hmic_before == params.hmic && imic_before == params.imic &&
                     audio_enh_preset_get_current() == preset_before &&
                     !rx_audio_live_restore_unconfirmed();
#endif
        bool useful = st.writes > 0;
        rx_qa_log("USEFUL writes>0=%d", useful);
        rx_qa_log("RESULT %s", (ok && clean && useful) ? "PASS_BASE_RESTORED" : "FAIL_OR_DEGRADED");
        if (ok && clean && useful && mode == RX_QA_SHORT) {
            atomic_fetch_add(&rx_qa_short_successes, 1);
        } else if (!clean || !ok || !useful) {
            atomic_store(&rx_qa_short_successes, 0);
        }
    }

done:
    rx_qa_log("DONE");
    atomic_store(&rx_qa_running, false);
    return NULL;
}

static bool rx_qa_start(rx_qa_mode_t mode) {
    bool expected = false;
    pthread_t th;

    if (!atomic_compare_exchange_strong(&rx_qa_running, &expected, true)) {
        rx_qa_log("REFUSE reentry");
        return false;
    }
    if (mode == RX_QA_SOAK && atomic_load(&rx_qa_short_successes) < 2) {
        rx_qa_log("REFUSE soak: need 2 short PASS in this build (%d)",
                  atomic_load(&rx_qa_short_successes));
        atomic_store(&rx_qa_running, false);
        return false;
    }
    if (pthread_create(&th, NULL, rx_qa_thread, (void *) (intptr_t) mode) != 0) {
        atomic_store(&rx_qa_running, false);
        return false;
    }
    pthread_detach(th);
    return true;
}

bool audio_qa_speaker_test_start(void) {
    return rx_qa_start(RX_QA_SHORT);
}

bool audio_qa_speaker_soak_start(void) {
    return rx_qa_start(RX_QA_SOAK);
}

bool audio_qa_speaker_echo_start(void) {
    atomic_store(&qa_echo_single_phase, 0);
    atomic_store(&qa_echo_preset_off, 0);
    return rx_qa_start(RX_QA_ECHO);
}

bool audio_qa_speaker_echo_phase_start(int phase) {
    if (phase < 1 || phase > 3) {
        return false;
    }
    atomic_store(&qa_echo_single_phase, phase);
    atomic_store(&qa_echo_preset_off, 0);
    return rx_qa_start(RX_QA_ECHO);
}

bool audio_qa_speaker_echo_processed_off_start(void) {
    atomic_store(&qa_echo_single_phase, 3);
    atomic_store(&qa_echo_preset_off, 1);
    return rx_qa_start(RX_QA_ECHO);
}

/* QA11: prueba de ruteo fisico BASE/DAC con un unico stream QA, sin router.
 * No toca DSP (preset OFF), no usa I2C externo y no crea streams entre fases. */
static void qa_hw_fill_block(int16_t *out, size_t n, uint64_t elapsed_ms, bool tone) {
    static float phase;
    bool on = tone && ((elapsed_ms / 500u) % 2u) == 0u;

    if (!on) {
        memset(out, 0, n * sizeof(*out));
        return;
    }
    for (size_t i = 0; i < n; i++) {
        out[i] = (int16_t) (519.0f * sinf(phase)); /* -36 dBFS */
        phase += 2.0f * 3.14159265358979323846f * 1000.0f / 44100.0f;
        if (phase > 2.0f * 3.14159265358979323846f) {
            phase -= 2.0f * 3.14159265358979323846f;
        }
    }
}

static bool qa_hw_write_block(const int16_t *block, size_t n) {
    while (!atomic_load(&qa_hw_abort)) {
        int wr = rx_live_pa_write(NULL, block, n);
        if (wr == RX_AUDIO_LIVE_WRITE_OK) {
            return true;
        }
        if (wr == RX_AUDIO_LIVE_WRITE_ERROR) {
            rx_qa_log("HW_ROUTE write error");
            return false;
        }
        usleep(5000);
    }
    return false;
}

static void *qa_hw_route_thread(void *arg) {
    (void) arg;
    audio_enh_preset_t preset_before = audio_enh_preset_get_current();
    uint32_t rxvol = x6200_control_get(x6200_rxvol);
    int16_t block[RX_AUDIO_LIVE_BLOCK_SAMPLES];
    bool opened = false;
    bool mux_dac = false;
    bool ok = false;

    rx_qa_log("HW_ROUTE START vol=%u state=%d reg12_shadow=0x%08x",
              rxvol, (int) radio_get_state(),
              x6200_control_get(x6200_sple_atue_trx));
    audio_enh_preset_load(AUDIO_ENH_PRESET_OFF);

    if (!audio_wait_ready(3000)) {
        rx_qa_log("HW_ROUTE FAIL capture not ready");
        goto cleanup;
    }
    if (!radio_audio_route_set(false)) {
        rx_qa_log("HW_ROUTE FAIL initial BASE");
        goto cleanup;
    }
    if (rx_live_stm != NULL) {
        rx_qa_log("HW_ROUTE FAIL stream already open");
        goto cleanup;
    }
    if (!rx_live_pa_open(NULL)) {
        rx_qa_log("HW_ROUTE FAIL stream open");
        goto cleanup;
    }
    opened = true;
    for (int i = 0; i < 300; i++) {
        if (rx_live_pa_ready(NULL)) {
            break;
        }
        usleep(10000);
    }

    int only = atomic_load(&qa_hw_only_phase);
    int first = (only >= 1 && only <= 5) ? only - 1 : 0;
    int last = (only >= 1 && only <= 5) ? only : 5;

    for (int phase = first; phase < last && !atomic_load(&qa_hw_abort); phase++) {
        static const char *names[] = { "A_BASE_ZERO", "B_DAC_ZERO", "C_DAC_TONE",
                                       "D_BASE_TONE", "E_BASE_ZERO" };
        bool tone = (phase == 2 || phase == 3);
        const char *name = names[phase];

        if (phase == 1 || (only == 3 && phase == 2)) {
            if (!radio_audio_route_set(true)) {
                rx_qa_log("HW_ROUTE FAIL DAC");
                goto cleanup;
            }
            mux_dac = true;
        } else if (phase == 3) {
            if (!radio_audio_route_set(false)) {
                rx_qa_log("HW_ROUTE FAIL BASE at D");
                goto cleanup;
            }
            mux_dac = false;
        }

        rx_qa_log("HW_ROUTE PHASE %s START dac_req=%d reg12_shadow=0x%08x",
                  name, mux_dac ? 1 : 0, x6200_control_get(x6200_sple_atue_trx));
        uint64_t t0 = rx_qa_mono_ms();
        while (!atomic_load(&qa_hw_abort)) {
            uint64_t elapsed = rx_qa_mono_ms() - t0;
            if (elapsed >= 5000u) {
                break;
            }
            qa_hw_fill_block(block, RX_AUDIO_LIVE_BLOCK_SAMPLES, elapsed, tone);
            if (!qa_hw_write_block(block, RX_AUDIO_LIVE_BLOCK_SAMPLES)) {
                break;
            }
        }
        rx_qa_log("HW_ROUTE PHASE %s END dac_req=%d reg12_shadow=0x%08x",
                  name, mux_dac ? 1 : 0, x6200_control_get(x6200_sple_atue_trx));
    }
    ok = !atomic_load(&qa_hw_abort);

cleanup:
    if (mux_dac) {
        (void) radio_audio_route_set(false);
    }
    if (opened) {
        rx_live_pa_close(NULL);
    }
    (void) radio_audio_route_set(false);
    audio_enh_preset_load(preset_before);
    rx_qa_log("HW_ROUTE %s vol=%u reg12_shadow=0x%08x state=%d",
              ok ? "DONE" : "ABORT/FAIL", x6200_control_get(x6200_rxvol),
              x6200_control_get(x6200_sple_atue_trx), (int) radio_get_state());
    atomic_store(&qa_hw_running, false);
    atomic_store(&qa_hw_abort, false);
    atomic_store(&qa_hw_only_phase, 0);
    return NULL;
}

bool audio_qa_speaker_hw_route_start(void) {
    bool expected = false;
    pthread_t th;

    if (!atomic_compare_exchange_strong(&qa_hw_running, &expected, true)) {
        rx_qa_log("HW_ROUTE REFUSE reentry");
        return false;
    }
    atomic_store(&qa_hw_only_phase, 0);
    atomic_store(&qa_hw_abort, false);
    if (pthread_create(&th, NULL, qa_hw_route_thread, NULL) != 0) {
        atomic_store(&qa_hw_running, false);
        return false;
    }
    pthread_detach(th);
    return true;
}

bool audio_qa_speaker_hw_phase_start(int phase) {
    bool expected = false;
    pthread_t th;

    if (phase < 1 || phase > 5) {
        return false;
    }
    if (!atomic_compare_exchange_strong(&qa_hw_running, &expected, true)) {
        rx_qa_log("HW_ROUTE REFUSE reentry phase");
        return false;
    }
    atomic_store(&qa_hw_only_phase, phase);
    atomic_store(&qa_hw_abort, false);
    if (pthread_create(&th, NULL, qa_hw_route_thread, NULL) != 0) {
        atomic_store(&qa_hw_running, false);
        atomic_store(&qa_hw_only_phase, 0);
        return false;
    }
    pthread_detach(th);
    return true;
}

void audio_qa_speaker_hw_route_abort(void) {
    atomic_store(&qa_hw_abort, true);
}

#endif

float audio_get_peak_db() {
    return peak_db;
}

static void monitor_cb(pa_stream *stream, size_t length, void *udata) {
    int16_t *buf = NULL;

    pa_stream_peek(stream, (const void**) &buf, &length);
    int16_t max_val = 1;
    int16_t cur_val;
    for (size_t i=0; i < length / 2; i++) {
        cur_val = buf[i];
        if (cur_val > max_val) {
            max_val = cur_val;
        }
    }
    peak_db = 20.0f * log10f((float) max_val / ((1UL << 15) - 1));
    pa_stream_drop(stream);
}


static void record_monitor_setup() {
    if (!audio_up || ctx == NULL) {
        return;
    }



    pa_sample_spec  spec = {
        .format = PA_SAMPLE_S16NE,
        .channels = 1
    };

    spec.rate = 30;

    monitor_stm = pa_stream_new(ctx, "X6200 GUI Monitor", &spec, NULL);

    pa_threaded_mainloop_lock(mloop);
    pa_stream_set_read_callback(monitor_stm, monitor_cb, NULL);
    pa_stream_connect_record(monitor_stm, capture_device, NULL, PA_STREAM_PEAK_DETECT);
    pa_threaded_mainloop_unlock(mloop);
}
