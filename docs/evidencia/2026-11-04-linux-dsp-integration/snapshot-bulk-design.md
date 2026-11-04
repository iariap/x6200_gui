# Snapshot autoritativo + bulk-write 218 B (task-28)

## Hallazgo clave

`x6200_control_get(cmd)` permite **leer** la sombra `all_cmd` (54 regs) sin escribir, pero con el proxy
activo esa sombra **no recibe** las escrituras proxied (la API no expone setter). Por eso el snapshot
no era autoritativo (task-27). Solución de diseño, entregada como **parche propuesto** para el repo
externo X6200Control (`patches/x6200-control-snapshot-bulk.patch`, NO aplicado en este PR):

1. `x6200_control_shadow_update(cmd,arg)` — el proxy, tras una escritura confirmada por el árbitro,
   actualiza la sombra autoritativa (sin segunda escritura I2C). Garantiza init/defaults + todo camino
   reg12 coherente; secuencia/lease los pone el contrato IPC (`REQ_REFRESH`).
2. `x6200_control_bulk_write(block,len)` — expone `send_regs` para enviar **218 B** (addr BE 2B +
   54×u32 LE) por el transporte existente. Sin este API público, el refresco no se habilita (no se
   usan 54 writes individuales: cambiarían atomicidad/framing).

## Implementado en la branch (feature ON, QA-only)

- `route_bus_t.write_block` (NULL si el transporte no soporta bulk).
- `ROUTE_IPC_REQ_REFRESH` + `route_ipc_client_refresh()` con bloque 54×u32; timeout 100 ms.
- `route_arbiter_refresh()`: exige boot ok, sombra válida, heartbeat fresco; **merge** de `voice_rec`
  desde el estado de ruta; con ruta activa **elimina** bits TX del bloque; sin bulk => `UNSUPPORTED`;
  bus error => sombra inválida + FAULT; serializado con PTT/ATU por el protocolo IPC.
- Transport adapter `route_transport_x6200_write_block` sólo `#ifdef X6200_CTRL_HAVE_BULK_WRITE`
  (parche instalado); sin parche, `write_block=NULL` y refresh deshabilitado.
- Tests `test_refresh.c` (47 checks): sin bulk => UNSUPPORTED sin writes; framing 218 B/endian;
  merge voice en ruta; bits TX eliminados; bus fail => FAULT; timeout; serialización con PTT.

## Estado runtime

**Refresh queda DESHABILITADO en la build normal** (y en feature ON sin parche): `write_block` es NULL
=> `ROUTE_ERR_UNSUPPORTED`; con proxy activo `idle` no escribe. Para habilitarlo: aplicar parche a
X6200Control, recompilar libaether con `X6200_CTRL_HAVE_BULK_WRITE`, y validar con tests + verificación
física (readback pendiente). RX_LINUX sigue bloqueado; feature DEFAULT OFF.

## Parche aplicado branch-localmente (task-29) + build

- Copia de trabajo: `qa-x6200ctrl-patched/src` (copia de `refs/X6200Control` 0.15.3); aplicado el parche:
  - `x6200_control_shadow_update(cmd,arg)` actualiza `all_cmd.arg[cmd]` (sin I2C; lo usa el proxy tras
    cada escritura confirmada por el árbitro; `sync_shadow12()` en la interposición).
  - `x6200_control_bulk_write(block,len)` con bounds `len==sizeof(all_cmd)` y `send_regs` (218 B).
  - ABI/API intactas: sólo se añaden símbolos; header incluye `<stddef.h>`.
- Build de la libaether parcheada: CMake + Buildroot toolchain, `BUILD/INSTALL` OK (`logs/x6200control-patched-build.log`).
- Build feature ON con API parcheada (`-DX6200_CTRL_HAVE_BULK_WRITE=ON`, headers `refs/X6200Control/include`,
  lib parcheada): **`x6200_gui` ARM EABI5 OK** y **`x6200_route_service` ARM EABI5 OK** (referencia a
  `x6200_control_bulk_write` en `nm -D`). Logs `build-feature-on-patched-*.log`, `cmake-feature-on-patched-configure.log`.
- **Idle refresh habilitado sólo en builds feature ON + `X6200_CTRL_HAVE_BULK_WRITE` + QA gate**:
  interposición sincroniza la sombra autoritativa y `idle` envía el bloque por IPC REFRESH al árbitro
  (merge voice_rec, strip TX si ruta activa, lease/bounds, bus-fail⇒FAULT). En build normal (OFF) y en
  feature ON sin parche sigue **deshabilitado/no directo** (`ROUTE_ERR_UNSUPPORTED`). RX_LINUX continúa
  bloqueado; verificación física/readback pendiente.
