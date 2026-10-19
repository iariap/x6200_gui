/*
 * Stub for src/cfg/cfg_api.h (native remote_server test). It pulls the REAL
 * x6200_mode_t/x6200_agc_t/x6200_vfo_t from the vendored control SDK and keeps
 * the x6100_* spellings the ported code uses, exactly like the firmware header.
 */
#pragma once

#include <stdint.h>
#include <aether_radio/x6200_control/control.h>

/* The ported server reaches cfg.vol/cfg_cur.* through this header (like src/cfg/cfg_api.h). */
#include "cfg.h"

#define x6100_agc_t         x6200_agc_t
#define x6100_agc_off       x6200_agc_off
#define x6100_agc_slow      x6200_agc_slow
#define x6100_agc_fast      x6200_agc_fast
#define x6100_agc_auto      x6200_agc_auto

#define x6100_mode_t        x6200_mode_t
#define x6100_mode_lsb      x6200_mode_lsb
#define x6100_mode_usb      x6200_mode_usb
#define x6100_mode_lsb_dig  x6200_mode_lsb_dig
#define x6100_mode_usb_dig  x6200_mode_usb_dig
#define x6100_mode_cw       x6200_mode_cw
#define x6100_mode_cwr      x6200_mode_cwr
#define x6100_mode_am       x6200_mode_am
#define x6100_mode_sam      x6200_mode_sam
#define x6100_mode_nfm      x6200_mode_nfm
#define x6100_mode_wfm      x6200_mode_wfm
