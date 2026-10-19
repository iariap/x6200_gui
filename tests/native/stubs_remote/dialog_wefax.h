/* Stub for src/dialog_wefax.h (native remote_server test). */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "dialog.h"
extern dialog_t *dialog_wefax;
void dialog_wefax_remote(const char *cmd, int32_t val, const char *text);
int  dialog_wefax_remote_state(char *buf, size_t size);
