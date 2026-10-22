/* Stub for src/dialog_swrscan.h (native remote_server test). */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "dialog.h"
extern dialog_t *dialog_swrscan;
bool dialog_swrscan_remote_run(bool start);
void dialog_swrscan_remote(const char *cmd, int32_t val, const char *text);
int  dialog_swrscan_remote_state(char *buf, size_t size);
