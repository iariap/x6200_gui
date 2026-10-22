/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - band table for the ported remote server.
 *
 *  The X6100 keeps its bands in a SQLite-backed BandsTable with a rich API; this
 *  GUI already has cfg_band_read_all_bands(). This header gives the ported code
 *  the same names over that, so remote_server.cpp compiles unchanged.
 */

#pragma once

#include "band.h"

#include <cstdint>
#include <string>
#include <vector>

#define BAND_UNDEFINED  (-1)
#define SUCCESS         0

typedef enum {
    BAND_INACTIVE = 0,
    BAND_ACTIVE,
} band_type_t;

struct BandInfo {
    int32_t     id = BAND_UNDEFINED;
    std::string name;
    uint32_t    start_freq = 0;
    uint32_t    stop_freq  = 0;
    band_type_t type       = BAND_INACTIVE;
};

struct BandInfoLoadResult {
    BandInfo value;
    int      rc = -1;
};

class BandsTable {
public:
    /* The bands, read once from the database and kept (the remote server asks
     * for them on every band change). */
    static const std::vector<BandInfo> &all_bands();
    static BandInfoLoadResult           get_by_id(int32_t id);
};
