/* Stub for src/util.h (native remote_server test): only the two helpers it uses. */
#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t get_time();
int32_t  align_int(int32_t x, uint16_t step);

#ifdef __cplusplus
}
#endif
