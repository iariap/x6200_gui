/*
 * Minimal LVGL stub for the radio_thread test (issue #219).
 *
 * src/radio.c only pulls the logging macros, the opaque object type the
 * dialog/message prototypes name, and lv_msg_send() (the LVGL message bus the
 * params changes publish on). The real header is not available in CI (the
 * submodule is not checked out: see tests/native/run_native_tests.sh --ci-parity),
 * so the same minimal surface is pinned here.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The opaque object type named by the info/msg/tx_info/dialog_swrscan stubs. */
typedef struct _lv_obj_t lv_obj_t;

/* Event code type (the real one comes from LVGL on target). */
typedef uint32_t lv_event_code_t;

/* Logging macros used by src/radio.c (no output in the tests). */
#define LV_LOG_ERROR(fmt, ...) do { } while (0)
#define LV_LOG_WARN(fmt, ...)  do { } while (0)
#define LV_LOG_USER(fmt, ...)  do { } while (0)
#define LV_LOG_INFO(fmt, ...)  do { } while (0)
#define LV_LOG_TRACE(fmt, ...) do { } while (0)

/* The LVGL message bus: radio.c publishes MSG_PARAM_CHANGED after a param
 * change. The tests never subscribe; the stub records the calls. */
void lv_msg_send(uint32_t msg_id, const void *payload);
