# Sesión física QA — 2026-11-04 (owner autorizó)

## Alcance
Sesión RX-only con build QA reproducible, sin TX/PTT/ATU, sin firmware/flash. No es prueba de
producción.

## Evidencia observada
- Build QA: paquete `x6200_gui` + `x6200_route_service` ARM EABI5, patch X6200Control, gate QA.
- GUI QA log:
  - `x6200 QA: gate OK ipc=/run/x6200_route.sock`
  - `x6200 QA: IPC conectado, wiring READY (sin activar)`
  - `x6200 QA: PCM post-Speex fresco; pidiendo ENABLE al arbitro`
  - `x6200 QA: activation err=0 state=ACTIVE`
- Mientras QA activo: `pactl list short sink-inputs` mostró **2 sink-inputs** hacia el sink
  `alsa_output.platform-sound.stereo-fallback`; esto confirma PCM de la ruta QA entrando al sink
  PulseAudio del dispositivo.
- El owner reportó **audio audible durante toda la prueba**, sin cortes evidentes.
- Al terminar la GUI QA con SIGTERM, el servicio registró:
  - `x6200_route_service: shutdown_safe physical_confirmed=0`
  - `BASE fw version: 1.0.8`
- Tras el cierre: **0 sink-inputs**; el feed QA dejó de existir.
- Rollback: GUI original restaurada (sha256 `03356797959d21c35500d95c23bbc04d8d1f0ac922a89ec3681215d4fdcb982e`),
  gate QA eliminado, sin QA/service, estado del operador restaurado (CB-D 27.635 MHz USB, filtro 2700,
  volumen 0, RX, tx=false).

## Límites
- No hay readback del MUX; `MUX_PHYSICAL_STATE_VERIFICATION` sigue **BLOCKED**.
- No se midió instrumentamente el retorno BASE ≤250 ms; `RESTORE_250MS_PHYSICAL` queda
  **PARTIAL/UNMEASURED** (audio continuó según el owner; sin timestamp físico).
- `PCM_TO_SPEAKER_PHYSICAL` = **PARTIAL**: activación del árbitro + sink-inputs + audio audible,
  pero no verificación independiente de que el MUX/DAC físico seleccionara Linux.
- `PRODUCTION_READY` sigue **FAIL**; activación sólo QA; sin merge.

## Conclusión
La cadena QA **activó** y entregó PCM post-SpeexDSP al sink del dispositivo; el owner escuchó audio
durante la prueba. La atribución física exacta del MUX y el tiempo de restore requieren readback o
instrumentación adicional. No se declara PASS físico de producción.
