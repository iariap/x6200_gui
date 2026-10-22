/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  JS8 map: the world with a path from the station to each QSO of the log
 */

#define _GNU_SOURCE

#include "js8_map.h"

#include "qth/qth.h"
#include "styles.h"
#include "theme.h"
#include "world_map.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Equirectangular, squeezed to the dialog: the polar caps are left out */
#define LAT_TOP   80.0
#define LAT_BOT   (-60.0)
#define MAX_QSOS  200
#define GC_STEPS  48
#define LOG_MAX   (512 * 1024)

typedef struct {
    char call[16];
    char grid[8];
} qso_t;

static lv_coord_t map_w, map_h;
static double     lon0;     /* the station's: the centre, so its paths don't cross the edge */

static lv_point_t project(double lat, double lon) {
    double dl = fmod(lon - lon0 + 540.0, 360.0) - 180.0;
    return (lv_point_t){(lv_coord_t)((dl + 180.0) / 360.0 * (map_w - 1)),
                        (lv_coord_t)((LAT_TOP - lat) / (LAT_TOP - LAT_BOT) * (map_h - 1))};
}

/* A polyline, broken where it goes over the edge (a jump of half the map) */
static void draw_path(lv_obj_t *canvas, lv_point_t *p, int n, const lv_draw_line_dsc_t *dsc) {
    int start = 0;
    for (int i = 1; i <= n; i++) {
        if (i == n || abs(p[i].x - p[i - 1].x) > map_w / 2) {
            if (i - start >= 2) lv_canvas_draw_line(canvas, p + start, (uint32_t)(i - start), dsc);
            start = i;
        }
    }
}

static void coastlines(lv_obj_t *canvas) {
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = TC(text_dim);
    dsc.width = 1;

    for (int k = 0; k < WORLD_MAP_RINGS; k++) {
        int        a = world_map_rings[k], n = world_map_rings[k + 1] - a;
        lv_point_t p[n + 1];
        for (int i = 0; i <= n; i++) {  /* closed: back to the first point */
            const int16_t *v = world_map_pts[a + (i % n)];
            p[i]             = project(v[1] / 10.0, v[0] / 10.0);
        }
        draw_path(canvas, p, n + 1, &dsc);
    }
}

static void graticule(lv_obj_t *canvas) {
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = TC(text_faint);
    dsc.width = 1;
    dsc.opa   = LV_OPA_50;

    for (int lat = -30; lat <= 60; lat += 30) {
        lv_point_t p[2] = {{0, project(lat, lon0).y}, {(lv_coord_t)(map_w - 1), project(lat, lon0).y}};
        lv_canvas_draw_line(canvas, p, 2, &dsc);
    }
    for (int lon = -180; lon < 180; lon += 30) {
        lv_coord_t x    = project(0, lon).x;
        lv_point_t p[2] = {{x, 0}, {x, (lv_coord_t)(map_h - 1)}};
        lv_canvas_draw_line(canvas, p, 2, &dsc);
    }
}

/* <name:len>value of an ADIF record, "" when missing */
static void adif_field(const char *rec, const char *end, const char *name, char *out, size_t size) {
    char tag[24];
    out[0] = 0;
    snprintf(tag, sizeof(tag), "<%s:", name);
    const char *t = strcasestr(rec, tag);
    if (!t || t >= end) return;
    size_t len = strtoul(t + strlen(tag), NULL, 10);
    const char *v = strchr(t, '>');
    if (!v || v >= end) return;
    v++;
    if (len >= size) len = size - 1;
    memcpy(out, v, len);
    out[len] = 0;
}

/* The calls of the log with a grid, the latest record of each */
static int read_qsos(const char *path, qso_t *q, int max) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    long from = size > LOG_MAX ? size - LOG_MAX : 0;   /* a long log: its latest QSOs */
    char *buf = malloc((size_t)(size - from) + 1);
    if (!buf) {
        fclose(f);
        return 0;
    }
    fseek(f, from, SEEK_SET);
    size_t got = fread(buf, 1, (size_t)(size - from), f);
    fclose(f);
    buf[got] = 0;

    int   n   = 0;
    char *rec = buf;
    char *eor;
    while ((eor = strcasestr(rec, "<eor>"))) {
        qso_t e;
        adif_field(rec, eor, "call", e.call, sizeof(e.call));
        adif_field(rec, eor, "gridsquare", e.grid, sizeof(e.grid));
        rec = eor + 5;
        if (!e.call[0] || strlen(e.grid) < 4) continue;
        int i = 0;
        while (i < n && strcasecmp(q[i].call, e.call)) i++;
        if (i == n && n < max) n++;
        if (i < n) q[i] = e;
    }
    free(buf);
    return n;
}

static void gc_path(lv_obj_t *canvas, double lat1, double lon1, double lat2, double lon2, const lv_draw_line_dsc_t *dsc) {
    double     r = M_PI / 180.0;
    double     a[3] = {cos(lat1 * r) * cos(lon1 * r), cos(lat1 * r) * sin(lon1 * r), sin(lat1 * r)};
    double     b[3] = {cos(lat2 * r) * cos(lon2 * r), cos(lat2 * r) * sin(lon2 * r), sin(lat2 * r)};
    double     d    = acos(fmax(-1.0, fmin(1.0, a[0] * b[0] + a[1] * b[1] + a[2] * b[2])));
    lv_point_t p[GC_STEPS + 1];

    for (int i = 0; i <= GC_STEPS; i++) {
        double t = (double)i / GC_STEPS, ka, kb;
        if (d < 1e-6) {
            ka = 1.0 - t;
            kb = t;
        } else {
            ka = sin((1.0 - t) * d) / sin(d);
            kb = sin(t * d) / sin(d);
        }
        double x = ka * a[0] + kb * b[0], y = ka * a[1] + kb * b[1], z = ka * a[2] + kb * b[2];
        p[i]     = project(atan2(z, sqrt(x * x + y * y)) / r, atan2(y, x) / r);
    }
    draw_path(canvas, p, GC_STEPS + 1, dsc);
}

static void dot(lv_obj_t *canvas, lv_point_t c, lv_coord_t r, lv_color_t color) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.radius   = LV_RADIUS_CIRCLE;
    lv_canvas_draw_rect(canvas, c.x - r, c.y - r, 2 * r + 1, 2 * r + 1, &dsc);
}

static void delete_cb(lv_event_t *e) {
    free(lv_event_get_user_data(e));
}

lv_obj_t *js8_map_create(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, const char *my_grid, const char *log_path) {
    lv_color_t *buf = malloc(LV_CANVAS_BUF_SIZE_TRUE_COLOR(w, h));
    if (!buf) return NULL;

    double my_lat = 0.0, my_lon = 0.0;
    if (my_grid && strlen(my_grid) >= 4) qth_str_to_pos(my_grid, &my_lat, &my_lon);
    map_w = w;
    map_h = h;
    lon0  = my_lon;

    lv_obj_t *canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(canvas, buf, w, h, LV_IMG_CF_TRUE_COLOR);
    lv_obj_add_event_cb(canvas, delete_cb, LV_EVENT_DELETE, buf);
    lv_canvas_fill_bg(canvas, TC(panel), LV_OPA_COVER);

    graticule(canvas);
    coastlines(canvas);

    static qso_t qsos[MAX_QSOS];
    int          n = read_qsos(log_path, qsos, MAX_QSOS);

    lv_draw_line_dsc_t path;
    lv_draw_line_dsc_init(&path);
    path.color = TC(accent);
    path.width = 1;
    path.opa   = LV_OPA_80;

    lv_draw_label_dsc_t label;
    lv_draw_label_dsc_init(&label);
    label.font  = &sony_14;
    label.color = TC(text);

    for (int i = 0; i < n; i++) {
        double lat, lon;
        qth_str_to_pos(qsos[i].grid, &lat, &lon);
        gc_path(canvas, my_lat, my_lon, lat, lon, &path);
    }
    for (int i = 0; i < n; i++) {   /* over all the paths */
        double lat, lon;
        qth_str_to_pos(qsos[i].grid, &lat, &lon);
        lv_point_t p = project(lat, lon);
        lv_point_t size;
        dot(canvas, p, 2, TC(ok));
        lv_txt_get_size(&size, qsos[i].call, label.font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        /* left of the dot near the right edge, not cut */
        lv_coord_t x = p.x + 4 + size.x < w ? p.x + 4 : p.x - 4 - size.x;
        lv_canvas_draw_text(canvas, x, p.y - 8, size.x + 2, &label, qsos[i].call);
    }
    dot(canvas, project(my_lat, my_lon), 4, TC(err));

    char head[48];
    snprintf(head, sizeof(head), "QSO map - %d station%s", n, n == 1 ? "" : "s");
    label.font  = &sony_18;
    label.color = TC(text_head);
    lv_canvas_draw_text(canvas, 8, 4, 300, &label, head);
    label.font  = &sony_14;
    label.color = TC(text_dim);
    lv_canvas_draw_text(canvas, w - 128, h - 20, 124, &label, "ESC / Map: close");
    return canvas;
}
