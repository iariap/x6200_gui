/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Themes: built-in tables and JSON files in /mnt/themes/ (docs/THEMES.md).
 */

#include "theme.h"

#include "cJSON.h"
#include "fonts/font_select.h"

#include <dirent.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#ifndef THEMES_DIR
#define THEMES_DIR "/mnt/themes"
#endif

/* $THEMES_DIR overrides it so the tests do not touch /mnt */
static const char *themes_dir(void) {
    const char *p = getenv("THEMES_DIR");

    return p && *p ? p : THEMES_DIR;
}

enum { B_IU1WXY, B_LEGACY, B_OUTDOOR, B_CLASSIC, B_OUTDOOR_NEG, B_VISION, B_COUNT };

static const char *const builtin_names[B_COUNT] = {"IU1WXY", "Legacy", "Outdoor", "Classic", "Outdoor-Negative",
                                                   "VISION"};

/* VISION: IU1WXY's colours made "warm anthracite": white digits on dark grey, orange for what is on,
 * aqua frames, the layout big (styles.c) */
static const struct {
    theme_color_t c;
    uint32_t      v;
} vision_colors[] = {
    {TC_bg, 0x1c1f24},          {TC_bar, 0x1c1f24},          {TC_bar_grad, 0x1c1f24},       {TC_bar_border, 0x5dcaa5},
    {TC_strip, 0x262a31},       {TC_strip_grad, 0x262a31},   {TC_strip_border, 0x5dcaa5},   {TC_dialog, 0x262a31},
    {TC_dialog_grad, 0x262a31}, {TC_dialog_border, 0x5dcaa5}, {TC_btn, 0x2a3038},           {TC_btn_grad, 0x2a3038},
    {TC_btn_border, 0x5dcaa5},  {TC_btn_text, 0xffffff},     {TC_btn_active, 0xff8a30},     {TC_btn_active_grad, 0xff8a30},
    {TC_btn_active_border, 0xff8a30}, {TC_btn_active_text, 0x2a1200}, {TC_btn_page, 0x2a3038}, {TC_btn_page_text, 0xe6fff6},
    {TC_text, 0xffffff},        {TC_text_value, 0xffffff},   {TC_text_soft, 0xe6fff6},      {TC_text_head, 0x5dcaa5},
    {TC_mode_text, 0x2a1200},   {TC_mode_bg, 0xff8a30},      {TC_mode_border, 0xff8a30},    {TC_pill_border, 0x5dcaa5},
    {TC_pill_off_text, 0x80868e}, {TC_focus, 0xff8a30},      {TC_focus_text, 0x2a1200},     {TC_meter_bar, 0x5dcaa5},
    {TC_meter_weak, 0x5dcaa5},  {TC_meter_low, 0x5dcaa5},    {TC_meter_high, 0xff5a3c},     {TC_meter_over, 0xffd040},
    {TC_popup, 0x262a31},       {TC_popup_border, 0x5dcaa5},
};

static const uint32_t builtin_colors[B_OUTDOOR_NEG][TC_COUNT] = {   /* VISION and Outdoor-Negative are made from these */
#define COL(name, a, b, c, d) a,
    {THEME_COLORS(COL)},
#undef COL
#define COL(name, a, b, c, d) b,
    {THEME_COLORS(COL)},
#undef COL
#define COL(name, a, b, c, d) c,
    {THEME_COLORS(COL)},
#undef COL
#define COL(name, a, b, c, d) d,
    {THEME_COLORS(COL)},
#undef COL
};

static const char *const color_names[TC_COUNT] = {
#define NAME(name, a, b, c, d) #name,
    THEME_COLORS(NAME)
#undef NAME
};

static const theme_shape_t builtin_shapes[B_OUTDOOR_NEG] = {
    [B_IU1WXY]  = {.radius = 7, .radius_btn = 10, .radius_popup = 10, .border = 1, .box_gradient = true,
                   .images = 1, .font = -1, .waterfall_table = 0},
    [B_LEGACY]  = {.radius = 7, .radius_btn = 10, .radius_popup = 10, .border = 1, .box_gradient = true,
                   .images = 2, .font = -1, .waterfall_table = 1},
    /* Light and high contrast for the sun, in grey: the noise light, weak signals already mid grey,
     * then darker at an even pace up to black, so a strong signal shows how strong it is */
    [B_OUTDOOR] = {.radius = 6, .radius_btn = 8, .radius_popup = 8, .border = 2, .focus_solid = true,
                   .font = -1, .meter_gradient = true, .mono_cluster = true,
                   .waterfall = {0xfafafa, 0xe6e6e6, 0xc8c8c8, 0xa0a0a0, 0x787878, 0x585858, 0x3c3c3c, 0x282828,
                                 0x181818, 0x0a0a0a, 0x000000},
                   .waterfall_n = 11},
    [B_CLASSIC] = {.radius = 3, .radius_btn = 4, .radius_popup = 4, .border = 1, .btn_gradient = true,
                   .focus_solid = true, .font = FONT_ARIAL,
                   .waterfall = {0x000000, 0x000060, 0x0040e0, 0x00c8ff, 0xffff00, 0xff6000, 0xffffff},
                   .waterfall_n = 7},
};

extern const uint32_t wf_palette_gauss[256], wf_palette_legacy[256];   /* styles.c */

static uint32_t      colors[TC_COUNT];
static uint32_t      palette[256];
theme_shape_t        theme_shape;

static const char *const image_names[TI_COUNT] = {"bg", "btn", "btn_active", "btn_off", "btn_page", "bar", "strip", "dialog", "mark", "mark_active"};

static struct {
    char path[600];
    int16_t slice[4];       /* left, top, right, bottom */
} images[TI_COUNT];

uint32_t theme_hex(theme_color_t c) {
    return c < TC_COUNT ? colors[c] : 0xff00ff;
}

static int builtin_index(const char *name) {
    for (int i = 0; i < B_COUNT; i++) {
        if (name && strcasecmp(name, builtin_names[i]) == 0) return i;
    }
    return -1;
}

/* Lightness inverted, hue and saturation kept: black <-> white, red stays red */
static uint32_t negative(uint32_t c) {
    int r = c >> 16 & 0xff, g = c >> 8 & 0xff, b = c & 0xff;
    int hi = r > g ? (r > b ? r : b) : (g > b ? g : b), lo = r < g ? (r < b ? r : b) : (g < b ? g : b);
    int d  = 255 - hi - lo;

    return (uint32_t)(r + d) << 16 | (uint32_t)(g + d) << 8 | (uint32_t)(b + d);
}

/* Outdoor-Negative: Outdoor with every colour and the waterfall through negative() */
static void use_builtin(int b) {
    int src = b == B_OUTDOOR_NEG ? B_OUTDOOR : b == B_VISION ? B_IU1WXY : b;

    memcpy(colors, builtin_colors[src], sizeof(colors));
    theme_shape = builtin_shapes[src];
    if (b == B_OUTDOOR_NEG) {
        for (int i = 0; i < TC_COUNT; i++) colors[i] = negative(colors[i]);
        /* the yellow of the selection stays yellow: its text stays black */
        colors[TC_sel_text]        = builtin_colors[B_OUTDOOR][TC_sel_text];
        colors[TC_btn_active_text] = builtin_colors[B_OUTDOOR][TC_btn_active_text];
        for (int i = 0; i < theme_shape.waterfall_n; i++) theme_shape.waterfall[i] = negative(theme_shape.waterfall[i]);
    }
    if (b == B_VISION) {
        for (size_t i = 0; i < sizeof(vision_colors) / sizeof(vision_colors[0]); i++)
            colors[vision_colors[i].c] = vision_colors[i].v;
        theme_shape = (theme_shape_t){.radius = 8, .radius_btn = 10, .radius_popup = 10, .border = 2,
                                      .focus_solid = true, .mode_box = true, .pill_border = 2, .font = -1,
                                      .vision = true};
    }
    snprintf(theme_shape.name, sizeof(theme_shape.name), "%s", builtin_names[b]);
}

/* "#rrggbb" or "rrggbb" */
static bool parse_color(const cJSON *j, uint32_t *out) {
    const char *s = cJSON_GetStringValue(j);
    char       *end;

    if (!s) return false;
    if (*s == '#') s++;
    unsigned long v = strtoul(s, &end, 16);
    if (end - s != 6 || *end) return false;
    *out = (uint32_t)v;
    return true;
}

static void get_int(const cJSON *root, const char *key, int16_t *out) {
    const cJSON *j = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsNumber(j)) *out = (int16_t)j->valueint;
}

static void get_bool(const cJSON *root, const char *key, bool *out) {
    const cJSON *j = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsBool(j)) *out = cJSON_IsTrue(j);
}

/* "images": {"btn": {"file": "btn.png", "slice": [l, t, r, b]}, ...}, files next to the JSON */
static void get_images(const cJSON *root, const char *dir) {
    const cJSON *j, *imgs = cJSON_GetObjectItemCaseSensitive(root, "images");

    cJSON_ArrayForEach(j, imgs) {
        int         i;
        const char *file = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j, "file"));
        const cJSON *sl  = cJSON_GetObjectItemCaseSensitive(j, "slice");

        for (i = 0; i < TI_COUNT && strcmp(j->string, image_names[i]) != 0; i++) {}
        if (i == TI_COUNT || !file) {
            fprintf(stderr, "Theme: bad image %s\n", j->string);
            continue;
        }
        if (snprintf(images[i].path, sizeof(images[i].path), "%s/%s", dir, file) >=
            (int) sizeof(images[i].path)) {
            continue;       /* too long a path: skip this image */
        }
        for (int k = 0; k < 4; k++) {
            const cJSON *v = cJSON_GetArrayItem(sl, k);
            images[i].slice[k] = cJSON_IsNumber(v) ? v->valueint : 0;
        }
    }
}

/* A theme file, THEMES_DIR/<name>.json or THEMES_DIR/<name>/theme.json (a
 * folder with its images): a base theme with the colours and shapes it sets */
static bool load_file(const char *name) {
    char  path[600], dir[600];
    FILE *f;

    if (snprintf(dir, sizeof(dir), "%s", themes_dir()) >= (int) sizeof(dir) ||
        snprintf(path, sizeof(path), "%s/%s.json", themes_dir(), name) >= (int) sizeof(path)) {
        return false;       /* the path does not fit: not a theme we can read */
    }

    f = fopen(path, "r");

    if (!f) {
        if (snprintf(dir, sizeof(dir), "%s/%s", themes_dir(), name) >= (int) sizeof(dir) ||
            snprintf(path, sizeof(path), "%s/theme.json", dir) >= (int) sizeof(path)) {
            return false;
        }

        f = fopen(path, "r");
    }
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = len > 0 && len < 65536 ? malloc(len + 1) : NULL;
    if (text) {
        text[fread(text, 1, len, f)] = 0;
    }
    fclose(f);
    cJSON *root = text ? cJSON_Parse(text) : NULL;
    free(text);
    if (!root) {
        fprintf(stderr, "Theme %s: not valid JSON\n", path);
        return false;
    }

    int b = builtin_index(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, "base")));
    use_builtin(b < 0 ? B_IU1WXY : b);
    snprintf(theme_shape.name, sizeof(theme_shape.name), "%s", name);

    const cJSON *c, *cols = cJSON_GetObjectItemCaseSensitive(root, "colors");
    cJSON_ArrayForEach(c, cols) {
        int i;
        for (i = 0; i < TC_COUNT && strcmp(c->string, color_names[i]) != 0; i++) {}
        if (i == TC_COUNT || !parse_color(c, &colors[i])) {
            fprintf(stderr, "Theme %s: bad colour %s\n", name, c->string);
        }
    }

    int16_t v;
    get_int(root, "radius", &theme_shape.radius);
    get_int(root, "radius_btn", &theme_shape.radius_btn);
    get_int(root, "radius_popup", &theme_shape.radius_popup);
    get_int(root, "border", &theme_shape.border);
    get_bool(root, "btn_gradient", &theme_shape.btn_gradient);
    get_bool(root, "box_gradient", &theme_shape.box_gradient);
    get_bool(root, "focus_solid", &theme_shape.focus_solid);
    get_int(root, "pill_border", &theme_shape.pill_border);
    get_bool(root, "mode_box", &theme_shape.mode_box);
    get_int(root, "btn_gap", &theme_shape.btn_gap);
    theme_shape.btn_gap = theme_shape.btn_gap < 0 ? 0 : theme_shape.btn_gap > 4 ? 4 : theme_shape.btn_gap;
    get_bool(root, "rule", &theme_shape.rule);
    v = theme_shape.waterfall_steps;
    get_int(root, "waterfall_steps", &v);
    theme_shape.waterfall_steps = v;

    const char *font = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(root, "font"));
    if (font) {
        theme_shape.font = !strcasecmp(font, "roboto") ? FONT_ROBOTO :
                           !strcasecmp(font, "dejavu") ? FONT_DEJAVU :
                           !strcasecmp(font, "arial") ? FONT_ARIAL :
                           !strcasecmp(font, "sony") ? FONT_SONY : -1;
    }
    const cJSON *wf = cJSON_GetObjectItemCaseSensitive(root, "waterfall");
    if (cJSON_IsArray(wf) && cJSON_GetArraySize(wf) >= 2) {
        int n = 0;
        cJSON_ArrayForEach(c, wf) {
            if (n < 16 && parse_color(c, &theme_shape.waterfall[n])) n++;
        }
        theme_shape.waterfall_n = n;
    }
    const cJSON *mark = cJSON_GetObjectItemCaseSensitive(root, "mark");
    for (int k = 0; k < 3 && cJSON_GetArraySize(mark) == 3; k++) {
        theme_shape.mark[k] = cJSON_GetArrayItem(mark, k)->valueint;
    }
    get_images(root, dir);
    cJSON_Delete(root);
    return true;
}

/* ---- Images ---- */

static uint8_t *load_png(const char *path, int *w, int *h) {
    png_image im;
    uint8_t  *buf;

    memset(&im, 0, sizeof(im));
    im.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_file(&im, path)) {
        fprintf(stderr, "Theme image %s: %s\n", path, im.message);
        return NULL;
    }
    im.format = PNG_FORMAT_BGRA;    /* lv_color32_t order */
    buf = malloc(PNG_IMAGE_SIZE(im));
    if (!buf || !png_image_finish_read(&im, NULL, buf, 0, NULL)) {
        fprintf(stderr, "Theme image %s: %s\n", path, im.message);
        free(buf);
        png_image_free(&im);
        return NULL;
    }
    *w = im.width;
    *h = im.height;
    return buf;
}

/* Source coordinate of x in [0, size): the insets a and b as drawn, the
 * middle stretched (pixel centres, so the stretch is symmetric) */
static float slice_map(int x, int size, int src, int a, int b) {
    if (a + b >= src || a + b >= size) return (x + 0.5f) * src / size - 0.5f;
    if (x < a) return x;
    if (x >= size - b) return src - (size - x);
    return a + (x - a + 0.5f) * (src - a - b) / (size - a - b) - 0.5f;
}

static void bilinear(const uint8_t *src, int sw, int sh, float fx, float fy, uint8_t *out) {
    int   x0, y0, x1, y1;
    float tx, ty;

    if (fx < 0) fx = 0;
    if (fy < 0) fy = 0;
    if (fx > sw - 1) fx = sw - 1;
    if (fy > sh - 1) fy = sh - 1;
    x0 = (int)fx;
    y0 = (int)fy;
    x1 = x0 + 1 < sw ? x0 + 1 : x0;
    y1 = y0 + 1 < sh ? y0 + 1 : y0;
    tx = fx - x0;
    ty = fy - y0;
    for (int c = 0; c < 4; c++) {
        float a = src[(y0 * sw + x0) * 4 + c] * (1 - tx) + src[(y0 * sw + x1) * 4 + c] * tx;
        float b = src[(y1 * sw + x0) * 4 + c] * (1 - tx) + src[(y1 * sw + x1) * 4 + c] * tx;
        out[c] = (uint8_t)(a * (1 - ty) + b * ty + 0.5f);
    }
}

static struct {
    theme_image_t img;
    lv_coord_t    w, h;
    lv_img_dsc_t  dsc;
} cache[16];
static int cached;

const lv_img_dsc_t *theme_image(theme_image_t img, lv_coord_t w, lv_coord_t h) {
    int sw, sh;

    if (img >= TI_COUNT || !images[img].path[0] || w <= 0 || h <= 0) return NULL;
    for (int i = 0; i < cached; i++) {
        if (cache[i].img == img && cache[i].w == w && cache[i].h == h) return &cache[i].dsc;
    }
    if (cached == 16) return NULL;

    uint8_t *src = load_png(images[img].path, &sw, &sh);
    uint8_t *dst = src ? malloc((size_t)w * h * 4) : NULL;
    if (!dst) {
        free(src);
        return NULL;
    }
    const int16_t *s = images[img].slice;
    for (int y = 0; y < h; y++) {
        float fy = slice_map(y, h, sh, s[1], s[3]);
        for (int x = 0; x < w; x++) {
            bilinear(src, sw, sh, slice_map(x, w, sw, s[0], s[2]), fy, &dst[(y * w + x) * 4]);
        }
    }
    free(src);

    lv_img_dsc_t *d = &cache[cached].dsc;
    memset(d, 0, sizeof(*d));
    d->header.cf     = LV_IMG_CF_TRUE_COLOR_ALPHA;
    d->header.w      = w;
    d->header.h      = h;
    d->data_size     = (uint32_t)w * h * 4;
    d->data          = dst;
    cache[cached].img = img;
    cache[cached].w   = w;
    cache[cached].h   = h;
    cached++;
    return d;
}

const lv_img_dsc_t *theme_png(const char *path) {
    static struct {
        char path[600];
        lv_img_dsc_t dsc;
    } pngs[24];
    static int n;
    int        w, h;

    for (int i = 0; i < n; i++) {
        if (!strcmp(pngs[i].path, path)) return &pngs[i].dsc;
    }
    if (n == 24) return NULL;
    uint8_t *data = load_png(path, &w, &h);
    if (!data) return NULL;

    lv_img_dsc_t *d = &pngs[n].dsc;
    snprintf(pngs[n].path, sizeof(pngs[n].path), "%s", path);
    memset(d, 0, sizeof(*d));
    d->header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
    d->header.w  = w;
    d->header.h  = h;
    d->data_size = (uint32_t)w * h * 4;
    d->data      = data;
    n++;
    return d;
}

/* 256 colours through the stops, smooth or in flat bands */
static void make_palette(void) {
    const uint32_t *s = theme_shape.waterfall;
    int             n = theme_shape.waterfall_n, steps = theme_shape.waterfall_steps;

    for (int i = 0; i < 256; i++) {
        int lvl = steps > 1 ? (i * steps / 256) * 255 / (steps - 1) : i;   /* 0..255 */
        int pos = lvl * (n - 1), seg = pos / 255, t = pos % 255;

        if (seg >= n - 1) {
            seg = n - 2;
            t   = 255;
        }
        uint32_t c = 0;
        for (int sh = 0; sh <= 16; sh += 8) {
            int a = (s[seg] >> sh) & 0xff, b = (s[seg + 1] >> sh) & 0xff;
            c |= (uint32_t)(a + (b - a) * t / 255) << sh;
        }
        palette[i] = c;
    }
}

void theme_init(const char *name) {
    int b = builtin_index(name);

    memset(images, 0, sizeof(images));
    for (int i = 0; i < cached; i++) {
        free((void *)cache[i].dsc.data);
    }
    cached = 0;

    if (b >= 0) {
        use_builtin(b);
    } else if (!name || !*name || !load_file(name)) {
        if (name && *name) fprintf(stderr, "Theme %s not found, using IU1WXY\n", name);
        use_builtin(B_IU1WXY);
    }
    if (theme_shape.waterfall_n >= 2) make_palette();
}

const uint32_t *theme_waterfall(void) {
    if (theme_shape.waterfall_n >= 2) return palette;
    return theme_shape.waterfall_table == 1 ? wf_palette_legacy : wf_palette_gauss;
}

/* Legacy is kept but not offered: IU1WXY, Outdoor, Classic, the files */
int theme_list(char names[][32], int max) {
    int            n = 0;
    DIR           *d;
    struct dirent *e;

    for (int i = 0; i < B_COUNT && n < max; i++) {
        if (i != B_LEGACY) snprintf(names[n++], 32, "%s", builtin_names[i]);
    }
    d = opendir(themes_dir());
    while (d && (e = readdir(d)) && n < max) {
        size_t len = strlen(e->d_name);
        char   path[600];

        if (len > 5 && len < 32 + 5 && !strcmp(e->d_name + len - 5, ".json")) {
            snprintf(names[n], 32, "%.*s", (int)(len - 5), e->d_name);
            n++;
        } else if (e->d_name[0] != '.' && len < 32) {   /* a folder with theme.json */
            if (snprintf(path, sizeof(path), "%s/%s/theme.json", themes_dir(), e->d_name) >= (int) sizeof(path)) {
                continue;
            }

            if (access(path, R_OK) == 0) snprintf(names[n++], 32, "%.*s", (int) sizeof(names[0]) - 1, e->d_name);
        }
    }
    if (d) closedir(d);
    return n;
}
