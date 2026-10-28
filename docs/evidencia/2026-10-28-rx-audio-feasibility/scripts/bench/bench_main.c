/*
 * Banco de SpeexDSP REAL para la cadena de src/audio_enhancement.c (issue #345, tarea B).
 *
 * Compila el archivo de produccion COPIADO VERBATIM (sha256 registrado por
 * bench_speex.py) contra libspeexdsp real. Lo unico sustituido es la periferia
 * que en la radio proveen LVGL/cfg/params/liquid (stubs_bench.c); SpeexDSP no
 * se sustituye nunca.
 *
 * Modos:
 *   bench --case NAME --in X.wav --out Y.raw --preset N --block N --seconds 60
 *   bench --probe polarity --in noise.wav
 *   bench --probe rate --in noise.wav --suppress -22
 *
 * Salida: una linea JSON por caso/probe en stdout.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>

#include <speex/speex_preprocess.h>

#include "audio_enhancement.h"

#define DATA_RATE_HZ 44100

static double mono_ms(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (double) ts.tv_sec * 1000.0 + (double) ts.tv_nsec / 1e6;
}

static void cpu_ms(double *user_ms, double *sys_ms) {
    struct rusage ru;

    getrusage(RUSAGE_SELF, &ru);
    *user_ms = (double) ru.ru_utime.tv_sec * 1000.0 + (double) ru.ru_utime.tv_usec / 1000.0;
    *sys_ms = (double) ru.ru_stime.tv_sec * 1000.0 + (double) ru.ru_stime.tv_usec / 1000.0;
}

static long rss_kb(void) {
    FILE *f = fopen("/proc/self/status", "r");
    char line[256];
    long kb = -1;

    if (!f) {
        return -1;
    }

    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            kb = strtol(line + 6, NULL, 10);
            break;
        }
    }

    fclose(f);

    return kb;
}

static int16_t *wav_load(const char *path, size_t *nsamples) {
    FILE *f = fopen(path, "rb");
    unsigned char riff[12];
    int16_t *data = NULL;
    size_t n = 0;
    int have_fmt = 0;
    unsigned fmt_tag = 0;
    unsigned fmt_channels = 0;
    unsigned fmt_bits = 0;

    if (!f) {
        fprintf(stderr, "no puedo abrir %s\n", path);
        return NULL;
    }

    if (fread(riff, 1, 12, f) != 12 || memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0) {
        fprintf(stderr, "%s: no es un WAV RIFF\n", path);
        fclose(f);
        return NULL;
    }

    for (;;) {
        unsigned char ch[8];
        unsigned long sz;

        if (fread(ch, 1, 8, f) != 8) {
            break;
        }

        sz = (unsigned long) ch[4] | ((unsigned long) ch[5] << 8) | ((unsigned long) ch[6] << 16) |
             ((unsigned long) ch[7] << 24);

        if (memcmp(ch, "fmt ", 4) == 0) {
            unsigned char b[16];

            if (sz < 16 || fread(b, 1, 16, f) != 16) {
                break;
            }

            fmt_tag = (unsigned) b[0] | ((unsigned) b[1] << 8);
            fmt_channels = (unsigned) b[2] | ((unsigned) b[3] << 8);
            fmt_bits = (unsigned) b[14] | ((unsigned) b[15] << 8);
            have_fmt = 1;

            if (sz > 16) {
                fseek(f, (long) (sz - 16), SEEK_CUR);
            }
        } else if (memcmp(ch, "data", 4) == 0) {
            data = malloc(sz);

            if (!data || fread(data, 1, sz, f) != sz) {
                free(data);
                data = NULL;
                break;
            }

            n = (size_t) sz / 2;
        } else {
            fseek(f, (long) sz, SEEK_CUR);
        }

        if (sz & 1UL) {
            fseek(f, 1, SEEK_CUR);
        }
    }

    fclose(f);

    if (!have_fmt || !data || fmt_tag != 1 || fmt_channels != 1 || fmt_bits != 16) {
        fprintf(stderr, "%s: se esperaba WAV PCM mono S16 (tag=%u ch=%u bits=%u)\n", path, fmt_tag,
                fmt_channels, fmt_bits);
        free(data);
        return NULL;
    }

    *nsamples = n;

    return data;
}

static const char *preset_name(int preset) {
    switch (preset) {
        case AUDIO_ENH_PRESET_OFF: return "OFF";
        case AUDIO_ENH_PRESET_CW_WEAK: return "CW_WEAK";
        case AUDIO_ENH_PRESET_SSB_VOICE: return "SSB_VOICE";
        case AUDIO_ENH_PRESET_AM_BROADCAST: return "AM_BROADCAST";
        case AUDIO_ENH_PRESET_DIGITAL: return "DIGITAL";
        default: return "UNKNOWN";
    }
}

static int run_stream(const char *name, const char *in_path, const char *out_path, int preset, size_t block,
                      int seconds) {
    size_t n_in = 0;
    int16_t *in = wav_load(in_path, &n_in);
    int16_t *work;
    FILE *out;
    size_t processed = 0;
    size_t unprocessed = 0;
    size_t callbacks = 0;
    double max_cb_ms = 0.0;
    double wall0;
    double wall1;
    double user0;
    double user1;
    double sys0;
    double sys1;
    long rss0;
    long rss1;
    long rss_preinit;
    int nr_on;
    int enh_on;
    int notches;
    size_t reps;

    if (!in || n_in == 0) {
        free(in);
        return 1;
    }

    if (block == 0 || block > n_in) {
        fprintf(stderr, "block invalido\n");
        free(in);
        return 1;
    }

    if (((size_t) seconds * DATA_RATE_HZ) % n_in != 0) {
        fprintf(stderr, "el archivo no divide exacto los %d s pedidos\n", seconds);
        free(in);
        return 1;
    }

    reps = ((size_t) seconds * DATA_RATE_HZ) / n_in;

    rss_preinit = rss_kb();

    audio_enh_init();
    audio_enh_preset_load((audio_enh_preset_t) preset);

    enh_on = audio_enh_is_enabled() ? 1 : 0;
    nr_on = audio_enh_nr_is_enabled() ? 1 : 0;
    notches = audio_enh_get_active_notch_count();

    work = malloc(block * sizeof *work);
    out = fopen(out_path, "wb");

    if (!work || !out) {
        fprintf(stderr, "no puedo preparar buffer/salida\n");
        free(work);
        free(in);
        if (out) {
            fclose(out);
        }
        return 1;
    }

    rss0 = rss_kb();
    cpu_ms(&user0, &sys0);
    wall0 = mono_ms();

    for (size_t r = 0; r < reps; r++) {
        for (size_t off = 0; off < n_in; off += block) {
            size_t n = (n_in - off < block) ? (n_in - off) : block;
            double t0;
            double dt;

            memcpy(work, in + off, n * sizeof *work);

            t0 = mono_ms();
            audio_enh_process(work, n);
            dt = mono_ms() - t0;

            if (dt > max_cb_ms) {
                max_cb_ms = dt;
            }

            callbacks++;

            if (enh_on && nr_on) {
                size_t done = n - (n % AUDIO_ENH_FRAME_SIZE);

                processed += done;
                unprocessed += n - done;
            } else {
                unprocessed += n;
            }

            if (r == 0) {
                if (fwrite(work, sizeof *work, n, out) != n) {
                    fprintf(stderr, "fallo de escritura\n");
                    fclose(out);
                    free(work);
                    free(in);
                    return 1;
                }
            }
        }
    }

    wall1 = mono_ms();
    cpu_ms(&user1, &sys1);
    rss1 = rss_kb();

    fclose(out);

    printf("{\"case\":\"%s\",\"kind\":\"stream\",\"preset\":%d,\"preset_name\":\"%s\","
           "\"block\":%zu,\"reps\":%zu,\"callbacks\":%zu,\"input_samples\":%zu,"
           "\"total_samples\":%zu,\"out_samples\":%zu,\"processed\":%zu,\"unprocessed\":%zu,"
           "\"wall_ms\":%.3f,\"user_ms\":%.3f,\"sys_ms\":%.3f,\"max_callback_ms\":%.4f,"
           "\"rss_preinit_kb\":%ld,\"rss_start_kb\":%ld,\"rss_end_kb\":%ld,\"enh\":%d,\"nr\":%d,\"notches\":%d,"
           "\"enh_frame\":%d,\"enh_rate\":%d,\"data_rate\":%d,\"seconds\":%d}\n",
           name, preset, preset_name(preset), block, reps, callbacks, n_in, n_in * reps, n_in, processed,
           unprocessed, wall1 - wall0, user1 - user0, sys1 - sys0, max_cb_ms, rss_preinit, rss0, rss1,
           enh_on, nr_on,
           notches, AUDIO_ENH_FRAME_SIZE, AUDIO_ENH_SAMPLE_RATE, DATA_RATE_HZ, seconds);

    audio_enh_deinit();
    free(work);
    free(in);

    return 0;
}

/* Procesa `n` muestras en frames de 256 con un estado Speex directo y devuelve
 * la atenuacion RMS resultante en dB (negativa = atenuo). */
static double speex_atten_db(const int16_t *in, size_t n, int init_rate, int suppress, int denoise,
                             int *readback, double *in_dbfs, double *out_dbfs) {
    SpeexPreprocessState *st = speex_preprocess_state_init(AUDIO_ENH_FRAME_SIZE, init_rate);
    double in_acc = 0.0;
    double out_acc = 0.0;
    size_t frames = n / AUDIO_ENH_FRAME_SIZE;
    int16_t frame[AUDIO_ENH_FRAME_SIZE];
    int rb = -999;

    *in_dbfs = NAN;
    *out_dbfs = NAN;

    if (!st) {
        *readback = -999;
        return NAN;
    }

    speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_DENOISE, &denoise);
    speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &suppress);
    speex_preprocess_ctl(st, SPEEX_PREPROCESS_GET_NOISE_SUPPRESS, &rb);
    *readback = rb;

    for (size_t f = 0; f < frames; f++) {
        for (int i = 0; i < AUDIO_ENH_FRAME_SIZE; i++) {
            frame[i] = in[f * AUDIO_ENH_FRAME_SIZE + i];
            in_acc += (double) frame[i] * (double) frame[i];
        }

        speex_preprocess_run(st, frame);

        for (int i = 0; i < AUDIO_ENH_FRAME_SIZE; i++) {
            out_acc += (double) frame[i] * (double) frame[i];
        }
    }

    speex_preprocess_state_destroy(st);

    if (in_acc <= 0.0) {
        return NAN;
    }

    *in_dbfs = 10.0 * log10(in_acc / (double) (frames * AUDIO_ENH_FRAME_SIZE) / (32768.0 * 32768.0));
    *out_dbfs = 10.0 * log10(out_acc / (double) (frames * AUDIO_ENH_FRAME_SIZE) / (32768.0 * 32768.0));

    return 10.0 * log10(out_acc / in_acc);
}

static int probe_polarity(const char *in_path) {
    size_t n = 0;
    int16_t *in = wav_load(in_path, &n);
    const int values[3] = {-30, 30, 0};

    if (!in) {
        return 1;
    }

    for (int k = 0; k < 3; k++) {
        int rb = 0;
        double in_db = NAN;
        double out_db = NAN;
        double att = speex_atten_db(in, n, AUDIO_ENH_SAMPLE_RATE, values[k], 1, &rb, &in_db, &out_db);

        printf("{\"case\":\"probe_polarity_%+d\",\"kind\":\"polarity\",\"requested\":%d,"
               "\"readback\":%d,\"denoise\":1,\"init_rate\":%d,\"frames\":%zu,\"in_rms_dbfs\":%.3f,"
               "\"out_rms_dbfs\":%.3f,\"atten_db\":%.3f}\n",
               values[k], values[k], rb, AUDIO_ENH_SAMPLE_RATE, n / AUDIO_ENH_FRAME_SIZE, in_db, out_db, att);
    }

    {
        int rb = 0;
        double in_db = NAN;
        double out_db = NAN;
        double att = speex_atten_db(in, n, AUDIO_ENH_SAMPLE_RATE, -30, 0, &rb, &in_db, &out_db);

        printf("{\"case\":\"probe_control_denoise_off\",\"kind\":\"polarity\",\"requested\":-30,"
               "\"readback\":%d,\"denoise\":0,\"init_rate\":%d,\"frames\":%zu,\"in_rms_dbfs\":%.3f,"
               "\"out_rms_dbfs\":%.3f,\"atten_db\":%.3f}\n",
               rb, AUDIO_ENH_SAMPLE_RATE, n / AUDIO_ENH_FRAME_SIZE, in_db, out_db, att);
    }

    free(in);

    return 0;
}

static int probe_rate(const char *in_path, int suppress) {
    size_t n = 0;
    int16_t *in = wav_load(in_path, &n);
    const int rates[2] = {AUDIO_ENH_SAMPLE_RATE, DATA_RATE_HZ};

    if (!in) {
        return 1;
    }

    for (int k = 0; k < 2; k++) {
        int rb = 0;
        double in_db = NAN;
        double out_db = NAN;
        double att = speex_atten_db(in, n, rates[k], suppress, 1, &rb, &in_db, &out_db);

        printf("{\"case\":\"probe_rate_%d\",\"kind\":\"rate\",\"init_rate\":%d,\"requested\":%d,"
               "\"readback\":%d,\"frames\":%zu,\"in_rms_dbfs\":%.3f,\"out_rms_dbfs\":%.3f,"
               "\"atten_db\":%.3f}\n",
               rates[k], rates[k], suppress, rb, n / AUDIO_ENH_FRAME_SIZE, in_db, out_db, att);
    }

    free(in);

    return 0;
}

static int probe_level(const char *in_path) {
    size_t n = 0;
    int16_t *in = wav_load(in_path, &n);
    const int levels[6] = {0, 20, 40, 60, 80, 100};
    size_t callbacks;

    if (!in) {
        return 1;
    }

    callbacks = n / 4410;

    for (int k = 0; k < 6; k++) {
        double in_acc = 0.0;
        double out_acc = 0.0;
        int16_t *buf = malloc(4410 * sizeof *buf);

        if (!buf) {
            free(in);
            return 1;
        }

        audio_enh_init();
        audio_enh_set_enabled(true);
        audio_enh_nr_set_enabled(true);
        audio_enh_nr_set_level((uint8_t) levels[k]);

        for (size_t b = 0; b < callbacks; b++) {
            memcpy(buf, in + b * 4410, 4410 * sizeof *buf);

            for (size_t i = 0; i < 4410; i++) {
                in_acc += (double) buf[i] * (double) buf[i];
            }

            audio_enh_process(buf, 4410);

            for (size_t i = 0; i < 4410; i++) {
                out_acc += (double) buf[i] * (double) buf[i];
            }
        }

        printf("{\"case\":\"probe_level_%d\",\"kind\":\"level\",\"level\":%d,"
               "\"requested_db\":%d,\"init_rate\":%d,\"frames\":%zu,\"in_rms_dbfs\":%.3f,"
               "\"out_rms_dbfs\":%.3f,\"atten_db\":%.3f}\n",
               levels[k], levels[k], -30 + levels[k] * 20 / 100, AUDIO_ENH_SAMPLE_RATE, callbacks * 17,
               10.0 * log10(in_acc / (double) (callbacks * 4410) / (32768.0 * 32768.0)),
               10.0 * log10(out_acc / (double) (callbacks * 4410) / (32768.0 * 32768.0)),
               10.0 * log10(out_acc / in_acc));

        audio_enh_deinit();
        free(buf);
    }

    free(in);

    return 0;
}

int main(int argc, char **argv) {
    const char *case_name = NULL;
    const char *in_path = NULL;
    const char *out_path = NULL;
    const char *probe = NULL;
    int preset = 0;
    int seconds = 60;
    int suppress = -22;
    size_t block = 4410;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--case") == 0 && i + 1 < argc) {
            case_name = argv[++i];
        } else if (strcmp(argv[i], "--in") == 0 && i + 1 < argc) {
            in_path = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (strcmp(argv[i], "--preset") == 0 && i + 1 < argc) {
            preset = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--block") == 0 && i + 1 < argc) {
            block = (size_t) strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
            seconds = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--suppress") == 0 && i + 1 < argc) {
            suppress = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--probe") == 0 && i + 1 < argc) {
            probe = argv[++i];
        } else {
            fprintf(stderr, "argumento desconocido: %s\n", argv[i]);
            return 2;
        }
    }

    if (!in_path) {
        fprintf(stderr, "falta --in\n");
        return 2;
    }

    if (probe && strcmp(probe, "polarity") == 0) {
        return probe_polarity(in_path);
    }

    if (probe && strcmp(probe, "rate") == 0) {
        return probe_rate(in_path, suppress);
    }

    if (probe && strcmp(probe, "level") == 0) {
        return probe_level(in_path);
    }

    if (!case_name || !out_path) {
        fprintf(stderr, "faltan --case/--out\n");
        return 2;
    }

    return run_stream(case_name, in_path, out_path, preset, block, seconds);
}
