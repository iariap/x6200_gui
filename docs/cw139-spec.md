# Spec — #139: auto-ajuste de los parámetros del decodificador CW

Fecha: 2026-10-18. Rama: `factory/cw-autotune-139`. Base: `7222abae` (trunk tip).
Implementación: subagente coder. Reporte final con evidencia:
[`docs/evidencia/2026-10-18-cw139-banco/`](evidencia/2026-10-18-cw139-banco/README.md).

## Contexto

Hoy los cuatro parámetros del decodificador CW son manuales y fijos (`src/cfg/cfg.c`,
`src/mfk.cpp`, `src/buttons.cpp`):

| Parámetro | Dónde | Default | Rango | Qué hace |
|---|---|---|---|---|
| `cw_decoder_snr` | botón `Dec SNR` en `(CW 1:2)` | 5,0 dB | 3,0–30, paso 0,1 | margen sobre el piso para declarar marca |
| `cw_decoder_snr_gist` | no expuesto | 1,0 dB | — | histéresis entre marca y silencio |
| `cw_decoder_peak_beta` | botón `Peak beta` en `(CW 2:2)` | 0,10 | 0,10–0,95, paso 0,01 | retención del LPF del umbral de pico |
| `cw_decoder_noise_beta` | botón `Noise beta` en `(CW 2:2)` | 0,80 | 0,10–0,95, paso 0,01 | retención del LPF del piso de ruido |

`src/cw.cpp` ya mide, por trama de FFT: piso de ruido suavizado (`noise_filtered`), nivel de
marca (`rms_db_max`), relación señal/ruido espectral (`sum_signal/noise`) y el pico del espectro
(`peak_freq`); el decodificador mide WPM (`cw_decoder_calc_wpm`, `src/cw_decoder.c`). El AUTO usa
esos datos: **no agrega DSP ni toca el camino de RF/TX**.

La lógica va en un **módulo C puro** (`src/cw_autotune.c/.h`) porque `src/cw.cpp` no se compila en
la suite nativa (necesita liquid-dsp y el hilo de audio): así las fórmulas, las guardas y la banda
muerta se prueban con vectores sintéticos, sin radio.

## Diseño

### Módulo

`src/cw_autotune.h` / `src/cw_autotune.c` (C, sin LVGL, sin `cfg`, sin radio):

```c
void cw_autotune_reset(cw_autotune_state_t *st);
void cw_autotune_compute(cw_autotune_state_t *st,
                         const cw_autotune_stats_t *in,   /* medidas de la ventana */
                         cw_autotune_result_t *out);      /* sugerencia + flags */
float cw_autotune_tau_peak_ms(int wpm);                   /* 600/WPM ms */
float cw_autotune_percentile(const float *v, int n, float q);  /* rango más cercano */
float cw_autotune_mean(const float *v, int n);
float cw_autotune_variance(const float *v, int n);        /* poblacional */
```

`in` trae: `noise_db`, `mark_db` (percentil 90 de la ventana), `noise_var_db2`,
`mark_var_db2`, `signal_ratio` (máximo `sum_signal/noise` de la ventana), `block_dt_s`, `peak_freq_hz`,
`wpm`, `mark_valid`, `peak_valid`.

`out` trae: `adapt` (la ventana sirve), `apply_*` por parámetro (superó la banda muerta) y los
valores `snr`, `gist`, `peak_beta`, `noise_beta`, `freq_offset_hz` (corrección absoluta a sumar al
centro del DDS del decodificador RX).

### Integración en `src/cw.cpp`

- Una ventana de ~5 s (`AUTOTUNE_WINDOW_MS`) acumula, por trama de FFT (~5,4 Hz), una muestra de
  marca (`rms_db_max`, solo en tramas con señal), una del piso (`min(-3, rms_db_min)`, solo en
  tramas sin señal) y una del pico (`peak_freq`). La ventana se acumula **siempre**; se evalúa solo
  con AUTO encendido. Eso deja la **calibración inmediata** al encender: la ventana ya está lista.
- Al cerrar la ventana, con `at_mark_n >= 3` y `at_noise_n > 0`, arma el `cw_autotune_stats_t`,
  llama al módulo y escribe **solo los `apply_*`** con `subject_set_float` a los mismos subjects de
  los botones/MFK (`cfg.cw_decoder_snr`, `cfg.cw_decoder_snr_gist`, `cfg.cw_decoder_peak_beta`,
  `cfg.cw_decoder_noise_beta`).
- El tono va **solo al DDS del decodificador RX**: `dds_dec_init()` usa
  `key_tone + rx_tone_offset`; `cfg.key_tone` (el tono de TX, que `src/radio.c` manda a la radio)
  **no se escribe nunca**. El offset vive en `static float rx_tone_offset` y vuelve a 0 al apagar
  AUTO.
- WPM: se usa la medida por el decodificador (`cw_decoder_get_wpm()`, nuevo); si todavía no midió
  (0) o se fue del rango 5–60, se usa `cw_get_wpm()` (la velocidad configurada del keyer).
- `cw_decoder_auto` es un `std::atomic<bool>` (lo escribe el hilo de la GUI y lo lee el del DSP).

### UI y configuración

- Nuevo `cfg.cw_decoder_auto` (bool, default `false`, persistido como los demás).
- Botón **`Auto: On/Off`** en `page_cw_decoder_2` (`(CW 2:2)`), a la derecha de Peak/Noise beta:
  rótulo `src/buttons.cpp` (`cw_auto_label_getter`), toggle
  `controls_toggle_cw_decoder_auto` (`src/controls.cpp`), sujeto `cfg.cw_decoder_auto`.
- También asignable al encoder como sus vecinos (`MFK_CW_DECODER_AUTO`, **append-only** al final
  del enum para no correr los bits persistidos de `params.mfk_modes`; caso en `src/mfk.cpp` y
  nombre en `src/knobs.c`).
- Con AUTO apagado no se escribe nada: los valores manuales se usan tal cual (comportamiento
  idéntico al de hoy). Al apagarlo quedan los últimos valores aplicados, visibles y editables.

## Fórmulas y fundamento numérico

### El bloque de FFT manda el rango temporal

`FFT=128` sobre el audio decimado `44100/64 = 689,0625 Hz` (`src/cw.cpp:34-36`), y las tramas no se
solapan:

```
Δ = 128 / 689,0625 = 0,18576 s = 185,76 ms
    689,0625 / 128 = 5,38 tramas/s
```

`update_thresholds()` corre una vez por trama, así que **todo LPF de `cw.cpp` se actualiza cada
185,76 ms**; una constante de tiempo menor que eso no existe en este lazo.

### `snr`: punto medio marca/piso, techo 10 dB

```
snr = clamp(0,5 · (marca_db − piso_db), 3, 10)      // clamp final 3..30 del MFK
gist = clamp(0,25 · snr, 0,5, 3)
```

El umbral que usa el decodificador es `piso + snr` y la marca observada está en `piso + Δ`. Poner
`snr = Δ/2` deja el umbral **a mitad de camino**: 6 dB de margen para declarar marca y 6 dB de
margen contra el pico, que es el punto con más tolerancia a QSB en ambos sentidos. El techo de
10 dB evita que un pico muy fuerte (Δ=40 dB → 20) alargue la cola de la señal: por encima de 10 dB
de margen el decodificador no gana nada y sí pierde inmunidad al ruido. `gist = snr/4` da la
histéresis en la misma proporción (1,0 dB con el `snr` default de 5).

| Δ = marca − piso | `0,5·Δ` | `snr` aplicado | `gist` |
|---|---|---|---|
| 4 dB | 2,0 | **no adapta** (guarda de 6 dB) | — |
| 6 dB | 3,0 | 3,0 | 0,8 |
| 8 dB | 4,0 | 4,0 | 1,0 |
| 10 dB | 5,0 | 5,0 | 1,3 |
| 16 dB | 8,0 | 8,0 | 2,0 |
| 20 dB | 10,0 | 10,0 | 2,5 |
| 40 dB | 20,0 | **10,0** (techo) | 2,5 |

(`gist` se cuantiza al paso de 0,1 dB con el que se persiste, `src/cfg/cfg.c`.)

### `peak_beta`: `1 − Δ/τ`, con τ = medio punto — y por qué satura

```
τ_peak = 0,5 · (1200/WPM) ms = 600/WPM ms
peak_beta = clamp(1 − Δ/τ_peak, 0,10, 0,95)
```

La forma sale de la recurrencia del LPF de `src/util.cpp`: `x[n] = β·x[n−1] + (1−β)·hoy`; en
régimen, `1−β = Δ/τ`. Un lazo de medio punto sigue la envolvente de la piqueta sin llegar a
"perseguir" cada punto. **Con el bloque real, el resultado cae siempre en el clamp inferior**:

| WPM | punto (ms) | τ_peak (ms) | Δ/τ | `1−Δ/τ` | aplicado |
|---|---|---|---|---|---|
| 5 | 240 | 120 | 1,55 | −0,55 | **0,10** |
| 10 | 120 | 60 | 3,10 | −2,10 | **0,10** |
| 20 | 60 | 30 | 6,19 | −5,19 | **0,10** |
| 30 | 40 | 20 | 9,29 | −8,29 | **0,10** |
| 40 | 30 | 15 | 12,38 | −11,38 | **0,10** |
| 60 | 20 | 10 | 18,58 | −17,58 | **0,10** |

Es un hecho del lazo, no un error de cuenta: **ningún τ menor que un bloque se puede resolver**
(ver [`HALLAZGOS.md` 53](HALLAZGOS.md#53-el-auto-ajuste-de-peak-beta-no-puede-variar-con-la-wpm-el-bloque-de-fft-es-mas-largo-que-medio-punto)).
El valor resultante (0,10) es el default del firmware y el más rápido que el bloque permite: el AUTO
no puede empeorar un ajuste manual de `peak_beta` y queda documentado como limitación, en vez de
inventar una constante de tiempo que el lazo no puede cumplir. La fórmula está probada igual: con el
bloque **sin decimar** (128/44100 = 2,90 ms, el que daría la tabla que el issue describe) da
0,95 @10 WPM, 0,90 @20, 0,81 @40 y 0,71 @60, y el test lo fija.

### `noise_beta`: estabilidad del piso

```
σ = sqrt(varianza del piso)
noise_beta = clamp(0,90 − 0,10 · (σ − 1 dB), 0,50, 0,90)
```

Un piso nervioso hay que seguirlo rápido (β chico = más peso a la muestra nueva); uno estable se
puede suavizar mucho (β grande). La recta ancla 0,90 en σ=1 dB (el caso estable) y baja 0,10 por
cada dB de más, con piso 0,50 y techo 0,90:

| σ del piso | `noise_beta` | lectura |
|---|---|---|
| 0,5 dB | 0,90 (techo) | estable |
| 1 dB | 0,90 | estable |
| 2 dB | 0,80 | apenas nervioso |
| 3 dB | 0,70 | nervioso |
| 5 dB | 0,50 | muy nervioso |
| 8 dB | 0,50 (piso) | ruido de banda ancha |

### `freq_offset`: signo medido, paso lento y tope

El mapeo de `peak_freq` (`src/cw.cpp:261`) **devuelve el offset con el signo invertido**; medido con
liquid 1.4.0 en el contenedor del arnés
([`signo-peak-freq.c`](evidencia/2026-10-18-cw139-banco/signo-peak-freq.c),
[`signo-peak-freq.txt`](evidencia/2026-10-18-cw139-banco/signo-peak-freq.txt)):

```
tone  +100.0 Hz -> bin  19 -> formula -102.28 Hz
tone  -100.0 Hz -> bin 109 -> formula +102.28 Hz
```

Por eso la corrección que aplica AUTO es `−peak_freq` (si el tono está 100 Hz abajo, hay que bajar
el centro 100 Hz, no subirlo). Reglas:

- solo con `peak_valid` (hubo pico en la ventana) y dentro de la misma guarda de señal;
- acumulativa sobre el offset ya aplicado, **tope ±300 Hz** (el ancho del filtro es 500 Hz);
- **≤ 30 Hz por ventana** (lento: 5 s por paso);
- banda muerta 10 Hz (un bloque del widget de tune, `BLOCK_HZ`), para no hacer vibrar el bloque;
- el DDS se reconstruye con `key_tone + rx_tone_offset`; `cfg.key_tone` no se toca.

### Guardas comunes

- **Solo con señal**: `signal_ratio > 1` (máximo de la ventana) y Δ ≥ 6 dB. Sin eso no se escribe
  **nada** (ni se pierde lo manual).
- **Banda muerta**: 10 % del rango — SNR 2,7 dB, gist 0,25, betas 0,085, tono 10 Hz.
- **Clamps**: `snr` 3–30 (formula 3–10), betas 0,10–0,95, `gist` 0,5–3; idénticos a los del MFK.
- **Sin oscilación**: un salto entre ventanas mayor que 2 × banda muerta se promedia con la
  sugerencia anterior (QSB, golpe de ruido). La banda muerta después decide si se escribe.
- **Ventana sin piso** (portadora continua, ninguna trama sin señal): no adapta; sin piso no hay
  SNR que medir.

## Tabla de casos (vectores de la suite nativa)

| Caso | Entrada (ventana) | Salida esperada |
|---|---|---|
| Señal fuerte | piso −80, marca −40, σ_piso 0,5, ratio 50, WPM 20, pico +100 | `snr` 10, `gist` 2,5, `peak_beta` 0,10, `noise_beta` 0,90, `freq` −30 (primer paso) |
| Señal débil | piso −80, marca −73 (Δ=7), σ_piso 1, pico +20 | `snr` 3,5, `gist` 0,9, `freq` −20 |
| Sin señal | ratio 0,9 / 1,0 / marca inválida | `adapt=false`, ningún `apply` |
| SNR insuficiente | Δ=4 dB | `adapt=false` |
| Ruido nervioso | σ 2 / 5 / 8 dB | `noise_beta` 0,80 / 0,50 / 0,50 |
| QSB | ventana 1 `snr` 10, ventana 2 `snr` 4 | ventana 2 aplica (10+4)/2 = 7; la 3.ª aplica 4 |
| Banda muerta | aplicado 10, sugerencia 9,8 | `apply_snr=false`, se conserva 10 |
| Clamps | Δ=6,2 / Δ=60 | `snr` 3,1 / 10 |
| τ por WPM | 10 / 20 / 30 / 60 | 60 / 30 / 20 / 10 ms |
| `peak_beta` | bloque real, WPM 5..60 | 0,10 en todos (HALLAZGOS 53) |
| `peak_beta` | bloque 2,90 ms, 10 / 40 WPM | 0,95 / 0,81 |
| Signo del tono | pico +100 / −100 | corrección −30 / +30 |
| Tope del tono | pico +1000, 10 ventanas | −300 (30 Hz por ventana) |
| Pico inválido | `peak_valid=false` | conserva el offset, `apply_freq=false` |
| NaN | marca NaN | `adapt=false` |
| Reinicio | reset y misma ventana | vuelve a aplicar todo |

Cada fila es una aserción en `tests/native/test_cw_autotune.c` (100 comprobaciones). RED real (con
un stub que devolvía ceros) y GREEN: logs en la evidencia.

## Pruebas y resultados

_(se completan con la salida real de cada gate; ver el README de la evidencia)_

| Qué | Cómo | Resultado |
|---|---|---|
| Nativo TDD | `tests/native/run_native_tests.sh` | RED 70 fallos → GREEN 18 621 comprobaciones, 0 fallos |
| Estáticos | los 8 de `check-matrix.sh` | 8/8 |
| Determinismo | `tests/check_harness_determinism.sh` | — |
| Contratos | `uv run x6200-ab contracts --revision <HEAD>` | — |
| A/B `AB-CW139` | `uv run x6200-ab compare --a 7222abae --b <HEAD>` | — |
| Framework | ruff / format / ty / pytest `not e2e` | — |
| ARM | cross-compile del banco | — |
| Banco QEMU | botón + toggle + persistencia | — |
| Hallazgos | `audita-hallazgos.sh` | — |

## Invariantes

- No se toca RF/TX ni el DSP de transmisión, ni `cfg.key_tone` (tono de TX), ni el modo, ni `lvgl/`.
- Sin AUTO: los valores manuales se usan tal cual; el DDS queda en `key_tone`.
- Clamps y rangos del MFK intactos; los `cfg_item_t` siguen siendo la única fuente de verdad.
- Las fórmulas y guardas viven en C puro y se prueban sin radio.
- Commits `Pablo Iaria <iariap@gmail.com>`; sin push.

## Pendientes

- **requires-radio**: convergencia real con una señal de CW en el aire (procedimiento en el
  README de la evidencia). El banco no tiene audio de radio: valida el botón, el toggle y la
  persistencia, no la convergencia.
- **Calibración del techo de `snr` (10 dB) y de la guarda de 6 dB** con señales reales y QSB.
- Si la radio muestra que `peak_beta` a 0,10 hace chatter con señales fuertes, la alternativa es
  subir τ_peak a un múltiplo del bloque (p. ej. 8·Δ → β≈0,88); hoy el lazo no lo justifica y el
  valor coincide con el default.
