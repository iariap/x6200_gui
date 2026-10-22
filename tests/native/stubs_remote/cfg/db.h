/* Stub for src/cfg/db.h (native remote_server test): the band table names. */
#pragma once

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
    static const std::vector<BandInfo> &all_bands();
    static BandInfoLoadResult           get_by_id(int32_t id);
};
