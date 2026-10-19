/* Stub for src/dialog_sstv.h (native remote_server test). */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "dialog.h"
extern dialog_t *dialog_sstv;
void dialog_sstv_remote(const char *cmd, int32_t val, const char *text);
int  dialog_sstv_remote_state(char *buf, size_t size);
