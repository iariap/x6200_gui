/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6200 LVGL GUI - Fine-tuning assistant UI (issue #190)
 *
 *  Copyright (c) 2024 LU2DSX
 *
 *  LVGL-side coordinator of the CW/CWR fine-tuning assistant. The policy lives in
 *  src/autotune.c (pure, native-tested); here live the subjects, the clock, the
 *  timer and the message bar. It never keys the radio and never touches the RF or
 *  TX path: Auto Tune is strictly RX.
 *
 *  Ownership discipline (issue #190, section 5): this layer only gathers the
 *  context (band/VFO/mode/pitch/frequency/RX/lock/RIT) and asks the core whether
 *  it still owns the dial. It writes a frequency ONLY through own_write(), which
 *  asks first. Every abort path that follows an external order says so: a manual
 *  dial move, CAT, a memory, a band/VFO/mode change or a key-up always wins and
 *  is never overwritten.
 *
 *  The timer is created on demand and destroyed by every terminal path (success,
 *  failure, cancel, dialog/app hook, TX), so no callback can run over a freed
 *  screen: it only pokes subjects and the message bar, never the widgets.
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Registra el observador de cambio externo de frecuencia y deja el asistente en
 * reposo. Lo llama main_screen() una sola vez. */
void autotune_ui_init(void);

/* ACTION_ATUNE: arranca una sesion (o rechaza con causa) o, si ya corria,
 * cancela con restauracion condicionada. */
void autotune_ui_toggle(void);

/* Hook de apertura de app/dialogo: cancela restaurando el origen SOLO si sigue
 * el contexto original, en RX y con el dial en manos de Auto Tune (5.2). */
void autotune_ui_cancel(void);

/* Hook de una orden manual de dial resuelta en GUI (rueda, teclas +/-/HKEY, CB):
 * aborta ANTES de que la orden se aplique y nunca restaura encima (5.1). */
void autotune_ui_abort_manual(void);

/* Hook previo a activar TX desde un handler de GUI: cancela y restaura solo si
 * todavia estamos en RX, con el contexto original y el dial propio (5.3). */
void autotune_ui_cancel_before_tx(void);

/* EVENT_RADIO_TX / cambio externo de estado con TX ya activo: aborta SIN
 * sintonizar jamas (5.3). */
void autotune_ui_abort_on_tx(void);

/* true mientras hay una sesion activa (lo usa la fixture del banco). */
bool autotune_ui_running(void);

#ifdef __cplusplus
}
#endif
