/*
 * Stub de `lvgl/src/misc/lv_log.h` para el test de persistencia (issue #213).
 *
 * src/params/db.c incluye este header con <...> (en la radio resuelve al LVGL del
 * submodulo). El stub declara la MISMA API que LVGL v8 y manda los mensajes a
 * _lv_log_add(), que tests/native/db_test_stubs.c captura: el test tiene que poder
 * afirmar que el error de escritura se reporta (y que no se repite por tick).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LV_LOG_LEVEL_TRACE = 0,
    LV_LOG_LEVEL_INFO,
    LV_LOG_LEVEL_WARN,
    LV_LOG_LEVEL_ERROR,
    LV_LOG_LEVEL_USER,
    LV_LOG_LEVEL_NONE
} lv_log_level_t;

void _lv_log_add(lv_log_level_t level, const char *file, int line,
                 const char *func, const char *format, ...);

#define LV_LOG_ERROR(...) _lv_log_add(LV_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LV_LOG_WARN(...)  _lv_log_add(LV_LOG_LEVEL_WARN,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LV_LOG_USER(...)  _lv_log_add(LV_LOG_LEVEL_USER,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LV_LOG_INFO(...)  _lv_log_add(LV_LOG_LEVEL_INFO,  __FILE__, __LINE__, __func__, __VA_ARGS__)
