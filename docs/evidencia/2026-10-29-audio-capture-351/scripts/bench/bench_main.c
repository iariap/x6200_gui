/*
 * Banco de SpeexDSP REAL para la cadena de captura de src/audio_enhancement.c
 * (issue #351, parte 3a).
 *
 * Compila el archivo de produccion COPIADO VERBATIM (sha256 registrado por
 * bench_capture.py) contra libspeexdsp real. Lo unico sustituido es la
 * periferia que en la radio proveen LVGL/cfg/params/liquid (stubs_bench.c);
 * SpeexDSP no se sustituye nunca.
 *
 * Modos:
 *   bench --case NAME --in X.wav --out Y.raw --preset N --blocks "4410" [--seconds 10]
 *       Procesa el WAV con audio_enh_process() en callbacks cuyo tamano cicla
 *       por la lista de --blocks. Escribe la salida del primer pase.
 *
 *   bench --partition-check NAME --in X.wav --preset N \
 *         --blocks-a "4410" --blocks-b "1,440,441,257" --out-a A.raw --out-b B.raw
 *       Procesa el MISMO input dos veces en un solo proceso, con dos
 *       particiones distintas y audio_enh_preset_reset() en el medio. Sirve
 *       para comparar la salida byte a byte "una vez reseteado el estado".
 *
 *   bench --probe levels --in X.wav
 *       audio_enh_nr_set_level() sobre la lib real: lee de vuelta
 *       SPEEX_PREPROCESS_GET_NOISE_SUPPRESS y mide la atenuacion RMS.
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
#define MAX_BLOCKS 64

typedef struct {
    size_t v[MAX_BLOCKS];
    int    n;
} block_list_t;

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

/* "4410" o "1,440,441,257" -> lista ciclada. Cualquier valor 0 o basura es error. */
static int parse_blocks(const char *text, block_list_t *out) {
    const char *p = text;

    out->n = 0;

    while (*p && out->n < MAX_BLOCKS) {
        char *end = NULL;
        unsigned long v = strtoul(p, &end, 10);

        if (end == p || v == 0) {
            return 1;
        }

        out->v[out->n++] = (size_t) v;

        p = end;

        if (*p == ',') {
            p++;
        } else if (*p != '\0') {
            return 1;
        }
    }

    return out->n == 0 ? 1 : 0;
}

static void blocks_text(const block_list_t *b, char *dst, size_t dst_len) {
    size_t used = 0;

    dst[0] = '\0';

    for (int i = 0; i < b->n; i++) {
        int wrote = snprintf(dst + used, dst_len - used, "%s%zu", i ? "," : "", b->v[i]);

        if (wrote < 0 || (size_t) wrote >= dst_len - used) {
            return;
        }

        used += (size_t) wrote;
    }
}

/* Nivel de supresion EFECTIVO: se lee de vuelta del estado Speex real, no de la
 * formula. -999 = no hay estado (NR sin Speex). */
static int effective_suppression_db(int *have_state) {
    int suppress = -999;

    if (g_audio_enh.nr.speex_state == NULL) {
        *have_state = 0;
        return suppress;
    }

    *have_state = 1;

    if (speex_preprocess_ctl(g_audio_enh.nr.speex_state, SPEEX_PREPROCESS_GET_NOISE_SUPPRESS,
                             &suppress) != 0) {
        return -999;
    }

    return suppress;
}

static double percentile_sorted(const double *sorted, size_t n, double q) {
    size_t idx;

    if (n == 0) {
        return 0.0;
    }

    idx = (size_t) (q * (double) (n - 1) + 0.5);

    if (idx >= n) {
        idx = n - 1;
    }

    return sorted[idx];
}

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *) a;
    double y = *(const double *) b;

    return (x < y) ? -1 : (x > y) ? 1 : 0;
}

/* Procesa un stream completo con la particion dada. `out_path` puede ser NULL
 * (no escribe). Devuelve 0 si todo salio bien. */
static int run_pass(int16_t *in, size_t n_in, const block_list_t *blocks,
                    const char *out_path, size_t *callbacks_out, size_t *out_samples_out,
                    double *cb_ms, size_t cb_capacity, double *max_cb_ms, double *wall_ms_out) {
    int16_t *work = NULL;
    FILE *out = NULL;
    size_t off = 0;
    size_t k = 0;
    size_t callbacks = 0;
    double wall0 = mono_ms();
    size_t max_block = 0;

    for (int i = 0; i < blocks->n; i++) {
        if (blocks->v[i] > max_block) {
            max_block = blocks->v[i];
        }
    }

    work = malloc(max_block * sizeof *work);

    if (!work) {
        return 1;
    }

    if (out_path) {
        out = fopen(out_path, "wb");

        if (!out) {
            free(work);
            return 1;
        }
    }

    *max_cb_ms = 0.0;

    while (off < n_in) {
        size_t n = blocks->v[k % (size_t) blocks->n];
        double t0;
        double dt;

        if (n > n_in - off) {
            n = n_in - off;
        }

        memcpy(work, in + off, n * sizeof *work);

        t0 = mono_ms();
        audio_enh_process(work, n);
        dt = mono_ms() - t0;

        if (dt > *max_cb_ms) {
            *max_cb_ms = dt;
        }

        if (cb_ms && callbacks < cb_capacity) {
            cb_ms[callbacks] = dt;
        }

        callbacks++;

        if (out && fwrite(work, sizeof *work, n, out) != n) {
            fprintf(stderr, "fallo de escritura en %s\n", out_path);
            fclose(out);
            free(work);
            return 1;
        }

        off += n;
        k++;
    }

    *wall_ms_out = mono_ms() - wall0;

    if (out) {
        fclose(out);
    }

    free(work);

    *callbacks_out = callbacks;
    *out_samples_out = n_in;

    return 0;
}

static void print_setup(int preset) {
    int have_state = 0;
    int suppress = effective_suppression_db(&have_state);

    printf("\"preset\":%d,\"preset_name\":\"%s\",\"enh\":%d,\"nr\":%d,\"nr_level\":%u,"
           "\"speex_state\":%d,\"suppression_db\":%d,"
           "\"frame\":%d,\"rate\":%d,\"data_rate\":%d",
           preset, preset_name(preset), audio_enh_is_enabled() ? 1 : 0,
           audio_enh_nr_is_enabled() ? 1 : 0, (unsigned) g_audio_enh.nr.level, have_state, suppress,
           AUDIO_ENH_FRAME_SIZE, AUDIO_ENH_SAMPLE_RATE, DATA_RATE_HZ);
}

static int run_stream(const char *name, const char *in_path, const char *out_path, int preset,
                      const block_list_t *blocks, int seconds) {
    size_t n_in = 0;
    int16_t *in = wav_load(in_path, &n_in);
    char btext[512];
    size_t reps;
    size_t total_in = 0;
    size_t total_out = 0;
    double *cb_ms = NULL;
    size_t cb_count = 0;
    size_t cb_capacity = 0;
    double max_cb_ms = 0.0;
    double wall_total = 0.0;
    double user0;
    double user1;
    double sys0;
    double sys1;
    long rss0;
    long rss1;

    if (!in || n_in == 0) {
        free(in);
        return 1;
    }

    if (((size_t) seconds * DATA_RATE_HZ) % n_in != 0) {
        fprintf(stderr, "el archivo no divide exacto los %d s pedidos\n", seconds);
        free(in);
        return 1;
    }

    reps = ((size_t) seconds * DATA_RATE_HZ) / n_in;

    audio_enh_init();
    audio_enh_preset_load((audio_enh_preset_t) preset);

    blocks_text(blocks, btext, sizeof btext);

    /* Cota superior de callbacks: el bloque mas chico de la particion. */
    {
        size_t min_block = n_in;

        for (int i = 0; i < blocks->n; i++) {
            if (blocks->v[i] < min_block) {
                min_block = blocks->v[i];
            }
        }

        cb_capacity = reps * ((n_in / min_block) + (size_t) blocks->n);
    }

    cb_ms = malloc(cb_capacity * sizeof *cb_ms);

    if (!cb_ms) {
        audio_enh_deinit();
        free(in);
        return 1;
    }

    rss0 = rss_kb();
    cpu_ms(&user0, &sys0);

    for (size_t r = 0; r < reps; r++) {
        size_t callbacks = 0;
        size_t out_samples = 0;
        double wall_pass = 0.0;
        double max_pass = 0.0;

        if (run_pass(in, n_in, blocks, (r == 0) ? out_path : NULL, &callbacks, &out_samples,
                     cb_ms + cb_count, cb_capacity - cb_count, &max_pass, &wall_pass) != 0) {
            audio_enh_deinit();
            free(cb_ms);
            free(in);
            return 1;
        }

        cb_count += callbacks;
        wall_total += wall_pass;

        if (max_pass > max_cb_ms) {
            max_cb_ms = max_pass;
        }

        total_in += n_in;
        total_out += out_samples;
    }

    cpu_ms(&user1, &sys1);
    rss1 = rss_kb();

    qsort(cb_ms, cb_count, sizeof *cb_ms, cmp_double);

    printf("{\"case\":\"%s\",\"kind\":\"stream\",", name);
    print_setup(preset);
    printf(",\"blocks\":\"%s\",\"callbacks\":%zu,\"reps\":%zu,\"input_samples\":%zu,"
           "\"total_in\":%zu,\"out_samples\":%zu,\"total_out\":%zu,\"length_preserved\":%d,"
           "\"wall_ms\":%.3f,\"user_ms\":%.3f,\"sys_ms\":%.3f,\"cb_p50_ms\":%.6f,"
           "\"cb_p99_ms\":%.6f,\"cb_max_ms\":%.6f,\"rss_start_kb\":%ld,\"rss_end_kb\":%ld,"
           "\"seconds\":%d}\n",
           btext, cb_count, reps, n_in, total_in, n_in, total_out,
           (total_in == total_out) ? 1 : 0, wall_total, user1 - user0, sys1 - sys0,
           percentile_sorted(cb_ms, cb_count, 0.50), percentile_sorted(cb_ms, cb_count, 0.99),
           max_cb_ms, rss0, rss1, seconds);

    audio_enh_deinit();
    free(cb_ms);
    free(in);

    return 0;
}

static int run_partition_check(const char *name, const char *in_path, int preset,
                               const block_list_t *blocks_a, const block_list_t *blocks_b,
                               const char *out_a, const char *out_b) {
    size_t n_in = 0;
    int16_t *in = wav_load(in_path, &n_in);
    char atext[512];
    char btext[512];
    size_t callbacks = 0;
    size_t out_samples = 0;
    double max_cb = 0.0;
    double wall = 0.0;

    if (!in || n_in == 0) {
        free(in);
        return 1;
    }

    audio_enh_init();
    audio_enh_preset_load((audio_enh_preset_t) preset);

    blocks_text(blocks_a, atext, sizeof atext);
    blocks_text(blocks_b, btext, sizeof btext);

    if (run_pass(in, n_in, blocks_a, out_a, &callbacks, &out_samples, NULL, 0,
                 &max_cb, &wall) != 0) {
        audio_enh_deinit();
        free(in);
        return 1;
    }

    /* El reset explicito entre pases: la maquina NR descarta la trama pendiente y la
     * lista, sin tocar el preset. La salida del segundo pase tiene que ser la misma
     * secuencia que si el proceso hubiera arrancado recien. */
    audio_enh_preset_reset();

    if (run_pass(in, n_in, blocks_b, out_b, &callbacks, &out_samples, NULL, 0,
                 &max_cb, &wall) != 0) {
        audio_enh_deinit();
        free(in);
        return 1;
    }

    printf("{\"case\":\"%s\",\"kind\":\"partition_check\",", name);
    print_setup(preset);
    printf(",\"blocks_a\":\"%s\",\"blocks_b\":\"%s\",\"input_samples\":%zu,"
           "\"out_samples_a\":%zu,\"out_samples_b\":%zu,\"length_preserved\":%d}\n",
           atext, btext, n_in, n_in, n_in, (n_in == out_samples) ? 1 : 0);

    audio_enh_deinit();
    free(in);

    return 0;
}

/* Estado Speex SOLO (sin la cadena del modulo): mide cuanto del retardo total
 * pone la libreria por su cuenta. Se escribe la salida para correlacionarla en
 * el host contra el input. */
static int probe_speex_only(const char *in_path, const char *out_path, int suppress) {
    size_t n = 0;
    int16_t *in = wav_load(in_path, &n);
    SpeexPreprocessState *st;
    FILE *out;
    size_t frames;
    int denoise = 1;
    int rb = 0;

    if (!in || n < (size_t) AUDIO_ENH_FRAME_SIZE) {
        free(in);
        return 1;
    }

    st = speex_preprocess_state_init(AUDIO_ENH_FRAME_SIZE, AUDIO_ENH_SAMPLE_RATE);

    if (!st) {
        fprintf(stderr, "speex_preprocess_state_init devolvio NULL\n");
        free(in);
        return 1;
    }

    speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_DENOISE, &denoise);
    speex_preprocess_ctl(st, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &suppress);
    speex_preprocess_ctl(st, SPEEX_PREPROCESS_GET_NOISE_SUPPRESS, &rb);

    out = fopen(out_path, "wb");

    if (!out) {
        speex_preprocess_state_destroy(st);
        free(in);
        return 1;
    }

    frames = n / (size_t) AUDIO_ENH_FRAME_SIZE;

    for (size_t f = 0; f < frames; f++) {
        int16_t frame[AUDIO_ENH_FRAME_SIZE];

        memcpy(frame, in + f * (size_t) AUDIO_ENH_FRAME_SIZE, sizeof frame);
        speex_preprocess_run(st, frame);

        if (fwrite(frame, sizeof frame, 1, out) != 1) {
            fprintf(stderr, "fallo de escritura en %s\n", out_path);
            fclose(out);
            speex_preprocess_state_destroy(st);
            free(in);
            return 1;
        }
    }

    fclose(out);
    speex_preprocess_state_destroy(st);

    printf("{\"case\":\"probe_speex_only\",\"kind\":\"speex_only\",\"frame\":%d,\"rate\":%d,"
           "\"suppression_db\":%d,\"frames\":%zu,\"samples\":%zu}\n",
           AUDIO_ENH_FRAME_SIZE, AUDIO_ENH_SAMPLE_RATE, rb, frames,
           frames * (size_t) AUDIO_ENH_FRAME_SIZE);

    free(in);

    return 0;
}

/* audio_enh_nr_set_level() contra la lib real: lo que pide la API vs lo que
 * queda configurado en el estado (GET) vs la atenuacion RMS medida. */
static int probe_levels(const char *in_path) {
    size_t n = 0;
    int16_t *in = wav_load(in_path, &n);
    const int levels[4] = {0, 40, 60, 100};
    size_t block = (size_t) AUDIO_ENH_SAMPLE_RATE / 10;

    if (!in || n < block) {
        free(in);
        return 1;
    }

    for (int k = 0; k < 4; k++) {
        int16_t *buf = malloc(block * sizeof *buf);
        double in_acc = 0.0;
        double out_acc = 0.0;
        size_t callbacks = n / block;
        int have_state = 0;
        int suppress;

        if (!buf) {
            free(in);
            return 1;
        }

        audio_enh_init();
        audio_enh_set_enabled(true);
        audio_enh_nr_set_enabled(true);
        audio_enh_nr_set_level((uint8_t) levels[k]);

        suppress = effective_suppression_db(&have_state);

        for (size_t b = 0; b < callbacks; b++) {
            memcpy(buf, in + b * block, block * sizeof *buf);

            for (size_t i = 0; i < block; i++) {
                in_acc += (double) buf[i] * (double) buf[i];
            }

            audio_enh_process(buf, block);

            for (size_t i = 0; i < block; i++) {
                out_acc += (double) buf[i] * (double) buf[i];
            }
        }

        printf("{\"case\":\"probe_level_%d\",\"kind\":\"level\",\"level\":%d,"
               "\"formula_db\":%d,\"speex_state\":%d,\"suppression_db\":%d,\"frame\":%d,"
               "\"rate\":%d,\"frames\":%zu,\"in_rms_dbfs\":%.3f,\"out_rms_dbfs\":%.3f,"
               "\"atten_db\":%.3f}\n",
               levels[k], levels[k], -10 - (levels[k] * 20) / 100, have_state, suppress,
               AUDIO_ENH_FRAME_SIZE, AUDIO_ENH_SAMPLE_RATE, callbacks * (block / AUDIO_ENH_FRAME_SIZE),
               10.0 * log10(in_acc / (double) (callbacks * block) / (32768.0 * 32768.0)),
               10.0 * log10(out_acc / (double) (callbacks * block) / (32768.0 * 32768.0)),
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
    const char *out_a = NULL;
    const char *out_b = NULL;
    const char *blocks_text_arg = NULL;
    const char *blocks_a_arg = NULL;
    const char *blocks_b_arg = NULL;
    const char *probe = NULL;
    int preset = 0;
    int seconds = 10;
    int partition_check = 0;
    block_list_t blocks = {{0}, 0};
    block_list_t blocks_a = {{0}, 0};
    block_list_t blocks_b = {{0}, 0};

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--case") == 0 && i + 1 < argc) {
            case_name = argv[++i];
        } else if (strcmp(argv[i], "--in") == 0 && i + 1 < argc) {
            in_path = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (strcmp(argv[i], "--out-a") == 0 && i + 1 < argc) {
            out_a = argv[++i];
        } else if (strcmp(argv[i], "--out-b") == 0 && i + 1 < argc) {
            out_b = argv[++i];
        } else if (strcmp(argv[i], "--preset") == 0 && i + 1 < argc) {
            preset = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--blocks") == 0 && i + 1 < argc) {
            blocks_text_arg = argv[++i];
        } else if (strcmp(argv[i], "--blocks-a") == 0 && i + 1 < argc) {
            blocks_a_arg = argv[++i];
        } else if (strcmp(argv[i], "--blocks-b") == 0 && i + 1 < argc) {
            blocks_b_arg = argv[++i];
        } else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
            seconds = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--partition-check") == 0 && i + 1 < argc) {
            partition_check = 1;
            case_name = argv[++i];
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

    if (probe && strcmp(probe, "levels") == 0) {
        return probe_levels(in_path);
    }

    if (probe && strcmp(probe, "speex-only") == 0) {
        if (!out_path) {
            fprintf(stderr, "speex-only necesita --out\n");
            return 2;
        }

        return probe_speex_only(in_path, out_path, 18);
    }

    if (partition_check) {
        if (!case_name || !out_a || !out_b || !blocks_a_arg || !blocks_b_arg) {
            fprintf(stderr, "partition-check necesita --blocks-a/--blocks-b/--out-a/--out-b\n");
            return 2;
        }

        if (parse_blocks(blocks_a_arg, &blocks_a) != 0 || parse_blocks(blocks_b_arg, &blocks_b) != 0) {
            fprintf(stderr, "particion invalida\n");
            return 2;
        }

        return run_partition_check(case_name, in_path, preset, &blocks_a, &blocks_b, out_a, out_b);
    }

    if (!case_name || !out_path || !blocks_text_arg) {
        fprintf(stderr, "faltan --case/--out/--blocks\n");
        return 2;
    }

    if (parse_blocks(blocks_text_arg, &blocks) != 0) {
        fprintf(stderr, "particion invalida: %s\n", blocks_text_arg);
        return 2;
    }

    return run_stream(case_name, in_path, out_path, preset, &blocks, seconds);
}
