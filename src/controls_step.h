/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdint.h>

/* Valor nuevo del control tras un giro `diff`, recortado a su rango (ver controls_step.c). */
int32_t controls_step(int ctrl, int32_t actual, int32_t diff);
