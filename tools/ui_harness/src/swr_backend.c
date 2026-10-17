/*
 *  Fake ROE acquisition backend for the host UI harness (see swr_backend.h).
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "swr_backend.h"

#include <stdlib.h>
#include <string.h>

#include "buttons.h"
#include "cfg/cfg.h"
#include "cfg/subjects.h"
#include "dialog_swrscan.h"
#include "scheduler.h"
#include "swr_session.h"
#include "virtual_time.h"

#include "mock_radio.h"

/* Ruta de los fixtures del owner (tests/fixtures/swrscan/): la fija el CMake con
 * la raiz del repo (HARNESS_SWR_FIXTURES_DIR), asi que el arnes no lee el entorno
 * para encontrarlos. El CSV es el mismo archivo que consume el test nativo
 * (tests/native/swrscan_fixtures.c): un solo origen de datos para las dos
 * superficies. */
#ifndef HARNESS_SWR_FIXTURES_DIR
#define HARNESS_SWR_FIXTURES_DIR "tests/fixtures/swrscan"
#endif

#define SWR_FX_MAX_SAMPLES 2048

typedef struct {
    int      pass_index;
    int      order_in_pass;
    int      point_index;
    uint32_t freq_hz;
    int      raw_tenths;
    int      is_valid;
} swr_fx_sample_t;

static swr_fx_sample_t samples[SWR_FX_MAX_SAMPLES];
static int             n_samples;
static int             passes_in_dataset;
static uint32_t        grid_lo;
static uint32_t        grid_hi;

typedef struct {
    int      passes;                /* pasadas pedidas al dialogo */
    int      fed;                   /* paquetes entregados */
    int      fed_per_pass[SWR_SESSION_MAX_PASSES];
    unsigned freq_cmds;             /* radio_set_freq() del barrido (sin el dial) */
    unsigned tx_on;                 /* radio_start_swrscan() */
    unsigned tx_off;                /* radio_stop_swrscan() */
    bool     ended;                 /* la sesion termino (Completed o Stopped) */
    bool     completed;
    bool     stopped;
    bool     frozen;                /* con la sesion terminada, mas paquetes no la mueven */
    uint64_t virtual_start_ms;
    uint64_t virtual_end_ms;
    char     state[64];
    char     state_per_pass[SWR_SESSION_MAX_PASSES][64];
} swr_trace_t;

static swr_trace_t trace;
static bool        prepared;

/* --- el dataset del fixture -------------------------------------------------- */

/* Campo `n` (0-based) de una linea CSV. Los fixtures son CSV planos, sin comillas
 * ni comas embebidas: alcanza con cortar por coma. */
static void csv_field(const char *line, int n, char *out, size_t cap)
{
    const char *p = line;
    int         i = 0;

    while (p != NULL && i < n) {
        p = strchr(p, ',');
        if (p != NULL) {
            p++;
        }
        i++;
    }

    size_t len = 0;

    if (p != NULL) {
        const char *end = strchr(p, ',');

        len = (end != NULL) ? (size_t) (end - p) : strlen(p);
    }
    if (len >= cap) {
        len = cap - 1;
    }
    if (p != NULL) {
        memcpy(out, p, len);
    }
    out[len] = 0;
}

static bool load_dataset(const char *name)
{
    char path[512];

    snprintf(path, sizeof(path), "%s/swr_mock_samples.csv", HARNESS_SWR_FIXTURES_DIR);

    FILE *f = fopen(path, "r");

    if (f == NULL) {
        fprintf(stderr, "swr_backend: no puedo abrir %s\n", path);
        return false;
    }

    char line[256];
    char field[64];
    int  lineno = 0;

    n_samples         = 0;
    passes_in_dataset = 0;
    grid_lo           = UINT32_MAX;
    grid_hi           = 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        lineno++;

        if (lineno == 1) {
            continue;   /* encabezado */
        }

        csv_field(line, 0, field, sizeof(field));

        if (strcmp(field, name) != 0) {
            continue;
        }

        if (n_samples >= SWR_FX_MAX_SAMPLES) {
            fprintf(stderr, "swr_backend: '%s' tiene mas de %d muestras\n", name,
                    SWR_FX_MAX_SAMPLES);
            fclose(f);
            return false;
        }

        swr_fx_sample_t *s = &samples[n_samples++];

        csv_field(line, 1, field, sizeof(field)); s->pass_index   = atoi(field);
        csv_field(line, 2, field, sizeof(field)); s->order_in_pass = atoi(field);
        csv_field(line, 4, field, sizeof(field)); s->point_index   = atoi(field);
        csv_field(line, 5, field, sizeof(field)); s->freq_hz      = (uint32_t) strtoul(field, NULL, 10);
        csv_field(line, 6, field, sizeof(field)); s->raw_tenths   = atoi(field);
        csv_field(line, 8, field, sizeof(field)); s->is_valid     = atoi(field);

        if (s->pass_index + 1 > passes_in_dataset) {
            passes_in_dataset = s->pass_index + 1;
        }
        if (s->freq_hz < grid_lo) {
            grid_lo = s->freq_hz;
        }
        if (s->freq_hz > grid_hi) {
            grid_hi = s->freq_hz;
        }
    }

    fclose(f);

    if (n_samples == 0) {
        fprintf(stderr, "swr_backend: el dataset '%s' no esta en %s\n", name, path);
        return false;
    }

    return true;
}

static const swr_fx_sample_t *find_sample(int pass, int order)
{
    for (int i = 0; i < n_samples; i++) {
        if (samples[i].pass_index == pass && samples[i].order_in_pass == order) {
            return &samples[i];
        }
    }

    return NULL;
}

/* --- el dialogo -------------------------------------------------------------- */

static button_item_t *run_button(void)
{
    return dialog_swrscan->btn_page->items[0];
}

static button_item_t *passes_button(void)
{
    return dialog_swrscan->btn_page->items[3];
}

/* "Passes:\n3 *" -> 3. */
static int passes_of(button_item_t *item)
{
    const char *s = item->label_fn();
    int         v = 0;

    for (; *s; s++) {
        if (*s >= '0' && *s <= '9') {
            v = v * 10 + (*s - '0');
        }
    }

    return v;
}

/* Lo que el boton de pasadas muestra DE VERDAD en la barra: el texto del objeto LVGL (no la
 * funcion que lo genera). Es lo que prueba que el callback del boton pidio el repintado: el
 * boton de pasadas no es un subject, asi que no hay MSG_PARAM_CHANGED que lo refresque. */
static const char *passes_label_text(void)
{
    button_item_t *item = passes_button();

    if (item == NULL || item->label_obj == NULL) {
        return "";
    }

    return lv_label_get_text(item->label_obj);
}

/* El label de la barra trae un \n (dos lineas): se escapa para que el JSON siga siendo JSON.
 * Mismo criterio que write_json_string() de ui_inspector.c, acotado a lo que el label puede
 * traer (texto ASCII del formato "Passes:\n%u%s"). */
static void json_label(FILE *f, const char *text)
{
    fputc('"', f);

    for (const char *p = text; *p; p++) {
        if (*p == '\n') {
            fputs("\\n", f);
        } else if (*p == '"' || *p == '\\') {
            fputc('\\', f);
            fputc(*p, f);
        } else {
            fputc(*p, f);
        }
    }

    fputc('"', f);
}

/* El barrido esta corriendo? El observable del usuario es el boton: el dialogo le
 * pone "Stop" mientras mide y lo vuelve a "Run" cuando el cierre de la sesion corre
 * (dialog_swrscan_run_cb / swrscan_finished_job). El `run` de dialog_t es otra cosa
 * (la ventana esta abierta), asi que no sirve para esto. */
static bool scanning(void)
{
    return strcmp(run_button()->label, "Stop") == 0;
}

static void state_snapshot(char *dst, size_t cap)
{
    snprintf(dst, cap, "%s", dialog_swrscan_state_text());
}

/* Un paquete del flow: el reloj virtual avanza el dwell real (~35 ms por paquete,
 * SWRSCAN_DWELL_MS), el dialogo recibe la lectura y corre el tick del hilo de la
 * radio (el watchdog). El scheduler del arnes corre el cierre de LVGL que el
 * dialogo encola al terminar. */
static void feed_one(float vswr)
{
    virtual_time_advance_step(SWRSCAN_DWELL_MS, SWRSCAN_DWELL_MS, NULL, NULL);
    dialog_swrscan_update(vswr);
    dialog_swrscan_poll();
    scheduler_work();
}

/* --- API -------------------------------------------------------------------- */

bool swr_backend_prepare(const scenario_t *scenario)
{
    if (scenario->swr_dataset[0] == 0) {
        fprintf(stderr,
                "swrscan: la pantalla necesita `swr_dataset <nombre>` (los datasets de "
                "tests/fixtures/swrscan/)\n");
        return false;
    }

    if (!load_dataset(scenario->swr_dataset)) {
        return false;
    }

    if (scenario->swr_passes < 1 || scenario->swr_passes > passes_in_dataset) {
        fprintf(stderr, "swrscan: el dataset '%s' tiene %d pasadas y el fixture pide %d\n",
                scenario->swr_dataset, passes_in_dataset, scenario->swr_passes);
        return false;
    }

    int32_t  span   = (int32_t) (grid_hi - grid_lo);
    uint32_t center = grid_lo + (uint32_t) (span / 2);

    /* La grilla que construye el dialogo sale del dial (cfg_cur.fg_freq) y del span
     * del cfg: los dos tienen que ser los del dataset o el fixture estaria midiendo
     * otra cosa (y el barrido pediria frecuencias que el dataset no tiene). */
    if (mock_radio.frequency_hz != center) {
        fprintf(stderr,
                "swrscan: el fixture puso el dial en %llu Hz y el dataset '%s' esta centrado en "
                "%u Hz\n",
                (unsigned long long) mock_radio.frequency_hz, scenario->swr_dataset, center);
        return false;
    }

    subject_set_int(cfg.swrscan_span.val, span);

    memset(&trace, 0, sizeof(trace));
    prepared = true;

    return true;
}

void swr_backend_run(const scenario_t *scenario)
{
    if (!prepared) {
        fprintf(stderr, "swrscan: el backend no fue preparado\n");
        exit(1);
    }

    button_item_t *run    = run_button();
    button_item_t *passes = passes_button();

    /* El boton de pasadas cicla 1 -> 3 -> 5 -> 1, como el usuario. */
    int guard = 0;

    while (passes_of(passes) != scenario->swr_passes && guard++ < 8) {
        passes->press(passes);
    }

    if (passes_of(passes) != scenario->swr_passes) {
        fprintf(stderr, "swrscan: no pude dejar el selector de pasadas en %d\n",
                scenario->swr_passes);
        exit(1);
    }

    trace.passes          = scenario->swr_passes;
    trace.virtual_start_ms = virtual_time_now_ms();

    unsigned freq_before = mock_radio_calls.set_freq;
    unsigned tx_before   = mock_radio_calls.start_swrscan;

    run->press(run);

    if (!scanning()) {
        /* El gate del dialogo (SWRSCAN_MULTIPASS_TX_ENABLED), la grilla fuera de rango
         * o una sesion que no arranco: el fixture tiene que verlo, no capturar una
         * pantalla en Error y salir verde. */
        fprintf(stderr, "swrscan: la sesion no arranco: %s\n", dialog_swrscan_state_text());
        exit(1);
    }

    int want = (scenario->swr_feed > 0) ? scenario->swr_feed
                                        : SWR_SESSION_POINTS * scenario->swr_passes;
    int cur_pass = -1;

    for (int k = 0; k < want; k++) {
        int                     pass  = k / SWR_SESSION_POINTS;
        int                     order = k % SWR_SESSION_POINTS;
        const swr_fx_sample_t  *s     = find_sample(pass, order);

        cur_pass = pass;

        if (s == NULL) {
            fprintf(stderr, "swrscan: el dataset no tiene la muestra pasada %d orden %d\n",
                    pass, order);
            exit(1);
        }

        /* La frecuencia que el dialogo acaba de pedir para este punto: la grilla y el
         * sentido de la pasada tienen que ser los del fixture (las pasadas impares barren
         * de arriba hacia abajo). El VFO del mock es el ultimo comando que salio. */
        if (mock_radio.frequency_hz != s->freq_hz) {
            fprintf(stderr,
                    "swrscan: punto %d/%d (pasada %d orden %d): el VFO quedo en %llu Hz y el "
                    "fixture pide %u Hz\n",
                    k + 1, want, pass, order, (unsigned long long) mock_radio.frequency_hz,
                    s->freq_hz);
            exit(1);
        }

        /* El estado justo antes de cerrar la pasada (el observable "p 51/51"): es la
         * evidencia por pasada que pide el issue #125. */
        if (order == SWR_SESSION_POINTS - 1) {
            state_snapshot(trace.state_per_pass[pass], sizeof(trace.state_per_pass[pass]));
        }

        /* Una muestra invalida se entrega como lectura faltante (0), que es lo que el
         * flow manda cuando no hay dato: el motor NUNCA la convierte en ROE 1.0. */
        feed_one(s->is_valid ? ((float) s->raw_tenths * 0.1f) : 0.0f);

        trace.fed++;
        trace.fed_per_pass[pass]++;

        if (!scanning()) {
            break;      /* la sesion termino sola (o se aborto) antes de la ultima muestra */
        }
    }

    if (scenario->swr_stop) {
        /* El operador cancela: el mismo boton, que ahora dice Stop. */
        run->press(run);
        trace.stopped = true;
        scheduler_work();
    }

    /* Un poco mas de reloj para que el frame quede asentado; el teardown de LVGL ya
     * corrio en el scheduler. */
    virtual_time_advance(50, 10);
    scheduler_work();

    state_snapshot(trace.state, sizeof(trace.state));
    trace.ended     = !scanning();
    trace.completed = (strcmp(trace.state, "Completed") == 0);
    trace.tx_on     = mock_radio_calls.start_swrscan - tx_before;
    trace.tx_off    = mock_radio_calls.stop_swrscan;
    trace.freq_cmds = mock_radio_calls.set_freq - freq_before;
    trace.virtual_end_ms = virtual_time_now_ms();

    /* La pasada en curso sin snapshot (se aborto o la corto el operador a mitad): su
     * estado final es el que quedo en pantalla. */
    if (cur_pass >= 0 && trace.ended && trace.state_per_pass[cur_pass][0] == 0) {
        state_snapshot(trace.state_per_pass[cur_pass], sizeof(trace.state_per_pass[cur_pass]));
    }

    if (!trace.ended) {
        /* A mitad de barrido: la sesion sigue viva y la grafica NO esta congelada (el
         * fixture captura el progreso de una pasada). Nada que probar del congelado. */
        return;
    }

    /* Congelado: con la sesion terminada, mas paquetes del flow (el hilo de la radio
     * puede tener los ultimos encolados) no pueden mover el VFO ni abrir otra sesion. */
    unsigned freq_end  = mock_radio_calls.set_freq;
    unsigned tx_on_end = mock_radio_calls.start_swrscan;
    char     state_end[64];

    state_snapshot(state_end, sizeof(state_end));

    for (int k = 0; k < 3; k++) {
        dialog_swrscan_update(1.3f);
    }
    scheduler_work();

    trace.frozen = (mock_radio_calls.set_freq == freq_end)
                   && (mock_radio_calls.start_swrscan == tx_on_end)
                   && (strcmp(dialog_swrscan_state_text(), state_end) == 0);

    if (!trace.frozen) {
        fprintf(stderr,
                "swrscan: con la sesion terminada (%s) los paquetes de mas siguen moviendo el "
                "barrido (comandos %u -> %u)\n",
                state_end, freq_end, mock_radio_calls.set_freq);
        exit(1);
    }
}

bool swr_backend_used(void)
{
    return prepared;
}

void swr_backend_json(FILE *f, const scenario_t *scenario)
{
    if (!prepared) {
        return;
    }

    fprintf(f, "  \"swr\": {\n");
    fprintf(f, "    \"dataset\": \"%s\",\n", scenario->swr_dataset);
    fprintf(f, "    \"source\": \"%s/swr_mock_samples.csv\",\n", HARNESS_SWR_FIXTURES_DIR);
    fprintf(f, "    \"radio\": \"mock (mocks/mock_radio.c): no RF library is linked, every "
               "command is a counter\",\n");
    fprintf(f, "    \"passes_requested\": %d,\n", trace.passes);
    fprintf(f, "    \"passes_in_dataset\": %d,\n", passes_in_dataset);
    fprintf(f, "    \"points_per_pass\": %d,\n", SWR_SESSION_POINTS);
    fprintf(f, "    \"samples_fed\": %d,\n", trace.fed);
    fprintf(f, "    \"samples_fed_per_pass\": [");
    for (int i = 0; i < trace.passes; i++) {
        fprintf(f, "%s%d", (i > 0) ? ", " : "", trace.fed_per_pass[i]);
    }
    fprintf(f, "],\n");
    fprintf(f, "    \"dwell_ms\": %d,\n", SWRSCAN_DWELL_MS);
    fprintf(f, "    \"virtual_ms\": %llu,\n",
            (unsigned long long) (trace.virtual_end_ms - trace.virtual_start_ms));
    fprintf(f, "    \"freq_commands\": %u,\n", trace.freq_cmds);
    fprintf(f, "    \"tx_on\": %u,\n", trace.tx_on);
    fprintf(f, "    \"tx_off\": %u,\n", trace.tx_off);
    fprintf(f, "    \"ended\": %s,\n", trace.ended ? "true" : "false");
    fprintf(f, "    \"completed\": %s,\n", trace.completed ? "true" : "false");
    fprintf(f, "    \"stopped\": %s,\n", trace.stopped ? "true" : "false");
    fprintf(f, "    \"frozen\": %s,\n", trace.ended ? (trace.frozen ? "true" : "false") : "null");
    fprintf(f, "    \"state\": \"%s\",\n", trace.state);
    fprintf(f, "    \"state_per_pass\": [");
    for (int i = 0; i < trace.passes; i++) {
        fprintf(f, "%s\"%s\"", (i > 0) ? ", " : "", trace.state_per_pass[i]);
    }
    fprintf(f, "],\n");
    fprintf(f, "    \"grid_center_hz\": %u,\n", grid_lo + (uint32_t) ((grid_hi - grid_lo) / 2));
    fprintf(f, "    \"grid_span_hz\": %u,\n", grid_hi - grid_lo);
    fputs("    \"passes_label\": ", f);
    json_label(f, passes_label_text());
    fputs(",\n", f);
    fprintf(f, "    \"readout\": \"en el PNG (el chart dibuja el readout con lv_draw_label, no "
               "es un objeto del arbol)\"\n");
    fprintf(f, "  },\n");
}
