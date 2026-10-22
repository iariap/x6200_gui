/*
 * C++ stubs for the native src/remote_server.cpp test (tests/native/test_remote.cpp):
 * the BandsTable methods the ported server calls. The band table is empty here, so
 * band_name() answers "" and no band command moves the radio.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "cfg/db.h"

const std::vector<BandInfo> &BandsTable::all_bands() {
    static std::vector<BandInfo> bands;
    return bands;
}

BandInfoLoadResult BandsTable::get_by_id(int32_t id) {
    BandInfoLoadResult result;
    (void)id;
    return result;   /* rc = -1: no band */
}
