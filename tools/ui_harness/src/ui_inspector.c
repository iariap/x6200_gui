/*
 *  Structural inspection of the live LVGL object tree (see ui_inspector.h).
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "ui_inspector.h"

#include "lvgl/lvgl.h"

#include "keyboard.h"

/* The firmware's fonts: font_select.c swaps their contents but the pointers stay
 * `&sony_N`, so pointer identity is a stable way to name the font a label draws
 * with. Nothing under src/ is modified.
 *
 * Every family by name comes from src/fonts/font_names.h (generated from
 * tools/fonts/candidates.json); it used to be a hand-written copy of the stock slot
 * list here, which drifts from the compiled families as soon as one is added.
 * Included after lvgl.h: it declares the symbols as `lv_font_t`. */
#include "fonts/font_names.h"

static const struct {
    const lv_obj_class_t *class_p;
    const char           *name;
} CLASS_NAMES[] = {
    { &lv_obj_class,        "lv_obj" },
    { &lv_label_class,      "lv_label" },
    { &lv_btn_class,        "lv_btn" },
    { &lv_line_class,       "lv_line" },
    { &lv_img_class,        "lv_img" },
    { &lv_bar_class,        "lv_bar" },
    { &lv_arc_class,        "lv_arc" },
    { &lv_slider_class,     "lv_slider" },
    { &lv_switch_class,     "lv_switch" },
    { &lv_dropdown_class,   "lv_dropdown" },
    { &lv_checkbox_class,   "lv_checkbox" },
    { &lv_table_class,      "lv_table" },
    { &lv_chart_class,      "lv_chart" },
    { &lv_canvas_class,     "lv_canvas" },
    { &lv_textarea_class,   "lv_textarea" },
    { &lv_roller_class,     "lv_roller" },
    { &lv_btnmatrix_class,  "lv_btnmatrix" },
    { &lv_spinbox_class,    "lv_spinbox" },
};

typedef struct {
    FILE *f;
} inspector_t;

static int area_w(const lv_area_t *a) { return a->x2 - a->x1 + 1; }
static int area_h(const lv_area_t *a) { return a->y2 - a->y1 + 1; }

static const char *class_name(const lv_obj_t *obj)
{
    const lv_obj_class_t *class_p = lv_obj_get_class(obj);

    for (size_t i = 0; i < sizeof(CLASS_NAMES) / sizeof(CLASS_NAMES[0]); i++) {
        if (class_p == CLASS_NAMES[i].class_p) {
            return CLASS_NAMES[i].name;
        }
    }
    return "other";
}

static const char *font_name(const lv_font_t *font)
{
    if (font == NULL) {
        return "";
    }
    for (size_t i = 0; i < sizeof(FONT_NAMES) / sizeof(FONT_NAMES[0]); i++) {
        if (font == FONT_NAMES[i].font) {
            return FONT_NAMES[i].name;
        }
    }
    return "other";
}

/* Minimal JSON string writer for label text. Valid UTF-8 is passed through;
 * a label whose text holds raw, non-UTF-8 bytes (the firmware can build one, see
 * clock.c) is escaped byte by byte as a Latin-1 code point so the JSON stays
 * parseable instead of being silently mangled. */
static void write_json_string(FILE *f, const char *text)
{
    fputc('"', f);
    const unsigned char *p = (const unsigned char *) text;
    while (*p) {
        if (*p < 0x80) {
            switch (*p) {
                case '"':  fputs("\\\"", f); break;
                case '\\': fputs("\\\\", f); break;
                case '\n': fputs("\\n", f); break;
                case '\r': fputs("\\r", f); break;
                case '\t': fputs("\\t", f); break;
                default:
                    if (*p < 0x20) {
                        fprintf(f, "\\u%04x", (unsigned) *p);
                    } else {
                        fputc(*p, f);
                    }
            }
            p++;
            continue;
        }
        uint32_t index = 0;
        uint32_t code = _lv_txt_encoded_next((const char *) p, &index);
        if (code == 0 || index == 0) {
            fprintf(f, "\\u%04x", (unsigned) *p);
            p++;
        } else {
            fwrite(p, 1, index, f);
            p += index;
        }
    }
    fputc('"', f);
}

static uint32_t colour_rgb(lv_color_t colour)
{
    return (uint32_t) (lv_color_to32(colour) & 0xFFFFFFu);
}

#define MAX_GLYPHS 32

/* Walk the label text and record the code points that fall back to another font
 * and the ones that are missing from every font in the chain (LVGL exposes both
 * through lv_font_glyph_dsc_t: resolved_font and is_placeholder). */
static void scan_glyphs(
    const char *text,
    const lv_font_t *font,
    bool recolor,
    uint32_t *fallback,
    int *fallback_count,
    uint32_t *missing,
    int *missing_count
)
{
    uint32_t index = 0;

    *fallback_count = 0;
    *missing_count = 0;

    while (text[index] != '\0') {
        uint32_t letter;
        uint32_t letter_next;
        uint32_t ofs = index;

        _lv_txt_encoded_letter_next_2(text, &letter, &letter_next, &ofs);
        index = ofs;

        if (letter == '\0' || letter == '\n' || letter < 0x20) {
            continue;
        }
        if (recolor && letter == '#') {
            /* Skip the in-line recolor command (`#RRGGBB text#`). */
            while (text[index] != '\0' && text[index] != '#') {
                index++;
            }
            if (text[index] == '#') {
                index++;
            }
            continue;
        }

        lv_font_glyph_dsc_t dsc;
        memset(&dsc, 0, sizeof(dsc));
        bool found = lv_font_get_glyph_dsc(font, &dsc, letter, letter_next);

        if (!found || dsc.is_placeholder) {
            if (*missing_count < MAX_GLYPHS) {
                missing[(*missing_count)++] = letter;
            }
        } else if (dsc.resolved_font != NULL && dsc.resolved_font != font) {
            if (*fallback_count < MAX_GLYPHS) {
                fallback[(*fallback_count)++] = letter;
            }
        }
    }
}

static void write_codepoints(FILE *f, const uint32_t *codes, int count)
{
    fputc('[', f);
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s\"U+%04X\"", i ? ", " : "", (unsigned) codes[i]);
    }
    fputc(']', f);
}

static const char *long_mode_name(lv_label_long_mode_t mode)
{
    switch (mode) {
        case LV_LABEL_LONG_WRAP:            return "WRAP";
        case LV_LABEL_LONG_DOT:             return "DOT";
        case LV_LABEL_LONG_SCROLL:          return "SCROLL";
        case LV_LABEL_LONG_SCROLL_CIRCULAR: return "SCROLL_CIRCULAR";
        case LV_LABEL_LONG_CLIP:            return "CLIP";
        default:                            return "unknown";
    }
}

/* The axis a container is allowed to scroll on. The structural checks need it to
 * tell content the user can scroll to (a list/dialog scroll area, like the
 * Ajustes grid) from content that is off the screen by mistake: a container that
 * clips its children *and* scrolls on that axis can be reached; the viewport
 * cannot. LVGL's default is LV_DIR_ALL, which is what an object that never
 * configured scrolling reports. */
static const char *scroll_dir_name(lv_dir_t dir)
{
    switch ((unsigned) dir) {
        case LV_DIR_NONE: return "none";
        case LV_DIR_HOR:  return "hor";
        case LV_DIR_VER:  return "ver";
        case LV_DIR_ALL:  return "all";
        default:          return "other";
    }
}

static void emit_label(FILE *f, const lv_obj_t *obj)
{
    const char *text = lv_label_get_text(obj);
    const lv_font_t *font = lv_obj_get_style_text_font(obj, LV_PART_MAIN);
    lv_coord_t letter_space = lv_obj_get_style_text_letter_space(obj, LV_PART_MAIN);
    lv_coord_t line_space = lv_obj_get_style_text_line_space(obj, LV_PART_MAIN);
    lv_label_long_mode_t mode = lv_label_get_long_mode(obj);
    bool recolor = lv_label_get_recolor(obj);
    lv_text_flag_t flags = recolor ? LV_TEXT_FLAG_RECOLOR : LV_TEXT_FLAG_NONE;

    lv_area_t box;
    lv_obj_get_content_coords(obj, &box);
    int box_w = area_w(&box);
    int box_h = area_h(&box);

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    int obj_w = area_w(&coords);
    int obj_h = area_h(&coords);

    lv_point_t natural = { 0, 0 };
    lv_point_t wrapped = { 0, 0 };
    lv_txt_get_size(&natural, text, font, letter_space, line_space, LV_COORD_MAX, flags);
    lv_txt_get_size(&wrapped, text, font, letter_space, line_space, box_w, flags);

    bool is_wrap = (mode == LV_LABEL_LONG_WRAP);
    /* A one-pixel slack absorbs the difference between the font's nominal line
     * height and the object's rounded padding. The comparison is against the
     * object's own box: that is what clips the text when it does not fit. */
    const int slack = 1;
    bool horizontal = natural.x > obj_w + slack;
    bool vertical = (is_wrap ? wrapped.y : natural.y) > obj_h + slack;
    /* WRAP reflows the text to the width, so a wide text is not a defect there;
     * the other modes keep one line and clip/dot/scroll it, which is the
     * truncation the user sees. */
    bool overflow = is_wrap ? vertical : (horizontal || vertical);
    bool truncated = !is_wrap
        && (mode == LV_LABEL_LONG_DOT || mode == LV_LABEL_LONG_CLIP)
        && horizontal;

    uint32_t fallback[MAX_GLYPHS];
    uint32_t missing[MAX_GLYPHS];
    int fallback_count = 0;
    int missing_count = 0;
    scan_glyphs(text, font, recolor, fallback, &fallback_count, missing, &missing_count);

    fprintf(f, ",\"text\":");
    write_json_string(f, text != NULL ? text : "");
    fprintf(f,
            ",\"text_font\":\"%s\",\"text_letter_space\":%d,\"text_line_space\":%d"
            ",\"long_mode\":\"%s\",\"recolor\":%s",
            font_name(font), (int) letter_space, (int) line_space,
            long_mode_name(mode), recolor ? "true" : "false");
    fprintf(f,
            ",\"text_box\":{\"w\":%d,\"h\":%d}"
            ",\"text_natural\":{\"w\":%d,\"h\":%d}"
            ",\"text_wrapped_h\":%d"
            ",\"text_line_height\":%d"
            ",\"text_overflow\":%s,\"text_truncated\":%s",
            box_w, box_h, (int) natural.x, (int) natural.y, (int) wrapped.y,
            font != NULL ? (int) font->line_height : 0,
            overflow ? "true" : "false", truncated ? "true" : "false");
    fprintf(f, ",\"missing_glyphs\":");
    write_codepoints(f, missing, missing_count);
    fprintf(f, ",\"fallback_glyphs\":");
    write_codepoints(f, fallback, fallback_count);
}

static void emit_node(inspector_t *ins, const lv_obj_t *obj, lv_area_t clip)
{
    FILE *f = ins->f;

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    int w = area_w(&coords);
    int h = area_h(&coords);

    lv_area_t visible;
    bool has_visible = _lv_area_intersect(&visible, &coords, &clip);

    fprintf(f, "{");
    fprintf(f, "\"class\":\"%s\"", class_name(obj));
    fprintf(f, ",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d", (int) coords.x1, (int) coords.y1, w, h);
    fprintf(f, ",\"clip\":{\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d}",
            (int) clip.x1, (int) clip.y1, area_w(&clip), area_h(&clip));
    if (has_visible) {
        fprintf(f, ",\"visible\":{\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d}",
                (int) visible.x1, (int) visible.y1, area_w(&visible), area_h(&visible));
    } else {
        fprintf(f, ",\"visible\":null");
    }
    fprintf(f, ",\"hidden\":%s", lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) ? "true" : "false");
    fprintf(f, ",\"overflow_visible\":%s",
            lv_obj_has_flag(obj, LV_OBJ_FLAG_OVERFLOW_VISIBLE) ? "true" : "false");
    fprintf(f, ",\"scrollable\":%s",
            lv_obj_has_flag(obj, LV_OBJ_FLAG_SCROLLABLE) ? "true" : "false");
    fprintf(f, ",\"scroll_dir\":\"%s\"", scroll_dir_name(lv_obj_get_scroll_dir(obj)));
    fprintf(f, ",\"clickable\":%s",
            lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE) ? "true" : "false");
    /* Focus-group membership (UI-004): `dialog_item()` is the only thing that puts a
     * widget into `keyboard_group` (src/dialog.c); without it neither the keys nor
     * the MFK can reach the widget (docs/HALLAZGOS.md 18). Emitted per node so the
     * structural check measures it on the real tree. */
    fprintf(f, ",\"in_keyboard_group\":%s",
            (keyboard_group != NULL && lv_obj_get_group(obj) == keyboard_group) ? "true" : "false");
    fprintf(f, ",\"opa\":%u", (unsigned) lv_obj_get_style_opa(obj, LV_PART_MAIN));

    lv_color_t text_color = lv_obj_get_style_text_color(obj, LV_PART_MAIN);
    lv_color_t bg_color = lv_obj_get_style_bg_color(obj, LV_PART_MAIN);
    const void *bg_img = lv_obj_get_style_bg_img_src(obj, LV_PART_MAIN);
    fprintf(f, ",\"text_color\":\"#%06X\",\"text_opa\":%u",
            (unsigned) colour_rgb(text_color),
            (unsigned) lv_obj_get_style_text_opa(obj, LV_PART_MAIN));
    fprintf(f, ",\"bg_color\":\"#%06X\",\"bg_opa\":%u,\"bg_image\":%s",
            (unsigned) colour_rgb(bg_color),
            (unsigned) lv_obj_get_style_bg_opa(obj, LV_PART_MAIN),
            bg_img != NULL ? "true" : "false");

    if (lv_obj_check_type(obj, &lv_label_class)) {
        emit_label(f, obj);
    }

    /* Children are clipped to this object's area unless it is overflow-visible. */
    lv_area_t child_clip = clip;
    if (!lv_obj_has_flag(obj, LV_OBJ_FLAG_OVERFLOW_VISIBLE)) {
        _lv_area_intersect(&child_clip, &clip, &coords);
    }

    uint32_t count = lv_obj_get_child_cnt(obj);
    fprintf(f, ",\"children\":[");
    for (uint32_t i = 0; i < count; i++) {
        if (i) {
            fputc(',', f);
        }
        emit_node(ins, lv_obj_get_child(obj, (int32_t) i), child_clip);
    }
    fprintf(f, "]}");
}

void ui_inspector_write_json(FILE *f, const lv_obj_t *screen)
{
    inspector_t ins = { .f = f };

    lv_area_t viewport;
    lv_coord_t hor = lv_disp_get_hor_res(NULL);
    lv_coord_t ver = lv_disp_get_ver_res(NULL);
    viewport.x1 = 0;
    viewport.y1 = 0;
    viewport.x2 = hor - 1;
    viewport.y2 = ver - 1;

    fprintf(f, "{\n");
    fprintf(f, "  \"instrumented\": true,\n");
    fprintf(f, "  \"source\": \"live LVGL object tree; absolute coordinates, "
               "font metrics from lv_txt_get_size/lv_font_get_glyph_dsc, no OCR\",\n");
    fprintf(f, "  \"viewport\": { \"width\": %d, \"height\": %d },\n", (int) hor, (int) ver);
    /* Whether the firmware's focus group exists in this capture. The `main` fixtures
     * do not create it (keyboard_init() is a no-op on the host), so the
     * focus-reachability rule only applies when a group is present (UI-004). */
    fprintf(f, "  \"keyboard_group\": %s,\n", keyboard_group != NULL ? "true" : "false");
    fprintf(f, "  \"nodes\": [");
    if (screen != NULL) {
        emit_node(&ins, screen, viewport);
    }
    fprintf(f, "]\n}\n");
}
