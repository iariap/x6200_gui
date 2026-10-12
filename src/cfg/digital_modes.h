#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    CFG_DIG_TYPE_FT8,
    CFG_DIG_TYPE_FT4,
    CFG_DIG_TYPE_JS8,
    CFG_DIG_TYPE_JS8_GHOSTNET,
    CFG_DIG_TYPE_WSPR,
} cfg_digital_type_t;

bool cfg_digital_load(int8_t dir, cfg_digital_type_t type);

char * cfg_digital_label_get();

/* Frequency of the last digital mode loaded (0 before the first load) */
int32_t cfg_digital_freq_get(void);
