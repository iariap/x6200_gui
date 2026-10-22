#pragma once

#include "dialog.h"
#include "wspr/wspr.h"

#ifdef __cplusplus
extern "C" {
#endif

extern dialog_t *dialog_wspr;

/* Web remote (X6100 Link): an app command (LVGL thread) and the "ax" state object (any thread) */
void dialog_wspr_remote(const char *cmd, int32_t val, const char *text);
int  dialog_wspr_remote_state(char *buf, size_t size);

/* Shows spots as if this cycle had just decoded them. For tests. */
void dialog_wspr_show_spots(const wspr_spot_t *spots, int n);

#ifdef __cplusplus
}
#endif
