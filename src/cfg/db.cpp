/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - band table for the ported remote server (see db.h).
 */

#include "db.h"

/* C bridge: this GUI's headers have no extern "C" guards (see cfg/cxx_bridge.c) */
extern "C" uint32_t x6200_bridge_band_read_all_bands(band_info_t **results, int32_t *cap);

#include <mutex>

static std::vector<BandInfo> bands;
static std::once_flag        once;

static void load() {
    band_info_t *list = nullptr;
    int32_t      cap  = 0;
    int32_t      n    = (int32_t) x6200_bridge_band_read_all_bands(&list, &cap);

    for (int32_t i = 0; i < n; i++) {
        BandInfo b;

        b.id         = list[i].id;
        b.name       = list[i].name ? list[i].name : "";
        b.start_freq = list[i].start_freq;
        b.stop_freq  = list[i].stop_freq;
        b.type       = list[i].active ? BAND_ACTIVE : BAND_INACTIVE;
        bands.push_back(b);
    }
}

const std::vector<BandInfo> &BandsTable::all_bands() {
    std::call_once(once, load);

    return bands;
}

BandInfoLoadResult BandsTable::get_by_id(int32_t id) {
    BandInfoLoadResult r;

    for (const BandInfo &b : all_bands()) {
        if (b.id == id) {
            r.value = b;
            r.rc    = SUCCESS;
            break;
        }
    }

    return r;
}
