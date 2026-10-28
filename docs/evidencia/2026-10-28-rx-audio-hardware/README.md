# Gate físico RX-only y factibilidad RNNoise en ARM — issue #347

- **Fecha**: 2026-10-28 (corrida 19:52Z–20:12Z).
- **Repo**: `iariap/x6200_gui` · **issue**: [#347](https://github.com/iariap/x6200_gui/issues/347) · **antecedente**: [#345](https://github.com/iariap/x6200_gui/issues/345) + [PR #346](https://github.com/iariap/x6200_gui/pull/346).
- **Rama**: `audit/rx-nr-hardware-20261028` · **worktree**: `x6200-rx-nr-hardware-20261028` · **base real**: `8b0d5de9b9bbf4f1c630c3570404c56c09f012d4`.
- **Qué es**: recolección de evidencia en la **radio física** para los cuatro gates G1–G4 del issue. **No** implementa RNNoise, **no** propone UI/routing, **no** toca el runtime.
- **Qué NO es**: no es un release GO, no valida inteligibilidad y no demuestra una ruta audible nueva (no existe).

## 0. Veredicto

| Gate | Estado | Clase | Evidencia |
|---|---|---|---|
| **G1** ruta audible RX→procesador→parlante/jack/A2DP | **NOT_VERIFIED → NO_GO_LIVE** | CODE + MEASURED-HARDWARE + HYPOTHESIS | `signal-route.csv`; `logs/g1-pactl-readback.txt`, `logs/g1-parec-stats.txt`; no existe retorno de audio procesado (E16) |
| **G2** presupuesto en el ARM real | **FAIL** | MEASURED-HARDWARE | `cpu-arm.csv`: media **13,14 ms/frame** (límite 5 ms), p99 **16,52 ms** (límite 10 ms), **100 %** de frames >10 ms; la pasada de protocolo (60 s de audio) **no entra en el timeout de 30 s** |
| **G3** inteligibilidad A/B con voz autorizada | **PENDING_VOICE** | BLOCKED por material | no hay voz HF con licencia/consentimiento (`physical-handoff.md` §3) |
| **G4** seguridad/release | **PASS** | MEASURED-HARDWARE + CODE | cero escrituras del agente a RF/ruteo/mute/modo/ajustes; `get-before` == `get-after`; GUI viva con el mismo sha256; sin TX; tests del `main` registrados sin arreglarlos |
| **Global** | **NO_GO_LIVE** | — | GO_EXPERIMENTAL_LIVE exige G1+G2+G3+G4 PASS; aquí G1 no está verificado, G2 falla y G3 está pendiente |

La recolección se considera **DONE**: hay un `BLOCKED`/`FAIL` reproducible, medido y sin riesgo para la radio.
**No** se habilita `Experimental NR` ni se justifica implementarlo con estos números.

## 1. Base real: el SHA del issue era el correcto; el clon local estaba viejo

- Al arrancar, `git fetch origin main` trajo **`origin/main = 8b0d5de9b9bbf4f1c630c3570404c56c09f012d4`**, que es el
  merge de documentación del PR #346 (mergeado `2026-10-28T19:46:31Z`), hijo directo de `a1c667ee`.
- El checkout base `x6200_qa` estaba en `a1c667ee` (un commit **anterior**): por eso sus `docs/evidencia/2026-10-28-rx-audio-feasibility/`
  no existían localmente hasta hacecer `git fetch` + partir el worktree de `origin/main`.
- Conclusión: el SHA `8b0d5de9` que cita el issue es el **vigente** (incluye #346); el supuesto «a1c667ee es el actual y
  8b0d5de9 quedó viejo» estaba invertido. Se trabajó desde `8b0d5de9` y se registra la diferencia.
- `git worktree add ../x6200-rx-nr-hardware-20261028 -b audit/rx-nr-hardware-20261028 8b0d5de9` → HEAD del worktree
  **`8b0d5de9`** (ver `logs/commands.log` y `commands.log`).

## 2. Entorno medido (todo read-only)

| Ítem | Valor | Fuente |
|---|---|---|
| Radio | `Linux X6200 6.1.82 #1 SMP PREEMPT ... armv7l` | `logs/radio-preflight.txt` |
| CPU | ARMv7 rev 5 (v7l), **4 × Cortex-A7** (`CPU part 0xc07`), governor `performance`, **1008 MHz** fijos | `logs/cpu-freq-check.txt` |
| RAM / `/tmp` | 1002 MB totales, ~845 MB disponibles; `/tmp` tmpfs 501 MB con ~432 MB libres antes de copiar | `logs/radio-preflight.txt` |
| GUI instalada | `/usr/sbin/x6200_gui`, pid 23711, sha256 `b1157fdb…0022` | `logs/radio-preflight.txt`, `logs/gui-sha-post.txt` |
| GUI vs `main` | **no mapeable a un commit**: la radio no expone build-id del binario; se registra el sha256 y se marca `UNVERIFIED` la correspondencia con `main`/`8b0d5de9` | `logs/radio-preflight.txt` |
| Audio | PulseAudio system; source `alsa_input.platform-sound.stereo-fallback` **RUNNING**; sink `alsa_output.platform-sound.stereo-fallback` **RUNNING**, `Mute: no`, volumen PA 83 %, **puerto activo `analog-output-headphones`**; **sin** `module-loopback` y **sin** sink `bluez` | `logs/g1-pactl-readback.txt`, `logs/radio-pa-pre.txt` |
| Toolchain ARM | `arm-buildroot-linux-gnueabihf-gcc 12.3.0` (Buildroot 2023.02.11-dirty), sysroot de la ROM; flags fijados: `-mcpu=cortex-a7 -mfloat-abi=hard -mfpu=neon-vfpv4 -march=armv7ve+simd` | `logs/build-bench-arm.log` |
| RNNoise | fuente Xiph `70f1d256acd4b34a572f999a05c87bf00b67730d`; modelo sha256 `0a8755f8…8b37` (58.603.099 B); hashes de fuente y pesos **idénticos a los del #345** | `logs/build-bench-arm.log` |
| Fixture | `white_noise_44100_s16` del #345, semilla 6200, 10 s; WAV `b6092692…7936`; raw S16LE `b718cf2c…a56e` (**= hash `off` del #345**) | `logs/gen_pcm-verify.log`, `logs/fixture.log` |

### 2.1 El estado RX cambió **por acción del operador**, no del agente

| Momento | Frecuencia | Modo | App | `vol` | Fuente |
|---|---|---|---|---|---|
| 19:52Z (primer snapshot) | 14.074 MHz | USB-D | FT8 | 0 | `logs/get-inicial.json` |
| 19:59Z (inicio de la corrida) | **7.094 MHz** | **LSB-D** (40 m SSB) | ninguna | **1** | `logs/get-before.json` |
| 20:10Z (estado final) | 7.094 MHz | LSB-D | ninguna | **0** | `logs/final-state.txt` |

Entre 19:52Z y 19:59Z el operador indicó que la radio tenía audio disponible en la banda de 40 m y sintonizó
el equipo a 7.094 kHz (LSB); más tarde cambió el volumen. **El agente NO retuneó ni cambió el volumen/mute**:
cambiar VFO/frecuencia está prohibido por el issue y por las reglas de la sesión, y `commands.log` no contiene
ninguna escritura de ese tipo. El cambio de estado se registra como **acción externa del operador**, sin citar
conversaciones. El detalle está en `physical-handoff.md` §4.

## 3. G1 — Ruta audible: NOT_VERIFIED (NO_GO_LIVE)

La tabla completa por edge está en **`signal-route.csv`** (E1–E17). Resumen:

- **CODE**: `read_callback()` (`src/audio.c:57-70`) es el único punto de entrada del RX; `audio_enh_process()`
  procesa **in situ** la copia del proceso (`src/audio.c:63`) y de ahí sólo va a `screen_rec_put_audio_samples()`
  y `dsp_put_audio_samples()`. El stream de reproducción (`play_stm`) lo escribe **sólo** `audio_play()`
  (`src/audio.c:281`), cuyos llamadores son audio **generado** por la GUI (TTS, FT8/JS8, mensajes, recorder, TX),
  no el RX. **No existe en el árbol una ruta RX → audio procesado → parlante** (E16 = `NOT_VERIFIED`).
- **MEASURED-HARDWARE**: el *source* de captura entrega señal real: `parec` pasivo de 2 s → **RMS −22,44 dBFS,
  pico −10,51 dBFS, 100 % de muestras ≠ 0** (`logs/g1-parec-stats.txt`). El *monitor* del sink está en
  **silencio digital** (0/78.569 muestras ≠ 0): la GUI no estaba generando audio en ese momento. Nada de esto
  prueba lo que suena por el parlante.
- **UNVERIFIED**: el trayecto eléctrico demodulador del BASE → parlante/jack (E14) sigue sin esquema ni medición;
  el códec del SoC → BASE (E15) sigue con evidencia previa contradictoria. El sink del SoC está armado y su
  puerto activo es **`analog-output-headphones`**, pero eso no demuestra que el RX del BASE pase por ahí.
- **NO EJECUTADO**: `audio_play_en()` (prohibido: escribe `record_set` y micrófonos del BASE); no se cargó
  `module-loopback`; no se conectó A2DP; no se emitió ningún tono al sink.
- **Verificación acústica**: `NOT PERFORMED` — no hubo escucha A/B ni micrófono externo; el ofrecimiento del
  operador no se usó porque retunear habría sido una escritura prohibida y porque escuchar el BASE **no**
  demuestra el retorno del audio procesado.

## 4. G2 — Presupuesto RNNoise en el ARM real: FAIL

### 4.1 Método (pre-registrado, igual que el #345)

- Banco del #345 **sin modificar**: `bench_rnnoise.c` + `stubs_bench.c` + `audio_enhancement.c` copiado
  **verbatim** (sha256 `acab3aad…6b25f` = el del #345); cross-compilado con el toolchain de la ROM (`-O2`,
  sin NEON/SSE agregados a mano). Se usó el código y el fixture pinneados.
- RNNoise ARM: 2 variantes. **`default`** = flags fijos del toolchain (definen `__ARM_NEON`, y RNNoise usa
  `vec_neon.h`) → correspondería al build real de la ROM. **`neonoff`** = `-DDISABLE_NEON` (C portable, el
  criterio «C portable» del issue y el comparable con el host del #345) como sonda de sensibilidad.
- Binario ARM copiado a un `/tmp` propio de la radio **después del preflight**; proceso aislado `nice -n 10`,
  **timeout de 30 s por pasada**, **3 warmups + 5 pasadas** con 60 s de audio por pasada (protocolo del issue).
- Monitoreo continuo 1 Hz de la GUI y la radio (`/proc/<pid>/stat`, `/proc/stat`, `/proc/loadavg`,
  `monitor.sh`), con corte de seguridad pre-registrado: load1 > 3,20; CPU total ocupada (idle < 20 %);
  CPU de la GUI > 95 % de un núcleo sostenida; GUI muerta; o timeout. **Ninguno disparó.**
- **La radio no trae `timeout`** (BusyBox 1.36.1 sin el applet): se implementó el mismo contrato con un
  watchdog (`scripts/radio/run-one.sh`) que manda TERM a los N s y reporta **exit 124**.

### 4.2 Resultado: el protocolo de 60 s **no entra** en el timeout

La primera pasada de protocolo (`--method rnnoise --seconds 60`, 6000 frames de 10 ms) fue **matada a los 30 s**
(exit **124**, `logs/protocol-60s/`). No se corrieron las 3+5: cota inferior **media > 5,0 ms/frame**
(6000 × 5 ms = 30 s). El protocolo se abortó como manda el issue.

### 4.3 Medición acotada (10 s de audio = 1000 frames), mismo binario y fixture

| Variante | run | p50 ms | p95 ms | p99 ms | media ms | frames >10 ms | RSS kB | exit |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| `default` (NEON) | pass1 | 12,781 | 14,363 | 15,616 | 12,934 | 1000/1000 | 3624 | 0 |
| `default` (NEON) | pass2 | 13,697 | 18,036 | 20,925 | 14,155 | 1000/1000 | 3560 | 0 |
| `default` (NEON) | pass3 | 13,002 | 15,237 | 16,523 | 13,143 | 1000/1000 | 3624 | 0 |
| **`default` mediana** | 3 pasadas | **13,002** | **15,237** | **16,523** | **13,143** | **3000/3000** | 3624 | 0 |
| `neonoff` (C portable) | 1 pasada | 28,485 | 29,882 | 31,581 | 28,389 | 1000/1000 | 3624 | 0 |

**Costo separado por frame (p50, variante `default`):** resampling 44,1→48 **1,28 ms** + RNNoise **10,25 ms**
+ resampling 48→44,1 **1,18 ms** ≈ **12,71 ms**. Sondas de método con el mismo banco (10 s de audio):
`off` **0,000 ms** (bypass bit-exacto), `resample` solo **1,96 ms** (media 2,07), `speex` (cadenas del #345)
**0,31 ms** (media 0,35). Es decir: **RNNoise ≈ 33× el costo de Speex** en este ARM, y el resampling solo ya
consume ~20 % del frame de 10 ms.

`cpu-arm.csv` tiene las 10 filas con todas las columnas pedidas (incluye el timeout y las sondas).

### 4.4 Controles y seguridad del bench

- **`off` bit-exacto en la radio**: `cmp` salida vs entrada → `OFF_BIT_EXACT_OK` (también en ARM). `frame_size = 480`
  reportado por ambas variantes (`logs/control-frame-size.txt`, `logs/control-off.json`).
- **Cero referencias a hardware/radio en el binario**: `nm`/`strings` filtrados por `x6200_control_|pa_stream|pulse|alsa|snd_|i2c|record_set|audio_play` → **vacío** (`logs/build-bench-arm.log`). El banco procesa archivos PCM, no abre ALSA/PulseAudio.
- **Monitoreo**: protocolo → GUI CPU mediana **85,5 %**, máx **90,1 %**; idle mín **29,6 %**; load1 máx **1,73**.
  Mediciones acotadas → GUI CPU mediana **74,8 %**, máx **92,8 %**; idle mín **38,6 %**; load1 máx **1,43**.
  Ningún umbral de aborto se disparó y no hubo XRUN/watchdog; la GUI siguió viva y el mismo pid durante toda la corrida.
  (El CPU de la GUI subió desde ~67 % de línea base por la contención del proceso `nice 10`; hay ~2 núcleos libres.
  El estado `RUNNING` de captura se mantuvo, `logs/radio-asound-post.txt`.)
- **Condición «waterfall activo»**: `UNVERIFIED` — no se leyó el framebuffer (para no publicar posible PII) ni se
  cambió de app; la GUI estaba en RX con la pantalla principal y sin app abierta al medir. Se registra como
  limitación: el número de CPU no se tomó con un waterfall confirmado en pantalla.

### 4.5 Dictamen G2

**FAIL.** Media **13,14 ms** > 5 ms; p99 **16,52 ms** > 10 ms; **100 %** de frames >10 ms; y el protocolo
de 60 s ni siquiera completa dentro del timeout de 30 s. La variante portable es 2,2× más lenta todavía
(28,39 ms de media). El costo es **~67× el medido en x86_64 por el #345**, y no se extrapola en ningún sentido:
el número que vale es este, medido en el R16. No se tocó frecuencia/afinidad ni se degradó nada para «hacerlo pasar».

## 5. G3 — Inteligibilidad: PENDING_VOICE

- **No hay** voz HF con licencia/consentimiento aportada por el operador. El único archivo versionado con voz,
  `docs/evidencia/2026-10-15-banco-audio/captura-voz.wav`, es una prueba sintética en **QEMU** sobre un
  `module-null-sink` del banco: **no** es señal HF real y QEMU no representa el audio del BASE → no sirve.
- Requisitos para cerrar G3 y planilla: `physical-handoff.md` §2–3 y el handoff del #345 (sigue vigente).
- No se generó voz sintética para afirmar inteligibilidad.

## 6. G4 — Seguridad y release: PASS

- **Cero escrituras del agente** a RF, ruteo, mute, modo, VFO, volumen, NR/NB/DNF, ajustes persistidos, BT ni
  firmware. `audio_play_en()`/`x6200_control_record_set()` **no se llamaron** (prohibido). No hubo TX
  (`tx=false` en todos los snapshots) ni flash/rebuild.
- **Prueba negativa**: `get-before` y `get-after` de la corrida son **idénticos** salvo `db` (nivel de señal):
  mismo VFO/freq/modo/banda/filtro/`vol`/`nb`/`nr`/`agc`/`tx` (`logs/get-before.json` vs `logs/get-after.json`).
  La GUI conservó pid 23711 y sha256 `b1157fdb…0022`; `pactl` sources/sinks/modules idénticos antes y después;
  sin `module-loopback`. El temporal `/tmp/x6200-rnnoise-bench-20261028` se borró (`logs/cleanup.txt` = `CLEAN`).
- **Cambios externos**: el operador retuneó 14,074→7,094 y luego cambió `vol` 1→0 (ver §2.1). Se documentan como
  acciones **del operador**, no del agente; no afectan el snapshot before/after de la corrida (mismo estado).
- **Tests** (en el worktree, nunca en la radio):
  - `sh tests/native/run_native_tests.sh` → **EXIT=0**.
  - `sh tests/run_all_tests.sh --quick` → **EXIT=1, 17 OK / 6 fallos**; los **seis fallos preexistentes** del #345,
    registrados sin arreglarlos ni atribuirlos a esta auditoría: `apps_menu` (`FALLA ACTION_APP_LOGS no esta en
    ninguna pagina APP`), `pruebas`, `wcag_rendered`, `widget_text`, `dialog_coverage` (`el mock declara dialog_logs
    y la tabla no lo tiene`; `remote_server.cpp::app_names[] tiene 26 apps (el contrato de #222 dice 25)`;
    `apps del barrido sin cobertura: ['Logs']`), `curve_evidence` (`FAIL: 7 de 8 pruebas`).
    - Nota de entorno: el primer intento dio **7** fallos porque el submódulo `lvgl` no estaba inicializado
      (`drawbuf_partial` fallaba por CMake). Se inicializó el submódulo pineado y el segundo intento dio los
      **6 fallos esperados** con `drawbuf_partial: OK`. No se modificó ningún test.
  - QEMU: **UNSUPPORTED** para audio del BASE y para medir desempeño; no se usó como sustituto del Cortex.

## 7. Clases de evidencia usadas

- `CODE`: leído en el árbol (`archivo:línea`), verificable con grep.
- `MEASURED-HARDWARE`: medido **en la radio** en esta corrida (PulseAudio/`/proc`/parec/banco ARM).
- `MEASURED-HOST`: del #345, x86_64; se cita como contraste, **nunca** como si fuera el R16.
- `HYPOTHESIS` / `UNVERIFIED` / `NOT_VERIFIED` / `PENDING_VOICE` / `BLOCKED`: exactamente como en
  `signal-route.csv`, `physical-handoff.md` y este documento.
- **`MEASURED-HARDWARE` en esta corrida**: ruta source/sink y readback, estado RX antes/después, banco G2 en el ARM.
- **Nunca** se presentó un número de x86 como Cortex-A7 ni un `pactl list` como evidencia acústica.

## 8. Qué falta para pasar a una etapa posterior (no autorizado por este documento)

1. **G1**: demostrar en hardware una ruta **RX → procesador → audio audible** sin `record_set` ni loopback
   inseguro, con esquema y prueba física no invasiva (hoy no existe en el código: E16).
2. **G2**: o bajar el costo real (p. ej. otra estrategia de resampling/NR o hardware más rápido — decisión de
   producto, **fuera de este issue**) o el gate falla: 13,14 ms de media por frame de 10 ms no es viable en vivo.
3. **G3**: A/B ciego con voz HF autorizada y valoración del operador (hoy `PENDING_VOICE`).
4. Recién con G1–G3 PASS se podría discutir `Experimental NR`; este PR no implementa nada ni hace release GO.

## 9. Archivos y comandos

| Archivo | Qué contiene |
|---|---|
| `README.md` | este informe: gates, clases de evidencia, veredicto y bloqueos |
| `signal-route.csv` | 18 filas por edge (E1–E17): productor→consumidor→salida física, evidencia CODE vs MEASURED-HARDWARE, verificación acústica |
| `cpu-arm.csv` | 10 filas del gate G2 en el ARM real: timeout del protocolo, 3 pasadas medidas default, sonda portable, sondas off/resample/speex, agregados |
| `commands.log` | comandos reales y exit codes (IP/credenciales enmascaradas) |
| `physical-handoff.md` | qué requiere presencia del operador (H1–H6), material de voz y la indicación del operador |
| `logs/` | salidas crudas sin PII: preflight, `pactl`, `parec` (sólo RMS/hash, audio borrado), monitoreo 1 Hz, JSON del banco, consolas de las corridas, tests |
| `scripts/build-bench-arm.sh` | wrapper de build cross del banco del #345 (verifica commit/modelo/hashes) |
| `scripts/run-bench-arm.sh` | wrapper de ejecución en la radio: preflight, copia a `/tmp` propio, controles, 3+5 con timeout y monitoreo, snapshots y limpieza |
| `scripts/analyse-bench.py` | arma `cpu-arm.csv` desde los JSON + monitor |
| `scripts/radio/monitor.sh`, `scripts/radio/run-one.sh` | monitoreo 1 Hz con corte de seguridad y `timeout` por pasada (la radio no trae `timeout`) |

Manifest de binarios y del fixture: `logs/bench-bin.sha256`, `logs/fixture.log`, `logs/build-bench-arm.log`.

## 10. Limitaciones explícitas

1. **No hay verificación acústica** ni escucha del operador en esta corrida (el audio crudo no se conservó ni publicó).
2. **Waterfall activo**: `UNVERIFIED` durante el bench (§4.4).
3. El protocolo de G2 **no pudo completarse** (3+5 con 60 s) por el timeout de 30 s: se midió con 10 s por pasada y
   se registra el timeout como evidencia de FAIL, no se maquilló.
4. La GUI instalada **no es mapeable a un commit** (no hay build-id en la radio): sólo se fija su sha256.
5. RNNoise se midió con el resampler **del banco del #345** (64 taps/fase), no con uno de la radio (no existe).
6. El banco no se conecta al audio productivo ni al parlante: `audio_destination = OFFLINE_CAPTURE_ONLY`.
