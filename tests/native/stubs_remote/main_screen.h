/* Stub for src/main_screen.h (native remote_server test). */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "params/params.h"

void main_screen_set_freq(uint64_t f);
bool main_screen_freq_locked(void);
bool main_screen_mode_locked(void);
bool main_screen_band_locked(void);
void main_screen_agc_next(void);
void main_screen_start_app(press_action_t page_app);
void main_screen_close_app(void);
