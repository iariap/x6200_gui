#pragma once
#include "route_common.h"
#include "route_shadow.h"

typedef struct route_arbiter route_arbiter_t;

/* Crea el arbitro: BASE inicial, sin auto-enable, shadow invalido hasta boot. */
route_arbiter_t *route_arbiter_create(const route_bus_t *bus, uint64_t now_ms);
void route_arbiter_destroy(route_arbiter_t *a);

/* Escritura BASE inicial coordinada. Si falla el bus -> FAULT y shadow invalido. */
route_err_t route_arbiter_boot(route_arbiter_t *a);

/* Heartbeat del cliente GUI: seq monotona (repetida/atrasada => ROUTE_ERR_STALE),
 * value = ultimo valor completo que la GUI pide escribir (incluye bits compartidos),
 * write_ok = su ultima escritura salio bien. No escribe. */
route_err_t route_arbiter_client_heartbeat(route_arbiter_t *a, uint64_t now_ms, uint32_t seq,
                                           uint32_t value, bool write_ok);
void route_arbiter_client_gone(route_arbiter_t *a);

/* Escritura coordinada de bits compartidos (PTT/ATU/otros) pedida por la GUI:
 * el arbitro es el unico que escribe. Si hay bits de TX y la ruta esta en
 * RX_LINUX, primero deshabilita RX Linux; si el bus falla -> FAULT.
 * Nunca acepta que la GUI cambie voice_rec por esta via. */
route_err_t route_arbiter_apply_shared(route_arbiter_t *a, uint64_t now_ms, uint32_t target);

/* Ruta. enable exige: boot ok, shadow valido, heartbeat fresco, write_ok,
 * PCM post-Speex valido, sin TX/ATU/generado, no FAULT. */
route_err_t route_arbiter_set_enabled(route_arbiter_t *a, uint64_t now_ms, bool enabled);

/* TX/ATU: disable-first y despues permite los bits de TX; si falla -> FAULT. */
route_err_t route_arbiter_tx_prepare(route_arbiter_t *a, uint64_t now_ms);
route_err_t route_arbiter_atu_prepare(route_arbiter_t *a, uint64_t now_ms);
/* Tras TX, el cliente escribe los bits reales con apply_shared (voice_rec=0). */

void route_arbiter_set_generated_audio(route_arbiter_t *a, bool active);
void route_arbiter_set_sink_ok(route_arbiter_t *a, bool ok);
void route_arbiter_set_capture_ok(route_arbiter_t *a, bool ok);
/* PCM post-SpeexDSP exactamente una vez: block_id monotona por bloque. */
route_err_t route_arbiter_pcm_submit(route_arbiter_t *a, size_t samples, uint32_t rate,
                                     bool post_speex, uint64_t block_id, uint64_t now_ms);
route_err_t route_arbiter_pcm_missing(route_arbiter_t *a, uint64_t now_ms);

/* Tick del arbitro: detecta lease vencido/guia caida y restaura BASE sin GUI. */
route_event_t route_arbiter_tick(route_arbiter_t *a, uint64_t now_ms);

route_state_t route_arbiter_state(const route_arbiter_t *a);
bool route_arbiter_enabled(const route_arbiter_t *a);
const route_shadow_t *route_arbiter_shadow(const route_arbiter_t *a);
const route_stats_t *route_arbiter_stats(const route_arbiter_t *a);
const char *route_arbiter_last_error_name(const route_arbiter_t *a);
