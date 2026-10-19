/* Stub for src/dialog_js8.h (native remote_server test). */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "dialog.h"
extern dialog_t *dialog_js8;
void dialog_js8_remote(const char *cmd, int32_t val, const char *text);
int  dialog_js8_remote_state(char *buf, size_t size);
