#pragma once
#include "route_common.h"
#include "route_ipc.h"
/* Proxy GUI/libaether -> IPC -> arbitro. Cubre TODAS las escrituras del registro
 * compartido 12 (ruta/PTT/ATU/modem/swrscan/split/calib/poweroff) mediante un
 * unico camino serializado; el bit voice_rec (0x8) SÓLO lo cambia el arbitro.
 *
 * Feature ON + QA gate: el proxy existe; RX_LINUX sigue BLOQUEADO hasta evidencia
 * física/seguridad. Feature OFF: proxy_create devuelve NULL y no hay IPC ni escrituras.
 *
 * NO reemplaza por sí solo los call-sites de libaether: el inventario documenta
 * cuáles se enrutan por proxy y cuáles quedan directos a propósito (periféricos
 * no-route: frecuencia/modo/filtros/volumen/etc.). El reemplazo mecánico de
 * call-sites es el paso siguiente antes de activar. */
typedef struct route_proxy route_proxy_t;

route_proxy_t *route_proxy_create(route_ipc_client_t *ipc, uint32_t cached_reg12, bool enabled);
void route_proxy_destroy(route_proxy_t *p);
bool route_proxy_is_enabled(const route_proxy_t *p);
route_err_t route_proxy_get_reg12(route_proxy_t *p, uint32_t *out); /* sombra cacheada; error si inválida */

/* Escritura coordinada del registro 12 (excepto voice_rec). */
route_err_t route_proxy_write_reg12(route_proxy_t *p, uint32_t target);
/* Wrappers con la MISMA semántica RMW que libaether (sobre la sombra cacheada). */
route_err_t route_proxy_ptt_set(route_proxy_t *p, bool on);
route_err_t route_proxy_atu_tune(route_proxy_t *p, bool on);
route_err_t route_proxy_atu_set(route_proxy_t *p, bool on);
route_err_t route_proxy_modem_set(route_proxy_t *p, bool on);
route_err_t route_proxy_swrscan_set(route_proxy_t *p, bool on);
route_err_t route_proxy_split_set(route_proxy_t *p, bool on);
route_err_t route_proxy_poweroff(route_proxy_t *p);
/* Refresh idle: bloque de 54 regs (snapshot autoritativo) vía IPC; el arbitro
 * decide merge de ruta/TX y escribe 218 B por su transporte bulk. */
route_err_t route_proxy_refresh_block(route_proxy_t *p, const uint32_t regs[ROUTE_SNAPSHOT_REGS]);
route_err_t route_proxy_calibration_set(route_proxy_t *p, bool on);
/* voice_rec: la GUI NO lo escribe; el arbitro es el dueño. Devuelve OWNERSHIP. */
route_err_t route_proxy_record_set(route_proxy_t *p, bool on);
/* Host commands de ATU (x6200_control_host_cmd) quedan DIRECTOS: no tocan el reg12. */
