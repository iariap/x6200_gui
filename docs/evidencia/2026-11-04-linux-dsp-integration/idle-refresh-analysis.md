# `x6200_control_idle` — semántica y decisión de refresco (task-26)

## Semántica real (refs/X6200Control/src/low/control.c:480-488)

- `all_cmd_struct_t` = `uint16_t addr` + `uint32_t arg[54]` = **218 B**, 54 registros (índices 0..53).
- `x6200_control_idle()` envía **el bloque completo** por I²C; si falla, cierra/reabre `/dev/i2c-0`,
  espera 1 ms y reintenta **una vez**.
- La GUI lo llama desde `radio_thread` cuando `idle > REFRASH_TIMEOUT (1000 ms)` (src/radio.c), bajo
  `WITH_RADIO_LOCK`. Incluye TODOS los registros: frecuencia/modo/filtros/AGC/NR/NB/DNF/EQ/volumen,
  y el **registro 12 compartido** (voice_rec/PTT/ATU/modem/swrscan/split/calib/poweroff).
- Es un refresco periódico del bloque desde la sombra RAM de libaether (`all_cmd`), no un readback;
  el MCU aplica el bloque completo.

## Diseño requerido para un refresco propiedad del árbitro

1. El árbitro debe poseer un **bloque autoritativo de 54 regs** (no reconstruible por él: sólo libaether
   tiene `all_cmd` completo, incluyendo defaults internos de init). El proxy bridge debería exportar el
   bloque completo y el árbitro escribir 218 B por su transporte, con orden/lease.
2. Merge de bits: antes de escribir, el árbitro debe sustituir su estado de ruta (`voice_rec`) y validar
   bits TX/ATU (disable-first), sin perder campos de la GUI.
3. Timing acotado ≤1000 ms, no concurrente con escrituras proxy; shadow inválido ⇒ no refrescar;
   error de bus ⇒ FAULT/no retry ciego; registro de latencia.
4. Feature OFF: sin refresco, BASE exacto (idle original intacto). Feature ON QA: refresco sólo por árbitro.

## Decisión: refresco **DESHABILITADO** (bloqueado con causa exacta)

No se puede demostrar seguro **offline** porque:
- La interposición actual no tiene acceso al bloque `all_cmd` de libaether (externo al repo; no expone
  API de snapshot). Reconstruir 54 regs por call-sites es incompleto (init escribe defaults directo en
  `all_cmd`; registros nunca pasados por `cmd` se perderían) ⇒ un refresco parcial **podría corromper**
  frecuencia/modo/filtros/calibración. No se improvisa.
- No hay readback del MCU (R′ #370) para verificar el bloque aplicado.

Implementación en branch: `x6200_control_idle` interpuesto en feature ON con proxy activo **no escribe
nada** y registra `ROUTE_ERR_UNSUPPORTED` (`route_libaether_proxy_last_error`). Sin proxy (ON sin QA) o
feature OFF, se delega al idle original (BASE).

**Capacidad exacta necesaria para habilitar refresco:** (a) export de `all_cmd` (54×u32) por X6200Control
o API de snapshot autoritativa; (b) contrato de orden/lease con el árbitro; (c) verificación física del
bloque aplicado (readback del MCU o instrumentación). Sin (a)-(c): refresco queda deshabilitado y
RX_LINUX bloqueado.
