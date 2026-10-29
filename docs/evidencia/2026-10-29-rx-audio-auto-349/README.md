# #349 — Benchmark objetivo de inteligibilidad RX con voz y SpeexDSP

Banco **offline, determinista y reproducible** para comparar el pipeline de captura de
`src/audio_enhancement.c` contra bypass y dos pasabandas proxy, sobre el **mismo audio** de voz.
Reemplaza el protocolo de escucha asistida original de #349; **no** mide el audio audible del
BASE, no elige un nivel de NR y no cierra la validación subjetiva.

**Estado: `DONE_AUTOMATED`.** El A/B del NR/DFL real del BASE queda
**`BASE_NR_DFL_NOT_MEASURED`** y se retoma desde el 2026-11-12 con el operador
([`operator-handoff.md`](operator-handoff.md)).

## Resultado en una mirada

- **288/288 filas válidas** = 12 clips × 3 SNR (0/5/10 dB) × 2 interferentes (AWGN / tono QRM
  2650 Hz) × 4 brazos. Sin escenarios inválidos, sin clipping, sin NaN/inf.
- **STOI mediano:** `N_OFF` 0,9084 · `N_SPEEX` **0,9203** · `DX_OFF` 0,8769 · `DX_SPEEX` 0,8847.
- **`N_SPEEX` vs `N_OFF`:** mediana pareada **+0,0199** (IC95 % bootstrap por speaker
  [0,0153, 0,0230]); **95,8 %** de los 72 escenarios con delta > 0. ESTOI mediano
  `N_OFF` 0,7514 → `N_SPEEX` 0,7891.
- **DX**: el pasabanda 300–2400 Hz reduce STOI frente a Normal (mediana −0,0224); con Speex
  queda −0,0085. Es el compromiso espectral esperado, no una mejora del equipo.
- **CPU host:** p50 0,114 ms y p99 0,160 ms por callback de 4410 muestras (100 ms de audio);
  máximo 0,795 ms. Es host x86_64, **no** el Cortex-A7 de la radio.
- **Controles:** Off sin filtro bit-exacto; Off con filtro proxy bit-exacto en las 144 filas Off;
  filtro proxy determinista; Speex con fragmentación 441/4410/irregular con el mismo hash;
  STOI(limpio, limpio) ≈ 1; retardo inyectado recuperado; segunda pasada idéntica
  (`determinism.json` → `equal: true` en `aggregate.json`).

**Lectura honesta:** en estas muestras controladas, la cadena SpeexDSP `SSB voice` conserva más
inteligibilidad estimada que el bypass y el pasabanda DX pierde frente a Normal. **No** es una
medición de comprensión humana, **no** es el DSP del BASE y **no** autoriza a declarar una mejora
audible en el parlante.

## Alcance y lo que NO responde

| Pregunta | Estado |
|---|---|
| ¿El pipeline SpeexDSP vigente preserva STOI/ESTOI frente a bypass en audio controlado? | **MEASURED-HOST** |
| ¿Qué compromiso tiene un pasabanda proxy tipo Normal/DX bajo ruido y QRM de laboratorio? | **MEASURED-HOST / SIMULATED_PASSBAND** |
| ¿El NR real 10/30/60 del BASE mejora la escucha? | **BASE_NR_DFL_NOT_MEASURED** |
| ¿El DFL real del BASE mejora el parlante? | **BASE_NR_DFL_NOT_MEASURED** |
| ¿La salida del parlante/jack/A2DP? | **NOT_VERIFIED** (sin instrumentación de salida física) |

El proxy Butterworth `Normal`/`DX` **no es** el filtro DSP cerrado del BASE. `parec` captura antes
de `audio_enh_process` (#359), por lo que este banco usa el driver del módulo, no la radio.

## Corpus y licencia

- Fuente única: **LibriSpeech `test-clean`**, OpenSLR SLR12,
  `https://www.openslr.org/resources/12/test-clean.tar.gz`, **CC BY 4.0**.
- MD5 publicado: `32fa31d27d2e1cad72775fee3f4849a9`; **MD5 calculado = mismo**.
- SHA256 calculado: `39fde525e59672dc6d1551919b1478f724438a95aa55f874b576be21967e6c23`.
- Tamaño: 346.663.984 B; descargado 2026-10-29T21:22:22Z; archivo y corpus **fuera del
  repositorio** (no se versiona ningún WAV/FLAC).
- Selección determinista: primeros 4 speaker IDs lexicográficos con ≥ 3 clips de 4–12 s;
  primeros 3 clips id-ordenados por speaker → **12 clips**. Metadatos, transcripts y hashes en
  [`manifest.json`](manifest.json). Son lecturas de inglés de LibriSpeech, no fonía de
  radioaficionados.

## Método

- Conversión 16 kHz mono → 44.100 Hz con `scipy.signal.resample_poly(x, 441, 160)`; S16LE con
  saturación y sin wrap. `clean_normal = Normal(voz)` una sola vez; la entrada de los cuatro brazos
  es `voz + ruido`.
- Escenarios: SNR `{0,5,10}` dB, `AWGN` seed `6200+scenario_index` y `QRM_TONE` 2650 Hz fase 0;
  ruido normalizado en las ventanas activas (20 ms, umbral −35 dB relativo al máximo del clip).
  Factor único por escenario si el pico supera 0,95; sin normalización por brazo.
- Brazos sobre la **misma entrada**:
  `N_OFF = Normal(input) + Off`, `N_SPEEX = Normal(input) + SSB voice`,
  `DX_OFF = DX(input) + Off`, `DX_SPEEX = DX(input) + SSB voice`.
- Procesador **SpeexDSP real 1.2.1** del contenedor de #351, con `src/audio_enhancement.c`
  **copiado verbatim** (sha256 en `manifest.json`), `audio_enh_init()` +
  `audio_enh_preset_load()` por fila, chunks de 4410 muestras y último chunk corto permitido.
  No hay stub de Speex ni una segunda implementación.
- STOI/ESTOI con `pystoi` contra `clean_normal`; offsets predefinidos desde el impulso:
  `N_OFF=0`, `N_SPEEX=882`, `DX_OFF=12`, `DX_SPEEX=894` muestras; misma ventana útil
  (guard 441) en todos los brazos. Sin búsqueda de retardo por clip.
- Secundarias: RMS in/out, pico, clipping, DC, muestras, residual en no-voz, pérdida de energía
  300–2400 y 2400–2850 Hz, supresión del tono QRM a 2650 Hz, CPU p50/p99 del driver host.
- Agregados: medianas, p10/p90, delta pareado vs `N_OFF`, proporción delta > 0 y bootstrap
  **por speaker** (4 unidades, 1000 remuestras, seed 6200, IC95 %). Con 4 speakers la
  incertidumbre es amplia; los 72 escenarios no son 72 hablantes independientes.

## Controles y gates

| Control | Resultado |
|---|---|
| Off sin filtro vs copia (bit-exacto) | **PASS** |
| Off con filtro proxy, 144 filas | **PASS** (0 diferencias) |
| Filtro proxy determinista (mismos hashes al repetir) | **PASS** |
| Speex, fragmentación 441 / 4410 / `1,440,441,442,257`, proceso nuevo | **PASS** (mismo hash) |
| STOI(limpio, limpio) ≈ 1 | **PASS** (0,9999999999999998) |
| Retardo inyectado de 882 muestras recuperado | **PASS** |
| Segunda pasada completa: hashes y métricas | **PASS** (`determinism.equal = true` en `aggregate.json`) |
| Clipping / NaN / longitudes | **PASS** (0 clipping, 0 NaN) |
| Test negativos (`scripts/test_benchmark.py`) | **9 passed** |

Filas completas en [`measurements.csv`](measurements.csv); estadísticas, bootstrap y controles en
[`aggregate.json`](aggregate.json); hashes, corpus, dependencias y offsets en
[`manifest.json`](manifest.json).

## Preflight de radio (sólo lectura)

Se ejecutó un preflight **read-only** el 2026-10-29T21:26:11Z; no se cambió nada en la radio:

| Campo | Valor |
|---|---|
| Sistema | `Linux X6200 6.1.82 #1 SMP PREEMPT armv7l` |
| GUI sha256 | `4ee87aefb389b6a6b5d2d90df2c51d0440aeff19ed4464d44649f48757dd66b0` |
| GUI md5 | `80bbe5bd70b345e1e9a563d06d047a1c` |
| CPU | ARMv7 Processor rev 5 (v7l) |
| Estado | 7.143,7 MHz LSB, DFL Normal 150/2850, NR 1/80, vol 52, `tx=false`, `crashlog 0` |
| PulseAudio | source `alsa_input...` RUNNING; sink `alsa_output...` RUNNING; `audio_enh_preset=0` |

El gate de hardware queda **`BASE_NR_DFL_NOT_MEASURED`**: no hay fuente PCM posterior al DSP del
BASE ni salida audible instrumentada. No se alteró NR/NB/DFL, modo, VFO, frecuencia, volumen, AGC,
RF, BT/A2DP, ALSA, mixer, PulseAudio ni la persistencia. No hubo TX y no se grabó RX de terceros.

## Limitaciones explícitas

1. STOI/ESTOI son **intrusivos** y no equivalen a comprensión humana ni a MOS; no están
   calibrados para HF.
2. El corpus es **inglés leído** de LibriSpeech, sin canal HF, sin QSB, sin AGC ni nivelador.
3. `QRM_TONE` es un interferente de laboratorio, no una señal HF capturada.
4. El proxy `Normal`/`DX` **no es** el filtro del BASE; no se puede convertir este resultado en una
   recomendación de NR/DFL del equipo.
5. El CPU es del host x86_64, no del Cortex-A7; no se extrapola a la radio.
6. Bootstrap con **4 speakers**: IC amplios; no es una muestra poblacional.
7. No se incluyeron WER/ASR ni POLQA/PESQ/DNSMOS (fuera de alcance del issue).
8. El incidente de Settings (centro negro) sigue **UNRESOLVED** y preexistente; este banco no toca
   UI ni firmware.

## Reproducción

```sh
# 1) corpus fuera del repo (URL + MD5 oficiales)
OUT=<scratch>/349-corpus sh scripts/fetch_corpus.sh

# 2) dependencias (Python 3.14.8)
python3 -m venv <scratch>/349-venv
<scratch>/349-venv/bin/pip install -r scripts/requirements.txt

# 3) driver real de SpeexDSP en el contenedor de #351
RX349_SCRATCH=<scratch>/349-auto \
  REPO=<worktree> OUT=<scratch>/349-auto/bench sh scripts/build_speex_driver.sh

# 4) tests negativos
<scratch>/349-venv/bin/python -m pytest -q scripts/test_benchmark.py

# 5) matriz completa (contenedor persistente para velocidad)
docker run -d --rm --name rx349-bench -v <scratch>/349-auto:/data:rw -w /data \
  x6200-speexdsp-capture-bench:1.2.1-1 tail -f /dev/null
RX349_BENCH_CONTAINER=rx349-bench RX349_SCRATCH=<scratch>/349-auto \
  <scratch>/349-venv/bin/python scripts/run_benchmark.py \
  --corpus-dir <scratch>/349-corpus/LibriSpeech/test-clean --scratch <scratch>/349-auto
docker rm -f rx349-bench

# 6) correr la suite de pruebas del proyecto (no hay código de producción modificado)
python3 tests/audita-pruebas.py   # no se tocó docs/PRUEBAS.md
```

Comandos exactos y códigos de salida: [`commands.log`](commands.log).

## Archivos

- [`measurements.csv`](measurements.csv) — 288 filas (escenario × brazo).
- [`aggregate.json`](aggregate.json) — medianas, deltas, bootstrap, CPU, controles, determinismo.
- [`manifest.json`](manifest.json) — corpus/licencia/hashes, clips, seeds, filtros, offsets,
  dependencias, Speex y fuente.
- [`commands.log`](commands.log) — comandos y exit status.
- [`operator-handoff.md`](operator-handoff.md) — una página para retomar la escucha del BASE
  después del 2026-11-12.
- `scripts/` — `run_benchmark.py`, `test_benchmark.py`, `speex_driver.c`,
  `build_speex_driver.sh`, `fetch_corpus.sh`, `requirements.txt`.
