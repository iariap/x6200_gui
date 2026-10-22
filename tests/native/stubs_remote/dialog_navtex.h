/* Stub for src/dialog_navtex.h (native remote_server test). */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "dialog.h"
extern dialog_t *dialog_navtex;
void dialog_navtex_remote(const char *cmd, int32_t val, const char *text);
int  dialog_navtex_remote_state(char *buf, size_t size);
