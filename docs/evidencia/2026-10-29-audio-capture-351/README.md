# Bench y matriz de verificación de la captura con SpeexDSP real — issue #351, partes 3a y 3b

- **Issue**: [iariap/x6200_gui#351](https://github.com/iariap/x6200_gui/issues/351) — punto 4 de
  «Tests y regresión» y «Etiquetas honradas» (contrato D) en la **parte 3a**; punto 5 (matriz de
  tests, build ARM, QEMU) y punto 6 (handoff físico) en la **parte 3b**. Son las dos últimas de 3.
- **Base**: `169bb807b0086dbb91d29a9ce6770d25ca396859` (`origin/main` al arrancar la unidad).
- **Rama**: `factory/audio-capture-pipeline-351` · **Worktree**:
  `state/worktrees/x6200/audio-capture-pipeline-351`.
- **Commits de la unidad**: `98de5e1b` (parte 1: init sincrónico, mutex único, NR de tramas de
  441), `83a0da6d` (parte 2: callback de captura seguro), el commit de la parte 3a y el de la
  parte 3b (`test(audio): matriz de verificación, ARM/QEMU y handoff físico de la captura`).
- **Qué es**: la evidencia del bench con **`libspeexdsp` real** sobre `src/audio_enhancement.c`
  copiado **verbatim**, el registro del rótulo `Capture filters` y de la guía honesta (§2–§8), y
  la matriz de verificación completa con sus exit codes, el estado de cada criterio de aceptación
  y el handoff físico (§9–§11).
- **Qué NO es**: no mide calidad audible, inteligibilidad, la ruta RX→parlante ni el CPU del
  R16. Las tres siguen `BLOCKED` (§6) y la validación física es `PENDING-HARDWARE`
  (`hardware-handoff.md`). **No se tocó `src/audio.c` ni `src/audio_enhancement.{c,h}` en la
  parte 3a** (ya venían de las partes 1 y 2), ni
  `docs/evidencia/2026-10-28-rx-audio-feasibility/**`, ni la radio. La parte 3b no cambia código
  de `src/` ni tests verdes: sólo agrega evidencia.
- **Build ARM y QEMU**: los dos quedan **`BLOCKED`** por falta de toolchain y de binario ARM
  (§9.4 y §9.5). No se presenta ningún build de host como ARM.

## 0. Resumen

| Verificación obligatoria | Resultado | Evidencia |
|---|---|---|
| La cadena corre contra `libspeexdsp` **real** (no un stub) | **PASS** — `libspeexdsp-dev=1.2.1-1`, `SPEEX_PREPROCESS_SET_NOISE_SUPPRESS=18` leído del header real | §2, `commands.log` |
| N muestras de salida por N de entrada | **PASS** — 441 000 → 441 000 en los 36 casos | `measurements.csv` |
| `Off` bit-exacto al input | **PASS** — 0 muestras distintas en los 12 casos `Off` | §3 |
| `Off`/`SSB voice` bit-exactos entre particiones | **PASS** — mismo SHA-256 en 4 particiones (4410 / 441 / 8820 / `1,440,441,442,257`) | §4 |
| `AM broadcast` **no** se exige bit-exacto | **registrado** — idéntico en 4410/441/8820; el patrón `1,440,441,442,257` difiere en 429 869 muestras, max abs diff 1103 LSB (nivelador AF por chunk de ≤441) | §4 |
| Sin clipping | **PASS** — 0 muestras en los 36 casos | §3 |
| Supresión efectiva == fórmula `-10 - level*20/100` | **PASS** — 0→−10, 40→−18, 60→−22, 100→−30 leídos de vuelta de la lib real | §5 |
| Retardo por correlación | **MEDIDO** — 0 muestras sin NR; **882 muestras (20 ms)** con NR = 441 de la máquina de tramas + 441 de la lib | §3, §5 |
| Seams en los bordes de callback | **MEDIDO** — SSB por debajo del salto propio del input; AM +140 LSB en la grilla de 441 por el paso de ganancia del nivelador | §3 |
| Dependencia real presente | **SÍ** — no hay `BLOCKED` por dependencia | `commands.log` |

## 1. Contenido de este directorio

| Archivo | Qué contiene |
|---|---|
| `README.md` | este informe |
| `measurements.csv` | 57 filas: 36 casos de la matriz, 3 `partition-check` con reset, 4 sondas de nivel, 1 sonda `speex_only`, 4 `BLOCKED` del banco y **9 de la matriz de verificación de la parte 3b** (§9) |
| `bench-results.json` | métricas crudas por caso, invariancia por partición, chequeos de reset, mapa de nivel, toolchain e imagen |
| `pcm-manifest.json` | sha256, RMS, peak, clipping y duración de los 4 WAV sintéticos |
| `commands.log` | comandos exactos, salidas y exit codes: build y matriz del banco (§1–§8) y matriz de verificación, ARM, QEMU y cierre (§9) |
| `hardware-handoff.md` | handoff físico `PENDING-HARDWARE` para el agente de la radio (§11) |
| `scripts/gen_pcm.py` | generador determinista (semilla 6200, 10 s, stdlib) + `--verify` |
| `scripts/bench_capture.py` | orquestador: build, corrida en contenedor, análisis, CSV/JSON/log |
| `scripts/check_settings_capture_filters.py` | test visible de la fila `Capture filters` en el arnés de host: 6 temas, rótulo sin recorte + tinta del dropdown (§9.3) |
| `scripts/probe_arm_toolchain.sh` | sonda del toolchain ARM del Buildroot: informa qué falta y sale 77 (`BLOCKED`), nunca finge un build de host (§9.4) |
| `captures/` | PNG de `settings-audio` en Aurora y Sunlit (los seis temas se regeneran con el script) |
| `scripts/bench/Dockerfile` | `debian:bookworm-slim` pinneado por digest + `libspeexdsp-dev=1.2.1-1` |
| `scripts/bench/run_in_container.sh` | copia verbatim de `src/audio_enhancement.c`, compila con `-Werror`, corre la matriz |
| `scripts/bench/bench_main.c` | banco: `--case`, `--partition-check`, `--probe levels`, `--probe speex-only` |
| `scripts/bench/stubs_bench.c`, `scripts/bench/stubs/**` | periferia sustituida (LVGL tick, cfg, params, liquid-dsp). liquid **aborta** si se usa |
| `artifacts/` | WAV y `.raw` (gitignored; se regeneran y su integridad queda anclada por sha256) |

La infraestructura se copió **como referencia** del banco pinneado de #345
(`docs/evidencia/2026-10-28-rx-audio-feasibility/scripts/bench/`): mismo digest de base, misma
versión de `libspeexdsp`, mismos stubs de periferia y el mismo generador de PCM (sin cambiar
fórmulas ni semilla, para que las señales sean byte a byte comparables). Este directorio es
**autónomo**: no lee nada del directorio histórico en runtime.

## 2. Procedencia y reproducibilidad

```
python3 docs/evidencia/2026-10-29-audio-capture-351/scripts/bench_capture.py
```

Ese único comando: genera/verifica los WAV, construye la imagen (`docker build`), corre la
matriz en el contenedor con el worktree montado **read-only**, analiza en el host y escribe
`measurements.csv`, `bench-results.json` y `commands.log`. Devuelve 0 sólo si pasan todas las
verificaciones obligatorias; si el build o la lib fallan, escribe las filas `BLOCKED` con la
causa y devuelve 1.

Datos de la corrida registrada acá:

| Dato | Valor |
|---|---|
| Imagen del banco | `sha256:46d344b88a0ccdfda0024461b18080233e023c1b8116657e86db5157ded79e1c` |
| Base | `debian:bookworm-slim@sha256:99c0b25c46d0214d5a2091092e7908907dcaecd6a962cde0ac700acee222b6f3` |
| `libspeexdsp-dev` | `1.2.1-1` (`pkg-config --modversion speexdsp` → 1.2.1) |
| Compilador | `gcc (Debian 12.2.0-14+deb12u1) 12.2.0`, `-O2 -std=c11 -Wall -Wextra -Werror` |
| Fuente | `src/audio_enhancement.c` sha256 `697e5dbca33ad8cd74a5533c68e6b1d6ab6c8f6d959cc44445d6c77e486c49bc` (copiado, no editado) |
| Header | `src/audio_enhancement.h` sha256 registrado en `bench-results.json` |
| PCM | `gen_pcm.py`, semilla 6200, 10 s, 441 000 muestras mono S16 44 100 Hz |

Lo único sustituido es la periferia que en la radio proveen LVGL/cfg/params/liquid-dsp. SpeexDSP
no se sustituye nunca: el contenedor enlaza la lib real y el bench lee de vuelta
`SPEEX_PREPROCESS_GET_NOISE_SUPPRESS` del estado efectivo.

## 3. Matriz principal (10 s, callback de 4410 = 100 ms)

| Señal | Preset | N in / N out | Retardo (muestras) | RMS salida | Atenuación | Peak | Clip | Seam cb / trama / input (LSB) | p99 por callback | 10 s en |
|---|---|---|---|---|---|---|---|---|---|---|
| ruido blanco | `Off` | 441000 / 441000 | **0** | −30.01 dBFS | +0.00 dB | −25.23 dBFS | 0 | 3189 / 3466 / 3587 | <1 µs | ≈1 ms |
| ruido blanco | `SSB voice` | 441000 / 441000 | **882** | −41.87 dBFS | −11.86 dB | −25.30 dBFS | 0 | 3336 / 3336 / 3587 | ≈0.4–0.6 ms | ≈26–28 ms |
| ruido blanco | `AM broadcast` | 441000 / 441000 | **882** | −39.32 dBFS | −9.32 dB | −24.29 dBFS | 0 | 3727 / 3727 / 3587 | ≈0.4–0.6 ms | ≈26–28 ms |
| multisine 300/1k/2.4k | `Off` | 441000 / 441000 | — (ambiguo) | −25.78 dBFS | +0.00 dB | −18.08 dBFS | 0 | 715 / 715 / 715 | <1 µs | ≈1 ms |
| multisine 300/1k/2.4k | `SSB voice` | 441000 / 441000 | — (ambiguo) | −38.22 dBFS | −12.44 dB | −18.15 dBFS | 0 | 709 / 710 / 715 | ≈0.4–0.5 ms | ≈23–25 ms |
| multisine 300/1k/2.4k | `AM broadcast` | 441000 / 441000 | — (ambiguo) | −38.98 dBFS | −13.20 dB | −18.15 dBFS | 0 | 709 / 710 / 715 | ≈0.4–0.6 ms | ≈23–25 ms |
| seno 1 kHz | `Off` | 441000 / 441000 | — (ambiguo) | −21.01 dBFS | +0.00 dB | −18.00 dBFS | 0 | 586 / 586 / 588 | <1 µs | ≈1 ms |
| seno 1 kHz | `SSB voice` | 441000 / 441000 | — (ambiguo) | −33.54 dBFS | −12.53 dB | −18.06 dBFS | 0 | 580 / 581 / 588 | ≈0.4–0.5 ms | ≈23–25 ms |
| seno 1 kHz | `AM broadcast` | 441000 / 441000 | — (ambiguo) | −34.33 dBFS | −13.32 dB | −18.06 dBFS | 0 | 580 / 581 / 588 | ≈0.35–0.5 ms | ≈23–25 ms |

Notas de lectura:

- **«ambiguo»** = la señal es periódica, así que el pico de correlación se repite cada período y
  no identifica el retardo. El delay inequívoco se mide con ruido blanco (§5).
- **Retardo 882 = 441 + 441.** La máquina de tramas emite una trama tarde y `libspeexdsp` tiene
  además un retardo algorítmico propio de una trama (ventana con solapamiento). Medido con un
  estado Speex directo, sin código del módulo: **441 muestras**. Es consistente con #345, que
  midió ese retardo como exactamente una trama (5.805 ms con tramas de 256).
- **Seams.** Con `SSB voice` el salto máximo en los bordes de callback y de trama (3336) es
  **menor** que el salto máximo que ya trae el input (3587): la partición no agrega
  discontinuidad. Con `AM broadcast` es **140 LSB mayor** (3727 vs 3587, ≈4 % del salto propio
  del input) sobre la grilla de 441: es el paso de ganancia del nivelador AF, que mide el pico
  por bloque. Se registra, no se oculta.
- **Un tono puro también se atenúa** (~12.5 dB con `SSB voice`): es el comportamiento del NR de
  Speex sobre señales tonales y es la razón por la que `CW weak` mantiene el NR apagado.
- **Tiempos**: son wall-clock de un x86_64 de host para 10 s de audio (≈0.25 % del tiempo real
  con NR) y **varían entre corridas** del host: los valores exactos de la corrida registrada están
  en `measurements.csv`. **No** son un presupuesto de CPU del R16 (§6).

## 4. Invariancia por partición

Mismo input, cuatro particiones, cada corrida desde estado recién inicializado
(`audio_enh_init()` + `audio_enh_preset_load()`):

| Señal | Preset | SHA-256 igual en 4410 / 441 / 8820 / `1,440,441,442,257` | Detalle |
|---|---|---|---|
| ruido blanco | `Off` | **SÍ** (`b718cf2c…`) | bit-exacto, sin priming ni retardo |
| ruido blanco | `SSB voice` | **SÍ** (`0789cf1c…`) | NR stateful: la trama continúa a través de los callbacks |
| ruido blanco | `AM broadcast` | **4410 = 441 = 8820**; el patrón mezclado difiere | 429 869 muestras distintas vs. 4410, max abs diff **1103 LSB** |
| multisine | `Off` / `SSB voice` | **SÍ** / **SÍ** | — |
| multisine | `AM broadcast` | **4410 = 441 = 8820**; el patrón mezclado difiere | 433 966 distintas, max abs diff 448 LSB |
| seno 1 kHz | `Off` / `SSB voice` | **SÍ** / **SÍ** | — |
| seno 1 kHz | `AM broadcast` | **4410 = 441 = 8820**; el patrón mezclado difiere | 424 783 distintas, max abs diff 446 LSB |

La diferencia de `AM broadcast` es esperada y **no** se exige bit-exacta (lo dice el issue):
`audio_enh_process()` parte cualquier callback en chunks de **a lo sumo 441 muestras**, así que
el nivelador AF mide el pico por chunk y sólo una partición que **fuerce chunks más cortos**
cambia la ganancia. Por eso las particiones múltiplos de 441 dan el mismo SHA-256 y el patrón
`1,440,441,442,257` no. Lo que se verifica en su lugar: longitud conservada, sin clipping,
estado estable y diferencias acotadas.

### 4.1 Reset de preset ≠ re-init (hallazgo medido)

`audio_enh_preset_reset()` (y el cambio de preset) descarta la trama pendiente de la máquina NR,
pero **no** destruye/recrea el `SpeexPreprocessState`. Procesando el mismo input dos veces en un
solo proceso con un reset en el medio:

| Preset | ¿Idénticas? | Muestras distintas | Max abs diff | Causa |
|---|---|---|---|---|
| `Off` | **SÍ** | 0 | 0 LSB | no hay estado |
| `SSB voice` | NO | 433 735 | 1539 LSB | `libspeexdsp` conserva su estimación adaptativa de ruido |
| `AM broadcast` | NO | 438 096 | 1580 LSB | estimación adaptativa + ganancia suavizada del nivelador AF |

No es un fallo de la verificación: la bit-exactitud entre particiones se exige (y se cumple)
**partiendo de estado recién inicializado**, que es lo que fija el contrato. Se documenta porque
un revisor puede leer «reset» como «re-init»; reiniciar la adaptación de Speex requeriría
recrear el estado en cada cambio de preset, cosa que esta revisión no hace. Queda registrado en
`bench-results.json → partition_reset_checks` y en la guía (§7).

## 5. Sondas directas contra la lib real

### 5.1 Polaridad del nivel (`audio_enh_nr_set_level()`)

| level | fórmula | `SPEEX_PREPROCESS_GET_NOISE_SUPPRESS` real | atenuación RMS medida (ruido blanco) |
|---|---|---|---|
| 0 | −10 dB | **−10 dB** | −8.49 dB |
| 40 | −18 dB | **−18 dB** | −11.86 dB |
| 60 | −22 dB | **−22 dB** | −12.61 dB |
| 100 | −30 dB | **−30 dB** | −13.27 dB |

La constante ABI real es 18 (`#define SPEEX_PREPROCESS_SET_NOISE_SUPPRESS 18`), leída del header
del contenedor. La atenuación crece de forma monótona con el nivel. Que la atenuación medida no
llegue a −30 dB es normal: el control es un objetivo del algoritmo, no un gain fijo; lo que se
verifica acá es la **polaridad** y el **valor efectivo** del control.

### 5.2 Retardo propio de la librería

| Medición | Muestras | ms |
|---|---|---|
| `SpeexPreprocessState` directo (441 / 44100, denoise on, −18 dB) | **441** | 10.000 |
| `audio_enh_process()` con `SSB voice` | **882** | 20.000 |
| `audio_enh_process()` con `Off` | **0** | 0.000 |

## 6. Lo que este banco NO mide (`BLOCKED`)

| Caso | Por qué |
|---|---|
| Calidad audible RX→parlante | La cadena procesa **sólo la copia de captura**; la ruta audible y cualquier retorno seguro siguen `NOT_VERIFIED` (#347) |
| Latencia extremo a extremo RX→parlante | Incluye PulseAudio (fragsize 100 ms), el DSP del BASE y el trayecto eléctrico; acá sólo se mide el retardo algorítmico de la copia |
| CPU en la radio (R16) | El banco es x86_64 de host, otra arquitectura y otra carga; no se extrapola |
| Inteligibilidad de voz | Las señales son tonos y ruido sintéticos, no habla |

Nada de esto se declara PASS. El bench prueba **integridad de procesamiento** (longitud, orden,
retardo, seams, clipping, determinismo por partición, polaridad), no calidad de escucha.

## 7. Rótulo y guía (contrato D)

- `src/dialog_settings.c::make_audio_enh()`: el rótulo visible pasa de `Audio enhance` a
  **`Capture filters`**. Las cinco opciones siguen exactamente `Off`, `CW weak`, `SSB voice`,
  `AM broadcast`, `Digital`, sin cambiar índices, orden ni serialización.
- `src/params/params.c`: la locución de `params.audio_enh_preset.voice` pasa a
  **`Capture filters`**. El nombre de DB `audio_enh_preset` queda literal intacto.
- Sin icono, indicador, botón ni ajuste nuevo.
- `docs/AUDIO_ENHANCEMENT_GUIDE.md` se reescribió sin afirmaciones aspiracionales: se eliminaron
  «el audio procesado llega al parlante», la comparación con el IC-7300, los porcentajes de CPU
  y la «UI de notch» inexistente; se documentan tasas reales, priming, presets, polaridad, reset
  y destino real, con enlaces a #345 y #347.

## 8. Reproducir

```
cd <worktree>
python3 docs/evidencia/2026-10-29-audio-capture-351/scripts/bench_capture.py
```

Requiere Docker y red para el `apt-get` del primer build (la imagen queda cacheada). El worktree
se monta read-only: el contenedor copia `src/audio_enhancement.c` y lo compila, así que la
medición es del archivo de producción, no de una copia editada.

---

# Parte 3b — matriz de verificación, ARM/QEMU y cierre

## 9. Matriz de tests (comandos exactos, exit codes, preexistentes separados)

Todo lo de esta sección se corrió en el worktree
`state/worktrees/x6200/audio-capture-pipeline-351`, HEAD `86bae74b`, base `169bb807`. Las salidas
completas, con los comandos literales, están en `commands.log` (§«PARTE 3b») y las filas
correspondientes en `measurements.csv` (`verify_*`).

### 9.1 Tabla

| # | Comando | Exit | Resultado | Clase |
|---|---|---|---|---|
| 1 | `sh tests/native/run_native_tests.sh /tmp/opencode/x6200-native-351-p3b` | **0** | 25 líneas de resumen, **75 723 comprobaciones, 0 fallos**. Incluye el stage #351 del callback (174) y el de `audio_enhancement` | `MEASURED-HOST` |
| 2 | `sh tests/run_all_tests.sh --quick` | **1** | **17 OK, 6 fallos, 6 omitidos**. Los 6 fallos son los preexistentes de #347 (§9.2). **Regresiones nuevas: 0** | `MEASURED-HOST` |
| 3 | `python3 tests/check_wcag_rendered.py --all` | **1** | No llega a renderizar: el binario del arnés **no linkea** (causa preexistente, §9.2) | `BLOCKED` |
| 4 | `python3 docs/evidencia/2026-10-29-audio-capture-351/scripts/check_settings_capture_filters.py` | **0** | Rótulo `Capture filters` visible y sin recorte en los **6 temas**; el dropdown dibuja su valor (§9.3) | `MEASURED-HOST` |
| 5 | `sh docs/evidencia/2026-10-29-audio-capture-351/scripts/probe_arm_toolchain.sh` | **77** | **`BLOCKED`**: no hay toolchain ARM del Buildroot (§9.4) | `BLOCKED` |
| 6 | `sh tests/qemu/run_apps_qemu.sh --check` | **77** | **`SKIP`/`BLOCKED`**: falta el binario ARM de esta revisión y el chroot privado (§9.5) | `BLOCKED` |
| 7 | `git diff --name-only 169bb807 HEAD` | **0** | 44 archivos, todos dentro del alcance declarado (§9.6) | `CODE` |

Los stages omitidos del punto 2 son `qemu_apps` (exit 77, §9.5), `wspr`/`sstv`/`wefax` (la imagen
`x6200-uiharness` local es la mínima: no trae `fftw3.h` ni `sndfile.h`; con la imagen `full`
corren), `js8` (lo omite `--quick` por diseño) y `webserver` (no está el checkout del
configurador). Ninguno de los omitidos toca la captura.

### 9.2 Los 6 fallos preexistentes (#347) frente a regresiones nuevas

| Stage | Causa medida en esta corrida | ¿Del alcance de #351? |
|---|---|---|
| `apps_menu` | `ACTION_APP_LOGS` no está en ninguna página APP (sólo se llegaría por long press) | no |
| `pruebas` | `docs/PRUEBAS.md`: entradas con evidencia citada que no existe | no |
| `wcag_rendered` | el arnés de host no linkea: `undefined reference to bt_state_set_power` / `bt_state_set_discoverable` | no |
| `widget_text` | ídem (mismo binario) | no |
| `dialog_coverage` | el mock declara `dialog_logs` y la tabla no lo tiene; `app_names[]` con 26 apps vs 25 del contrato #222 | no |
| `curve_evidence` | 7 de 8 casos (evidencia de la curva de ROE en PNG versionados) | no |

**Regresiones nuevas introducidas por #351: 0.** El stage `host` —que es la suite nativa, la que
sí toca este cambio— **pasa** dentro de la misma corrida (`174 comprobaciones, 0 fallos` en el
stage #351, y la suite completa en el punto 1).

### 9.3 Test visible de Ajustes (`Capture filters`, inglés, 6 temas)

```
$ python3 docs/evidencia/2026-10-29-audio-capture-351/scripts/check_settings_capture_filters.py
  Sunlit    tema_activo=Sunlit    rotulo=(16,213) 200x29 natural={'w': 188, 'h': 29} caja={'w': 188, 'h': 29} overflow=False truncado=False dropdown_ink_px=290 -> OK
  Redshift  tema_activo=Redshift  rotulo=(16,213) 173x29 natural={'w': 161, 'h': 29} caja={'w': 161, 'h': 29} overflow=False truncado=False dropdown_ink_px=116 -> OK
  Sprint    tema_activo=Sprint    rotulo=(16,213) 159x28 natural={'w': 147, 'h': 28} caja={'w': 147, 'h': 28} overflow=False truncado=False dropdown_ink_px=203 -> OK
  Cathode   tema_activo=Cathode   rotulo=(16,213) 199x29 natural={'w': 187, 'h': 29} caja={'w': 187, 'h': 29} overflow=False truncado=False dropdown_ink_px=288 -> OK
  Trail     tema_activo=Trail     rotulo=(16,213) 199x29 natural={'w': 187, 'h': 29} caja={'w': 187, 'h': 29} overflow=False truncado=False dropdown_ink_px=279 -> OK
  Aurora    tema_activo=Aurora    rotulo=(16,213) 176x28 natural={'w': 164, 'h': 28} caja={'w': 164, 'h': 28} overflow=False truncado=False dropdown_ink_px=207 -> OK

  codigo sin cambios por esta medicion (git status -- src tools): si
  parche local aplicado solo en /tmp/opencode/x6200-351-settings/mocks/mock_ui_deps.c (no commiteado)

PASS: `Capture filters` visible y sin recorte en los 6 temas
exit=0
```

Qué mide, exactamente:

- **Rótulo**: existe **una** vez por tema, cae en el viewport (`visible` no nulo, `(16,213)`), y
  `text_overflow=false`, `text_truncated=false`, sin glifos faltantes. El ancho natural del texto
  **iguala** la caja de contenido en los seis temas (188/161/147/187/187/164 px): no hay recorte
  ni siquiera de un píxel. Es la medida estructural del árbol LVGL vivo (`--inspect`), no OCR.
- **Valor del dropdown**: el texto de un `lv_dropdown` lo dibuja el widget y **no** aparece como
  nodo en el árbol, así que la única medida del valor visible es el píxel: la caja del dropdown
  está visible y dibuja tinta del valor actual en los seis temas (116–290 px de tinta con el mismo
  criterio de cobertura que `tests/check_widget_text.py`).
- **Capturas**: `captures/settings-audio-Aurora.png` (tema por defecto) y
  `captures/settings-audio-Sunlit.png`. Los seis PNG se regeneran con el mismo comando.
- **Idioma**: inglés, que es el literal del ROM; no hay traducción de esta fila.

**Las cinco opciones** (`Off`, `CW weak`, `SSB voice`, `AM broadcast`, `Digital`) **no** son
observables en el árbol: el widget dibuja su texto y el arnés no tiene una tecla que abra la lista
(no hay clave de escenario para `params.audio_enh_preset` ni para abrir el desplegable). Se
verifican por las dos fuentes que fijan el contrato:

```
$ sed -n '1411,1413p' src/dialog_settings.c
    obj = dropdown_uint8_custom_cb(grid, &params.audio_enh_preset,
                                   " Off \n CW weak \n SSB voice \n AM broadcast \n Digital",
                                   audio_enh_update_cb);
$ grep -n 'audio_enh_preset ' src/params/params.c
164:    .audio_enh_preset       = { .x = 0, .max = 4, .name = "audio_enh_preset", .voice = "Capture filters" },
```

`.max = 4` ⇒ cinco valores `0..4`, los cinco presets del contrato. `git diff 169bb807 HEAD --
src/dialog_settings.c` muestra que **la línea del dropdown no cambió**: sólo cambió el rótulo y el
comentario de la fila. Un test *visible* de la lista abierta queda como deuda: requiere una clave
de escenario nueva en `tools/ui_harness`, fuera del alcance y de la lista de archivos de esta
unidad.

**Parche local (no commiteado) y por qué**: `main` en `169bb807` **no linkea** el arnés. #343 hizo
que `src/dialog_bt.c` llame `bt_state_set_power()`/`bt_state_set_discoverable()` y el doble
`tools/ui_harness/mocks/mock_ui_deps.c` no los define, así que `ui_harness` y
`ui_harness_dialogs` fallan al linkear y los cuatro gates que renderizan quedan rojos (es
exactamente la causa de `wcag_rendered` y `widget_text` en §9.2). Para poder medir la fila, el
script trabaja sobre una **copia** de `tools/ui_harness` en un directorio scratch y agrega ahí los
dos setters. El worktree **no** se modifica: el propio script imprime
`git status --short --untracked-files=no -- src tools` vacío. No se commitea el parche porque es de
#343, no de #351.

### 9.4 Build ARM — `BLOCKED`

```
$ sh docs/evidencia/2026-10-29-audio-capture-pipeline-351/scripts/probe_arm_toolchain.sh
  imagen x6200-buildroot: ok
  /workspace/build (salida del build del proyecto): VACIA
  x6200-build-output: target/ con libspeexdsp y header: FALTA
  cross-compiler armhf en el host: no (binarios *gnueabihf* en /usr/bin: 0)

BLOCKED: no hay toolchain ARM del Buildroot disponible (toolchain, target-libspeexdsp, cross-host)
exit=77
```

Detalle de la causa, medida:

- la imagen `x6200-buildroot` existe y **trae las fuentes de Buildroot** (con 5.4 GB de caché en
  `/workspace/buildroot/dl`), pero el toolchain se **construye en runtime** y su salida
  (`/workspace/build`, bind del clon del owner) está **vacía**;
- `find / -xdev -name "*-gcc" -type f` dentro de la imagen devuelve sólo `/usr/bin/c89-gcc` y
  `/usr/bin/c99-gcc`: no hay ningún cross-compiler de target;
- el volumen `x6200-build-output` está vacío: no hay `target/` con `libspeexdsp` ni su header, así
  que tampoco se puede verificar el linking contra la lib del target;
- en el host no hay cross-compiler **armhf** (0 binarios `*gnueabihf*` en `/usr/bin`). El único
  `arm-*` es `arm-linux-gnu-gcc` **soft-float** y sin headers de libc
  (`fatal error: stdint.h: No such file or directory`), o sea que tampoco sirve como sustituto.

**No se compiló nada ARM, así que no hay `readelf -h` que mostrar.** El criterio del issue queda
`BLOCKED`, **no** PASS: un build de host (x86_64) no dice nada sobre la arquitectura del R16 y no
se presenta como si lo hiciera. El comando que lo construiría (no ejecutado acá) queda en
`commands.log` §5.

### 9.5 QEMU — `BLOCKED`

```
$ sh tests/qemu/run_apps_qemu.sh --check
  docker: ok | imagen x6200-buildroot: ok | python3: ok | PIL: ok | reporteador: ok
  guard #86: ok | libs del banco: ok | cliente de control (fuente): ok | binfmt qemu-arm: ok
  FALTA: el volumen x6200-next222-build no tiene /b/target/usr/sbin/x6200_gui (CONSTRUIR=si + BUILDROOT=<clon>)
  FALTA: falta el chroot privado (/tmp/x6200-next222-qemu/chroot): pasar ROOTFS=<rootfs.tar privada> o dejar el volumen con target/ (se arma solo)
SKIP: barrido QEMU sin entorno privado (2 requisito(s) faltante(s))
exit=77
```

El barrido QEMU corre el **binario ARM real** de la GUI; sin build ARM de esta revisión (§9.4) no
hay binario que correr, así que el flujo de captura con *source* sintético, Recorder,
decodificadores y Settings en QEMU queda `BLOCKED` por la misma causa. **Y aunque corriera**: QEMU
usa el userland ARM sobre el kernel del host, sin radio, sin codec y sin DSP del BASE, así que un
PASS de QEMU **no** demuestra la ruta electrónica BASE→parlante. Eso es `PENDING-HARDWARE`
(`hardware-handoff.md`).

### 9.6 Alcance del diff

`git diff --name-only 169bb807 HEAD` → **44 archivos** en el HEAD medido (`86bae74b`) y **49** en
total con el commit de la parte 3b, que agrega `hardware-handoff.md`, los dos scripts de esta
sección y los dos PNG de `captures/`. Todos caen dentro de: `src/audio.c`,
`src/audio_enhancement.{c,h}`, `src/dialog_settings.c`, `src/params/params.c`, `tests/native/**`,
`docs/AUDIO_ENHANCEMENT_GUIDE.md` y `docs/evidencia/2026-10-29-audio-capture-351/**`.

Verificado que **no** hay cambios en `docs/evidencia/2026-10-28-rx-audio-feasibility/**` (es de
#345), `src/radio.c`, `rootfs/`, `rom-build/`, ni nada de TX/PTT, volumen o A2DP.

## 10. Estado de los criterios de aceptación del issue, uno por uno

| # | Criterio del issue | Estado | Evidencia |
|---|---|---|---|
| 1 | Rama real desde `origin/main` verificada y worktree aislado; PR a `main` con `Closes #351` y SHA | **PASS (worktree) / PENDING (PR)** | Base `169bb807` verificada; worktree y rama `factory/audio-capture-pipeline-351` propios; el PR lo abre el orquestador con `Closes #351; Refs #345; Refs #347; Refs #349` |
| 2 | `pa_stream_peek` validado, source `const` jamás mutado, holes/empty/error/drop exactos; sin heap ni llamadas de salida en el callback | **PASS** | Stage de captura de la suite nativa (174 comprobaciones: `tests/native/test_audio_callback.c` + `gcc -fanalyzer` sobre `src/audio.c`): error→0 drops, empty→0 drops, hole→ceros+1 drop+reset del NR, dato `const` de 8820 y de más de 2× scratch, `nbytes` impar, error de drop; orden `screen_rec`→`dsp`; cero `malloc`; cero `pa_stream_write`/`x6200_control_*`/lock del mainloop |
| 3 | Entradas/salidas mono S16 44100 con cantidad y orden conservados, trama NR de 441 continua, sin cola cruda, priming de 10 ms una vez | **PASS (host)** | Suite nativa (particiones 1/440/441/442/257/4410/8820, N in = N out) + bench con lib real (`441 000 → 441 000` en los 36 casos, `measurements.csv`) |
| 4 | `Off` y `Digital` bit-exactos; CW/CWR sin NR de Speex; `audio_enh_preset` persistido sin variación | **PASS** | Suite nativa (0 muestras distintas en `Off`/`Digital`; CW conserva el nivelador sin NR) + bench (`Off` bit-exacto en los 12 casos) + `audio_enh_apply_mode_preset` no toca `params` |
| 5 | Signo Speex correcto (0→−10, 40→−18, 60→−22, 100→−30) y stub ABI congruente con la real | **PASS** | `probe_level_*` del bench: valores leídos de vuelta de `libspeexdsp 1.2.1-1`; stub con `SPEEX_PREPROCESS_SET_NOISE_SUPPRESS=18` y 441/44100 |
| 6 | Mutex único evita `run/ctl/destroy` concurrentes, startup no sobrescribe preset, sin deadlocks ni escrituras a radio/TX | **PASS por código / SIN TEST de carrera / PENDING (radio)** | `CODE`: un solo `pthread_mutex_t` estático (`src/audio_enhancement.c:45`) con helpers `*_locked`, y `audio_enh_init()` corre **antes** de `pthread_create()` (`src/audio.c:204` vs `:206`), así que la inicialización ya no puede pisar el preset del usuario. **La suite no tiene test de concurrencia** (no hay `pthread`/race en `tests/native/test_audio_enhancement.c`): la propiedad la garantiza la estructura del código, no está medida — se declara como hueco, no como PASS medido. Deadlocks en la radio: `PENDING-HARDWARE` (§11) |
| 7 | Tests con mock, particiones arbitrarias, sanitizers cuando estén disponibles, Speex REAL, build ARM, suite/QEMU y errores documentados con exit code | **PARCIAL** | Mock + particiones + Speex real + suite: PASS. Sanitizers: **no disponibles** en el toolchain del host (el runner lo declara). Build ARM: **`BLOCKED`** (§9.4). QEMU: **`BLOCKED`** (§9.5) |
| 8 | UI en inglés `Capture filters` y guía sin alegar NR audible en parlante ni superioridad no medida | **PASS** | §9.3 (6 temas, sin recorte) + `docs/AUDIO_ENHANCEMENT_GUIDE.md` reescrita (sin «llega al parlante», sin IC-7300, sin % de CPU, sin UI de notch) |
| 9 | Sin cambio de audio del BASE, filtros DFL, NR físico, TX/PTT, volumen, A2DP, DSP waterfall ni UI fuera de la fila citada | **PASS** | §9.6 (diff) + `test_audio_callback` exige cero llamadas de reproducción/ruteo; `src/radio.c`, `rootfs/` y `rom-build/` intactos; la cadena sólo escribe su buffer de captura |
| 10 | Handoff de QA físico preparado, no declarado PASS sin radio; nada integrado en release ni auto-merge | **PASS** | `hardware-handoff.md` `PENDING-HARDWARE` con checklist sin TX; sin deploy, sin flash, sin auto-merge |

Ningún criterio se declara PASS por inferencia: los que necesitan la radio son
`PENDING-HARDWARE` y los que necesitan el toolchain ARM o un binario ARM son `BLOCKED`, con la
causa y el comando exactos.

## 11. Handoff físico y cierre

- `hardware-handoff.md` define las series A–D (decodificadores LSB/USB/CW/CWR/FT8, grabadores
  AVI (`src/screen_rec.c`) y MP3 (`src/recorder.c`), cambios `Off`/`SSB`/`AM`/`Digital` + waterfall, CPU/p99 del callback, pops,
  underrun y `crashlog`) con su planilla, el checklist de seguridad (**sin TX**, sin cambiar
  modo/ruteo/mute/registros, sin flashear) y el estado `PENDING-HARDWARE` de cada ítem. **No se
  ejecutó nada de eso**: no se encendió la radio.
- Tres notas que un revisor tiene que leer juntas con la matriz:
  1. **Retardo de 882 muestras = 20 ms = 441 (máquina de tramas) + 441 (`libspeexdsp`).** Medido
     con ruido blanco: 882 con NR, 0 con `Off`; el retardo propio de la lib, medido con un
     `SpeexPreprocessState` directo, es 441 (`delay_nr_total_882`, `probe_speex_only_delay`). No es
     latencia extremo a extremo: la copia de captura no va al parlante.
  2. **`audio_enh_preset_reset()` no recrea el estado de `libspeexdsp`.** Descarta la trama
     pendiente del módulo, pero la estimación adaptativa de ruido de la lib sobrevive: dos pasadas
     del mismo input con un reset en el medio difieren en 433 735 muestras (max abs diff 1539 LSB)
     con `SSB voice`, y en 0 con `Off`. Por eso la **bit-exactitud entre particiones se exige y se
     cumple desde estado recién inicializado**, que es lo que fija el contrato; un «reset» no es un
     «re-init» (`preset_reset_no_reinit`, §4.1).
  3. **Seam de `AM broadcast`: +140 LSB.** El salto máximo entre muestras consecutivas en la
     salida es 3727 LSB contra 3587 LSB del propio input, en la grilla de 441: es el paso de
     ganancia del nivelador AF, que mide el pico por bloque. Con `SSB voice` el seam (3336) queda
     **por debajo** del salto del input. Se registra, no se oculta; el issue no exige
     bit-exactitud para AM (`seam_am_broadcast_plus_140`, §3).
- **Cierre**: el PR queda **no listo para merge** hasta que (a) exista un build ARM con el
  toolchain del Buildroot (§9.4) y (b) el handoff físico se ejecute en la radio (§11). Las dos
  condiciones están declaradas `BLOCKED`/`PENDING-HARDWARE`, no compensadas con un build de host
  ni con una afirmación de mejora audible.
