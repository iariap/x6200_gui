/* Stub for src/dialog_field.h (native remote_server test). */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "dialog.h"
extern dialog_t *dialog_field;
void dialog_field_remote(const char *cmd, int32_t val, const char *text);
int  dialog_field_remote_state(char *buf, size_t size);
bool dialog_field_qso_log(const char *text);
