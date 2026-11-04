# Qué quedó CABLEADO y qué sigue DESACTIVADO (etapa wiring)

Feature: `X6200_LINUX_DSP_ROUTE` — **DEFAULT OFF**. Normal build = BASE, no procesos, no IPC, no
escrituras de transporte.

## Cableado (compilable, probado, aún no activado)

| componente | archivo | estado |
|---|---|---|
| Worker PCM/control (SPSC submit sin malloc/bloqueo, ring 4 bloques, TTS priority) | `src/linux_dsp_route/route_client_worker.[ch]` | cableado |
| Sink backend inyectable (producción PulseAudio; tests fake) | `route_sink.h` | cableado |
| Wiring/QA gate (`X6200_LINUX_DSP_ROUTE_QA=1` + `/etc/x6200_route_qa.enable`) | `route_wiring.[ch]` | cableado; activación RX_LINUX **bloqueada** por proxy incompleto |
| Adaptador transporte existente (x6200_control_cmd, mismo registro/framing) | `route_transport_x6200.c` | cableado sólo feature ON; no I2C nuevo |
| Hooks GUI (post-Speex → worker; TTS/generado; TX) | `src/audio.c`, `src/radio.c` `#ifdef X6200_LINUX_DSP_ROUTE` | cableados; en OFF no se compilan/llaman |
| Servicio árbitro/supervisor (`--arbiter/--supervisor/--self-check`) | `route_service_main.c` | compila; supervisa sin escribir reg12 |
| Ciclo de vida con `x6200_daemon` | `rootfs/usr/sbin/x6200_daemon` | arranca el servicio sólo si existe binario (feature ON) **y** gate `/etc/x6200_route_qa.enable`; en OFF no-op |
| CMake/install | `CMakeLists.txt`, `src/CMakeLists.txt`, `src/linux_dsp_route/CMakeLists.txt` | option OFF; subdir/install sólo ON |
| **Proxy GUI/libaether→IPC→arbitro** | `route_proxy.[ch]` + tests (88 checks) | cableado como API |
| **Call-sites libaether interpuestos** | `route_libaether_proxy.[ch]` + tests (52 checks) | cableado feature ON: reg12 sólo proxy, record OWNERSHIP, idle suprimido |

## Desactivado / pendiente (bloqueo exacto)

1. **Proxy completo de libaether**: el módulo proxy ya cubre y prueba todas las escrituras del
   registro 12 (record/PTT/ATU/modem/swrscan/split/poweroff/genérico) con RMW serializado, orden
   TX-disable-first, timeouts y caché invalidada; pero **falta el reemplazo mecánico de los
   call-sites** de libaether/GUI y redirigir `x6200_control_idle` al árbitro. Mientras la GUI siga
   escribiendo directo, RX_LINUX permanece **bloqueado** (`ROUTE_ERR_OWNERSHIP`). Bloqueo #1.
2. **Transporte real/estado físico**: sin readback del MCU (R′ #370); la seguridad lógica es
   árbitro+sombra, pero la verificación física del MUX/restore sigue PENDING/BLOCKED.
3. **Build GUI feature-ON no verificado** en este entorno: falta sysroot/lvgl/buildroot completo;
   los módulos se compilan host/ARM/QEMU con gcc/toolchain ROM. Normal build OFF no se ve afectado
   (hooks `#ifdef`, option OFF).
4. Activación runtime QA no operator-accessible (env+archivo no instalados); sin menús/settings.

## No cambios de comportamiento en OFF

`CMakeLists` option OFF ⇒ no se agrega subdirectorio; `src/audio.c`/`src/radio.c` hooks no se
compilan; `x6200_daemon` condición binario+gate no se cumple (binario no instalado). BASE intacto.
