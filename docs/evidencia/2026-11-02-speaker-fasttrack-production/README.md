# Producción #364 — router RX procesado al parlante, default OFF

Fecha: 2026-11-02. Base verificada antes de empezar:

- GUI `origin/main` = `681074e0adac5a79f0392b4e51284167abf5e342` (confirmado con `git ls-remote` y API).
- Buildroot `origin/main` = `745af77e12b1c8fdabb234ec688608e386eb5be0`.
- Rama de esta unidad: `feat/rx-speaker-production-364`, worktree `../x6200-rx-speaker-364`.
- PR: **#384** — https://github.com/iariap/x6200_gui/pull/384

## Evidencia física histórica

El operador confirmó personalmente:

- `SPEAKER_AUDIO_HEARD = YES`
- `PRESET_DIFFERENCE_HEARD = YES`

En una prueba A/B con `Off` y `SSB voice` de 0,7 s cada una, LSB 7.103 MHz, volumen 40:
18 escrituras, 0 drops, 0 underruns, retorno a BASE, `reg12=0x00001000`, HMic/IMic y preset
restaurados, sin TX/PTT/ATU. La radio quedó con la GUI original y volumen 10.

Los documentos originales de la rama experimental `test/rx-speaker-fasttrack` se conservan
verbatim en [`historical/`](historical/). No se reescribieron resultados.

## Código de esta entrega

- `src/rx_audio_live.c/.h`: router reutilizable, ring fijo 4x4410, worker separado, stream propio.
- `src/audio.c`: integración en el callback de captura (misma PCM post-`audio_enh_process` que
  recorder/decoders), backend PulseAudio, **fail-closed por defecto**: `mux_ready=false` y
  `mux_request=true` devuelve `NOT_ENTERED`. No hay endpoint experimental en producción.
- `src/radio.c/.h`: interlocks TX/PTT/ATU/SWR-SCAN/poweroff y hook periódico del estado BASE.
- `src/CMakeLists.txt`: compila el router.
- `tests/native/...`: 23 casos de router / 124 comprobaciones (activación/desactivación, carrera
  MUX, restore fallido, estado incierto, stale, overflow, backpressure, write/open/ready fail,
  TX/audio generado, ciclo de vida, shutdown).

### Correcciones de concurrencia y ciclo de vida

- La obligación de `MUX_OFF` nace apenas el backend confirma `MUX_ON`; un `disable` o TX durante
  la llamada ya no pierde la selección.
- `MUX_ON` incierto no declara BASE: se intenta restaurar; si falla queda
  `BASE_RESTORE_UNCONFIRMED`, se bloquean nuevas activaciones y se reporta.
- La restauración no depende de la autorización de entrada.
- Sin `volatile` como sincronización; todo el estado compartido usa mutex/cond.
- Buffer objetivo de 200 ms; backpressure consulta `pa_stream_writable_size()`, no escribe un
  bloque entero si no hay lugar, y degrada a BASE si el sink no sostiene el objetivo.
- Cierre del stream en deshabilitar/degradar/shutdown; sin dos transiciones físicas simultáneas.

## Resultados de tests

| Prueba | Resultado |
|---|---|
| `sh tests/native/run_native_tests.sh` | **PASS** exit 0; router **124 comprobaciones, 0 fallos**; suite completa OK |
| `sh tests/native/run_native_tests.sh --ci-parity` | **PASS** exit 0; `ci-parity: OK`; router 124/0 |
| `sh tests/run_all_tests.sh --quick` | **20 OK, 3 fallos de entorno**: `wcag_rendered`, `widget_text` y `drawbuf_partial` fallan al construir/ejecutar el arnés Docker (`x6200-uiharness`); `host` 124/0, `host_ci_parity` 124/0, daemon OK, qemu SKIP, wspr/sstv/wefax SKIP por imagen incompleta |
| ASan/UBSan | **SKIP**: GCC del host no tiene `libasan.so.8`/`libubsan.so.1`; clang no instalado |
| QEMU | **SKIP**: el barrido QEMU pide entorno privado; no disponible |
| `git diff --check` | **OK** |
| Compilación ARM (Buildroot vigente, SDK v0.1.1 + patch) | **PASS** exit 0; binario 14.012.016 B, SHA256 `ce11bece6217ca3f1911a5ff33ad3343e94089c32c258f099e5dd0b9a577033a`, gate de firmware OK |

La prueba física **no se repitió** en esta entrega. No se desplegó ni flasheó la radio.

## Estado de entrega

- `SPEAKER_AUDIO_HEARD = PASS` (evidencia física histórica).
- `ROUTER_CODE_READY = PASS` (compilación ARM + tests host + CI parity).
- `MUX_NOMINAL_RETURN = PASS` (evidencia histórica, no garantía ante crash).
- `CRASH_RECOVERY = NOT_VERIFIED`.
- `PRODUCTION_READY = NO`.
- `DEFAULT_OFF = YES`, sin auto-start, sin endpoint remoto.
- Backend real sigue denegando DAC hasta que #367 acredite readback/restauración independiente.
