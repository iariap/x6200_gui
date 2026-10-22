/* Stub for src/dialog.h (native remote_server test). */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct dialog_t dialog_t;

bool dialog_type_is_run(dialog_t *dialog);
void dialog_restore_radio_on_close(void);
void dialog_restore_mode_on_close(int32_t mode, int32_t forced);
