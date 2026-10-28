# Handoff físico — qué requiere presencia del operador

- **Issue**: [iariap/x6200_gui#347](https://github.com/iariap/x6200_gui/issues/347), gate G1 (ruta
  audible) y G3 (inteligibilidad con voz real). Extiende el handoff del #345
  (`docs/evidencia/2026-10-28-rx-audio-feasibility/hardware-handoff.md`), que sigue vigente.
- **Estado**: **PENDING-OPERATOR** para G1 y G3. En esta corrida el agente **no** cambió VFO, modo,
  volumen, mute, NR/NB/DNF, ruteo, ajustes ni BT; tampoco transmitió, ni flasheó, ni cargó módulos.
- **Base**: `8b0d5de9` (= `origin/main` real al arrancar; ver README §1). Worktree
  `x6200-rx-nr-hardware-20261028`, rama `audit/rx-nr-hardware-20261028`.

## 1. Lo que el agente sí pudo verificar (sin operador)

| Ítem | Estado | Evidencia |
|---|---|---|
| La GUI corre en la radio y la captura de RX está activa | `MEASURED-HARDWARE` | `logs/radio-preflight.txt`: `x6200_gui` pid 23711, sha256 del binario; `pactl` source `alsa_input.platform-sound.stereo-fallback` en `RUNNING`; `/proc/asound/card0/pcm0c/sub0/status` en `RUNNING` |
| El *source* de PulseAudio entrega muestras (no es un source muerto) | `MEASURED-HARDWARE` | `logs/g1-parec-capture.txt`: `parec` pasivo de 2 s del source, RMS/pico medidos **sin publicar el audio** |
| El sink del SoC está armado | `MEASURED-HARDWARE` | `pactl list sinks`: `RUNNING`, `Mute: no`, volumen PA 83 % (−5,00 dB), **puerto activo `analog-output-headphones`**; `logs/g1-pactl-readback.txt` |
| No hay ruta de audio RX procesado al parlante en el software | `CODE` | `src/audio.c:57-70` sólo alimenta `screen_rec` y `dsp`; el único `pa_stream_write` es `audio_play()` (`src/audio.c:281`), cuyos llamadores son audio generado por la GUI |
| El loopback A2DP **no** está activo en esta corrida | `MEASURED-HARDWARE` | `pactl list short modules` sin `module-loopback`; `pactl list short sinks` sin `bluez_sink`; no se conectó ni desconectó BT |
| Estado RX estable durante todo el trabajo | `MEASURED-HARDWARE` | snapshots GET read-only antes/después idénticos salvo `db` (nivel de señal): `logs/protocol-60s/get-before.json` vs `get-after.json` |

## 2. Lo que **sólo** el operador puede cerrar

| # | Pregunta | Por qué hace falta el operador | Qué registrar |
|---|---|---|---|
| H1 | ¿Qué salida suena hoy (parlante del equipo, jack, auriculares A2DP)? | requiere escucha humana en la misma salida física | salida usada, volumen mostrado por la UI, confirmación de audio presente |
| H2 | ¿El audio procesado por la GUI llega al parlante? | **no existe ruta**; además hay que demostrarla con una prueba física no invasiva | esquema o medición en la salida del DSP del BASE y en el parlante |
| H3 | ¿El loopback A2DP entrega RX sin el procesamiento de la GUI? | requiere auriculares A2DP reales y dos capturas simultáneas | monitor del sink A2DP vs copia de captura, con preset de NR fuerte |
| H4 | ¿Se oye el click del borde de callback (2216 LSB)? | escucha + conteo | clicks/30 s con voz y con tono |
| H5 | ¿El preset de NR de la GUI cambia la inteligibilidad audible? | A/B con voz SSB/AM real y la misma cadena física | valoración 1–5, palabras/indicativo perdidos, naturalidad |
| H6 | ¿Cuánto CPU consume la GUI con y sin la cadena? | ya hay lectura pasiva de `/proc` en esta corrida; falta la comparación con la ruta audible implementada | `/proc/<pid>/stat` antes/después con la ruta nueva |

## 3. Material de voz (G3)

- **No hay** grabaciones de voz HF con licencia/consentimiento aportadas por el operador en este
  entorno. El único archivo de voz versionado (`docs/evidencia/2026-10-15-banco-audio/captura-voz.wav`)
  es una **prueba de audio sintética en QEMU** sobre un `module-null-sink` del banco, **no** una señal
  HF real, y QEMU no representa el códec ni el audio del BASE: no sirve para G3.
- **G3 queda `PENDING_VOICE`.** Para cerrarlo hace falta: (1) grabaciones de la propia voz del operador
  (SSB/AM) o material con derecho de uso y consentimiento, con formato y sha256 inventariados;
  (2) un A/B ciego original/Speex/RNNoise en dos pasadas con orden alternado; (3) valoración del
  operador (palabras/indicativo correctos, naturalidad, sonidos omitidos). No se genera voz sintética
  para afirmar inteligibilidad.

## 4. Indicación del operador recibida durante la sesión

- A las ~20:00Z del 28-10-2026 el operador indicó, por el canal de trabajo, que la radio tenía audio
  disponible en 40 m. No se cita el texto de la conversación (regla del issue).
- Entre el primer snapshot del agente (19:52Z: 14.074 MHz, USB-D, app FT8, `vol=0`) y el snapshot del
  comienzo de la corrida (19:59Z: **7.094 MHz, LSB-D, banda 40m SSB, `vol=1`, app ninguna**) el estado
  de la radio **cambió por acción externa al agente** (el operador sintonizó). **El agente no ejecutó
  ningún comando de retuneo, modo, volumen ni mute**: está prohibido por el issue y por las reglas de
  esta sesión, y no hay en `commands.log` ninguna escritura de ese tipo.
- El ofrecimiento **no se usó** para G1: (a) retunear habría sido una escritura prohibida; (b) aunque
  el operador escuche audio del BASE, eso no demuestra la ruta **RX → procesador → parlante**, que no
  existe en el software; (c) no hubo A/B ni registro de escucha. Queda como `PENDING-OPERATOR` para
  la fase física, no como evidencia `MEASURED-HARDWARE`.
- Al terminar este handoff la radio queda **exactamente en el estado en que la dejó el operador**
  (7.094 MHz LSB-D, sin app): `vol=1` al inicio de la corrida y `vol=0` en el snapshot final de las 20:10Z
  (el operador cambió el volumen; ver README §2.1). El agente no escribió ese valor.

## 5. Checklist de seguridad (repetir antes de cada pasada física)

- [ ] **No TX**, PTT no se toca.
- [ ] **No** se cambia modo/VFO/volumen/mute/NR/NB/DNF/bandas/BT desde el agente.
- [ ] **No** se llama `audio_play_en()` ni `x6200_control_record_set()`.
- [ ] **No** se cargan/descargan módulos de PulseAudio ni loopbacks.
- [ ] **No** se flashea ni se recompila la ROM.
- [ ] Un solo control por pasada, misma salida física, mismo volumen, mismo filtro.
- [ ] Si algo queda mudo o raro: **detener** y reportar; no «arreglar» escribiendo registros.
- [ ] Al terminar, dejar el equipo como estaba y anotarlo.

## 6. Criterio de cierre

G1 pasa a `PASS` sólo si existe una ruta de **audio RX procesado** al parlante/jack/A2DP **demostrada
en hardware** sin `record_set` ni loopback inseguro y sin cambiar BASE/RX/TX, con esquema/prueba
física documentada. G3 pasa a `PASS` sólo con el A/B ciego con voz autorizada. Hasta entonces el
veredicto global es **NO_GO_LIVE** (ver README §5).
