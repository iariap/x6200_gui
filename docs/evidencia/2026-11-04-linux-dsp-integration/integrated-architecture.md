# Arquitectura integrada (branch-local, DEFAULT OFF)

**Producto:** RX Xiegu → captura Linux 44.1 kHz mono S16 → SpeexDSP (`audio_enh_process`) → RX audio
router → DAC del SoC → MUX (`voice_rec` bit 0x8 del registro 12) → parlante interno. BASE nativo =
fallback. No se re-evalúa arquitectura ni se reemplaza SpeexDSP.

## Componentes integrados (este PR)

```
GUI / router (consumidor PCM post-Speex)
   │  route_ipc_client_request()  (UNIX SOCK_SEQPACKET, timeout, sin bloqueo de callback)
   ▼
x6200_route_arbiter  (proceso separado; ÚNICO escritor del registro 12)
   │  route_shadow_t (sombra autoritativa; se invalida ante error/restart/ownership loss)
   │  route_arbiter_t (estados BASE/READY/RX_LINUX/DEGRADED_BASE/FAULT + interlocks)
   ▼
route_bus write_reg12()  → transporte X6200 (producción: x6200_control_cmd; aquí: bus mock)
   ▲
x6200_route_supervisor (proceso separado del GUI; heartbeat/lease; NO escribe reg12)
   └─ vigila GUI/sink/worker; lease vencido => pide restore al árbitro; árbitro no responde => FAULT
```

- **Owner único lógico**: la GUI deja de abrir `/dev/i2c-0`/libaether directo cuando la feature está
  activa (proxy IPC); el árbitro serializa y aplica. DEFAULT OFF: esta integración no se arranca.
- **Verdad de sombra**: el árbitro conoce el último valor aplicado; no depende del readback inexistente
  del MCU (R′ #370). La sombra SOLO se actualiza por escrituras coordinadas exitosas; error/restart/
  ownership loss la invalida y bloquea RX_LINUX.
- **Supervisor separado**: no escribe el registro 12 (evita segunda escritura/race); si no puede
  restaurar vía árbitro ⇒ FAULT y no se activa; el fallback es GUI/servicio nativo.
- **Audio**: mismo PCM post-SpeexDSP al recorder y al router (una sola pasada; sin doble NR); ring
  fijo, worker dedicado, underrun/overflow deterministas; prioridad audio generado/TTS; sin nuevos
  settings/menús/presets; default OFF no operator-accessible.

## Archivos

- `src/linux_dsp_route/route_common.h` — bits/estados/errores/constantes.
- `route_shadow.[ch]` — sombra autoritativa e invalidación.
- `route_arbiter.[ch]` — FSM/interlocks/restore/PCM.
- `route_ipc.[ch]` — contrato IPC (request/response/errores/timeouts/secuencia/ownership).
- `route_supervisor.[ch]` — supervisor independiente.
- `route_daemon_main.c` — binario `x6200_route_arbiter` (`--self-check` en host/ARM/QEMU).
- `CMakeLists.txt` top: `option(X6200_LINUX_DSP_ROUTE ... OFF)`; subdirectorio sólo si ON.
- `tests/linux-dsp-integration/` — 208 comprobaciones host + self-check ARM/QEMU.

## Integración vs desactivado

- **Compilable y testeado**: librería, binario daemon, tests host/ARM/QEMU.
- **Desactivado**: wiring real GUI/libaether→IPC, lanzamiento por `x6200_daemon`, transporte I2C real y
  activación física. Sin `-DX6200_LINUX_DSP_ROUTE=ON` no se construye; no hay auto-start ni menú; el
  build normal queda BASE (sin cambios de comportamiento).
- **No modifica** `src/audio.c`, `src/radio.c`, `src/audio_enhancement.*`, X6200Control ni el daemon;
  sólo agrega módulos y la opción CMake default OFF.
