/*
 * Driver mínimo del banco objetivo de #349.
 *
 * Procesa un archivo RAW PCM S16LE mono 44100 Hz con la cadena de producción
 * src/audio_enhancement.c COPIADA VERBATIM (sha256 registrado por
 * scripts/build_speex_driver.sh) y libspeexdsp real 1.2.1 del contenedor de
 * #351. No reimplementa Speex ni sustituye el módulo: sólo lo llama en
 * callbacks del tamaño pedido y devuelve el PCM resultante.
 *
 * Uso:
 *   speex_driver --in input.raw --out output.raw --preset 0|2 \
 *                [--blocks "4410"] [--json-out stats.json]
 *
 * Salida: una línea JSON por invocación en stdout.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "audio_enhancement.h"

typedef struct {
    size_t v[64];
    int    n;
} block_list_t;

static double now_ms(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (double) ts.tv_sec * 1000.0 + (double) ts.tv_nsec / 1e6;
}

static int parse_blocks(const char *text, block_list_t *blocks) {
    char        *copy;
    char        *token;
    char        *save = NULL;
    int          count = 0;

    copy = strdup(text);

    if (!copy) {
        return -1;
    }

    for (token = strtok_r(copy, ",", &save);
         token != NULL && count < (int) (sizeof blocks->v / sizeof blocks->v[0]);
         token = strtok_r(NULL, ",", &save)) {
        char     *end = NULL;
        unsigned long value = strtoul(token, &end, 10);

        if (end == token || *end != '\0' || value == 0) {
            free(copy);
            return -1;
        }

        blocks->v[count++] = (size_t) value;
    }

    free(copy);

    if (count == 0) {
        return -1;
    }

    blocks->n = count;

    return 0;
}

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *) a;
    double y = *(const double *) b;

    return (x > y) - (x < y);
}

static double percentile(const double *values, size_t n, double p) {
    if (n == 0) {
        return 0.0;
    }

    double index = (p / 100.0) * (double) (n - 1);
    size_t lo = (size_t) floor(index);
    size_t hi = (size_t) ceil(index);

    if (lo == hi) {
        return values[lo];
    }

    return values[lo] + (values[hi] - values[lo]) * (index - (double) lo);
}

int main(int argc, char **argv) {
    const char    *in_path = NULL;
    const char    *out_path = NULL;
    const char    *json_path = NULL;
    int            preset = 0;
    block_list_t   blocks = {{4410}, 1};

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--in") == 0 && i + 1 < argc) {
            in_path = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (strcmp(argv[i], "--preset") == 0 && i + 1 < argc) {
            preset = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--blocks") == 0 && i + 1 < argc) {
            if (parse_blocks(argv[++i], &blocks) != 0) {
                fprintf(stderr, "bloques invalidos\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--json-out") == 0 && i + 1 < argc) {
            json_path = argv[++i];
        } else {
            fprintf(stderr, "argumento desconocido: %s\n", argv[i]);
            return 2;
        }
    }

    if (!in_path || !out_path) {
        fprintf(stderr, "faltan --in/--out\n");
        return 2;
    }

    FILE *fi = fopen(in_path, "rb");

    if (!fi) {
        perror(in_path);
        return 1;
    }

    if (fseek(fi, 0, SEEK_END) != 0) {
        fclose(fi);
        return 1;
    }

    long bytes = ftell(fi);

    if (bytes < 0 || bytes % 2 != 0) {
        fclose(fi);
        fprintf(stderr, "tamano invalido\n");
        return 1;
    }

    if (fseek(fi, 0, SEEK_SET) != 0) {
        fclose(fi);
        return 1;
    }

    size_t nsamples = (size_t) bytes / 2;
    int16_t *input = malloc(nsamples * sizeof *input);

    if (!input) {
        fclose(fi);
        return 1;
    }

    if (fread(input, sizeof *input, nsamples, fi) != nsamples) {
        fclose(fi);
        free(input);
        fprintf(stderr, "lectura incompleta\n");
        return 1;
    }

    fclose(fi);

    if (nsamples == 0) {
        free(input);
        fprintf(stderr, "input vacio\n");
        return 1;
    }

    int16_t *work = malloc(nsamples * sizeof *work);

    if (!work) {
        free(input);
        return 1;
    }

    memcpy(work, input, nsamples * sizeof *work);

    double *cb_ms = malloc((nsamples + 1) * sizeof *cb_ms);
    size_t  cb_count = 0;
    double  t0 = now_ms();

    audio_enh_init();
    audio_enh_preset_load((audio_enh_preset_t) preset);

    size_t pos = 0;
    int    pattern = 0;

    while (pos < nsamples) {
        size_t block = blocks.v[pattern % blocks.n];

        if (block > nsamples - pos) {
            block = nsamples - pos;
        }

        double cb0 = now_ms();

        audio_enh_process(work + pos, block);

        double cb1 = now_ms();

        if (cb_ms) {
            cb_ms[cb_count++] = cb1 - cb0;
        }

        pos += block;
        pattern++;
    }

    double t1 = now_ms();

    audio_enh_deinit();

    FILE *fo = fopen(out_path, "wb");

    if (!fo) {
        perror(out_path);
        free(input);
        free(work);
        free(cb_ms);
        return 1;
    }

    if (fwrite(work, sizeof *work, nsamples, fo) != nsamples) {
        fclose(fo);
        free(input);
        free(work);
        free(cb_ms);
        return 1;
    }

    fclose(fo);

    if (cb_ms) {
        qsort(cb_ms, cb_count, sizeof *cb_ms, cmp_double);
    }

    double p50 = cb_ms ? percentile(cb_ms, cb_count, 50.0) : 0.0;
    double p99 = cb_ms ? percentile(cb_ms, cb_count, 99.0) : 0.0;
    double max_cb = (cb_ms && cb_count) ? cb_ms[cb_count - 1] : 0.0;
    double sum_cb = 0.0;

    for (size_t i = 0; i < cb_count; i++) {
        sum_cb += cb_ms[i];
    }

    char line[1024];

    snprintf(line, sizeof line,
             "{\"input\":\"%s\",\"output\":\"%s\",\"preset\":%d,"
             "\"block\":%zu,\"input_samples\":%zu,\"out_samples\":%zu,"
             "\"callbacks\":%zu,\"length_preserved\":%d,"
             "\"cb_p50_ms\":%.6f,\"cb_p99_ms\":%.6f,\"cb_max_ms\":%.6f,"
             "\"wall_ms\":%.3f,\"cb_sum_ms\":%.3f}\n",
             in_path, out_path, preset, blocks.v[0], nsamples, nsamples,
             cb_count, 1, p50, p99, max_cb, t1 - t0, sum_cb);
    fputs(line, stdout);

    if (json_path) {
        FILE *js = fopen(json_path, "a");
        if (js) {
            fputs(line, js);
            fclose(js);
        }
    }

    free(input);
    free(work);
    free(cb_ms);

    return 0;
}
