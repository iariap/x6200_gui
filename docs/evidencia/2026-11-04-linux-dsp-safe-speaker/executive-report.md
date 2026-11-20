# QA-LINUX-DSP-ROUTE — informe ejecutivo

**Épica:** #362 · **Issues:** #363, #364, #365, #367 · **PRs preservados:** #393 `25ae632a`, #392 `697cea97`, #391 `de35affa`, #389 `c0299e0b`, #388 `7cb97cf6`, #386 `84777dff`, #385 `68c501a8` · **PRs/evidencia previa:** #368 A′ (draft), #370 R′, #384 #364 (open, default OFF).
**Base real verificada:** `681074e0adac5a79f0392b4e51284167abf5e342` (`fork/main`); `origin/main`=`e1476ece` distinto, no usado; `main` local `6a29687d` intacto. Sin cambios de base/main.
**Trabajo:** offline; **sin radio, sin I2C/ALSA/MUX, sin flash, sin deploy, sin menús/settings.**

## Decisión de producto y encuadre

La decisión de producto es **final**: el RX procesado por Linux **debe** oírse por el parlante interno;
el DSP nativo BASE es **fallback**, no el objetivo de reemplazo. Este PR **no** abandona la ruta Linux:
resuelve (offline) la parte lógica de seguridad que faltaba y deja la integración de producción
condicionada a la verificación física y al reemplazo del dueño único del bus por un **árbitro**.

## Qué se encontró (reconstrucción exacta de la ruta audible)

La prueba física 2026-11-02 con `SPEAKER_AUDIO_HEARD=YES` y `PRESET_DIFFERENCE_HEARD=YES` usó:

```
captura PulseAudio 44.1 kHz mono S16NE (Mic1 del BASE)
  -> audio_enh_process (notch -> AF leveler -> SpeexDSP NR; post-Speex, una sola vez)
  -> rx_audio_live: ring fijo 4x4410 (400 ms), worker separado, prefill 2 bloques,
     stream de reproducción propio -> DAC del SoC
  -> MUX del BASE: voice_rec=bit 0x8 del registro 12 (selector BASE/DAC)
  -> parlante interno original
```

- La conmutación se hizo con **un solo punto** (`radio_audio_route_set()`), serializada con
  `radio_thread`/`x6200_control_idle`, sin `audio_play_en` y sin tocar HMic/IMic.
- Mediciones de esa prueba: `writes=18 drops=0 underruns=0`, retorno a BASE, preset/reg12/HMic/IMic/
  volumen restaurados; operador confirmó escucha y diferencia Off vs SSB voice.
- Integración posterior #364 (PR #384, **default OFF**): estados BASE/PREPARE_DAC/RX_DAC/
  DEGRADED_BASE/BASE_RESTORE_UNCONFIRMED, ring fijo, sin alloc/bloqueo en callback, interlocks
  TX/PTT/ATU/SWR/poweroff y audio generado, backend real **fail-closed** (`mux_ready=false`), tests
  host 124/0 y audio callback 178/0, ARM build OK. **No está en la base** ni se activa.

## Qué falta (diagnóstico)

1. **Verdad del MUX**: no hay readback del registro 12 (R′ #370: lecturas uniformes `00000000`,
   `R_INCONCLUSIVE`; `x6200_control_get` devuelve sólo la sombra RAM). A′ #368 exige readback real
   para armarse ⇒ queda inactivo (`RXMUX_HELPER_READBACK_AVAILABLE 0`) y el supervisor no lo lanza.
2. **Dueño único**: la GUI es el único proceso que abre `/dev/i2c-0`, pero `x6200_control_idle()`
   reescribe los 54 registros (218 B) desde su sombra cada ≤1000 ms; un segundo escritor (guardián)
   sería clobbeado o clobbearía. La serialización de #364 es intra-GUI; no sobrevive a la GUI.
3. **Restore independiente ≤250 ms**: el supervisor `x6200_daemon` sólo reinicia la GUI (`while true`),
   lo que tarda ~2 s+init; no hay restauración independiente ni guardián lanzado en producción.
4. **Aislamiento TX/ATU/físico**: los hooks existen en #364/A′ pero no hay verificación física del
   orden "deshabilitar RX Linux antes de PTT/ATU", ni de los bits compartidos en hardware.
5. **Continuidad PCM**: el router está probado con mocks/host; falta sonda física de underruns/
   clipping y de la ruta post-Speex por el parlante en la build actual.

## Solución propuesta (offline, implementada como modelo de referencia)

**Arbitro de ruta con verdad de sombra autoritativa**: un proceso independiente (`x6200_route_arbiter`)
pasa a ser el **único dueño** de `/dev/i2c-0` (la GUI/libaether deja de escribir directo: envía los
comandos por IPC y el árbitro los aplica). El árbitro:

- arranca en **BASE**, sin auto-enable, y sólo se declara `TRUTH_ARBITER_SHADOW` tras una escritura
  BASE exitosa (no necesita readback del MCU);
- conoce el **último valor aplicado** del registro 12 (sombra autoritativa), lo que da **verdad
  lógica** bajo dueño único; cualquier error de bus/escritura ⇒ `UNKNOWN`/`FAULT` y **nunca habilita**;
- recibe heartbeats ≤50 ms con el último valor de la GUI y `write_ok`; lease 100 ms;
- ante pérdida de heartbeat/GUI/worker/sink: **restaura BASE** escribiendo `sombra & ~voice_rec`
  (preserva PTT/ATU/resto) sin depender de la GUI; presupuesto 250 ms;
- al entrar TX/PTT/ATU: **deshabilita RX Linux primero** y sólo después permite los bits de TX;
- mantiene el PCM post-Speex una sola vez, ring fijo, sin alloc/bloqueo, con underrun/overflow
  definidos; no toca recorder/decoders/TTS/CW ni agrega menús/settings.

Referencia implementada **sólo offline** en `tools/linux-dsp-safe-speaker/route_arbiter_model.[ch]` +
harness determinista `tests/linux-dsp-safe-speaker/` (156 comprobaciones, 0 fallos; restore medido
110 ms en el modelo; ARM EABI5 compila). **No se integró a `src/` ni se desplegó**: la integración de
producción requiere el árbitro/proxy y la prueba física; el PR no toca la GUI/ROM.

## GATES (ver `gate-matrix.csv`)

| Gate | Estado |
|---|---|
| `MUX_STATE_TRUTH` | **PASS (offline, arbiter-shadow)**: verdad lógica por dueño único; sin readback MCU; físico no verificado |
| `SINGLE_WRITER_CONTROL` | **PASS (offline model)**: árbitro único; heartbeat no escribe; integración de proxy NO desplegada |
| `TX_PTT_ATU_ISOLATION` | **PASS (offline model)**: disables-first, bus-fail→FAULT; físico pendiente |
| `INDEPENDENT_BASE_RESTORE` | **PASS (offline model)**: restore sin GUI; no desplegado |
| `RESTORE_250MS_OFFLINE` | **PASS**: 110 ms medido en el modelo (≤250 ms); A′ host previo p99 103,3 ms con readback |
| `RESTORE_250MS_PHYSICAL` | **BLOCKED / PENDING**: no ejecutado (sin autorización/owner ni build integrado) |
| `RX_PCM_INTEGRITY` | **PASS (offline model)**: 44,1 kHz/4410, post-Speex 1×, bloque duplicado rechazado, ring acotado; físico pendiente |
| `SPEAKER_LINUX_AUDIO_PHYSICAL` | **PENDING**: evidencia audible 2026-11-02 en build experimental; build actual no verificada |
| `PRODUCTION_READY` | **FAIL**: gates físicos pendientes + árbitro/proxy no implementado/desplegado |

## Bloqueo principal (uno) y capacidad exacta necesaria

**Bloqueo:** no existe aún el **árbitro único de I2C en producción** (dueño único + proxy de la GUI +
lanzamiento por supervisor), y no hay verificación física del restore/ruta en la build nueva.

**Capacidad exacta necesaria:** (1) sesión física autorizada por el owner (RX-only primero) con
instrumentación mínima de audio; (2) implementación/deploy controlado del `x6200_route_arbiter` +
proxy IPC + arranque por `x6200_daemon` (branch-local, default OFF) y (3) verificación física de
restore ≤250 ms, no-interferencia TX/PTT/ATU y audio post-Speex por parlante; sin eso no se declara
`SPEAKER_LINUX_AUDIO_PHYSICAL` ni `PRODUCTION_READY`. No requiere firmware/FPGA: la verdad física del
MUX sigue sin observarse, pero el diseño la sustituye por verdad de árbitro para la seguridad lógica;
la validación acústica sigue pendiente.

## Próximo plan (#363→#364→#365)

1. **#363** (físico): protocolo de `physical-protocol.md`; medir restore real (kill GUI/worker, pérdida
   de sink) y continuidad; obtener `RESTORE_250MS_PHYSICAL` y el visto bueno del owner.
2. **#364**: integrar el árbitro/proxy branch-local, default OFF, con gates offline ya verdes;
   activación sólo con precondiciones y sin auto-enable.
3. **#365**: watchdog/rollback físico final (pérdida de heartbeat, crash, sink caído, TX/ATU) y
   rollback a BASE con la GUI original.

## Estado / seguridad

No se tocó la radio. No hubo I2C/ALSA/MUX/GPIO/reg12/reg24/0x34, ni flash/IAP/XMODEM, ni TX/PTT/ATU,
ni deploy/firmware, ni cambios de GUI/menús/settings/defaults. Firmware oficial 1.0.8 y estado del
operador preservados. `main` intacto; PRs previos preservados; sin merge; sin issues nuevas.
