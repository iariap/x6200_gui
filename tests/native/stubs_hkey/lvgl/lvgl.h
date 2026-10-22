/*
 * Minimal LVGL stub for the native hkey test (issue #131).
 *
 * Only the surface src/hkey.c touches: the active screen, the group reads and
 * the three timer primitives. Every entry point records the calling thread in
 * tests/native/hkey_stubs.c, which is what pins that hkey_put() never touches
 * LVGL from the radio thread.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Concrete here (the stub instantiates them); opaque to the firmware. */
struct _lv_obj_t { int unused; };
struct _lv_group_t { bool editing; struct _lv_obj_t *focused; };

typedef struct _lv_obj_t lv_obj_t;
typedef struct _lv_group_t lv_group_t;

/* Only the codes src/hkey.c's path names (event_send, event_send_key). */
typedef enum {
    LV_EVENT_KEY = 0,
    LV_EVENT_REFRESH,
} lv_event_code_t;

/* Key code used by events.h (tables of keys, never sent by hkey.c). */
typedef enum {
    LV_KEY_BACKSPACE = 8,
} lv_key_t;

struct _lv_timer_t;
typedef struct _lv_timer_t lv_timer_t;
typedef void (*lv_timer_cb_t)(lv_timer_t *);

struct _lv_timer_t {
    lv_timer_cb_t   timer_cb;
    uint32_t        period;
    int32_t         repeat_count;
    void           *user_data;
};

lv_obj_t   *lv_scr_act(void);
bool        lv_group_get_editing(const lv_group_t *group);
lv_obj_t   *lv_group_get_focused(const lv_group_t *group);
lv_timer_t *lv_timer_create(lv_timer_cb_t timer_cb, uint32_t period, void *user_data);
void        lv_timer_del(lv_timer_t *timer);
void        lv_timer_set_repeat_count(lv_timer_t *timer, int32_t repeat_count);
