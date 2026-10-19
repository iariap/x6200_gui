/* Stub for src/dialog_cw_kbd.h (native remote_server test). */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "dialog.h"
extern dialog_t *dialog_cw_kbd;
void dialog_cw_kbd_remote(const char *cmd, int32_t val, const char *text);
int  dialog_cw_kbd_remote_state(char *buf, size_t size);
