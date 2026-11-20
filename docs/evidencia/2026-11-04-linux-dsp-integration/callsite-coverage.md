# Cobertura de call-sites route-relevant (interposición libaether)

Feature `X6200_LINUX_DSP_ROUTE` **DEFAULT OFF**. En OFF el archivo de interposición no se compila:
BASE exacto, sin proxy/IPC/procesos. En ON (QA) la API pública de libaether se interpone:

| símbolo libaether | con proxy activo | original | notas |
|---|---|---|---|
| `x6200_control_record_set` | **NO toca 0x8**: `route_proxy_record_set` ⇒ `OWNERSHIP` | no llamado | el bit voice_rec es del árbitro |
| `x6200_control_ptt_set` | `route_proxy_ptt_set` (TX_PREPARE + APPLY_SHARED) | no llamado | disable-first, bits preservados |
| `x6200_control_atu_tune` | `route_proxy_atu_tune`; `host_cmd 0x8004/0x8003` sigue directo | no llamado (sólo host_cmd) | host_cmd no toca reg12 |
| `x6200_control_atu_set` | `route_proxy_atu_set` | no llamado | bit 0x1000 |
| `x6200_control_modem_set` | `route_proxy_modem_set` | no llamado | bit 0x4000 |
| `x6200_control_swrscan_set` | `route_proxy_swrscan_set` | no llamado | bit 0x10 |
| `x6200_control_split_set` | `route_proxy_split_set` | no llamado | bit 0x2 |
| `x6200_control_poweroff` | `route_proxy_poweroff`; `kill(1,SIGUSR2)` **suprimido en QA** | no llamado | no apagar radio durante pruebas |
| `x6200_control_calibration_set` | `route_proxy_calibration_set` | no llamado | bit 0x8000 |
| `x6200_control_cmd(cmd,arg)` | si `cmd==x6200_sple_atue_trx`: proxy (`OWNERSHIP` si `arg&0x8`) | otros comandos van al original | sin writes arbitrarios |
| `x6200_control_idle` | **no reescribe directo** (evita escritor paralelo) | no llamado | QA-only: refresco periódico deshabilitado mientras proxy activo |
| `x6200_control_host_cmd` | **directo** (canal host, no reg12) | sí | por diseño |
| resto APIs (frecuencia/modo/filtros/volumen/etc.) | **directas** | sí | periféricos no-route; documentado |

Implementación: `src/linux_dsp_route/route_libaether_proxy.[ch]` (interposición fuerte de símbolos;
`dlsym(RTLD_NEXT)` para delegar los no-route; en tests `ROUTE_LIB_PROXY_TEST` con stubs inyectados).
Semántica original void/bool preservada; para errores del camino proxy se registra
`route_libaether_proxy_last_error()` (diagnóstico QA), sin cambiar firmas públicas.

Tests: `tests/linux-dsp-integration/test_callsites.c` (52 checks) — proxy inactivo delega al original;
proxy activo no llama originales para reg12; record rechazado; PTT/ATU/modem/swrscan/split/calib/
poweroff por proxy con bits preservados; host_cmd ATU directo; cmd genérico sólo reg12 por proxy;
idle suprimido; 20 PTT concurrentes serializados; timeout sin llamar al original.

**Bloqueo restante:** con proxy activo, `idle` no refresca periódicamente (QA-only) y la activación
RX_LINUX sigue bloqueada por gate físico/seguridad; falta build GUI completa feature ON (deps entorno)
y el protocolo físico.
