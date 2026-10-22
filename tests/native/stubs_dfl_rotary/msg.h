/*
 * Stub de src/msg.h para el test del motor de la rueda del editor DFL: no hay
 * GUI. El texto se guarda para poder afirmar que sale del Subject (nunca de un
 * valor optimista).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

void msg_update_text_fmt(const char *fmt, ...);

/* Solo test: ultimo texto formateado. */
const char *dfl_test_last_msg(void);
