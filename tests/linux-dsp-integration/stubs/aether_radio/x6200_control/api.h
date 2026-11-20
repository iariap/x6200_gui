#pragma once
#include "aether_radio/x6200_control/low/control.h"
#include <stdbool.h>
void x6200_control_record_set(bool on);
void x6200_control_ptt_set(bool on);
void x6200_control_atu_tune(bool on);
void x6200_control_atu_set(bool on);
void x6200_control_modem_set(bool on);
void x6200_control_swrscan_set(bool on);
void x6200_control_split_set(bool on);
void x6200_control_poweroff(void);
void x6200_control_calibration_set(bool on);
