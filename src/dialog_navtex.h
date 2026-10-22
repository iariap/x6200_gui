#pragma once

#include <stdbool.h>

#include "dialog.h"

#ifdef __cplusplus
extern "C" {
#endif

extern dialog_t *dialog_navtex;
bool dialog_navtex_is_active(void);

/* Web remote (X6100 Link): an app command (LVGL thread) and the "ax" state object (any thread) */
void dialog_navtex_remote(const char *cmd, int32_t val, const char *text);
int  dialog_navtex_remote_state(char *buf, size_t size);

#ifdef __cplusplus
}
#endif
