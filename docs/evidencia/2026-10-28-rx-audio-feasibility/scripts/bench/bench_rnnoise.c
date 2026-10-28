/*
 * Banco offline de tres cadenas de audio RX (issue #345, anexo vinculante, tanda D).
 *
 * Cadenas comparables sobre el MISMO PCM mono S16 44100 Hz:
 *   off          passthrough bit-exacto (sin resampling)  -> control obligatorio
 *   resample     resampling real 44100 -> 48000 -> 44100, sin NR (sonda: costo y
 *                latencia del resampler aislados)
 *   speex        src/audio_enhancement.c COPIADO VERBATIM contra libspeexdsp real,
 *                preset SSB_VOICE (igual que la tarea B)
 *   rnnoise      resample -> RNNoise (480 muestras, escala S16) -> resample
 *   rnnoise_norm idem rnnoise pero con la conversion /32768 (control de escala)
 *
 * Unidad de trabajo: 441 muestras de entrada = 10.000 ms a 44100 Hz, que mapean
 * EXACTO a 480 muestras a 48000 Hz (441*160/147 = 480) y de vuelta. Cada unidad
 * se cronometra con CLOCK_MONOTONIC y sus tiempos se acumulan para p50/p95/p99.
 * El estado (RNNoise, resampler, Speex) se crea al principio de la pasada y NO se
 * resetea entre repeticiones: una pasada = N repeticiones del corpus de 10 s.
 *
 * RNNoise se usa en la escala de PCM S16 (amplitudes ~+-32768), que es la escala
 * con la que se entreno el modelo (src/dump_features.c:440-446 normaliza a RMS
 * ~3000 y recorta a +-32767) y la que usa el demo upstream. La variante /32768
 * existe SOLO como control: con amplitudes ~1.0 la energia por banda cae bajo el
 * umbral de silencio de rnnoise (src/denoise.c:389-392) y la cadena no atenua.
 *
 * Sin RNNoise disponible (compilado con -DX6200_BENCH_NO_RNNOISE, o --simulate-missing)
 * la cadena rnnoise cae a off: salida bit-exacta, nunca silencio ni archivo corto.
 *
 * Salida: una linea JSON por pasada en stdout.
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

#ifndef X6200_BENCH_NO_RNNOISE
#include "rnnoise.h"
#endif

#define IN_RATE 44100
#define MID_RATE 48000
#define UNIT_IN 441 /* 10 ms a 44100 Hz */
#define UNIT_MID 480 /* 10 ms a 48000 Hz = rnnoise frame */
#define RS_TAPS 64 /* taps por fase del resampler polifasico */
#define RS_BETA 8.0 /* ventana Kaiser (atenuacion de stopband ~80 dB) */
#define MAX_UNIT 1024
#define BENCH_PI 3.14159265358979323846 /* M_PI no esta en C11 estricto */

/* ------------------------------------------------------------------------- */
/* utilidades                                                                */
/* ------------------------------------------------------------------------- */

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

static long status_kb(const char *key) {
    FILE *f = fopen("/proc/self/status", "r");
    char line[256];
    size_t klen = strlen(key);
    long kb = -1;

    if (!f) {
        return -1;
    }

    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, key, klen) == 0) {
            kb = strtol(line + klen, NULL, 10);
            break;
        }
    }

    fclose(f);

    return kb;
}

/* Redondeo simetrico explicito: no depende del modo de redondeo de libm. */
static int16_t s16_from_double(double v) {
    double r = (v >= 0.0) ? floor(v + 0.5) : -floor(-v + 0.5);

    if (r > 32767.0) {
        return 32767;
    }

    if (r < -32768.0) {
        return -32768;
    }

    return (int16_t) r;
}

static int16_t *raw_load(const char *path, size_t *nsamples) {
    FILE *f = fopen(path, "rb");
    long sz;
    int16_t *data;

    if (!f) {
        fprintf(stderr, "no puedo abrir %s\n", path);
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }

    sz = ftell(f);

    if (sz <= 0 || (sz & 1) != 0) {
        fprintf(stderr, "%s: tamano invalido (%ld)\n", path, sz);
        fclose(f);
        return NULL;
    }

    rewind(f);
    data = malloc((size_t) sz);

    if (!data || fread(data, 1, (size_t) sz, f) != (size_t) sz) {
        fprintf(stderr, "%s: no puedo leer %ld bytes\n", path, sz);
        free(data);
        fclose(f);
        return NULL;
    }

    fclose(f);
    *nsamples = (size_t) sz / 2;

    return data;
}

/* ------------------------------------------------------------------------- */
/* percentiles (nearest-rank sobre copia ordenada)                           */
/* ------------------------------------------------------------------------- */

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *) a;
    double y = *(const double *) b;

    return (x < y) ? -1 : (x > y) ? 1 : 0;
}

static double percentile(const double *v, size_t n, double p) {
    double *copy;
    size_t rank;
    double out;

    if (n == 0) {
        return NAN;
    }

    copy = malloc(n * sizeof *copy);

    if (!copy) {
        return NAN;
    }

    memcpy(copy, v, n * sizeof *copy);
    qsort(copy, n, sizeof *copy, cmp_double);
    rank = (size_t) ceil(p / 100.0 * (double) n);

    if (rank < 1) {
        rank = 1;
    }

    if (rank > n) {
        rank = n;
    }

    out = copy[rank - 1];
    free(copy);

    return out;
}

/* ------------------------------------------------------------------------- */
/* resampler polifasico racional (windowed sinc + Kaiser), estado streaming   */
/* ------------------------------------------------------------------------- */

typedef struct {
    int up;
    int down;
    int taps;
    double *coef; /* up*taps: coef[fase*taps + k] */
    double *win; /* taps + MAX_UNIT muestras de historia */
    size_t win_len;
    long long fed;
    long long out_count;
    long long out_target;
} rs_t;

static double bessel_i0(double x) {
    double sum = 1.0;
    double term = 1.0;
    double y = x * x / 4.0;
    int k;

    for (k = 1; k < 100; k++) {
        term *= y / ((double) k * (double) k);
        sum += term;

        if (term < 1e-17 * sum) {
            break;
        }
    }

    return sum;
}

static int rs_init(rs_t *rs, int up, int down, long long total_in) {
    int n = up * RS_TAPS;
    int i;
    int p;
    int k;
    /* Corte en el Nyquist menor, normalizado a la tasa INTERPOLADA (fs_in*up):
     * fc = min(1, up/down) / (2*up). Sin el /up el prototipo queda con corte en
     * fs_in/2 de la tasa interpolada (all-pass) y el resampler se degrada a
     * repetir/descartar muestras, que es justo lo que el anexo prohibe. */
    double fc = 0.5 * ((up < down) ? (double) up / (double) down : 1.0) / (double) up;
    double center = (double) (n - 1) / 2.0;
    double i0b = bessel_i0(RS_BETA);

    memset(rs, 0, sizeof *rs);
    rs->up = up;
    rs->down = down;
    rs->taps = RS_TAPS;
    rs->coef = calloc((size_t) n, sizeof *rs->coef);
    rs->win = calloc((size_t) RS_TAPS + MAX_UNIT, sizeof *rs->win);

    if (!rs->coef || !rs->win) {
        return -1;
    }

    for (i = 0; i < n; i++) {
        double t = (double) i - center;
        double sinc = (fabs(t) < 1e-12) ? 2.0 * fc : sin(2.0 * BENCH_PI * fc * t) / (BENCH_PI * t);
        double r = 2.0 * (double) i / (double) (n - 1) - 1.0;
        double arg = 1.0 - r * r;

        if (arg < 0.0) {
            arg = 0.0;
        }

        rs->coef[i] = sinc * bessel_i0(RS_BETA * sqrt(arg)) / i0b;
    }

    /* Normalizacion por fase: ganancia DC exactamente 1 en cada una. */
    for (p = 0; p < up; p++) {
        double sum = 0.0;

        for (k = 0; k < rs->taps; k++) {
            sum += rs->coef[p + k * up];
        }

        if (fabs(sum) < 1e-12) {
            fprintf(stderr, "resampler: fase %d sin energia\n", p);
            return -1;
        }

        for (k = 0; k < rs->taps; k++) {
            rs->coef[p + k * up] /= sum;
        }
    }

    rs->out_target = total_in * up / down;

    return 0;
}

static void rs_free(rs_t *rs) {
    free(rs->coef);
    free(rs->win);
    rs->coef = NULL;
    rs->win = NULL;
}

/* Procesa n_in muestras y escribe hasta out_cap de salida. Devuelve cuantas. */
static size_t rs_process(rs_t *rs, const double *in, size_t n_in, double *out, size_t out_cap) {
    size_t produced = 0;
    size_t j;
    long long start;

    if (n_in > MAX_UNIT) {
        fprintf(stderr, "resampler: bloque %zu > MAX_UNIT\n", n_in);
        exit(3);
    }

    if (rs->win_len > (size_t) rs->taps) {
        size_t drop = rs->win_len - (size_t) rs->taps;

        memmove(rs->win, rs->win + drop, (size_t) rs->taps * sizeof *rs->win);
        rs->win_len = (size_t) rs->taps;
    }

    start = rs->fed - (long long) rs->win_len;

    for (j = 0; j < n_in; j++) {
        rs->win[rs->win_len + j] = in[j];
    }

    rs->win_len += n_in;
    rs->fed += (long long) n_in;

    while (produced < out_cap && rs->out_count < rs->out_target) {
        long long m = rs->out_count;
        long long pos = m * rs->down;
        int p = (int) (pos % rs->up);
        long long base = pos / rs->up;
        double acc = 0.0;
        int k;

        if (base >= rs->fed) {
            break;
        }

        for (k = 0; k < rs->taps; k++) {
            long long idx = base - k;
            double x;

            if (idx < 0) {
                continue; /* antes del comienzo del stream: cero implicito */
            }

            if (idx < start) {
                fprintf(stderr, "resampler: ventana insuficiente (idx %lld < %lld)\n", idx, start);
                exit(3);
            }

            x = rs->win[idx - start];
            acc += rs->coef[p + k * rs->up] * x;
        }

        out[produced++] = acc;
        rs->out_count++;
    }

    return produced;
}

/* ------------------------------------------------------------------------- */
/* RNNoise (opcional en tiempo de compilacion)                               */
/* ------------------------------------------------------------------------- */

static int g_simulate_missing = 0;

static int rnnoise_available(void) {
#ifdef X6200_BENCH_NO_RNNOISE
    return 0;
#else
    if (g_simulate_missing) {
        return 0;
    }

    return 1;
#endif
}

static int rnnoise_frame_size(void) {
#ifdef X6200_BENCH_NO_RNNOISE
    return -1;
#else
    if (!rnnoise_available()) {
        return -1;
    }

    return rnnoise_get_frame_size();
#endif
}

/* Apertura/cierre de estado y una trama de 480 muestras.
 *
 * `norm == 0` (cadena principal): la muestra S16 se pasa tal cual, que es la
 * escala de entrenamiento del modelo. `norm == 1` (control): se divide por
 * 32768 y se multiplica de vuelta, para medir el efecto de la otra lectura.
 */
#ifdef X6200_BENCH_NO_RNNOISE

static int nr_open(void) {
    return -1;
}

static void nr_close(void) {}

static void nr_frame(const double *in, double *out, int norm) {
    (void) in;
    (void) out;
    (void) norm;
    fprintf(stderr, "FATAL: trama RNNoise sin libreria (no deberia ejecutarse)\n");
    exit(3);
}

#else

static DenoiseState *g_st = NULL;

static int nr_open(void) {
    g_st = rnnoise_create(NULL);

    return g_st ? 0 : -1;
}

static void nr_close(void) {
    if (g_st) {
        rnnoise_destroy(g_st);
        g_st = NULL;
    }
}

static void nr_frame(const double *in, double *out, int norm) {
    float fin[UNIT_MID];
    float fout[UNIT_MID];
    int i;

    for (i = 0; i < UNIT_MID; i++) {
        fin[i] = norm ? (float) (in[i] / 32768.0) : (float) in[i];
    }

    (void) rnnoise_process_frame(g_st, fout, fin);

    for (i = 0; i < UNIT_MID; i++) {
        out[i] = norm ? (double) fout[i] * 32768.0 : (double) fout[i];
    }
}

#endif

/* ------------------------------------------------------------------------- */
/* pasada                                                                    */
/* ------------------------------------------------------------------------- */

typedef enum {
    M_OFF = 0,
    M_RESAMPLE,
    M_SPEEX,
    M_RNNOISE,
    M_RNNOISE_NORM
} method_t;

static const char *method_name(method_t m) {
    switch (m) {
        case M_OFF: return "off";
        case M_RESAMPLE: return "resample";
        case M_SPEEX: return "speex";
        case M_RNNOISE: return "rnnoise";
        case M_RNNOISE_NORM: return "rnnoise_norm";
        default: return "unknown";
    }
}

static int parse_method(const char *s, method_t *out) {
    if (strcmp(s, "off") == 0) {
        *out = M_OFF;
    } else if (strcmp(s, "resample") == 0) {
        *out = M_RESAMPLE;
    } else if (strcmp(s, "speex") == 0) {
        *out = M_SPEEX;
    } else if (strcmp(s, "rnnoise") == 0) {
        *out = M_RNNOISE;
    } else if (strcmp(s, "rnnoise_norm") == 0) {
        *out = M_RNNOISE_NORM;
    } else {
        return -1;
    }

    return 0;
}

static int run_pass(const char *name, const char *in_path, const char *out_path, method_t method,
                    int seconds, int preset, size_t block) {
    size_t n_in = 0;
    int16_t *in = raw_load(in_path, &n_in);
    int16_t *work = NULL;
    double *unit_in = NULL;
    double *mid = NULL;
    double *mid2 = NULL;
    double *unit_out = NULL;
    double *t_total = NULL;
    double *t_up = NULL;
    double *t_nr = NULL;
    double *t_down = NULL;
    double *t_speex = NULL;
    rs_t up = {0};
    rs_t down = {0};
    FILE *out = NULL;
    size_t reps;
    size_t units_per_rep;
    size_t units = 0;
    size_t units_cap;
    size_t over10 = 0;
    double wall0;
    double wall1;
    double user0;
    double user1;
    double sys0;
    double sys1;
    long rss_preinit;
    long rss_start;
    long rss_end;
    long rss_peak;
    int fallback = 0;
    const char *effective = method_name(method);
    int rn_frames = 0;
    int fs;

    if (!in || n_in == 0) {
        free(in);
        return 1;
    }

    if (block == 0 || block > n_in) {
        fprintf(stderr, "bloque %zu invalido para %zu muestras\n", block, n_in);
        free(in);
        return 1;
    }

    if (block != UNIT_IN && method != M_OFF && method != M_SPEEX) {
        fprintf(stderr, "--block solo aplica a off/speex (el resampler fija 441 muestras = 10 ms)\n");
        free(in);
        return 1;
    }

    reps = (size_t) seconds * IN_RATE / n_in;
    units_per_rep = (n_in + block - 1) / block; /* el ultimo bloque puede ser parcial */
    units_cap = units_per_rep * reps;

    if (reps == 0 || units_cap == 0) {
        fprintf(stderr, "combinacion de --seconds y archivo invalida\n");
        free(in);
        return 1;
    }

    fs = rnnoise_frame_size();

    if ((method == M_RNNOISE || method == M_RNNOISE_NORM) && !rnnoise_available()) {
        fallback = 1;
        effective = "off";
        fprintf(stderr, "AVISO: RNNoise no disponible; la cadena cae a off (bit-exacto)\n");
    } else if (method == M_RNNOISE || method == M_RNNOISE_NORM) {
        if (fs != UNIT_MID) {
            fprintf(stderr, "rnnoise_get_frame_size() = %d, se esperaba %d\n", fs, UNIT_MID);
            free(in);
            return 3;
        }
    }

    work = malloc(block * sizeof *work);
    unit_in = malloc(UNIT_IN * sizeof *unit_in);
    mid = malloc(UNIT_MID * sizeof *mid);
    mid2 = malloc(UNIT_MID * sizeof *mid2);
    unit_out = malloc(UNIT_IN * sizeof *unit_out);
    t_total = malloc(units_cap * sizeof *t_total);
    t_up = malloc(units_cap * sizeof *t_up);
    t_nr = malloc(units_cap * sizeof *t_nr);
    t_down = malloc(units_cap * sizeof *t_down);
    t_speex = malloc(units_cap * sizeof *t_speex);

    if (!work || !unit_in || !mid || !mid2 || !unit_out || !t_total || !t_up || !t_nr || !t_down ||
        !t_speex) {
        fprintf(stderr, "sin memoria\n");
        free(in);
        return 1;
    }

    rss_preinit = status_kb("VmRSS:");

    if (method == M_SPEEX) {
        audio_enh_init();
        audio_enh_preset_load((audio_enh_preset_t) preset);
    }

    if (method == M_RESAMPLE || method == M_RNNOISE || method == M_RNNOISE_NORM) {
        if (rs_init(&up, 160, 147, (long long) units_cap * UNIT_IN) != 0 ||
            rs_init(&down, 147, 160, (long long) units_cap * UNIT_MID) != 0) {
            fprintf(stderr, "no puedo inicializar el resampler\n");
            free(in);
            return 1;
        }
    }

    if ((method == M_RNNOISE || method == M_RNNOISE_NORM) && !fallback) {
        if (nr_open() != 0) {
            fprintf(stderr, "rnnoise_create(NULL) fallo; la cadena cae a off\n");
            fallback = 1;
            effective = "off";
        }
    }

    if (out_path) {
        out = fopen(out_path, "wb");

        if (!out) {
            fprintf(stderr, "no puedo abrir %s\n", out_path);
            free(in);
            return 1;
        }
    }

    rss_start = status_kb("VmRSS:");
    cpu_ms(&user0, &sys0);
    wall0 = mono_ms();

    for (size_t r = 0; r < reps; r++) {
        for (size_t u = 0; u < units_per_rep; u++) {
            const int16_t *src = in + u * block;
            size_t n = (n_in - u * block < block) ? (n_in - u * block) : block;
            double t0;
            double t1;
            double t2;
            double t3;
            double t4;

            t0 = mono_ms();

            if (method == M_OFF || fallback) {
                memcpy(work, src, n * sizeof *work);
                t1 = t0;
                t2 = t0;
                t3 = t0;
                t4 = t0;
            } else if (method == M_SPEEX) {
                memcpy(work, src, n * sizeof *work);
                t1 = t0; /* la cadena Speex no resamplea */
                audio_enh_process(work, n);
                t2 = mono_ms();
                t3 = t2;
                t4 = t2;
            } else {
                /* La conversion S16 -> double cuenta como parte de la subida y
                 * la vuelta como parte de la bajada: total = subida + NR + bajada. */
                t1 = mono_ms();

                for (size_t i = 0; i < UNIT_IN; i++) {
                    unit_in[i] = (double) src[i];
                }

                (void) rs_process(&up, unit_in, UNIT_IN, mid, UNIT_MID);
                t2 = mono_ms();

                if (method == M_RESAMPLE) {
                    memcpy(mid2, mid, UNIT_MID * sizeof *mid2);
                } else {
                    nr_frame(mid, mid2, method == M_RNNOISE_NORM);
                    rn_frames++;
                }

                t3 = mono_ms();
                (void) rs_process(&down, mid2, UNIT_MID, unit_out, UNIT_IN);

                for (size_t i = 0; i < UNIT_IN; i++) {
                    work[i] = s16_from_double(unit_out[i]);
                }

                t4 = mono_ms();
            }

            t_total[units] = t4 - t0;
            t_up[units] = t2 - t1;
            t_nr[units] = t3 - t2;
            t_down[units] = t4 - t3;
            t_speex[units] = (method == M_SPEEX) ? (t2 - t1) : 0.0;

            if (t_total[units] > 10.0) {
                over10++;
            }

            units++;

            /* Solo la primera repeticion se escribe: es la pasada con estado fresco. */
            if (out && r == 0) {
                if (fwrite(work, sizeof *work, n, out) != n) {
                    fprintf(stderr, "fallo de escritura\n");
                    free(in);
                    return 1;
                }
            }
        }
    }

    wall1 = mono_ms();
    cpu_ms(&user1, &sys1);
    rss_end = status_kb("VmRSS:");
    rss_peak = status_kb("VmHWM:");

    if (out) {
        fclose(out);
    }

    if (method == M_SPEEX) {
        audio_enh_deinit();
    }

    nr_close();

    printf("{\"case\":\"%s\",\"method\":\"%s\",\"method_effective\":\"%s\",\"fallback\":%d,"
           "\"seconds\":%d,\"reps\":%zu,\"units\":%zu,\"block\":%zu,\"unit_in\":%d,\"unit_mid\":%d,"
           "\"in_samples\":%zu,\"out_samples\":%zu,\"rnnoise_frame_size\":%d,\"rnnoise_frames\":%d,"
           "\"taps\":%d,\"up\":\"160/147\",\"down\":\"147/160\","
           "\"wall_ms\":%.3f,\"user_ms\":%.3f,\"sys_ms\":%.3f,"
           "\"frame_p50_ms\":%.8f,\"frame_p95_ms\":%.8f,\"frame_p99_ms\":%.8f,\"frame_max_ms\":%.8f,"
           "\"up_p50_ms\":%.8f,\"up_p95_ms\":%.8f,\"up_p99_ms\":%.8f,"
           "\"nr_p50_ms\":%.8f,\"nr_p95_ms\":%.8f,\"nr_p99_ms\":%.8f,"
           "\"down_p50_ms\":%.8f,\"down_p95_ms\":%.8f,\"down_p99_ms\":%.8f,"
           "\"speex_p50_ms\":%.8f,\"speex_p95_ms\":%.8f,\"speex_p99_ms\":%.8f,"
           "\"over_10ms\":%zu,\"pct_over_10ms\":%.4f,"
           "\"rss_preinit_kb\":%ld,\"rss_start_kb\":%ld,\"rss_end_kb\":%ld,\"rss_peak_kb\":%ld}\n",
           name, method_name(method), effective, fallback, seconds, reps, units, block, UNIT_IN, UNIT_MID,
           n_in, (size_t) reps * n_in, fs, rn_frames, RS_TAPS, wall1 - wall0, user1 - user0, sys1 - sys0,
           percentile(t_total, units, 50.0), percentile(t_total, units, 95.0),
           percentile(t_total, units, 99.0), percentile(t_total, units, 100.0),
           percentile(t_up, units, 50.0), percentile(t_up, units, 95.0), percentile(t_up, units, 99.0),
           percentile(t_nr, units, 50.0), percentile(t_nr, units, 95.0), percentile(t_nr, units, 99.0),
           percentile(t_down, units, 50.0), percentile(t_down, units, 95.0),
           percentile(t_down, units, 99.0), percentile(t_speex, units, 50.0),
           percentile(t_speex, units, 95.0), percentile(t_speex, units, 99.0), over10,
           100.0 * (double) over10 / (double) units, rss_preinit, rss_start, rss_end, rss_peak);

    if (method == M_RESAMPLE || method == M_RNNOISE || method == M_RNNOISE_NORM) {
        rs_free(&up);
        rs_free(&down);
    }

    free(work);
    free(unit_in);
    free(mid);
    free(mid2);
    free(unit_out);
    free(t_total);
    free(t_up);
    free(t_nr);
    free(t_down);
    free(t_speex);
    free(in);

    return 0;
}

int main(int argc, char **argv) {
    const char *case_name = NULL;
    const char *in_path = NULL;
    const char *out_path = NULL;
    const char *method_s = NULL;
    int seconds = 60;
    int preset = 2; /* AUDIO_ENH_PRESET_SSB_VOICE */
    size_t block = UNIT_IN;
    method_t method = M_OFF;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--case") == 0 && i + 1 < argc) {
            case_name = argv[++i];
        } else if (strcmp(argv[i], "--in") == 0 && i + 1 < argc) {
            in_path = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (strcmp(argv[i], "--method") == 0 && i + 1 < argc) {
            method_s = argv[++i];
        } else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
            seconds = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--preset") == 0 && i + 1 < argc) {
            preset = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--block") == 0 && i + 1 < argc) {
            block = (size_t) strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--simulate-missing") == 0) {
            g_simulate_missing = 1;
        } else if (strcmp(argv[i], "--rnnoise-frame-size") == 0) {
            printf("%d\n", rnnoise_frame_size());
            return (rnnoise_frame_size() > 0) ? 0 : 4;
        } else {
            fprintf(stderr, "argumento desconocido: %s\n", argv[i]);
            return 2;
        }
    }

    if (!in_path || !method_s || !case_name) {
        fprintf(stderr, "faltan --in/--method/--case\n");
        return 2;
    }

    if (parse_method(method_s, &method) != 0) {
        fprintf(stderr, "metodo desconocido: %s\n", method_s);
        return 2;
    }

    if (seconds <= 0) {
        fprintf(stderr, "--seconds invalido\n");
        return 2;
    }

    return run_pass(case_name, in_path, out_path, method, seconds, preset, block);
}
