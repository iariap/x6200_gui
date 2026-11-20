# Snapshot de `all_cmd` e idle refresh (task-27)

## Qué se puede exportar (factible, sin segundo escritor)

`X6200Control` expone `x6200_control_get(cmd)` que devuelve `all_cmd.arg[cmd]` (sombra RAM, 54 regs).
Se implementó `route_snapshot.[ch]`:
- captura `0..53` con GETTER inyectable (en producción: `x6200_control_get`), **sin escribir**;
- serializa el bloque `addr BE 2 B + 54×u32 LE = 218 B` (`route_snapshot_wire`);
- valida versión/secuencia/lease y **coherencia del reg12** con el getter actual.

Tests: 24 checks (captura valores/índices, wire de 218 B, lease/versión, divergencia reg12 ⇒ no
autoritativo, snapshot no muta el SDK).

## Por qué el refresco sigue DESHABILITADO

1. **Autoridad del snapshot**: con el proxy activo, las escrituras route-relevant se interponen y **no
   actualizan `all_cmd` de libaether** (la API no permite mutar la sombra desde afuera). `get()` devuelve
   el valor viejo para reg12/PTT/ATU etc. ⇒ el snapshot no es autoritativo (el test de divergencia lo
   demuestra). Refrescar con ese bloque podría revertir cambios proxied.
2. **Escritura bulk**: el refresco original envía **218 B en un solo `send_regs`**. El transporte actual
   expuesto por libaether sólo permite `x6200_control_cmd` por registro; no hay API pública de bulk-write.
   Improvisar 54 writes individuales cambia semántica/atomicidad (no autorizado).
3. **Verificación física**: sin readback del MCU (R′ #370) no se puede confirmar el bloque aplicado.

**Implementación actual**: `x6200_control_idle` con proxy activo captura snapshot, comprueba autoridad
y **no escribe**; registra `ROUTE_ERR_UNSUPPORTED`. Sin proxy/feature OFF delega al idle original (BASE).

## Capacidad exacta para habilitar refresh
(a) API en X6200Control para **actualizar la sombra** con las escrituras proxied (o snapshot autoritativo
del árbitro), con secuencia/lease/ownership; (b) **bulk-write** de 218 B por el transporte del árbitro
(mismo framing); (c) verificación física/readback del bloque aplicado. Sin (a)-(c): refresh OFF y
RX_LINUX bloqueado.
