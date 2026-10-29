# Handoff B — contrato SRC / PCM / MUX / restore

**Estado: NO VALIDADO. Este documento NO habilita #364.** Es el contrato que la fase A **no pudo
probar** y que #364 tendría que demostrar de punta a punta antes de tocar el parlante. `GATE_A` de esta
corrida es `BLOCKED_SAFE_MUX` (ver `gates.csv`); #364 **no debe arrancar** hasta que exista un camino de
readback/restore/owner demostrado (nuevo trabajo, no este PR).

## 1. Qué demostró A (y qué no)

| ítem | A lo demostró | evidencia |
|---|---|---|
| bit 0x8 del reg 12 = `voice_rec`; comparte registro con PTT (`0x40000`) y ATU | **sí** (código + REA del binario desplegado) | `registers.md` §1, `reverse-engineering.md` §3-§4 |
| `x6200_control_get()` es sombra RAM; no hay readback real del reg 12 | **sí** | `registers.md` §2 |
| la GUI es el único dueño de `/dev/i2c-0` y reescribe 218 B (54 regs) cada ≤1 s | **sí** (medido un solo dueño; código+REA del idle) | `registers.md` §2.1 |
| existe un guardián independiente con restore ≤250 ms | **NO** (no existe; el único restore es reinicio de GUI ≳4 s) | `registers.md` §3 |
| la conmutación de MUX produce audio audible en parlante/jack de **nuestro** equipo | **NO MEDIDO** (`PHYSICAL_OUTPUT_NOT_VERIFIED`) | `physical-signal.md` |
| semántica interna del MCU (MUX/PA/restore en TX/crash) | **UNKNOWN** (firmware encriptado) | `reverse-engineering.md` §5 |

**Por lo tanto:** el contrato de abajo es una **propuesta**, no un resultado. Cada casilla "cómo lo
prueba B" es obligatoria; sin esa prueba, #364 no puede declarar `CODE_READY` para hardware.

## 2. Contrato exacto propuesto

### 2.1 SRC — de dónde sale el PCM

- **Fuente única:** el PCM de RX que ya procesa la GUI: `read_callback()` de
  `alsa_input.platform-sound.stereo-fallback` (`src/audio.c:85`), mono S16NE **44100 Hz**
  (`AUDIO_CAPTURE_RATE`), **después** de `audio_enh_process()` (`src/audio.c:63,102`).
- **Un solo procesamiento:** la misma copia procesada que va a `screen_rec_put_audio_samples` y
  `dsp_put_audio_samples` debe ser la que se reproduzca; **prohibido** aplicar NR/DFL dos veces o crear una
  segunda cadena (requisito del épico).
- **Sin tocar TX/PTT/ATU/micrófonos:** el stream de reproducción del router **no** puede pasar por
  `audio_play_en()` (ver 2.3) ni cambiar `hmic/imic/spmode/mic/rxvol` fuera del contrato.
- **Prioridad:** TTS y audio generado por la GUI (FT8/JS8/mensajes/recorder/TX) conservan prioridad; si el
  router no puede escribir, se descarta el frame RX (nunca se bloquea el callback).

### 2.2 PCM — formato, nivel y buffer

| propiedad | contrato | medido en A |
|---|---|---|
| formato | `s16le`, mono, 44100 Hz en el callback; el sink PA declara `s16le 2ch 44100 Hz` y hace el mapeo (el play stream actual de la GUI es `s16le 1ch 44100 Hz`) | sí: `pactl list sinks` + sink-input de la GUI (`logs/a0-preflight.txt`) |
| nivel de prueba | ≤ **−24 dBFS** (fixture propio/licenciado; nunca voz de terceros) | no aplica (A4 no ejecutado) |
| buffer | bounded, sin malloc en el callback, sin esperas; el callback nunca escribe directo a PulseAudio | código actual: `play_stm` sólo lo escribe `audio_play()` (`src/audio.c:326`) |
| stream | **dedicado y separado** de TTS/digital; nombre propio para poder observarlo/cancelarlo | no existe hoy |
| fallback | si el sink no acepta datos o hay underrun ⇒ volver a BASE y no reintentar en loop | no existe hoy |

### 2.3 MUX — quién conmuta el bit 0x8

- **Dueño único dentro del proceso que ya tiene `/dev/i2c-0`** (la GUI), serializado con el mismo lock del
  hilo de radio (`radio_lock`, `src/radio.c:74-91`) y **con el mismo dueño del bit de PTT**. No se admite
  un segundo proceso escribiendo el bus (hoy es imposible: §2.1 de `registers.md`).
- **Interlock duro antes de escribir:** estado `RX`, `radio_tx_allowed()`/sin PTT, sin `atu_tune`,
  `modem_trx`, `swrscan_trx` ni `power_off` en curso; si cualquiera de esos aparece, `record=0` inmediato.
- **Conmutación transitoria ≤2 s** (regla del issue), con debounce; el bit se escribe **sólo** en la
  transición (no en cada frame).
- **PA/volumen:** si se sigue la documentación externa, el parlante requiere `rxvol>0` (reg 14). El router
  **no** debe cambiar el nivel del usuario; si necesita garantizar PA, debe probarlo y documentar el
  contrato exacto (A no lo midió).
- **Prohibido:** `audio_play_en()`, `hmic/imic`, `spmode`, `mic`, ATU, PTT, flash/ROM, I2C crudo desde otro
  proceso, RMW sobre sombra desincronizada.
- **Estado del MUX:** como no hay readback del reg 12, el dueño debe mantener el estado en proceso **y**
  probar que el BASE obedece (la única forma es con instrumentación física; si no hay readback, B debe
  declarar `NOT_VERIFIED` y no puede afirmar restore).

### 2.4 Restore — fail-safe

| condición | acción | deadline | prueba exigida a B |
|---|---|---|---|
| fin normal del router | `record=0` selectivo (máscara `~0x8`) y cierre del stream | ≤250 ms | timestamp de escritura I2C + verificación física |
| pérdida de heartbeat del worker | `record=0` + fallback a BASE | ≤250 ms | fault injection con reloj monótono; medir p50/p99 |
| falta de PCM / underrun / sink caído | `record=0` y parar router | ≤250 ms | inyección de fallo |
| entrada en TX (PTT/ATU/modem/swrscan) | `record=0` **antes** de que el bit de TX viaje; restaurar al volver a RX | atómico con el lock | test que fuerce PTT mientras el router está activo |
| arranque de la GUI | `record=0` (ya lo hace `radio.c:782`), sin depender de la sombra | en init | test nativo |
| crash/cierre de la GUI | guardián **dentro del proceso** (hoy no existe; el supervisor tarda ≳4 s) | ≤250 ms | prueba de crash real; si no se logra, BLOCKED |
| shutdown/apagado | `record=0` antes del power-off | — | test |

- **Máscara:** `next = prev | 0x8` / `prev & ~0x8`; **nunca** tocar otros bits (`iptt`, ATU, etc.). En A se
  verificó la aritmética con **2009 casos** (fijos + aleatorios, incluido PTT prendido): PTT intacto
  (`logs/mask-test.txt`). Eso prueba la máscara, **no** el restore físico.
- **Lectura de estado:** sin readback real, el contrato exige que B implemente/consiga una fuente de
  verdad (p.ej. leer el bloque donde el BASE refleje el registro, o un reporte de estado del MCU) **o**
  que demuestre con instrumentación que el restore ocurre. Declarar "restaurado" a partir de la sombra RAM
  está prohibido (es el error exacto que A encontró).

## 3. Tests mínimos para que B pueda decir `CODE_READY` (host + banco + radio)

1. Host nativo: tests del owner/lock, máscara, debounce, interlock TX, restore por heartbeat (con stubs
   que registren orden/timestamps de las escrituras, como `tests/native/radio_stubs.c`).
2. Inyección de fallos: heartbeat perdido, stream caído, underrun, GUI "muerta" — verificar que la última
   escritura del owner deja `record=0` y ningún otro bit cambiado.
3. Banco QEMU (sin radio): arranque/rollback del router, sin tocar PTT/mic; determinismo.
4. Radio (cuando A esté desbloqueado): **A4 completo** de `physical-signal.md` §6; si no hay instrumento,
   `PHYSICAL_OUTPUT_NOT_VERIFIED` y no hay `CODE_READY` para hardware.
5. Observabilidad: la GUI/daemon debe loguear cada transición del owner (`record=1/0`, motivo, timestamp,
   deadline) y la recuperación; sin eso no se puede auditar el restore.

## 4. Bloqueos conocidos que B hereda (si arranca)

- **Sin readback del reg 12** ⇒ no se puede "verificar restore" desde software: hace falta instrumentación
  física o implementar una fuente de readback (trabajo nuevo, riesgo de tocar el protocolo del MCU).
- **Supervisor `x6200_daemon` ≳4 s** ⇒ no cumple ≤250 ms; el guardián debe vivir dentro del proceso.
- **`audio_play_en()` no es neutral** (cambia HMic/IMic) y no debe usarse como vía de RX.
- **El firmware del MCU está encriptado** ⇒ su comportamiento en TX/crash no se puede deducir; cualquier
  afirmación de "el BASE restaura solo" queda **sin fundar** hasta medirla.

**Conclusión del handoff:** con el estado actual del registro compartido, el único camino honesto es
`BLOCKED_SAFE_MUX`. La fase B sólo puede arrancar si alguien demuestra primero readback real + owner
serializado + restore ≤250 ms medido — y eso es trabajo de una nueva fase A (o un rediseño explícito del
owner dentro de la GUI con evidencia física), no de #364.
