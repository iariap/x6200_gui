/*
 *  LVGL configuration for the host UI harness.
 *
 *  Only the settings the harness needs are set here; LVGL 8's
 *  lv_conf_internal.h fills in the rest with its defaults. The important ones:
 *    - 32-bit colour, a large heap (the real fonts and images are in RAM);
 *    - the 'A:' drive mapped to the host filesystem so the theme's `A:/...`
 *      background bitmaps resolve to the repository (no /dev/shm on the PC);
 *    - the built-in Montserrat 14 kept as the default font (LVGL's own widgets
 *      fall back to it; the GUI uses its own fonts).
 */

#pragma once

/* Marks the file as found (LVGL's lv_conf_internal.h checks this). */
#define LV_CONF_H

#define LV_COLOR_DEPTH 32
/* Same as the ROM's lv_conf.h: some widgets (opacity/layers) need it. */
#define LV_COLOR_SCREEN_TRANSP 1
#define LV_MEM_SIZE (64U * 1024U * 1024U)

/* Log to stdout at warnings and above (image/driver problems show up here). */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/* Los formatos con float de las etiquetas (%f/%.1f), como en la ROM: el lv_conf.h de la raiz
 * los habilita (LV_SPRINTF_USE_FLOAT 1) y LVGL trae el default en 0. Sin esto LVGL imprime la
 * letra 'f' en vez del numero (el arnes decia "f" donde la radio dice el valor) y una etiqueta
 * que la ROM dibuja con su numero quedaba sin medir en el banco. */
#define LV_SPRINTF_USE_FLOAT 1

/* Host filesystem for the theme bitmaps: A:/path -> /path */
#define LV_USE_FS_STDIO 1
#if LV_USE_FS_STDIO
    #define LV_FS_STDIO_LETTER 'A'
    #define LV_FS_STDIO_PATH ""
    #define LV_FS_STDIO_CACHE_SIZE 0
#endif

#define LV_USE_ASSERT_NULL 0
#define LV_USE_ASSERT_MALLOC 0

/* Same as the ROM's lv_conf.h: the software-rotation temp buffer. The harness
 * itself never rotates (it draws landscape into a full-screen buffer), so this
 * is inert here; the draw-buffer probe (probe/disp_buf_probe.c) needs it to
 * model the firmware's ROT_90 path with the same chunk size. */
#define LV_DISP_ROT_MAX_BUF (4 * 800 * 50)

/* info.c subscribes to WiFi state messages (LVGL's lv_msg). */
#define LV_USE_MSG 1
