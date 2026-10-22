/* Stub for src/dialog_ft8.h (native remote_server test). */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "dialog.h"
extern dialog_t *dialog_ft8;
void dialog_ft8_remote(const char *cmd, int32_t val, const char *text);
int  dialog_ft8_remote_state(char *buf, size_t size);
