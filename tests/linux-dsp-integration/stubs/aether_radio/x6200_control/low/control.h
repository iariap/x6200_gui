#pragma once
#include <stdint.h>
#include <stdbool.h>
/* Stub SOLO para compilar módulos feature-ON en host (no es el SDK real). */
typedef enum { x6200_sple_atue_trx = 12 } x6200_cmd_enum_t;
bool x6200_control_cmd(x6200_cmd_enum_t cmd, uint32_t arg);
/* stubs de símbolos para test de interposición */
void x6200_control_idle(void);
void x6200_control_host_cmd(int cmd);
/* Nothing extra */
