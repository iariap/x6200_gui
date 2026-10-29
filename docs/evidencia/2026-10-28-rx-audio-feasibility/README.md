# Factibilidad de una mejora de audio de RX: ruta audible, NR del BASE y SpeexDSP

- **Issue**: [iariap/x6200_gui#345](https://github.com/iariap/x6200_gui/issues/345) — tarea **C** de 3:
  informe de factibilidad, handoff de hardware y corrida de los tests obligatorios.
- **Base**: `a1c667eee6d4c138416e5ec459dd86de93cb630f` (= `origin/main` al arrancar la tarea A).
- **Rama**: `factory/rx-audio-feasibility` · **Worktree**: `state/worktrees/x6200/rx-audio-feasibility`
  (el issue proponía `audit/rx-audio-feasibility-20261028` / `../x6200-rx-audio-feasibility-20261028`;
  el harness de esta corrida usa rama y worktree propios, mismo base y mismo destino `main`).
- **Commits de la unidad**: `3ab3330a` (tarea A: mapa de señal + matriz de controles),
  `a6d7ac2f` (tarea B: auditoría numérica + banco PCM), `97617351` (tarea C: este informe) y el commit
  de la tarea D (anexo vinculante RNNoise: laboratorio A/B offline, ver `rnnoise-vs-speex.md`).
- **Qué es**: el informe que responde, con evidencia clasificada, las cinco preguntas del issue, los
  riesgos **medidos** de la cadena actual, el punto donde *podría* insertarse audio procesado (marcado
  `HYPOTHESIS`, sin diseñar nada) y la secuencia de QA físico que **no** se ejecutó.
- **Qué NO es**: no es una implementación, ni una propuesta de routing/UI/presets/DSP, ni una medición
  de hardware. **No se tocó `src/`, `rootfs/`, `tests/` ni la radio.** Ninguna señal usada es voz: los
  cuatro WAV son silencio, un seno, una mezcla de tres tonos y ruido blanco. Nada de esto mide
  inteligibilidad. RNNoise se compiló **sólo** para el banco offline: no entra en la ROM ni en el
  runtime (`rnnoise-manifest.json`, y el control `check_no_stub_claims.py` (e) lo verifica).

## 0. Resumen: una línea por pregunta

| # | Pregunta del issue | Respuesta en una línea | Clase dominante |
|---|---|---|---|
| 1 | Ruta real al parlante/auriculares vs captura; ¿hay retorno seguro de RX procesado por Linux? | La GUI procesa **sólo su copia de captura** (`src/audio.c:63`), esa copia va a la grabadora y al DSP (`:66`, `:68`) y **no** al parlante; el parlante lo alimenta el BASE (sin esquema) y **no hay hoy ninguna ruta de retorno probada** de audio procesado hacia él. | `CODE` + `HYPOTHESIS` |
| 2 | Qué controles del BASE ya actúan sobre el audio audible y quién los manda | 26 controles inventariados (NR, NR level, NB, NB level/width, DNF, filtros RX, RX EQ ×2 juegos, más `rxvol`/`record`/`spmode`), todos con observer → registro I2C; el **efecto audible** de cada uno sigue `UNVERIFIED` y el rango semántico del BASE es `UNKNOWN`. | `CODE` + `UNVERIFIED`/`UNKNOWN` |
| 3 | Defectos medibles de `audio_enhancement.c` y qué demuestran sus tests | Cuatro defectos medidos: recorte de 58 muestras por callback, retardo de un frame (5.805 ms) sin compensar, `level` invertido con techo inalcanzable (−30 pedidos ≈ −13.5 dB), y salto de 2216 LSB en el borde; los tests con stub **no** prueban atenuación, calidad, CPU ni latencia: sólo troceado, tail y clamps. | `MEASURED-HOST` + `CODE` |
| 4 | Cuánto cuestan y cómo se comportan SpeexDSP REAL y RNNoise upstream REAL sobre idénticos PCM (resampling, calidad, memoria, CPU, discontinuidades) | Medido en host: RNNoise **1,95 % del frame** (p50 0,195 ms, p99 0,271 ms) contra Speex **0,013 ms** y resampling **0,036 ms**; RNNoise atenúa **32,6 dB** el ruido y **54,8 dB** un tono puro; **0 clipping** y **0 tramas > 10 ms** en 21 casos. No hay voz: nada de esto es inteligibilidad. | `MEASURED-HOST` + `SKIPPED` |
| 5 | Presupuesto de CPU/latencia y muestras reales que faltan para un A/B con escucha | Latencia algorítmica medida: RNNoise **21,38 ms** (2 frames de 20 ms + 1,38 ms de resampling) y Speex **5,805 ms**; el CPU del R16 sigue `BLOCKED`; faltan **muestras de voz reales** y el vector voz + ruido está `SKIPPED` con causa (no hay audio con licencia/consentimiento). | `MEASURED-HOST` + `BLOCKED`/`SKIPPED` |

**Veredicto de esta corrida**: hay evidencia suficiente para descartar varias creencias (el audio
procesado no llega al parlante; el `level` está invertido; el stub no prueba nada acústico) y **no** hay
evidencia suficiente para afirmar que exista una ruta audible segura ni que un NR nuevo mejore la
inteligibilidad. La decisión de arquitectura/DSP/UX queda en el coordinador, como pide el issue.

**Dictamen del anexo RNNoise (tanda D)**: el laboratorio offline es sólido y reproducible, pero con
`MEASURED-HARDWARE` = 0 filas y sin ruta audible demostrada el resultado **live es NO-GO por el gate 1**
del anexo. El experimento se reporta como **`RESEARCH/PROTOTYPE OFFLINE`**, nunca como feature. Detalle,
tablas y artefactos en `rnnoise-vs-speex.md`.

## 1. Contenido de este directorio

| Archivo | Qué contiene | Tarea |
|---|---|---|
| `README.md` | este informe: matriz de evidencia, respuestas, riesgos, punto de inserción, tests | C |
| `signal-flow.md` | mapa de señal por edge (E1–E15), productores/consumidores, ciclo de vida de `SpeexPreprocessState` | A |
| `base-controls.md` | matriz de 26 controles del BASE (fuente, default, persistencia, estado OFF, CW/digital) | A |
| `speex-audit.md` | aritmética del recorte, latencia declarada vs real, mapa `level → supresión` contra lib real, qué prueban los tests | B |
| `hardware-handoff.md` | secuencia de QA **RX-only, no ejecutada** (`PENDING-HARDWARE`) | C |
| `measurements.csv` | 68 filas: 11 `CODE`, 50 `MEASURED-HOST` (29 de B + 21 de D), 6 `BLOCKED`, 1 `SKIPPED`; columnas exactas del contrato 6 | B–D |
| `bench-results.json` | métricas crudas del banco (17 casos × 3 corridas + 12 sondas) | B |
| `pcm-manifest.json` | sha256, RMS, peak, clipping y duración de los 4 WAV sintéticos | B |
| `rnnoise-vs-speex.md` | banco A/B offline RNNoise vs SpeexDSP vs OFF: procedencia, protocolo, tablas, artefactos, dictamen GO/NO-GO | D |
| `rnnoise-vs-speex.csv` | 21 filas (4 vectores × 5 métodos + sonda de latencia) con `audio_destination = OFFLINE_CAPTURE_ONLY` | D |
| `rnnoise-bench.json` | métricas crudas por caso, controles completos (bit-exacto, repetibilidad, prefijo, fallback) y toolchain | D |
| `rnnoise-manifest.json` | commit/licencia/URL/sha256 del modelo de RNNoise, build y flags de no redistribución | D |
| `audio/` | 6 WAV de 10 s (`multisine` y `noise` × `off`/`speex`/`rnnoise`) + `sha256sums.txt` | D |
| `scripts/bench_rnnoise.py`, `scripts/bench/bench_rnnoise.c`, `scripts/bench/rnnoise_build.sh`, `scripts/bench/Dockerfile.rnnoise` | banco offline con RNNoise upstream pinneado, resampler propio y build en contenedor | D |
| `commands.log` | comandos exactos, salidas y exit codes de las cuatro tareas | A–D |
| `scripts/gen_pcm.py` | generador determinista de PCM (stdlib, semilla 6200) + `--verify` | B |
| `scripts/bench_speex.py`, `scripts/bench/` | banco con **libspeexdsp real** en contenedor pinneado (Dockerfile, `bench_main.c`, stubs de periferia) | B |
| `scripts/check_no_stub_claims.py` | control negativo: un resultado de stub no puede reportarse como real | C |
| `scripts/tamper_fixture.txt` | fixture con la frase prohibida que el control tiene que rechazar (no se barre: es `.txt`) | C |

`artifacts/` (WAV y `.raw`) está gitignoreado: se regenera y su integridad queda anclada por sha256 en
`pcm-manifest.json` y `measurements.csv` (`speex-audit.md` §7.1).

## 2. Clases de evidencia

| Clase | Significado acá | Dónde vive |
|---|---|---|
| `CODE` | Leído en el árbol de esta base (`archivo:línea`), verificable con `grep`/`sed`. | `signal-flow.md`, `base-controls.md`, `speex-audit.md` |
| `ARITH` | Derivación aritmética sobre constantes de `CODE`; se muestra la cuenta. | `speex-audit.md` §1–§2, filas `arith_*` del CSV |
| `MEASURED-HOST` | Medido en este host x86_64 en contenedor con **libspeexdsp real** (tarea B) y **RNNoise upstream real** (tarea D). **No es la radio.** | 50 filas del CSV, `bench-results.json`, `rnnoise-bench.json` |
| `MEASURED-HARDWARE` | Medición hecha en la radio **en esta corrida**. | **Ninguna**: 0 filas. Ver §7.1 |
| `BLOCKED` | Se quiso medir y no se pudo; se dice la causa. | 6 filas del CSV (3 de B + 3 de D) |
| `SKIPPED` | El anexo condiciona la medición a un insumo que no existe; se dice la causa y no se inventa el insumo. | 1 fila: `rxd_voice_noise_ab` (voz + ruido) |
| `HYPOTHESIS` | Inferencia sin medición que la cierre. Nunca se presenta como hecho. | prosa de los cuatro documentos |

`MEASURED-HARDWARE` **está vacío en esta corrida**: no se encendió la radio, no se transmitió y no se
flasheó nada. Las mediciones de radio que aparecen citadas son **registros previos** de otras corridas
(`docs/HALLAZGOS.md:155-168`, `src/audio_enhancement.c:325`, `docs/evidencia/2026-10-22-cwaudio244/README.md:80-86`),
y se marcan como tales, no como evidencia de este informe.

## 3. Matriz de evidencia por pregunta

`✓` = hay evidencia de esa clase para esa pregunta (con la referencia) · `—` = no la hay.

| Pregunta / afirmación | `CODE` | `MEASURED-HOST` | `MEASURED-HARDWARE` | `BLOCKED` | `HYPOTHESIS` |
|---|---|---|---|---|---|
| **P1** la GUI procesa in situ y entrega a grabadora + DSP | ✓ `src/audio.c:63,66,68`; `signal-flow.md` §1 | — | — | — | — |
| **P1** el RX procesado **no** se escribe en el stream de reproducción | ✓ `src/audio.c:281` (único `pa_stream_write`), llamadores E10a-c | — | — | — | — |
| **P1** qué alimenta el parlante del operador | — | — | — | ✓ sin esquema ni medición | ✓ el DSP del BASE (`docs/HALLAZGOS.md:937`) |
| **P1** el códec del SoC llega al parlante | ✓ cadena configurada (`docs/HALLAZGOS.md:155-168`) | — | — | ✓ `speaker-test` bloqueado por placa tomada (`:167`) | ✓ evidencia contradictoria (`2026-10-22-cwaudio244/README.md:80-86`) |
| **P1** el loopback A2DP entrega RX **sin** el procesamiento de la GUI | ✓ `rootfs/usr/sbin/btaudio-ctl:23,108-109` (cliente distinto) | — | — | — | ✓ resultado audible |
| **P1** existe una ruta de retorno segura al parlante | ✓ no está escrita | — | — | ✓ no verificable sin radio | ✓ condiciones en §6 |
| **P2** cada control del BASE tiene observer → registro | ✓ `src/radio.c:669-690`, `:517-558`, `:634-635` | — | — | — | — |
| **P2** rangos semánticos que acepta el BASE | ✓ sólo tipo y ancho de bits | — | — | ✓ no se puede leer del equipo | ✓ recorte vs wrap |
| **P2** efecto **audible** de NR/NB/DNF/EQ/filtros | ✓ están en el camino del DSP del equipo | — | — | ✓ requiere escucha en radio | — |
| **P2** Speex NR es independiente del NR del BASE | ✓ `audio_enhancement.c` no llama a `x6200_control_*` (0 ocurrencias) | — | — | — | — |
| **P3** recorte por frames: 58/4410 sin NR | ✓ `src/audio_enhancement.c:293-317,354-355` | ✓ filas `arith_*`, `noise_p2_b4410` | — | — | — |
| **P3** retardo de un frame (5.805 ms) sin compensar | ✓ `audio_enh_process()` no desplaza | ✓ lag 256 muestras (`noise_p2_b4410`) | — | — | ✓ audibilidad |
| **P3** `level` invertido y techo inalcanzable | ✓ `:151-158` | ✓ `probe_level_0…100` (−13.51…−8.53 dB) | — | — | — |
| **P3** salto en el borde de cada callback | ✓ tail sin retardo | ✓ 2216 LSB vs slew 588 (`sine1k_p2_b4410`) | — | — | ✓ audibilidad |
| **P3** los tests con stub **no** prueban calidad/CPU/latencia (sólo troceado/tail/clamps) | ✓ el stub atenúa 6 dB fijos y usa ids falsos (`tests/native/stubs/stubs.c:107-109`, `stubs/speex/speex_preprocess.h:15-16`; ids: registro del 2026-10-28, ver errata de #351 en §4.3) | ✓ suite 44837 checks, 0 fallos | — | — | — |
| **P3** carrera `speex_preprocess_ctl` vs `run` | ✓ no hay mutex en el archivo | — | — | ✓ sin TSan/helgrind sobre binario real | ✓ acceso concurrente posible |
| **P4** CPU del host por 60 s de audio | — | ✓ 104–137 ms (0.17–0.23 %), máx. 1.1–2.1 ms/callback | — | — | — |
| **P4** CPU en la radio (R16) | — | — | — | ✓ otra arquitectura y carga (fila `cpu_on_radio`) | — |
| **P4** latencia extremo a extremo RX→parlante / RX→apps | — | — | — | ✓ fila `latency_end_to_end_rx_to_speaker` | — |
| **P4** muestras reales de voz para A/B | — | — | — | ✓ fila `audible_intelligibility`; no hay voz en el repo | — |

## 4. Respuesta a las 4 preguntas

### 4.1 Pregunta 1 — ¿qué ruta alimenta el parlante y la captura? ¿hay retorno seguro?

**Qué alimenta la captura (hecho, `CODE`).** Un único punto de entrada:
`read_callback()` (`src/audio.c:57-70`) toma el *source*
`alsa_input.platform-sound.stereo-fallback` (`src/audio.c:40`), mono S16NE 44100 (`:204-207`, `:225`),
con fragmentos de 100 ms (`:29`, `:226`). Aplica `audio_enh_process()` **in situ** (`:63`) y entrega el
**mismo** buffer a `screen_rec_put_audio_samples()` (`:66`) y a `dsp_put_audio_samples()` (`:68`). No hay
una tercera llamada (`signal-flow.md` §1, tabla E1–E15).

**Qué alimenta el parlante (código + hipótesis).** En este árbol **no hay** transferencia del RX
capturado al stream de reproducción: el único escritor de `play_stm` es `pa_stream_write()` dentro de
`audio_play()` (`src/audio.c:281`), y sus llamadores son audio **generado** por la GUI (TTS, FT8/JS8,
mensajes, recorder, `tx_player`; `signal-flow.md` E10a–E10d). La única ruta de RX hacia otra salida es
el `module-loopback` de PulseAudio que carga `btaudio-ctl` (`rootfs/usr/sbin/btaudio-ctl:23,108-109`),
un cliente **distinto** del mismo *source*: no ve el procesamiento de la GUI (`HYPOTHESIS` sobre el
resultado audible, no medido). Que el parlante del equipo lo alimente el DSP del BASE es `HYPOTHESIS`
sin esquema ni medición (`docs/HALLAZGOS.md:937`); el códec del SoC está configurado y **no se midió
sonido** (`docs/HALLAZGOS.md:155-168`, con `speaker-test` bloqueado por placa tomada en `:167`).

**¿Existe una ruta de retorno segura de RX procesado por Linux?** **No está probada, y no está
escrita.** Las tres condiciones que faltan están en §6 y ninguna es una opinión: (a) `play_stm` no
llega al parlante sin el ruteo que escribe `audio_play_en()` en registros del BASE (`src/audio.c:370-389`),
prohibido por el issue (#244/#292/#341); (b) `read_callback()` corre en el hilo del *threaded mainloop*
y `audio_play()` toma ese mismo lock (`src/audio.c:256-267,280-282`), así que llamarlo desde el callback
re-entra el lock (`HYPOTHESIS`: deadlock/aserción; `BLOCKED` para verificarlo sin libpulse ni radio);
(c) no hay medición de qué sink suena.

### 4.2 Pregunta 2 — ¿qué controles ya actúan sobre el audio audible y quién los manda?

`base-controls.md` inventaría **26 filas**: 8 de DSP del BASE (NR on/off, NR level, NB on/off, NB level,
NB width, DNF modo/center/width), 3 de filtro RX (borde bajo, borde alto, par), 12 de RX EQ (juego
general y juego WFM) y 3 adyacentes (`rxvol`, `record`, `spmode`). Todos siguen el mismo camino: subject
en `cfg`/`params` → observer de `radio_init` (`src/radio.c:669-690`) → `x6200_control_*` → registro I2C.
Defaults en `src/cfg/cfg.c` y persistencia en `/mnt/params.db` (`src/cfg/params.c:23-28`).

Lo que **está verificado por código**: la conexión perilla → registro, el tipo de cada parámetro y el
ancho de campo del SDK. Lo que **no** está verificado: el rango semántico que acepta el BASE (`UNKNOWN`
en cada fila, §6 de `base-controls.md`) y el **efecto audible** de cada control (`UNVERIFIED`; requiere
escucha en la radio). El estado OFF de los niveles **no existe**: los observers de `nr_level`,
`nb_level`, `nb_width`, `dnf_*` y `rx_eq_*` escriben el registro aunque el control esté apagado
(`src/radio.c:670-676,678-690`).

Separación que el issue pide no conflactar: **NR del BASE** (registro 23, dentro del equipo) ≠
**Audio enhance** (cadena en la GUI sobre la copia de captura) ≠ **Speex NR** (motor del preset de
*Audio enhance*). `src/audio_enhancement.c` no llama a ninguna `x6200_control_*` (0 ocurrencias) y el
camino del NR del BASE no conoce `g_audio_enh` (`base-controls.md` §7).

### 4.3 Pregunta 3 — defectos medibles de `audio_enhancement.c` y qué demuestran sus tests

| Defecto | Valor medido / derivado | Evidencia |
|---|---|---|
| Recorte por frames: el tail no entra al NR | 58 de 4410 muestras por callback (1.3152 %); 116 de 8820; con callbacks de 1 muestra el NR **nunca** corre | `src/audio_enhancement.c:293-317`, `:354-355`; filas `arith_44100_4410`, `noise_p2_b4410` |
| Tasa declarada ≠ tasa real | 256/48000 = 5.333 ms declarados vs 256/44100 = **5.805 ms** reales (+8.84 %); el bin de la FFT también se corre 8.84 % | `src/audio_enhancement.c:26`, `src/audio_enhancement.h:21-22`; `speex-audit.md` §2.2 |
| Retardo de un frame, sin compensar | lag medido **256 muestras** con ruido (control OFF: lag 0 y salida bit-exacta); con señales periódicas `UNKNOWN` por ambigüedad | fila `noise_p2_b4410`; `speex-audit.md` §2.3 |
| `level` invertido y techo inalcanzable | level 0 → −30 dB pedidos, **−13.51 dB** medidos; level 100 → −10 pedidos, **−8.53 dB** medidos | `src/audio_enhancement.c:151-158`; filas `probe_level_0`…`probe_level_100` |
| Discontinuidad en el borde de cada callback | seno 1 kHz: salto de **2216 LSB** en la muestra 4410 con slew natural de 588 LSB (3.8×); en ruido el salto (1932) queda por debajo del slew natural (3587) | fila `sine1k_p2_b4410`; `speex-audit.md` §4.1 |
| Notch y nivelador | el notch no tiene llamador de producción (no se ejercita); el nivelador cambia el tail (AM_BROADCAST: 101/5800 muestras distintas) | `signal-flow.md` §4; `speex-audit.md` §1.3 |
| Sin sincronización entre hilos | 0 mutex/atómicos en el archivo; `ctl` desde LVGL y `run` desde el hilo de audio sobre el mismo estado | `signal-flow.md` §4.1 (`HYPOTHESIS` de carrera, no probada) |

**Qué demuestran los tests actuales y qué no.** `tests/native/test_audio_enhancement.c` corre contra
**stubs** de Speex/LVGL/liquid (`tests/native/run_native_tests.sh:4`). El stub atenúa 6 dB fijos
(`tests/native/stubs/stubs.c:107-109`) y su header define ids que **no** son los reales
(`SPEEX_PREPROCESS_SET_NOISE_SUPPRESS` = **1** en `tests/native/stubs/speex/speex_preprocess.h:16`
vs **18** en libspeexdsp 1.2.1). Un PASS prueba troceado, tail, clamps, presets y no-crash; **no**
prueba atenuación, calidad, CPU, latencia ni retardo (`speex-audit.md` §5). *(Ids del stub: registro del
2026-10-28; ver la errata de #351 abajo.)*

> **Errata 2026-10-29 (#351).** Las menciones al id de `SET_NOISE_SUPPRESS` de este informe (§3, §4.3,
> R4 en §5, T4 en §8 y la tabla de §9) son el **registro histórico del 2026-10-28** y se conservan tal
> cual. Después de este informe, #351 alineó `tests/native/stubs/speex/speex_preprocess.h` con el ABI
> real de libspeexdsp 1.2.1: el header del stub ahora declara `SPEEX_PREPROCESS_SET_NOISE_SUPPRESS` =
> **18** (antes 1). El control `scripts/check_no_stub_claims.py` punto (c) dejó de verificar la
> discrepancia "stub = 1 / real = 18" y ahora verifica la **congruencia** "stub = real = 18": falla si
> el header del stub declara cualquier otro valor, y su `--self-test` incluye el caso "id del stub
> cambiado ⇒ FAIL". Sigue vigente lo demás observado en §4.3: la atenuación fija de 6 dB del stub, los
> parámetros sin tipar y que un PASS de la suite host no sustituye una corrida contra la lib real.
> Detalle en `speex-audit.md` §5.

### 4.4 Pregunta 4 — presupuesto de CPU/latencia y muestras que faltan para A/B

**CPU (host, `MEASURED-HOST`)**: 60 s de audio procesados en 104–137 ms (0.17–0.23 % del tiempo real),
con máximo de 1.1–2.1 ms por callback de 100 ms (incluye posible preempción). Presets OFF: 0.8–2.4 ms
totales. **No extrapolable al R16**: otra arquitectura, otra carga, otra toolchain (fila `cpu_on_radio`
= `BLOCKED`). La versión de libspeexdsp que corre en la ROM no se verificó (`UNVERIFIED`).

**Latencia**: el retardo del NR es 5.805 ms reales y ocurre **dentro** del callback, así que no suma
5.8 ms al stream; lo que sí hace es correr la señal procesada un frame respecto del tail (§5, R2). La
latencia extremo a extremo RX→parlante y RX→apps es `BLOCKED`: incluye PulseAudio, el DSP del BASE y el
trayecto eléctrico.

**Muestras que faltan**: hoy sólo existen cuatro señales sintéticas (silencio, seno 1 kHz, multisine
300/1000/2400 Hz, ruido blanco; `pcm-manifest.json`). Para una comparación A/B con escucha falta voz
SSB y AM **real** —grabada por el operador, no QSO de terceros—, ruido constante, QRM cercano e
impulsos, más la medición en radio (`hardware-handoff.md`). Tonos y ruido prueban integridad de
procesamiento, **no** inteligibilidad (fila `audible_intelligibility` = `BLOCKED`).

## 5. Riesgos medidos de la cadena actual

Ninguno de estos riesgos se declara audible: la audibilidad es `HYPOTHESIS` hasta que haya voz y
escucha (§7).

| # | Riesgo | Valor medido | Consecuencia | Clase |
|---|---|---|---|---|
| R1 | **Retardo de un frame sin compensar**: la salida del NR está corrida 256 muestras (5.805 ms) y `audio_enh_process()` no desplaza ni descarta nada | lag 256 en `noise_p2_b4410`; control OFF lag 0 y bit-exacto | un retardo constante en la ruta de captura no se percibe como eco, pero **sí** corre la señal respecto del tail (R2) | `MEASURED-HOST` + `CODE` |
| R2 | **Seam del tail sin retrasar**: el tail (58 muestras por callback de 4410) sale del stream interno de Speex y **no** está retrasado, mientras el resto sí | seno 1 kHz: salto de 2216 LSB en la muestra 4410 (slew natural 588); multisine: 1963 (slew 715); ruido: 1932 (slew 3587) | con señales tonales el borde introduce un salto mayor que cualquier salto natural; con ruido no se distingue. Audibilidad `HYPOTHESIS` | `MEASURED-HOST` |
| R3 | **`level` invertido**: 0 = máxima supresión, 100 = mínima; y el techo configurado no se alcanza | `-30 + level*20/100` (división entera, 21 valores distintos); medido: −13.51 dB (level 0) a −8.53 dB (level 100). `SSB_VOICE` usa 40 y `AM_BROADCAST` 60 (`src/audio_enhancement.c:188-199`) | la etiqueta «level» sugiere lo contrario de lo que hace; "más NR" hoy es "menos supresión" | `CODE` + `MEASURED-HOST` |
| R4 | **El stub miente en los ids y en la atenuación**: `SET_NOISE_SUPPRESS` = 1 (real 18) y atenúa 6 dB fijos (ids: registro del 2026-10-28, ver errata de #351 en §4.3) | PASS de 44837 comprobaciones, 0 fallos | cualquier frase "NR verificado" basada en ese PASS es un no sequitur; un id equivocado no se detectaría | `CODE` + `MEASURED-HOST` |
| R5 | **Tasa declarada ≠ real** | 48000 declarado vs 44100 real (+8.84 %) | sobre ruido blanco la atenuación difiere 0.027 dB (−13.854 vs −13.881); el efecto sobre voz tonal/estimación de ruido **no** se midió | `MEASURED-HOST` (ruido) + `HYPOTHESIS` (voz) |
| R6 | **Polaridad**: pasar `+30` no da ganancia | `SET +30` → readback −30, atenuación −14.94 dB (idéntica a −30); `SET 0` → −0.004 dB | la convención negativa del código es correcta; el signo no es un bug | `MEASURED-HOST` |

## 6. Punto exacto de inserción de audio procesado — `HYPOTHESIS`, no es un diseño

Esta sección **describe** dónde hay (o no hay) muestras procesadas y qué condiciones faltan. **No**
propone arquitectura, no elige una, no agrega streams, ni UI, ni presets. La decisión es del
coordinador y necesita el handoff físico (`hardware-handoff.md`).

**Dónde existen hoy muestras procesadas.** En un solo lugar: el buffer de `read_callback()` después de
`audio_enh_process(buf, nbytes/2)` (`src/audio.c:63`), antes de sus dos consumidores (`:66`, `:68`). Ese
buffer es la copia del *stream* `capture_stm`; se reescribe in situ y se descarta con `pa_stream_drop()`
(`:69`). No hay otra copia procesada en el proceso.

**Los dos únicos puntos candidatos que el código permite describir** (`HYPOTHESIS`):

1. **Dentro de `read_callback()`, antes de los consumidores** (`src/audio.c:63-68`): la señal ya está
   procesada y en formato del stream. Para que suene habría que entregarla al stream de reproducción,
   cuyo único escritor es `audio_play()` (`:281`).
2. **Un stream nuevo alimentado con una copia del buffer**, consumido fuera del hilo del mainloop: evita
   re-entrar el lock desde el callback, pero sigue dependiendo de las condiciones (a) y (c) de abajo y
   agrega un buffer cuyo aporte de latencia es `UNKNOWN`.

**Condiciones que faltan (ninguna es opinión; cada una es un hecho ausente):**

| # | Condición | Por qué falta | Clase |
|---|---|---|---|
| C1 | Ruta de retorno al parlante **sin** tocar TX ni ruteo | `play_stm` es `alsa_output.platform-sound.stereo-fallback` (`src/audio.c:36-37,213-221`) y el ruteo del BASE lo mueve `audio_play_en()` escribiendo `hmic`/`imic`/`record` (`src/audio.c:370-389`), prohibido por el issue (#244/#292/#341). Que el sink suene sin ese ruteo es `UNVERIFIED` (E15) | `UNVERIFIED` |
| C2 | No re-entrar el lock del mainloop | `read_callback()` corre en el hilo del *threaded mainloop* (`src/audio.c:170,231`) y `audio_play()` toma `pa_threaded_mainloop_lock` (`:256-267,280-282`) | `HYPOTHESIS` (deadlock/aserción) + `BLOCKED` para verificarlo |
| C3 | No realimentar la captura | el *source* es el ADC del RX; escribir en el sink del SoC y volver a capturarlo depende del trayecto eléctrico, que no está documentado (E14/E15) | `HYPOTHESIS` |
| C4 | No duplicar el procesamiento | `g_audio_enh.float_buffer[256]` es único y no reentrante (`src/audio_enhancement.h:84`); un segundo consumidor no puede volver a llamar `audio_enh_process()` sobre el mismo estado | `CODE` (estructura) |
| C5 | Evidencia de hardware de qué sink suena | no hay medición en radio en esta corrida; `speaker-test` está bloqueado por placa tomada (`docs/HALLAZGOS.md:167`) | `BLOCKED` |

**Lo que este informe NO decide**: si el audio procesado debe ir al parlante, si conviene un stream
nuevo, si hay que compensar el frame, si el NR debe seguir en la copia de captura, ni qué pasa con
CW/digital. Nada de eso se puede decidir sin C1–C5 y sin voz real.

## 7. Qué falta para decidir

1. **Muestras reales de voz SSB y AM** grabadas por el operador (no QSO de terceros, sin datos
   privados), más ruido constante, QRM cercano e impulsos. Hoy sólo hay tonos y ruido.
2. **Medición en la radio**: qué sink/salida suena (C1/C5), CPU de la GUI (`/proc`), jitter, latencia,
   clicks, y si se preserva la voz y el identificativo.
3. **Comparación A/B con escucha** controlada: mismo VFO, volumen, filtro y salida física; NR OFF/ON y
   sus niveles. Secuencia completa en `hardware-handoff.md` (no ejecutada).
4. **Versión real de libspeexdsp en la ROM** (`UNVERIFIED`): el banco usó 1.2.1 de Debian; el R16 usa
   Buildroot y el paquete puede diferir.
5. **Decisión de producto** sobre la semántica de `level` (hoy invertida) y sobre si el NR debe existir
   en la copia de captura o en otra ruta. Es del coordinador, no de este informe.

## 8. Tests obligatorios: matriz real de ejecución

Corridos en este worktree el 2026-10-28; salidas completas y exit codes en `commands.log` §9.

| # | Test | Comando | Exit | Resultado real |
|---|---|---|---:|---|
| T1 | Suite nativa (host, con stubs) | `sh tests/native/run_native_tests.sh /tmp/opencode/x6200-native-taskC` | **0** | PASS: suite principal `44837 comprobaciones, 0 fallos`, más las suites por módulo (597, 57, …). Compilador `gcc 16.2.1`; **sin sanitizers** (no disponibles). No es medición de Speex: ver R4 |
| T2 | Suite completa rápida | `sh tests/run_all_tests.sh --quick` | **1** | **17 OK, 6 fallos**: `apps_menu`, `pruebas`, `wcag_rendered`, `widget_text`, `dialog_coverage`, `curve_evidence`. Fallos **preexistentes y ajenos** a este cambio (§9.2). `qemu_apps` SKIP (entorno privado faltante); `wspr`/`sstv`/`wefax` SKIP (la imagen no trae `fftw3.h`/`sndfile.h`); `js8`/`webserver` omitidos por `--quick` |
| T3 | Determinismo del PCM sintético | `python3 …/scripts/gen_pcm.py --verify` | **0** | OK: los cuatro WAV regenerados coinciden **byte a byte** con `pcm-manifest.json` (sha256 idénticos) |
| T4 | Control negativo anti-stub | `python3 …/scripts/check_no_stub_claims.py` | **0** | OK: 29 filas `MEASURED-HOST` del banco real, 0 `MEASURED-HARDWARE`, todos los campos del CSV y todas las líneas de los documentos barridos sin atribuciones al stub (conteo exacto en `commands.log` §9.4), stub=1 ≠ real=18, banco sin stub de Speex; ids del stub: registro del 2026-10-28, ver errata de #351 en §4.3 |
| T4b | Self-test del control negativo | `python3 …/scripts/check_no_stub_claims.py --self-test` | **0** | OK: las regex detectan las 5 formas prohibidas y dejan pasar las 4 correctas |
| T4c | Demostración de que el control **falla** con artefactos adulterados | `cp scripts/tamper_fixture.txt zz-tamper.md` y luego `check_no_stub_claims.py` | **1** | FAIL esperado en `zz-tamper.md:11` (la frase prohibida del fixture); al borrar el temporal vuelve a `0`. También se probó con una fila falsa `MEASURED-HOST` (mismo exit 1); artefactos restaurados y verificados con `cmp` |
| T5 | Alcance del diff | `git diff --name-only a1c667ee HEAD` | **0** | Sólo `docs/evidencia/2026-10-28-rx-audio-feasibility/**` (verificado con `grep -v` sobre esa ruta: sin coincidencias) |
| T6 | QEMU para audio de BASE | — | — | **`UNSUPPORTED`**: QEMU no simula el códec, el DSP del BASE ni el trayecto eléctrico, así que no puede validar audio de RX. **No** se reporta PASS ni FAIL. El barrido QEMU de apps existe (`tests/qemu/run_apps_qemu.sh`) pero en esta corrida salió SKIP por entorno |
| T7 | Banco A/B offline RNNoise vs SpeexDSP vs OFF | `python3 $EV/scripts/bench_rnnoise.py` | **0** | 21 filas medidas (4 vectores × 5 métodos + sonda), 3 warmups + 5 repeticiones de 60 s cada una; build de RNNoise en el commit pinneado con el modelo validado por `download_model.sh` |
| T8 | Control negativo extendido (tanda D) | `python3 $EV/scripts/check_no_stub_claims.py` | **0** | (a)–(f): 50 filas `MEASURED-HOST`, 0 `MEASURED-HARDWARE`, 0 atribuciones al stub (conteo exacto en commands.log §10), tabla comparativa toda `OFFLINE_CAPTURE_ONLY`, RNNoise fuera del repo/runtime, manifest con modelo pinneado |
| T9 | Controles del banco (bit-exacto / repetibilidad / prefijo / fallback) | dentro de T7 | **0** | `off` bit-exacto en los 4 vectores; hashes repetibles; prefijo de 10 s == pasada de 10 s; fallback sin RNNoise bit-exacto (3 variantes) |

**Nada de lo anterior es una medición de hardware.** T1/T2 prueban el árbol; T3/T4 prueban
reproducibilidad y disciplina de reporte; T6 no puede probar audio.

## 9. Afirmaciones fuertes: afirmación → clase → evidencia

| Afirmación | Clase | Evidencia |
|---|---|---|
| El audio que procesa la GUI no llega al parlante | `CODE` | `src/audio.c:63,66,68`; único `pa_stream_write` en `:281` |
| El parlante lo alimenta el BASE (no el SoC) | `HYPOTHESIS` | `docs/HALLAZGOS.md:937` (sin medición ni esquema) |
| El códec del SoC está configurado y no se midió sonido | `MEASURED-HARDWARE` (registro previo) | `docs/HALLAZGOS.md:155-168`, `:167` |
| El NR deja 58/4410 muestras sin procesar | `CODE` + `ARITH` + `MEASURED-HOST` | `src/audio_enhancement.c:293-317`; fila `noise_p2_b4410` |
| El NR retrasa la señal 256 muestras (5.805 ms) | `MEASURED-HOST` | fila `noise_p2_b4410` (lag 256); `speex-audit.md` §2.3 |
| El código no compensa ese retraso | `CODE` | `src/audio_enhancement.c:347-380` (no desplaza ni descarta) |
| `level` 0 es la máxima supresión y 100 la mínima | `MEASURED-HOST` | filas `probe_level_0` (−13.51 dB) y `probe_level_100` (−8.53 dB) |
| El techo de −30 dB configurado no se alcanza | `MEASURED-HOST` | `probe_level_0`: −30 pedidos, −13.51 medidos |
| Pasar `+30` no produce ganancia (la lib normaliza el signo) | `MEASURED-HOST` | `probe_polarity_+30`: readback −30, −14.94 dB |
| El stub usa `SET_NOISE_SUPPRESS` = 1 y la lib real 18 (histórico del 2026-10-28; ver errata de #351 en §4.3) | `CODE` | `tests/native/stubs/speex/speex_preprocess.h:16` vs `speex-audit.md` §5 |
| El PASS de la suite nativa no prueba calidad ni CPU de Speex | `CODE` | `tests/native/stubs/stubs.c:107-109`; `run_native_tests.sh:4` |
| El salto en el borde del callback es 2216 LSB con seno 1 kHz | `MEASURED-HOST` | fila `sine1k_p2_b4410`; `speex-audit.md` §4.1 |
| Que ese salto sea audible | `HYPOTHESIS` | no hay voz ni escucha |
| El CPU del host es 0.17–0.23 % del tiempo real | `MEASURED-HOST` | filas `*_b4410`; `bench-results.json` |
| El CPU de la radio es extrapolable del host | **falso** | fila `cpu_on_radio` = `BLOCKED` |
| Los tonos/ruido sintéticos prueban inteligibilidad | **falso** | fila `audible_intelligibility` = `BLOCKED` |
| Hay una carrera entre `ctl` y `run` | `HYPOTHESIS` | `signal-flow.md` §4.1; sin TSan/helgrind |
| Existe una ruta de retorno segura al parlante | `BLOCKED` | §6 C1–C5 |

## 10. Límites de este informe y errata

### 10.1 Límites

1. **`MEASURED-HARDWARE` está vacío**: no hubo radio en esta corrida. Toda cifra de CPU/tiempo es del
   host x86_64 con libspeexdsp 1.2.1 de Debian, no del R16.
2. **Nada es voz**: los WAV son sintéticos y deterministas; miden integridad, no inteligibilidad.
3. **Los efectos audibles** (seam, retardo, nivel) son `HYPOTHESIS` hasta el handoff físico.
4. **La versión de Speex en la ROM** es `UNVERIFIED`.
5. **El control negativo** es textual y determinista: detecta la forma prohibida de la frase, no
   semántica arbitraria. Su alcance exacto y sus límites están en el docstring del script y fijados por
   `--self-test`.

### 10.2 Errata corregida en este commit (fix explícito y mínimo)

Al verificar las citas para este informe se encontraron tres referencias de línea equivocadas en
`speex-audit.md` (documento de la tarea B, ya commiteado). Se corrigieron **sólo las líneas citadas**,
sin cambiar ninguna afirmación:

| Dónde | Decía | Ahora | Verificación |
|---|---|---|---|
| `speex-audit.md` §2.1 | `src/audio_enhancement.h:15-16` | `src/audio_enhancement.h:21-22` | `AUDIO_ENH_SAMPLE_RATE`/`AUDIO_ENH_FRAME_SIZE` están en 21-22 |
| `speex-audit.md` §6.1 | `src/audio_enhancement.h:84-95` | `src/audio_enhancement.h:71-81` | `audio_enh_state_t` va de 71 a 81 (`float_buffer` en 80) |

### 10.3 Por qué `run_all_tests.sh --quick` falla y no es este cambio

Los seis fallos de T2 son **preexistentes** y no pueden venir de este diff: `git diff --name-only
a1c667ee HEAD` sólo toca `docs/evidencia/2026-10-28-rx-audio-feasibility/**`, y ningún test de
`tests/` lee ese directorio (`grep -rln 2026-10-28-rx-audio-feasibility tests/ scripts/` → sin
coincidencias). Detalle:

- `apps_menu`: `ACTION_APP_LOGS` no está en ninguna página APP (contrato de la UI, ajeno a audio).
- `pruebas`: `docs/PRUEBAS.md` — estructura/citas del manual.
- `dialog_coverage`: la app `Logs` sin cobertura y `app_names[]` con 26 vs 25 del contrato #222.
- `curve_evidence`: 7 de 8 pruebas de la curva de ROE sobre PNGs commiteados.
- `wcag_rendered` y `widget_text`: el arnés docker falla al construir (`make ... Error 2`), es decir
  entorno, no diff.

No se corrigieron: están fuera del alcance de esta tarea (docs de audio) y el issue prohíbe tocar
`tests/` y `src/`.

## 11. Reproducir

```sh
cd <worktree>                                    # state/worktrees/x6200/rx-audio-feasibility
EV=docs/evidencia/2026-10-28-rx-audio-feasibility

python3 $EV/scripts/gen_pcm.py --verify          # T3: hashes byte a byte
python3 $EV/scripts/check_no_stub_claims.py      # T4
python3 $EV/scripts/check_no_stub_claims.py --self-test   # T4b
sh tests/native/run_native_tests.sh /tmp/opencode/x6200-native   # T1
sh tests/run_all_tests.sh --quick                # T2 (falla por lo de §10.3)
git diff --name-only a1c667ee HEAD               # T5
```

El banco con lib real (tarea B) se reproduce con
`python3 $EV/scripts/bench_speex.py` (requiere docker; pines en `speex-audit.md` §8).

El banco RNNoise (tarea D) necesita además el clone upstream **fuera del repo**:

```sh
git clone https://github.com/xiph/rnnoise /tmp/opencode/rnnoise-src
git -C /tmp/opencode/rnnoise-src checkout 70f1d256acd4b34a572f999a05c87bf00b67730d
python3 $EV/scripts/bench_rnnoise.py            # build + matriz + analisis + controles
python3 $EV/scripts/bench_rnnoise.py --analyse  # solo re-analizar artifacts/rnnoise
```

## 12. Tanda D — anexo vinculante RNNoise: laboratorio A/B offline

Resumen ejecutivo; el detalle (procedencia, protocolo, tablas completas, artefactos y limitaciones) está
en **`rnnoise-vs-speex.md`**.

**Qué se agregó al anexo**: RNNoise upstream de Xiph en el commit
`70f1d256acd4b34a572f999a05c87bf00b67730d` (BSD-3-Clause), compilado en `debian:bookworm-slim` pinneado
por digest, con el modelo que ese commit descarga (`download_model.sh` valida el sha256
`0a8755f8e2d834eff6a54714ecc7d75f9932e845df35f8b59bc52a7cfe6e8b37` contra `model_version`). Los pesos y
la librería **no** se versionan ni entran a la ROM: quedan en `/tmp` y en el contenedor, y
`rnnoise-manifest.json` lo declara (`vendored_in_repo=false`, `redistributed_in_repo=false`,
`in_firmware=false`).

**Cadenas comparadas** sobre los mismos cuatro vectores sintéticos (semilla 6200): `off` (bit-exacto, sin
resampling), `speex` (`audio_enh_process()` verbatim contra libspeexdsp 1.2.1 real, preset SSB_VOICE) y
`rnnoise` (resampling real 44.100 → 48.000 → 44.100, tramas de 480 muestras verificadas con
`rnnoise_get_frame_size()` = 480, escala S16). El resampler es polifásico, 64 taps por fase, Kaiser
β = 8, corte 22,05 kHz, con latencia de grupo de 31,5 muestras por etapa: **61 muestras = 1,3832 ms
medidos**, contra 60,4 muestras teóricas.

**Resultado por frame de 10 ms** (vector de ruido, 60 s, 5 repeticiones): RNNoise **p50 0,195 ms /
p95 0,222 ms / p99 0,271 ms** (2,04 % del tiempo real), Speex **0,013 ms**, resampling **0,036 ms**;
**0 %** de tramas sobre 10 ms y **0 clipping** en los 21 casos. Costo separado y RSS pico (7.092 kB con
RNNoise contra 3.460 kB con Speex) en `rnnoise-vs-speex.md` §6.

**Tres hallazgos que cambian decisiones**:
1. La escala correcta es **S16 cruda**, no `[-1,1]`: con `/32768` la energía cae bajo el umbral de
   silencio de RNNoise (`src/denoise.c:389-392`) y la cadena queda en passthrough (medido: −30,144 dBFS
   de salida contra −30,009 de entrada, y el costo baja a 0,036 ms/frame).
2. El retardo real de RNNoise es de **2 frames (20 ms)**, no de uno: análisis con `analysis_mem` +
   síntesis con `delayed_X` (`src/denoise.c:332-339,496-498`). Medido **943 muestras = 21,3832 ms** con
   el resampler incluido, confirmado con el control de passthrough.
3. RNNoise **suprime un tono puro** (−54,8 dB en el seno de 1 kHz): es un denoiser de voz. No se puede
   presentar como mejora sin habla HF real.

**Controles**: `off` bit-exacto en los cuatro vectores (sha igual, 0 muestras distintas); hashes
repetibles con misma entrada y versión; el prefijo de 10 s de una pasada de 60 s es idéntico a una pasada
de 10 s con estado fresco; y si falta RNNoise (binario compilado sin header ni librería, o
`--simulate-missing`) la cadena **vuelve a `off` bit-exacto**, con el tamaño correcto y el RMS
preservado (nunca silencio ni archivo truncado).

**Dictamen**: **live NO-GO** por el gate 1 del anexo (no hay ruta audible demostrada y
`MEASURED-HARDWARE` sigue en 0 filas); el offline se reporta como **`RESEARCH/PROTOTYPE OFFLINE`**. El
gate 2 (presupuesto en el R16 con waterfall/GUI) y el gate 4 (A/B ciego con habla HF) siguen pendientes y
no se extrapolan del host.
