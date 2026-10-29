# Bench de la captura con SpeexDSP real — issue #351, parte 3a

- **Issue**: [iariap/x6200_gui#351](https://github.com/iariap/x6200_gui/issues/351) — punto 4 de
  «Tests y regresión» y «Etiquetas honradas» (contrato D). Esta es la **parte 3a de 3**.
- **Base**: `169bb807b0086dbb91d29a9ce6770d25ca396859` (`origin/main` al arrancar la unidad).
- **Rama**: `factory/audio-capture-pipeline-351` · **Worktree**:
  `state/worktrees/x6200/audio-capture-pipeline-351`.
- **Commits de la unidad**: `98de5e1b` (parte 1: init sincrónico, mutex único, NR de tramas de
  441), `83a0da6d` (parte 2: callback de captura seguro) y el commit de esta parte 3a.
- **Qué es**: la evidencia del bench con **`libspeexdsp` real** sobre `src/audio_enhancement.c`
  copiado **verbatim**, más el registro del rótulo `Capture filters` y de la guía honesta.
- **Qué NO es**: no mide calidad audible, inteligibilidad, la ruta RX→parlante ni el CPU del
  R16. Las tres siguen `BLOCKED` (ver §6). **No se tocó `src/audio.c` ni
  `src/audio_enhancement.{c,h}`** (ya venían de las partes 1 y 2), ni
  `docs/evidencia/2026-10-28-rx-audio-feasibility/**`, ni la radio.
- **Parte 3b** (otra sesión): matriz completa de tests, build ARM, handoff físico y PR. Acá no
  se corre `tests/run_all_tests.sh --quick` ni el build ARM.

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
| `measurements.csv` | 48 filas: 36 casos de la matriz, 3 `partition-check` con reset, 4 sondas de nivel, 1 sonda `speex_only`, 4 `BLOCKED` |
| `bench-results.json` | métricas crudas por caso, invariancia por partición, chequeos de reset, mapa de nivel, toolchain e imagen |
| `pcm-manifest.json` | sha256, RMS, peak, clipping y duración de los 4 WAV sintéticos |
| `commands.log` | comandos exactos, salidas y exit codes (build, matriz, análisis) |
| `scripts/gen_pcm.py` | generador determinista (semilla 6200, 10 s, stdlib) + `--verify` |
| `scripts/bench_capture.py` | orquestador: build, corrida en contenedor, análisis, CSV/JSON/log |
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
| ruido blanco | `Off` | 441000 / 441000 | **0** | −30.01 dBFS | +0.00 dB | −25.23 dBFS | 0 | 3189 / 3466 / 3587 | <1 µs | ≈1.0 ms |
| ruido blanco | `SSB voice` | 441000 / 441000 | **882** | −41.87 dBFS | −11.86 dB | −25.30 dBFS | 0 | 3336 / 3336 / 3587 | ≈440 µs | ≈26 ms |
| ruido blanco | `AM broadcast` | 441000 / 441000 | **882** | −39.32 dBFS | −9.32 dB | −24.29 dBFS | 0 | 3727 / 3727 / 3587 | ≈425 µs | ≈28 ms |
| multisine 300/1k/2.4k | `Off` | 441000 / 441000 | — (ambiguo) | −25.78 dBFS | +0.00 dB | −18.08 dBFS | 0 | 715 / 715 / 715 | <1 µs | ≈1.0 ms |
| multisine 300/1k/2.4k | `SSB voice` | 441000 / 441000 | — (ambiguo) | −38.22 dBFS | −12.44 dB | −18.15 dBFS | 0 | 709 / 710 / 715 | ≈480 µs | ≈24 ms |
| multisine 300/1k/2.4k | `AM broadcast` | 441000 / 441000 | — (ambiguo) | −38.98 dBFS | −13.20 dB | −18.15 dBFS | 0 | 709 / 710 / 715 | ≈520 µs | ≈24 ms |
| seno 1 kHz | `Off` | 441000 / 441000 | — (ambiguo) | −21.01 dBFS | +0.00 dB | −18.00 dBFS | 0 | 586 / 586 / 588 | <1 µs | ≈1.0 ms |
| seno 1 kHz | `SSB voice` | 441000 / 441000 | — (ambiguo) | −33.54 dBFS | −12.53 dB | −18.06 dBFS | 0 | 580 / 581 / 588 | ≈480 µs | ≈24 ms |
| seno 1 kHz | `AM broadcast` | 441000 / 441000 | — (ambiguo) | −34.33 dBFS | −13.32 dB | −18.06 dBFS | 0 | 580 / 581 / 588 | ≈360 µs | ≈23 ms |

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
