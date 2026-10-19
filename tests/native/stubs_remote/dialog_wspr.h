/* Stub for src/dialog_wspr.h (native remote_server test). */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "dialog.h"
extern dialog_t *dialog_wspr;
void dialog_wspr_remote(const char *cmd, int32_t val, const char *text);
int  dialog_wspr_remote_state(char *buf, size_t size);
