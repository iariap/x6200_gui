/*
 * Loader for the SWR Scan mock fixtures (see swrscan_fixtures.h).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "swrscan_fixtures.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SWR_FX_LINE 256
#define SWR_FX_FIELDS 16

const char *swr_fx_dir(void) {
    const char *env = getenv("X6200_SWRSCAN_FIXTURES");

    return (env && *env) ? env : "tests/fixtures/swrscan";
}

/* Splits `line` on commas in place, filling `fields` (at most `max`). Returns
 * the number of fields. The fixtures have no quoted fields and no embedded
 * commas. */
static int split_fields(char *line, char **fields, int max) {
    int n = 0;
    char *p = line;

    fields[n++] = p;

    for (; *p; p++) {
        if (*p == ',') {
            *p = '\0';
            if (n < max) {
                fields[n++] = p + 1;
            }
        }
    }

    return n;
}

static void strip_eol(char *line) {
    size_t n = strlen(line);

    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
        line[--n] = '\0';
    }
}

static FILE *open_table(const char *name) {
    char path[512];

    snprintf(path, sizeof(path), "%s/%s", swr_fx_dir(), name);

    return fopen(path, "r");
}

/* Reads and discards the header row; false when the file is empty. */
static bool read_header(FILE *f, char *line, size_t n) {
    return fgets(line, (int) n, f) != NULL;
}

int swr_fx_load_samples(swr_fx_sample_t *out, int max) {
    FILE *f = open_table("swr_mock_samples.csv");

    if (!f) {
        return -1;
    }

    char line[SWR_FX_LINE];
    char *fields[SWR_FX_FIELDS];

    if (!read_header(f, line, sizeof(line))) {
        fclose(f);
        return -1;
    }

    int count = 0;

    while (count < max && fgets(line, sizeof(line), f)) {
        strip_eol(line);

        if (line[0] == '\0') {
            continue;
        }

        if (split_fields(line, fields, SWR_FX_FIELDS) < 10) {
            fclose(f);
            return -1;
        }

        swr_fx_sample_t *s = &out[count];

        snprintf(s->scenario, sizeof(s->scenario), "%s", fields[0]);
        s->pass_index   = atoi(fields[1]);
        s->order_in_pass = atoi(fields[2]);
        s->point_index  = atoi(fields[4]);
        s->freq_hz      = (uint32_t) strtoul(fields[5], NULL, 10);
        s->raw_tenths   = atoi(fields[6]);
        s->is_valid     = atoi(fields[8]);
        s->timestamp_ms = (uint32_t) strtoul(fields[9], NULL, 10);

        count++;
    }

    fclose(f);

    return count;
}

int swr_fx_load_aggregates(swr_fx_aggregate_t *out, int max) {
    FILE *f = open_table("swr_expected_aggregates.csv");

    if (!f) {
        return -1;
    }

    char line[SWR_FX_LINE];
    char *fields[SWR_FX_FIELDS];

    if (!read_header(f, line, sizeof(line))) {
        fclose(f);
        return -1;
    }

    int count = 0;

    while (count < max && fgets(line, sizeof(line), f)) {
        strip_eol(line);

        if (line[0] == '\0') {
            continue;
        }

        if (split_fields(line, fields, SWR_FX_FIELDS) < 8) {
            fclose(f);
            return -1;
        }

        swr_fx_aggregate_t *a = &out[count];

        snprintf(a->scenario, sizeof(a->scenario), "%s", fields[0]);
        a->point_index    = atoi(fields[1]);
        a->freq_hz        = (uint32_t) strtoul(fields[2], NULL, 10);
        a->valid_samples  = atoi(fields[3]);
        a->median_tenths  = atoi(fields[5]);
        a->mad_tenths     = atoi(fields[7]);

        count++;
    }

    fclose(f);

    return count;
}

int swr_fx_load_flow(swr_fx_flow_event_t *out, int max) {
    FILE *f = open_table("swr_mock_delayed_flow.csv");

    if (!f) {
        return -1;
    }

    char line[SWR_FX_LINE];
    char *fields[SWR_FX_FIELDS];

    if (!read_header(f, line, sizeof(line))) {
        fclose(f);
        return -1;
    }

    int count = 0;

    while (count < max && fgets(line, sizeof(line), f)) {
        strip_eol(line);

        if (line[0] == '\0') {
            continue;
        }

        if (split_fields(line, fields, SWR_FX_FIELDS) < 10) {
            fclose(f);
            return -1;
        }

        swr_fx_flow_event_t *e = &out[count];

        e->seq            = atoi(fields[0]);
        e->ts_ms          = (uint32_t) strtoul(fields[1], NULL, 10);
        e->is_set_freq    = strcmp(fields[2], "set_frequency") == 0;
        e->pass_index     = atoi(fields[3]);
        e->point_index    = atoi(fields[4]);
        e->target_freq_hz = (uint32_t) strtoul(fields[5], NULL, 10);
        e->source_freq_hz = (fields[6][0] != '\0') ? (uint32_t) strtoul(fields[6], NULL, 10) : 0;
        e->swr_tenths     = (fields[7][0] != '\0') ? (int) lroundf(strtof(fields[7], NULL) * 10.0f) : -1;
        e->stale          = (fields[8][0] != '\0') ? atoi(fields[8]) : 0;
        e->tx_flag        = (fields[9][0] != '\0') ? atoi(fields[9]) : 0;

        count++;
    }

    fclose(f);

    return count;
}
