# RNNoise vs SpeexDSP vs OFF — banco A/B offline

- **Issue**: [iariap/x6200_gui#345](https://github.com/iariap/x6200_gui/issues/345), **anexo vinculante**
  «RNNoise experimental solicitado por el owner (2026-10-28)», tanda **D** (laboratorio A/B offline).
- **Qué es**: tres cadenas comparables —`off`, `speex`, `rnnoise`— medidas sobre los mismos cuatro
  vectores sintéticos, con **RNNoise upstream real** (Xiph, commit pinneado) compilado en contenedor
  aislado, **libspeexdsp real** y resampling 44.100 → 48.000 → 44.100 de verdad.
- **Qué NO es**: no es una feature, no hay ruta audible, no se tocó la radio ni `src/`, `rootfs/`,
  `tests/`. **No hay voz** en el banco y por lo tanto **no hay ninguna afirmación de inteligibilidad**.
  Todas las filas llevan `audio_destination = OFFLINE_CAPTURE_ONLY`; ninguna dice `SPEAKER`.
- **Resultado**: el experimento se reporta como **`RESEARCH/PROTOTYPE OFFLINE`**. Dictamen **live:
  NO-GO** (gate 1 del anexo, ver §0 y §10).

## 0. Dictamen en una tabla

| Gate del anexo | Estado en esta corrida | Evidencia |
|---|---|---|
| **1.** Ruta demostrada **RX → procesador → audio audible**, aislada, sin `x6200_control_record_set()` | **NO CUMPLE → NO-GO live** | 0 filas `MEASURED-HARDWARE`; el único procesamiento existe en la copia de captura (`src/audio.c:63`) y el único escritor del stream de reproducción es `audio_play()` (`src/audio.c:281`) |
| **2.** Presupuesto en **hardware** (waterfall/GUI activos): media < 50 % del frame, p99 < 10 ms, sin XRUN/clipping/caídas | **NO MEDIDO** en la radio | en este host la cadena RNNoise usa **1,95 %** del tiempo real por frame y p99 **0,271 ms** por 10 ms de PCM (§6.2), pero el host es x86_64 con C portable: no se extrapola al R16 |
| **3.** Control `Experimental NR` (Off/RNNoise, Off por defecto) | **NO EXISTE**, por diseño | el anexo prohíbe agregarlo en este issue |
| **4.** A/B ciego de inteligibilidad con habla HF débil y la misma cadena física | **NO MEDIDO** | no hay audio de voz con licencia/consentimiento en este entorno (`SKIPPED`, fila `rxd_voice_noise_ab`) |

**Dictamen**: con `MEASURED-HARDWARE` = 0 filas y sin ruta audible demostrada, el resultado **live es
NO-GO** por el gate 1, aunque el offline sea técnicamente sólido. El offline se clasifica
**`RESEARCH/PROTOTYPE OFFLINE`**, nunca feature. La habilitación de una opción experimental requiere
primero el gate 1 y después el gate 2 medidos en la radio.

## 1. Qué se midió y qué no

| Se midió | Clase | Dónde |
|---|---|---|
| Costo por frame de 10 ms, total y separado (resampling / RNNoise / Speex) | `MEASURED-HOST` | §6.2, §6.3, `rnnoise-vs-speex.csv` |
| Nivel RMS/peak, clipping, discontinuidades en empalmes, RSS pico | `MEASURED-HOST` | §6.1, §8 |
| Latencia por correlación, con el retardo alineado antes de comparar | `MEASURED-HOST` | §6.1, §8.2 |
| Bypass `off` bit-exacto, repetibilidad de hashes, prefijo de 10 s, fallback sin RNNoise | `MEASURED-HOST` | §7 |
| Commit, sha256 de fuentes/pesos/librería/binarios, licencia | `CODE` (artefacto verificable) | §2, `rnnoise-manifest.json` |
| Algoritmo, taps y latencia **teórica** del resampler | `ARITH` | §3.4 |
| Retardo de 2 frames y compuerta de silencio de RNNoise | `CODE` + `MEASURED-HOST` | §8.2, §4 |

| **No** se midió | Clase | Causa |
|---|---|---|
| Audio de voz + ruido | `SKIPPED` | no existe grabación de voz con licencia/consentimiento válido; el anexo prohíbe inventar voz o usar grabaciones de terceros |
| Inteligibilidad, naturalidad, pérdida de sílabas | `BLOCKED` | requiere A/B ciego con habla HF real y la radio |
| CPU/latencia en la radio (R16) | `BLOCKED` | otra arquitectura, otra carga, otra toolchain |
| Ruta audible RX → parlante | `BLOCKED` | no hay medición física en esta corrida |

## 2. Procedencia y pines

| Ítem | Valor |
|---|---|
| Fuente RNNoise | `https://github.com/xiph/rnnoise` @ **`70f1d256acd4b34a572f999a05c87bf00b67730d`** (es el `HEAD` que publica el repo: `git ls-remote` lo devolvió así, y el `git rev-parse HEAD` del clon coincidió) |
| Licencia | **BSD-3-Clause** (`COPYING` del commit, sha256 `45d37ca1cdb278c088e1aa85e0e65ca3a534ed86a28dcc96ca16810248a61d35`) |
| Modelo (pesos) | `https://media.xiph.org/rnnoise/models/rnnoise_data-0a8755f8e2d834eff6a54714ecc7d75f9932e845df35f8b59bc52a7cfe6e8b37.tar.gz` |
| sha256 del archivo de pesos | **`0a8755f8e2d834eff6a54714ecc7d75f9932e845df35f8b59bc52a7cfe6e8b37`** (58.603.099 bytes) |
| Cómo se fijó | el commit trae `model_version` con ese hash y `download_model.sh` lo compara; la corrida usó **el script oficial** y registró `checksums match` (`rnnoise-build.log`) |
| Pesos dentro del tarball | `src/rnnoise_data.c` sha256 `d6021b76…9be5` (78.003.012 bytes) y `src/rnnoise_data.h` sha256 `09ff880b…5855` |
| Redistribución | **no** se versionan ni se redistribuyen; quedan fuera del repo y fuera de la ROM (`rnnoise-manifest.json`: `vendored_in_repo=false`, `redistributed_in_repo=false`, `in_firmware=false`) |
| Nota de licencia del tarball | no trae archivo de licencia propio: se distribuye desde Xiph como parte del proyecto, cuyo `COPYING` de ese commit es BSD-3-Clause. Al no redistribuirse, acá no se dispara obligación de redistribución |
| `rnnoise_get_frame_size()` | **480** (verificado en ejecución; `src/denoise.h:31` define `FRAME_SIZE 480`) |
| Build | `./autogen.sh` + `./configure --disable-examples --disable-x86-rtcd --disable-shared --enable-static CFLAGS=-O2` + `make` |
| `librnnoise.a` | sha256 `e0272f1d342ee208cd37bd8e2d529752400b9adf5c84fd5a5d3ff9dee02c6474` (3.629.566 bytes) |
| Contenedor | `debian:bookworm-slim@sha256:99c0b25c46d0214d5a2091092e7908907dcaecd6a962cde0ac700acee222b6f3` + `build-essential autoconf automake libtool git ca-certificates wget pkg-config libspeexdsp-dev=1.2.1-1` |
| Compilador | `gcc (Debian 12.2.0-14+deb12u1) 12.2.0`, flags `-O2 -std=c11 -Wall -Wextra -Werror` |
| Speex | `libspeexdsp 1.2.1-1`; `src/audio_enhancement.c` copiado **verbatim** (sha256 `acab3aad…6b25f`) |
| Arquitectura / CPU del banco | `x86_64`, AMD Ryzen 7 PRO 8840HS (16 hilos) — **no es la radio** |
| Binarios del banco | `bench_rnnoise` sha256 `ed0d0001…62ad`; `bench_rnnoise_absent` sha256 `90a502bc…f1f2` |

**Por qué `--disable-x86-rtcd`**: el default de ese commit ya es sin rtcd; se declara explícito para que
la ruta medida sea la **C portable** (determinista y comparable con un build ARM), no la que despacha
AVX2/SSE4.1 según la CPU del host. El build igual emite `#warning "Only SSE and SSE2 are available"`
(`src/nnet.h:163`), que es informativo y no afecta la comparación entre cadenas.

## 3. Las tres cadenas (y dos sondas)

Unidad de trabajo: **441 muestras de entrada = 10,000 ms a 44.100 Hz**, que mapean **exacto** a 480
muestras a 48.000 Hz (`441·160/147 = 480`) y de vuelta. Cada unidad se cronometra con
`CLOCK_MONOTONIC`; la salida se escribe sólo en la primera repetición de cada pasada (estado fresco).

### 3.1 `off`
`memcpy` de la unidad: **sin resampling**, sin tocar una muestra. Es el control obligatorio: la salida
tiene que ser bit-exacta a la entrada (§7.1).

### 3.2 `speex`
`audio_enh_process(buf, 441)` de `src/audio_enhancement.c` **copiado verbatim** y enlazado contra
libspeexdsp real, con `AUDIO_ENH_PRESET_SSB_VOICE` (preset 2). Con callbacks de 441 muestras,
`apply_noise_reduction()` procesa **un** frame de 256 y deja **185 muestras (41,95 %) sin NR**
(`src/audio_enhancement.c:293-317`), igual que en producción con 4410. Se agrega la sonda
`speex_b256` (un frame por callback) sólo para medir la latencia sin empalme.

### 3.3 `rnnoise`
`resample 44.1→48` → **`rnnoise_process_frame()` en tramas de 480** (escala S16, §4) → `resample 48→44.1`.
El estado (`rnnoise_create(NULL)`) se crea una vez por pasada y **no** se resetea entre repeticiones.

### 3.4 El resampler: algoritmo, taps y latencia

| Parámetro | 44.100 → 48.000 | 48.000 → 44.100 |
|---|---|---|
| Factor racional | L = 160, M = 147 | L = 147, M = 160 |
| Prototipo | FIR polifásico de `L·T = 10.240` coeficientes | `L·T = 9.408` coeficientes |
| Taps por fase | **T = 64** | T = 64 |
| Ventana | Kaiser **β = 8,0** (~80 dB de stopband) | idem |
| Corte | `0,5·min(fs_in,fs_out)` = **22.050 Hz**, normalizado a la tasa interpolada: `fc = min(1,L/M)/(2L)` | idem |
| Normalización | cada fase se normaliza a suma 1 (ganancia DC exacta) | idem |
| Latencia de grupo (`ARITH`) | `(T−1)/2 = 31,5` muestras de entrada = **0,714 ms** | `31,5` muestras de 48 k = 0,656 ms → **28,9** muestras de 44,1 k |
| Latencia total teórica | **60,4 muestras = 1,370 ms** | |
| Latencia total **medida** por correlación | **61 muestras = 1,3832 ms** (vector de ruido) | |

La fórmula del corte es la que evita el error clásico: con `fc = 0,5·min(1, L/M)` (sin dividir por
`L`) el prototipo queda con corte en la mitad de la tasa interpolada, es decir **all-pass**, y el
"resampler" se degrada a repetir/descartar muestras — exactamente lo que el anexo prohíbe. El banco usa
la forma correcta y la validación numérica previa dio: RMS preservado (razón 0,977 sobre ruido blanco),
energía por fase ≈ 1, retardo de impulso 61 muestras. Efecto lateral documentado: la banda
**20,3–22,05 kHz** queda atenuada, así que sobre ruido blanco la correlación con la entrada baja a
**0,769** (§8.4).

### 3.5 Sondas
- **`resample`**: sólo ida y vuelta del resampler, sin NR → costo y latencia del resampling aislados.
- **`speex_b256`**: Speex con 256 muestras por callback (un frame completo, sin empalme) → latencia
  única de la cadena Speex. Es una sonda: 1 pasada, sin protocolo de 5 repeticiones.

## 4. Escala S16: por qué la cadena principal no divide por 32768 (y qué mide el control)

El anexo pide «amplitudes de RNNoise en escala de PCM S16 (no floats [-1,1] sin conversión)» y también
menciona `/32768`. Las dos lecturas no son equivalentes y el banco resuelve la ambigüedad **midiendo
las dos**, con la evidencia de código del propio commit:

- El entrenamiento usa amplitudes **de short crudo**: `src/dump_features.c:440-446` normaliza cada
  señal a RMS ≈ 3000 y recorta a ±32767, o sea que la red ve la escala S16.
- El demo upstream pasa los `short` **sin escalar** (`examples/rnnoise_demo.c:60-63`).
- Con amplitudes ~1,0 la energía por banda cae bajo el umbral de silencio `E < 0,04`
  (`src/denoise.c:389-392`): la red no corre y la salida es el passthrough enmarcado.

Medición (vector de ruido, 60 s):

| Cadena | RMS salida | Costo RNNoise p50 | Interpretación |
|---|---|---|---|
| `rnnoise` (escala S16) | **−62,563 dBFS** (entrada −30,009) | **0,159 ms/frame** | la red corre y atenúa 32,6 dB |
| `rnnoise_norm` (`/32768`) | **−30,144 dBFS** (entrada −30,009) | **0,036 ms/frame** | la red se saltea: passthrough + high-pass + 2 frames de retardo |

Por eso la cadena principal usa escala S16 y `rnnoise_norm` queda **sólo como control**: con `/32768`
la cadena no denoisa nada. El control además sirve de testigo del retardo (§8.2): su correlación en el
lag medido (0,7688) es idéntica a la del resampler solo (0,7690), lo que confirma que es un passthrough
desplazado.

## 5. Protocolo

| Ítem | Valor |
|---|---|
| Señales | los 4 vectores de `gen_pcm.py` (semilla 6200, 10 s): silencio, seno 1 kHz −18 dBFS, mezcla 300/1000/2400 Hz, ruido blanco −30 dBFS |
| Duración medida | **60 s** = 6 repeticiones del corpus de 10 s, **sin resetear estado** entre repeticiones |
| Warmup | **3** pasadas completas descartadas |
| Repeticiones medidas | **5** pasadas completas |
| Reloj | `CLOCK_MONOTONIC` (`clock_gettime`), una lectura por etapa por frame |
| Percentiles | nearest-rank sobre las 6.000 unidades de 10 ms de cada pasada; se reporta la mediana de las 5 pasadas |
| Memoria | `VmHWM` (RSS pico) y `VmRSS` al inicio/fin |
| Estado | una pasada = un proceso: el estado se crea al empezar y no se resetea entre las 6 repeticiones |

## 6. Resultados

### 6.1 Niveles, discontinuidades y latencia (60 s por caso)

| vector | método | RMS dBFS | peak dBFS | clip | discontinuidades | latencia ms | p50 ms | p95 ms | p99 ms | total 60 s ms | RSS kB | % >10 ms |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| silence | off | UNKNOWN | UNKNOWN | 0 | 0 | 0,0000 | 0,00000 | 0,00000 | 0,00000 | 0,490 | 3064 | 0,0000 |
| silence | resample | UNKNOWN | UNKNOWN | 0 | 0 | 0,0000 | 0,03402 | 0,03492 | 0,03956 | 209,965 | 3576 | 0,0000 |
| silence | speex | UNKNOWN | UNKNOWN | 0 | 0 | 0,0000 | 0,01016 | 0,01033 | 0,01364 | 62,929 | 3472 | 0,0000 |
| silence | rnnoise | UNKNOWN | UNKNOWN | 0 | 0 | 0,0000 | 0,06797 | 0,07177 | 0,08741 | 421,456 | 6412 | 0,0000 |
| silence | rnnoise_norm | UNKNOWN | UNKNOWN | 0 | 0 | 0,0000 | 0,06851 | 0,07229 | 0,09083 | 424,064 | 6424 | 0,0000 |
| sine1k | off | −21,010 | −18,001 | 0 | 0 | 0,0000 | 0,00000 | 0,00000 | 0,00000 | 0,391 | 3104 | 0,0000 |
| sine1k | resample | −21,011 | −18,001 | 0 | 0 | UNKNOWN | 0,03411 | 0,03654 | 0,04936 | 213,770 | 3588 | 0,0000 |
| sine1k | speex | −24,529 | −17,979 | 0 | **965** | UNKNOWN | 0,01080 | 0,01133 | 0,02564 | 68,150 | 3464 | 0,0000 |
| sine1k | rnnoise | **−75,821** | −36,684 | 0 | 0 | UNKNOWN | 0,19205 | 0,21800 | 0,28338 | 1206,860 | 7088 | 0,0000 |
| sine1k | rnnoise_norm | −21,001 | −17,739 | 0 | 0 | UNKNOWN | 0,06990 | 0,07401 | 0,10114 | 432,121 | 6332 | 0,0000 |
| multisine | off | −25,782 | −18,077 | 0 | 0 | 0,0000 | 0,00000 | 0,00000 | 0,00000 | 0,433 | 3056 | 0,0000 |
| multisine | resample | −25,782 | −18,085 | 0 | 0 | UNKNOWN | 0,03415 | 0,03665 | 0,05197 | 213,354 | 3584 | 0,0000 |
| multisine | speex | −29,482 | −17,994 | 0 | **1849** | UNKNOWN | 0,01091 | 0,01241 | 0,01546 | 68,810 | 3476 | 0,0000 |
| multisine | rnnoise | **−59,539** | −26,135 | 0 | 0 | UNKNOWN | 0,19306 | 0,21860 | 0,26672 | 1211,859 | 7092 | 0,0000 |
| multisine | rnnoise_norm | −25,770 | −17,790 | 0 | 0 | UNKNOWN | 0,06904 | 0,07239 | 0,07998 | 426,458 | 6252 | 0,0000 |
| noise | off | −30,009 | −25,228 | 0 | 0 | 0,0000 | 0,00000 | 0,00000 | 0,00000 | 0,267 | 3084 | 0,0000 |
| noise | resample | −30,150 | −20,002 | 0 | 0 | **1,3832** | 0,03598 | 0,03765 | 0,04412 | 222,375 | 3600 | 0,0000 |
| noise | speex | −33,456 | −25,228 | 0 | 0 | UNKNOWN | 0,01299 | 0,01350 | 0,01728 | 80,428 | 3460 | 0,0000 |
| noise | rnnoise | **−62,563** | −45,350 | 0 | 0 | **21,3832** | 0,19522 | 0,22161 | 0,27087 | 1222,502 | 7092 | 0,0000 |
| noise | rnnoise_norm | −30,144 | −19,894 | 0 | 0 | **21,3832** | 0,07180 | 0,07545 | 0,08637 | 443,171 | 6292 | 0,0000 |
| noise | speex_b256 (sonda) | −43,834 | −25,310 | 0 | 0 | **5,8050** | UNKNOWN | UNKNOWN | UNKNOWN | 141,057 | 3668 | 0,0000 |

Lectura:
- **Cero clipping** en las 21 filas y **0 % de frames sobre 10 ms** en este host.
- `silence` tiene RMS `UNKNOWN` porque el RMS lineal es 0 y el dBFS no está definido (no es un dato
  faltante): las cinco cadenas devuelven el silencio bit-exacto.
- **`discontinuidades`** cuenta posiciones de empalme internas (borde de callback de 441 y, para Speex,
  el corte de 256 dentro del callback) cuyo salto supera el máximo salto de la **misma salida** fuera de
  esos empalmes (±3 muestras). Se autoclaibra, no depende de un umbral en dB.
- La latencia se mide **sólo** con el vector de ruido: en los vectores periódicos el pico de correlación
  cae en cualquier múltiplo del período y no identifica el retardo (`UNKNOWN` con causa, no un número
  inventado).

### 6.2 Tiempos por frame de 10 ms

Ya en la tabla anterior; resumen por cadena (vector de ruido, la peor condición de las cuatro):

| Cadena | p50 | p95 | p99 | total 60 s | % del tiempo real | RSS pico |
|---|---|---|---|---|---|---|
| `off` | 0,00000 ms | 0,00000 ms | 0,00000 ms | **0,267 ms** | 0,0004 % | 3.084 kB |
| `speex` (441) | 0,01299 ms | 0,01350 ms | 0,01728 ms | **80,428 ms** | 0,134 % | 3.460 kB |
| `resample` | 0,03598 ms | 0,03765 ms | 0,04412 ms | **222,375 ms** | 0,371 % | 3.600 kB |
| `rnnoise` | 0,19522 ms | 0,22161 ms | 0,27087 ms | **1.222,502 ms** | **2,04 %** (p50 = 1,95 % del frame) | 7.092 kB |

### 6.3 Costo separado por frame (p50, vector de ruido)

| Cadena | resampling subida | RNNoise | resampling bajada | Speex | suma |
|---|---|---|---|---|---|
| `off` | 0 | 0 | 0 | 0 | 0,00000 ms |
| `resample` | 0,01748 ms | 0,00008 ms¹ | 0,01839 ms | 0 | 0,03598 ms |
| `speex` (441) | 0 | 0 | 0 | **0,01299 ms** | 0,01299 ms |
| `rnnoise` | 0,01760 ms | **0,15907 ms** | 0,01789 ms | 0 | 0,19522 ms |
| `rnnoise_norm` | 0,01730 ms | 0,03603 ms | 0,01823 ms | 0 | 0,07180 ms |

¹ en `resample` la etapa "RNNoise" es la copia del buffer intermedio: no hay red.

Relaciones medidas: **RNNoise cuesta 15,0× Speex** y **5,5× el resampling**; el resampling es el 18 %
del costo de la cadena RNNoise pero **2,8×** el costo de la cadena Speex. La conversión S16→double cuenta
en la subida y la vuelta en la bajada, así que `subida + RNNoise + bajada = total`.

## 7. Controles obligatorios

### 7.1 `off` bit-exacto
Los cuatro vectores: **sha256 de la salida = sha256 de la entrada** y **0 muestras distintas**
(882.000 bytes cada uno). Ver `rnnoise-bench.json` → `controls.off_bit_exacto`.

| vector | sha256 salida `off` | = entrada | muestras distintas |
|---|---|---|---|
| silence | `ff793c626048816a52aa19c55232c5226be1f990b3b028ddc435877fc2676547` | sí | 0 |
| sine1k | `d28a1dffa08349ec1cb5602e3e2dec0a0c2cc74b8ecf6b56d0021104d936378d` | sí | 0 |
| multisine | `2b552525ad8f3ac4e174b9c848b51cc63cad400a9d0ddd49e82e714e1e961b36` | sí | 0 |
| noise | `b718cf2c118cfb465a0e7894a7eaa7baa23719406736e494d0fbf7fcd1aaa56e` | sí | 0 |

**Verificación cruzada con el corpus de la tarea B**: los WAV `off` commiteados en `audio/` son
**byte a byte idénticos** a los fixtures de entrada cuyo sha256 está anclado en `pcm-manifest.json`:
`audio/noise_off_10s.wav` = `white_noise_44100_s16.wav` = `b60926922b9734790cfe9d7817cd391d1d1bf07283dbecdff413e7d7f727f936`
y `audio/multisine_off_10s.wav` = `multisine_44100_s16.wav` = `b7afb34f75f84b4914b1db7363614539ca27842720aae71ca8c3b5c5067d75bf`.
Es decir: la cadena `off` no sólo conserva el hash dentro de la corrida, sino que reproduce exactamente
el fixture ya publicado en la tarea B.

### 7.2 Repetibilidad con misma entrada y misma versión
| Cadena | sha256 pasada | sha256 repetición | iguales |
|---|---|---|---|
| `speex` | `6fd6f885fb024a68384a9c61aefdd853f7ed0044361471d714fcf798e7ab4a79` | idem | **sí** |
| `rnnoise` | `c15b60235e1839980bc0d1ff5c531c0374012ae7bef44efd6ae9f7e77c41a5c1` | idem | **sí** |

### 7.3 El prefijo de 10 s es una pasada con estado fresco
Los primeros 441.000 samples de la pasada de 60 s son **idénticos** a una pasada de 10 s independiente
(`speex` y `rnnoise`), y esa pasada de 10 s es repetible. Prueba que el estado arranca limpio en cada
pasada y que las 6 repeticiones comparten estado sólo dentro de la pasada.

### 7.4 Fallback sin RNNoise: vuelve a `off`, sin silencio ni archivo corrupto

Se probaron **dos** formas de faltante, y las dos dan salida bit-exacta:

| Caso | Cómo se simula | exit | `method_effective` | bytes | RMS salida | RMS entrada |
|---|---|---|---|---|---|---|
| `noise_rnnoise_absent` | binario `bench_rnnoise_absent` compilado **sin header ni librería** de RNNoise (`-DX6200_BENCH_NO_RNNOISE`, sin `-lrnnoise`; `ldd` no muestra rnnoise) | 0 | `off` | 882.000 | 1035,09 | 1035,09 |
| `silence_rnnoise_absent` | idem, vector silencio | 0 | `off` | 882.000 | 0,0 | 0,0 |
| `noise_rnnoise_simulated_missing` | binario normal con `--simulate-missing` (falla de `rnnoise_create`) | 0 | `off` | 882.000 | 1035,09 | 1035,09 |

En los tres casos la salida es **byte a byte igual a la entrada** (`cmp` OK en el contenedor y control
en el JSON), el tamaño es el correcto y el RMS se preserva: no hay silencio artificial ni archivo
truncado. Además la matriz principal no tuvo ningún fallback (`controls.matriz_sin_fallback = true`).

## 8. Artefactos observables

Ninguno de estos artefactos se declara audible: son propiedades de la señal de salida medidas en el
host. La audibilidad requiere el gate 1.

### 8.1 Primer frame en cero (10 ms de silencio al inicio de cada pasada)
`rnnoise_process_frame()` sintetiza el frame **anterior** desde `st->delayed_X`
(`src/denoise.c:496-498`), que arranca en cero: la primera trama de 480 muestras de salida es
**exactamente 0** (480 de 480 muestras nulas, RMS 0,0). El demo upstream descarta el primer frame por
eso (`examples/rnnoise_demo.c:63`). En una implementación real haría falta un *priming*; acá se
documenta como artefacto del estado frío.

### 8.2 Retardo real de 2 frames (20 ms), no de 1
- `CODE`: el análisis arma la ventana con `[analysis_mem, in]` (`src/denoise.c:332-339`) y la síntesis
  usa `delayed_X` (`:496-498`): **dos** memorias de un frame.
- `MEASURED-HOST`: **943 muestras = 21,3832 ms** con correlación 0,4334 en el vector de ruido.
  943 = **882** (2 frames de 480 a 48 k, expresados a 44,1 k) + **61** (resampler). El control
  `rnnoise_norm`, que es un passthrough desplazado, mide el mismo lag 943 con correlación 0,7688 —
  idéntica a la del resampler solo (0,7690) — lo que cierra la cuenta.
- Consecuencia para el gate 2: 21,4 ms de latencia algorítmica **más** la latencia de la ruta que
  todavía no existe; el anexo no fija un presupuesto de latencia, pero es un dato que el coordinador
  necesita antes de cualquier diseño.

### 8.3 RNNoise trata un tono puro como ruido
| vector | atenuación RMS de `rnnoise` | de `speex` (441) |
|---|---|---|
| sine1k (−18 dBFS) | **−54,8 dB** | −3,5 dB |
| multisine | **−33,8 dB** | −3,7 dB |
| noise (−30 dBFS) | **−32,6 dB** | −3,4 dB |

Es coherente con que el modelo esté entrenado para voz general: un tono no es voz y se suprime. No es
una virtud ni un defecto por sí mismo, pero **descarta** presentar RNNoise como «mejora» sin habla real:
en CW/tonos de batido se comería la señal. El anexo ya excluye CW/digital del runtime.

### 8.4 Empalmes de la cadena Speex con callbacks de 10 ms
Con 441 muestras por callback, la salida alterna **256 muestras procesadas** (con retardo de un frame)
y **185 muestras crudas**. Eso produce:
- **965** empalmes discontinuos de 1.999 en el seno 1 kHz y **1.849** en el multisine (saltos de hasta
  3.900 LSB contra 588 LSB de slew natural del seno);
- atenuación efectiva de sólo **−3,4 dB** sobre ruido, porque el 42 % del audio no pasa por el NR;
- latencia **sin valor único** (dos retardos superpuestos: 0 y 441 muestras; la correlación da 0,5088 en
  el lag 0 y 0,4062 en 441).
Con un frame por callback (`speex_b256`) la cadena sí es un retardo limpio de **256 muestras =
5,8050 ms** y atenúa **−13,8 dB**. Es la misma aritmética de la tarea B, ahora medida sobre estos
vectores.

### 8.5 El resampler sube el pico del ruido y atenúa el borde de banda
- Peak de ruido: −25,228 dBFS (entrada) → **−20,002 dBFS** (resample). El ruido uniforme se vuelve
  aproximadamente gaussiano al pasar por el FIR: el pico sube ~5,2 dB **sin clipping** (0 muestras).
- Correlación con la entrada: **0,769** por la atenuación de 20,3–22,05 kHz (§3.4). Afecta por igual a
  todas las cadenas resampleadas, así que la comparación entre ellas sigue siendo válida.

### 8.6 Memoria
RSS pico: **7.092 kB** con RNNoise contra **3.460 kB** con Speex (Δ ≈ 3,6 MB, consistente con el
modelo compilado en `librnnoise.a`, 3.629.566 bytes). Es un dato de host; en la ROM el modelo pesaría lo
mismo en `.rodata` más el estado de `DenoiseState`, cuyo tamaño **no** se midió en esta corrida (`UNKNOWN`).

## 9. Limitaciones (explícitas)

1. **No hay voz**: los cuatro vectores son sintéticos y deterministas. Miden integridad de
   procesamiento, **no** inteligibilidad, naturalidad ni pérdida de sílabas. El vector voz + ruido está
   `SKIPPED` con causa: no existe audio de voz con licencia/consentimiento válido en este entorno y el
   anexo prohíbe inventar voz o usar grabaciones de terceros.
2. **Host ≠ radio**: x86_64 (Ryzen 7 PRO 8840HS) con C portable. El R16 es otra arquitectura y otra
   carga. Ningún número de CPU/latencia se extrapola.
3. **El resampler es del banco**, no el de la radio: 64 taps por fase, Kaiser β = 8, corte 22,05 kHz.
   Otra elección de taps cambiaría costo y latencia (la latencia de grupo es `(T−1)/2` muestras por
   etapa).
4. **Ruido blanco ≠ ruido HF**: el ruido de banda completa no representa QRM, impulsos ni ruido de
   banda angosta de una fonía SSB/AM.
5. **Latencia por correlación sólo en el vector de ruido**; en los periódicos es ambigua por
   construcción.
6. **Speex se midió en dos configuraciones** (441 y 256 por callback). La de 441 es el caso comparable
   con la unidad de 10 ms y también el peor caso de empalmes; no es la configuración de producción
   (4410), que la tarea B ya midió.
7. **`rnnoise_norm` no es una cadena candidata**: es el control de la conversión de escala (§4).
8. **`MEASURED-HARDWARE` = 0 filas**. Nada de este documento es una medición de la radio.

## 10. Dictamen GO/NO-GO (como pide el anexo)

**Offline: viable como prototipo de investigación.** RNNoise upstream compila, corre, es determinista
(hashes repetibles), cae a `off` sin romper nada si falta la librería, y en este host su costo es
**1,95 % del tiempo real** por frame de 10 ms con p99 0,271 ms. No hay clipping ni frames sobre 10 ms en
ninguno de los 21 casos.

**Live: NO-GO.** El gate 1 no se cumple: no existe ruta demostrada **RX → procesador → audio audible**.
Lo único que la GUI procesa hoy es su copia de captura (`src/audio.c:63`) y el único escritor del stream
de reproducción es `audio_play()` (`src/audio.c:281`), cuyos llamadores son audio generado por la GUI.
Que el parlante lo alimente el DSP del BASE sigue siendo `HYPOTHESIS` (ver `README.md` §4.1 y §6). Por lo
tanto, aunque el offline «suene» bien, no habilita ninguna opción en la radio.

**Qué haría falta para pasar a una etapa posterior** (no autorizado por este documento):
1. Gate 1 en hardware: demostrar una ruta audible aislada, sin `x6200_control_record_set()` y sin tocar
   TX/mic/BT, con un preview **offline** de WAV fuera del equipo mientras tanto.
2. Gate 2 en hardware: media < 50 % del frame y p99 < 10 ms **con waterfall/GUI activos**, sin XRUN,
   clipping, caídas de audio ni aumento de latencia del PTT. Ojo con el retardo medido de **21,4 ms** de
   la cadena RNNoise (§8.2) al evaluar el presupuesto total.
3. Gate 4: A/B ciego con habla HF débil, misma cadena física, con operador; medir sílabas/indicativos,
   naturalidad, artefactos y QRM. RNNoise es una red entrenada para voz general: no hay evidencia de
   superioridad en HF.

## 11. Reproducir

```sh
cd <worktree>                                  # state/worktrees/x6200/rx-audio-feasibility
EV=docs/evidencia/2026-10-28-rx-audio-feasibility

# 1) dependencia upstream FUERA del repo (queda en /tmp, no se versiona)
git clone https://github.com/xiph/rnnoise /tmp/opencode/rnnoise-src
git -C /tmp/opencode/rnnoise-src checkout 70f1d256acd4b34a572f999a05c87bf00b67730d

# 2) banco completo (docker build + build de RNNoise + matriz + analisis)
python3 $EV/scripts/bench_rnnoise.py

# 3) solo re-analizar los artefactos ya producidos
python3 $EV/scripts/bench_rnnoise.py --analyse

# 4) controles
python3 $EV/scripts/gen_pcm.py --verify
python3 $EV/scripts/check_no_stub_claims.py
```

El banco requiere docker (el host no tiene libspeexdsp ni RNNoise) y descarga el modelo de 56 MB en el
contenedor, además del toolchain de autotools. Los `.raw` de 60 s y los binarios quedan en `artifacts/rnnoise/` (gitignored); el
JSON ancla todos los sha256.

## 12. Archivos de esta tanda

| Archivo | Qué contiene |
|---|---|
| `rnnoise-vs-speex.md` | este documento |
| `rnnoise-vs-speex.csv` | 21 filas (4 vectores × 5 métodos + sonda), columnas pedidas por el anexo |
| `rnnoise-bench.json` | métricas crudas por caso, controles completos, toolchain, hashes |
| `rnnoise-manifest.json` | commit, licencia, URL y sha256 del modelo, build, `frame_size`, flags de no redistribución |
| `audio/` | **6 WAV** de 10 s: `multisine` y `noise` × `off`/`speex`/`rnnoise` + `sha256sums.txt` |
| `scripts/bench_rnnoise.py` | orquestador + análisis + verificación de controles |
| `scripts/bench/bench_rnnoise.c` | banco C (resampler, 5 métodos, timing, JSON) |
| `scripts/bench/rnnoise_build.sh` | build verificado + matriz dentro del contenedor |
| `scripts/bench/Dockerfile.rnnoise` | imagen pinneada por digest con el toolchain de autotools |

**Por qué 6 WAV y no 21**: el mínimo pedido es un vector × 3 cadenas. Se commitean **dos** vectores
porque responden preguntas distintas: `noise` es donde el efecto del NR es medible/escuchable y donde la
correlación da un retardo inequívoco, y `multisine` es donde los empalmes de Speex se ven en la forma de
onda. Son 5,3 MB en total (882 KB por WAV de 10 s); el resto de las salidas (60 s) queda hasheado en el
JSON y se regenera determinísticamente con el script.
