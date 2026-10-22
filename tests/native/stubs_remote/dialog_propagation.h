/* Stub for src/dialog_propagation.h (native remote_server test).
 * Mirrors the remote-control API the server's command tables point at
 * (issue #145); the implementations live in tests/native/remote_stubs.c. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "dialog.h"
extern dialog_t *dialog_propagation;
bool dialog_propagation_remote_run(bool start);
void dialog_propagation_remote(const char *cmd, int32_t val, const char *text);
int  dialog_propagation_remote_state(char *buf, size_t size);
