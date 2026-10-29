# Auditoría numérica de SpeexDSP en la cadena de RX

- **Issue**: [iariap/x6200_gui#345](https://github.com/iariap/x6200_gui/issues/345) — tarea **B** (auditoría numérica + banco PCM reproducible).
- **Base**: `a1c667eee6d4c138416e5ec459dd86de93cb630f` (= `origin/main` al arrancar la tarea A). Commit de la tarea A: `3ab3330a`.
- **Worktree**: `state/worktrees/x6200/rx-audio-feasibility`, rama `factory/rx-audio-feasibility`.
- **Qué es**: la aritmética del recorte por frames de `audio_enh_process()`, la latencia declarada vs la real,
  el mapa `level → suppression` verificado contra **libspeexdsp real** en un contenedor, y lo que los tests
  actuales sí y no prueban.
- **Qué NO es**: no se tocó `src/`, `rootfs/`, `tests/`, la radio, PulseAudio ni ALSA. Ninguna señal es voz:
  son tonos y ruido sintéticos, así que **nada de esto mide inteligibilidad**. No se propone NR nuevo, ni
  presets, ni UI, ni routing.

## Clases de evidencia

| Clase | Significado acá |
|---|---|
| `CODE` | Leído en el árbol (`archivo:línea`) o en el fuente de libspeexdsp 1.2.1 (tarball pinneado, sha256 en §8). |
| `ARITH` | Derivación aritmética sobre constantes de `CODE`; se muestra la cuenta, no sólo el resultado. |
| `MEASURED-HOST` | Medido en este host x86_64 con libspeexdsp real dentro de un contenedor. **No es la radio.** |
| `HYPOTHESIS` | Inferencia sin medición que la cierre. |
| `BLOCKED` | Se quiso medir y no se pudo; se dice por qué. |

Las filas correspondientes están en `measurements.csv` (columnas exactas del issue) y el detalle crudo en
`bench-results.json` + `artifacts/bench/container.log`.

## 0. Resumen

1. **Recorte por frames** (`CODE` + `ARITH`): con el callback nominal de 4410 muestras, el NR procesa
   17×256 = 4352 y **deja 58 sin procesar** (1.32% por buffer). Con 8820 quedan 116 (1.32%). Con 1 muestra
   quedan todas: el NR **nunca corre**.
2. **Latencia** (`ARITH` + `MEASURED-HOST`): el frame declarado a 48 kHz son 5.333 ms; a la tasa real de
   44.1 kHz son **5.805 ms** (8.84% más). Además, SpeexDSP usa WOLA con solapamiento y su salida está
   **retrasada exactamente un frame (256 muestras = 5.805 ms)**, medido por correlación sobre ruido blanco
   (lag 256; control OFF: lag 0 y salida bit-exacta). El código no compensa ese retraso.
3. **Mapa de level** (`CODE` + `ARITH` + `MEASURED-HOST`): `-30 + level*20/100` con división entera da 21
   valores distintos, paso de 5 niveles por dB, de −30 dB (level 0) a −10 dB (level 100). La atenuación
   **medida** sobre ruido blanco va de −13.5 dB a −8.5 dB: el techo configurado no se alcanza y **más level
   = menos supresión**, al revés de lo que sugiere la etiqueta.
4. **Polaridad** (`MEASURED-HOST`): `SET_NOISE_SUPPRESS = +30` y `= −30` dan el **mismo** resultado
   (readback −30, atenuación −14.94 dB en ambos). La lib normaliza a −|x|: pasar un positivo no produce
   ganancia. La convención negativa del código es correcta.
5. **Tests actuales** (`CODE` + `MEASURED-HOST`): el stub atenúa 6 dB fijos y usaba **ids de request
   distintos de los reales** (`SET_NOISE_SUPPRESS` = 1 vs 18; alineado con el ABI real el 2026-10-29 por
   #351, ver errata en §5). Un PASS prueba el troceado, el tail y los clamps; **no**
   prueba atenuación, calidad, CPU ni latencia.
6. **CPU** (`MEASURED-HOST`, x86_64): 60 s de audio en 104–137 ms (0.17–0.23% del tiempo real); máximo por
   callback de 100 ms: 1.1–2.1 ms de wall (incluye posible preempción). **No extrapolable al R16.**
7. **Defecto medible del recorte**: el tail no procesado **no está retrasado** mientras el resto sí (1 frame),
   y además sale del stream interno de Speex. Resultado: un salto de hasta **2216 LSB** en el borde de cada
   callback con un seno de 1 kHz cuyo slew natural es 588 LSB (3.8×). Que eso sea audible no está probado.

---

## 1. Aritmética del recorte por frames

### 1.1 De dónde sale el recorte (`CODE`)

```
293  static void apply_noise_reduction(float *samples, size_t nsamples) {
294      if (!g_audio_enh.nr.enabled || !g_audio_enh.nr.speex_state) return;
296      /* SpeexDSP runs on fixed 256-sample frames, so only complete frames are
297       * processed; a partial tail (up to 255 samples) is passed through. */
298      size_t n = nsamples - (nsamples % AUDIO_ENH_FRAME_SIZE);
300      for (size_t off = 0; off < n; off += AUDIO_ENH_FRAME_SIZE) {
```
`src/audio_enhancement.c:293-317`. `AUDIO_ENH_FRAME_SIZE` = 256 (`src/audio_enhancement.h:16`).
La función recibe **chunks de hasta 256** porque `audio_enh_process()` trocea el buffer del callback:

```
354      while (nsamples > 0) {
355          size_t chunk = (nsamples > AUDIO_ENH_FRAME_SIZE) ? AUDIO_ENH_FRAME_SIZE : nsamples;
```
`src/audio_enhancement.c:354-355`.

Consecuencia algebraica, para un buffer de `B` muestras:

```
frames_completos = floor(B / 256)
procesadas       = 256 * floor(B / 256)      = B - (B mod 256)
sin_procesar     = B mod 256
fraccion_intacta = (B mod 256) / B
```

### 1.2 Tabla por tasa y tamaño de bloque

Los conteos en muestras no dependen de la tasa; la tasa cambia **cuánto dura** cada buffer y cada frame.
Las dos primeras columnas de tiempo son `ARITH` sobre 256/rate y B/rate.

| tasa | B (muestras) | cuenta | procesadas | sin NR | % intacto | duración del buffer | duración del frame |
|---:|---:|---|---:|---:|---:|---:|---:|
| 44100 | 1 | 0×256+1 | 0 | 1 | 100.00% | 0.023 ms | 5.805 ms |
| 44100 | 256 | 1×256+0 | 256 | 0 | 0.00% | 5.805 ms | 5.805 ms |
| 44100 | 257 | 1×256+1 | 256 | 1 | 0.39% | 5.828 ms | 5.805 ms |
| 44100 | 4410 | 17×256+58 = 4352+58 | 4352 | 58 | 1.32% | 100.000 ms | 5.805 ms |
| 44100 | 8820 | 34×256+116 = 8704+116 | 8704 | 116 | 1.32% | 200.000 ms | 5.805 ms |
| 48000 | 1 | 0×256+1 | 0 | 1 | 100.00% | 0.021 ms | 5.333 ms |
| 48000 | 256 | 1×256+0 | 256 | 0 | 0.00% | 5.333 ms | 5.333 ms |
| 48000 | 257 | 1×256+1 | 256 | 1 | 0.39% | 5.354 ms | 5.333 ms |
| 48000 | 4410 | 17×256+58 | 4352 | 58 | 1.32% | 91.875 ms | 5.333 ms |
| 48000 | 8820 | 34×256+116 | 8704 | 116 | 1.32% | 183.750 ms | 5.333 ms |
| 48000 | 4800 | 18×256+192 = 4608+192 | 4608 | 192 | 4.00% | 100.000 ms | 5.333 ms |

La última fila (4800) es el callback de 100 ms **a la tasa declarada**: ahí el tail sin NR no es 1.32% sino
**4.00%**, porque 4800 no es múltiplo de 256. A 44.1 kHz el callback real de 100 ms (4410) deja 1.32%.

Fracciones exactas: 58/4410 = 116/8820 = 1.3152%; 192/4800 = 4.0000%; 1/257 = 0.3891%.

### 1.3 Lo que el tail implica además del "no procesado" (`CODE` + `HYPOTHESIS`)

- El tail **no entra al estado de Speex**: el próximo callback empieza a alimentar frames desde la muestra
  `B`, no desde `B-58`. El stream interno de Speex tiene, entonces, un hueco de 58 muestras cada 100 ms
  (`CODE`: el bucle de `apply_noise_reduction()` sólo ve los frames completos; nadie guarda el remanente).
- El tail **sí** pasa por el ida y vuelta float de `audio_enh_process()` (`src/audio_enhancement.c:358-374`),
  que es **exacto** (divide y multiplica por 32768, una potencia de dos), pero **no** por la conversión a
  32767 que usan los frames de NR (`:303-316`): ahí hay truncamiento hacia cero y un factor 32768/32767
  (+0.000265 dB). `CODE`; medido: el tail queda bit-idéntico (5800/5800), consistente con el round trip
  exacto.
- `MEASURED-HOST`: con preset `SSB_VOICE` y callback de 4410, **las 58 muestras del tail son bit-idénticas
  a la entrada en los 100 callbacks** (5800/5800). Con `AM_BROADCAST` (nivelador AF encendido) el tail ya no
  es idéntico (5/5800 en ruido, 101/5800 en multisine) porque el nivelador aplica ganancia a todo el chunk:
  el "tail intacto" es una propiedad **del NR**, no de la cadena.

---

## 2. Latencia: declarada, real y del callback

### 2.1 Lo declarado (`CODE`)

```
15  #define AUDIO_ENH_SAMPLE_RATE 48000
16  #define AUDIO_ENH_FRAME_SIZE 256
```
`src/audio_enhancement.h:21-22`, usados tal cual en `speex_preprocess_state_init(AUDIO_ENH_FRAME_SIZE,
AUDIO_ENH_SAMPLE_RATE)` (`src/audio_enhancement.c:26`).

`ARITH`: 256 / 48000 = **5.3333 ms**.

### 2.2 La tasa real (`CODE` + `ARITH`)

La captura es mono S16NE a 44100 Hz (`src/audio.h:15`, `src/audio.c:204-207`, `:225`). El frame de 256
muestras reales dura 256 / 44100 = **5.80499 ms**. El desajuste es 48000/44100 = 1.088435, es decir
**+8.84%**: Speex cree que el frame dura 5.33 ms cuando dura 5.80 ms.

Consecuencia aritmética (`HYPOTHESIS` sobre el efecto, `ARITH` sobre el número): la FFT del preprocesador es
de `2*N` = 512 puntos (`libspeexdsp/preprocess.c:522`, `spx_fft_init(2*N)`), así que el bin mide
48000/512 = 93.75 Hz según la lib y 44100/512 = 86.13 Hz en la realidad: **8.84% de error** en la frecuencia
de cada bin. No se midió el efecto sobre voz.

La recomendación oficial de la API es frame = 10–20 ms (`speex_preprocess_state_init`: "should correspond to
10-20 ms", `/usr/include/speex/speex_preprocess.h:60-61`, 1.2.1). 5.80 ms queda **por debajo** del rango
recomendado, tanto a 44.1k como a 48k.

### 2.3 La latencia algorítmica real del NR (`MEASURED-HOST` + `CODE` de la lib)

SpeexDSP 1.2.1 no es un filtro muestra a muestra: usa ventana y solapamiento (WOLA). En
`libspeexdsp/preprocess.c` (tarball 1.2.1):

```
618   int N3 = 2*N - st->frame_size;
623   for (i=0;i<N3;i++) st->frame[i]=st->inbuf[i];      /* mitad previa */
625   for (i=0;i<st->frame_size;i++) st->frame[N3+i]=x[i]; /* frame actual */
...
983   for (i=0;i<N3;i++) x[i] = outbuf[i] + frame[i];
989   for (i=0;i<N3;i++) st->outbuf[i] = st->frame[st->frame_size+i];
```
Con `ps_size = frame_size` (`preprocess.c:402`, `:418`) ⇒ `N3 = 256`, `N4 = 0`: la primera mitad del IFFT
actual **se emite en la llamada siguiente**. Es un retraso de **un frame exacto**.

Medición (no lectura): correlación normalizada entrada/salida sobre 1 s de ruido blanco.

| caso | lag medido | control |
|---|---:|---|
| `noise_p0_b4410` (OFF) | **0 muestras** | salida bit-exacta (0 muestras distintas) |
| `noise_p2_b4410` (SSB_VOICE) | **256 muestras = 5.805 ms** | pico inequívoco (2º pico a 0.06 del primero) |
| `noise_p2_b256` | 256 muestras | idem |
| `noise_p2_b8820` | 256 muestras | idem |
| `sine1k_p2_b4410` / `multisine_p2_b4410` | `UNKNOWN` | señal periódica: el pico de correlación es ambiguo (2º pico ≥ 0.995 del primero) |

Es decir: **la latencia del NR es 5.805 ms reales (un frame a 44.1k), no 5.333 ms**, y el código no la
compensa en ningún lado (`audio_enh_process()` no desplaza ni descarta muestras).

### 2.4 El callback (`CODE` + `ARITH`)

`attr.fragsize = pa_usec_to_bytes(AUDIO_RATE_MS = 100 ms, &spec)` (`src/audio.c:29`, `:226`) ⇒ a 44100 Hz,
4410 muestras = 100.000 ms por callback. El retraso del NR (5.805 ms) ocurre **dentro** del mismo callback:
el buffer completo vuelve al llamador después de procesarse, así que el NR **no suma** 5.8 ms al retardo del
stream; lo que sí hace es correr la señal procesada un frame respecto del tail no procesado (§4.1).

### 2.5 Lo que no se puede afirmar

| afirmación | clase | por qué |
|---|---|---|
| latencia RX→parlante / auriculares | `BLOCKED` | incluye PulseAudio, el DSP del BASE y el trayecto eléctrico; no se mide acá y no se toca la radio (ver fila `latency_end_to_end_rx_to_speaker` del CSV) |
| latencia RX→Recorder/apps | `BLOCKED` | mismo motivo: el consumidor es el hilo de audio de la GUI en la radio |
| "5.33 ms" como dato del camino real | **falso** | es el número declarado; el real es 5.805 ms (`ARITH` + `MEASURED-HOST`) |
| que el retraso de 1 frame sea audible | `HYPOTHESIS` | no hay escucha ni prueba con voz; en la ruta de captura un retraso constante no se percibe como eco |

---

## 3. Mapa `level` → supresión

### 3.1 Fórmula real (`CODE`)

```
151  void audio_enh_nr_set_level(uint8_t level) {
152      g_audio_enh.nr.level = level;
154      if (g_audio_enh.nr.speex_state) {
155          // Map 0-100 to Speex noise suppression level (typically -30 to -10 dB)
156          int noise_suppress = -30 + (level * 20 / 100);
157          speex_preprocess_ctl(g_audio_enh.nr.speex_state, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &noise_suppress);
```
`src/audio_enhancement.c:151-158`.

`ARITH`: `level * 20 / 100` es división **entera** en C, así que el paso es de 1 dB cada 5 niveles y hay
`101 - 4 = 21` valores distintos (level 0..4 → −30; 5..9 → −29; …; 95..99 → −11; 100 → −10).

| level | −30 + level*20/100 | suppression pedida | atenuación medida (ruido blanco) |
|---:|---:|---:|---:|
| 0 | −30 + 0 | −30 dB | **−13.51 dB** |
| 20 | −30 + 400/100 = −26 | −26 dB | **−13.22 dB** |
| 40 | −30 + 800/100 = −22 | −22 dB | **−12.72 dB** |
| 60 | −30 + 1200/100 = −18 | −18 dB | **−11.91 dB** |
| 80 | −30 + 1600/100 = −14 | −14 dB | **−10.60 dB** |
| 100 | −30 + 2000/100 = −10 | −10 dB | **−8.53 dB** |

Medición: `./bench --probe level` con `audio_enh_nr_set_level()` de producción sobre 10 s de ruido blanco
a −30 dBFS, estado inicializado como en producción (256/48000), callbacks de 4410 (`MEASURED-HOST`,
filas `probe_level_*` del CSV).

Dos hechos que se desprenden:

1. **El techo configurado no se alcanza**: con −30 dB configurados se miden −13.5 dB. `noise_suppress` es
   una **cota máxima** de atenuación, no una atenuación entregada.
2. **La etiqueta está invertida**: `level 0` es la **máxima** supresión y `level 100` la **mínima**.
   `SSB_VOICE` usa level 40 (−22 dB pedidos) y `AM_BROADCAST` level 60 (−18 dB pedidos)
   (`src/audio_enhancement.c:191`, `:198`). Que "más level" deba significar "más NR" es una decisión de
   producto, no un hecho del código: acá sólo se registra la dirección real.

### 3.2 Polaridad: verificada contra la lib real (`MEASURED-HOST`)

La API oficial documenta el parámetro como negativo: `/** Set maximum attenuation of the noise in dB
(negative number) */` y `#define SPEEX_PREPROCESS_SET_NOISE_SUPPRESS 18`
(`/usr/include/speex/speex_preprocess.h:143-144`, libspeexdsp 1.2.1). No fue necesario marcar `BLOCKED`:
se verificó empíricamente con la lib real.

| `SET_NOISE_SUPPRESS` | `GET_NOISE_SUPPRESS` | atenuación medida |
|---:|---:|---:|
| −30 | −30 | −14.938 dB |
| **+30** | **−30** | **−14.938 dB** (idéntico) |
| 0 | 0 | −0.004 dB |
| −30 con `SET_DENOISE = 0` | −30 | −0.002 dB (control) |

Conclusión: la lib **normaliza el signo a −|x|** (el readback de +30 es −30 y la atenuación es la misma), de
modo que pasar un valor positivo **no** produce ganancia; y `0` desactiva la supresión. El signo negativo que
usa `src/audio_enhancement.c:156` es correcto. El control con denoise apagado da 0 dB, así que la medición
distingue "atenuó" de "no atenuó".

### 3.3 La tasa declarada vs la real, medida

Misma sonda, `SET_NOISE_SUPPRESS = −22`, sobre el mismo ruido, cambiando sólo la tasa que recibe la lib:

| tasa inicial de la lib | atenuación |
|---:|---:|
| 48000 (la que usa producción) | −13.854 dB |
| 44100 (la tasa real de los datos) | −13.881 dB |

`MEASURED-HOST`: sobre ruido blanco la diferencia es 0.027 dB, es decir **no se observa un efecto de la tasa
en la atenuación de banda ancha**. Eso **no** autoriza a decir que el desajuste sea inocuo para voz tonal o
para la estimación de ruido: sólo se midió ruido blanco estacionario.

---

## 4. Defectos medibles de la cadena

### 4.1 Discontinuidad en el borde de cada callback (`MEASURED-HOST`)

El tail no procesado **no está retrasado** y el resto **sí** (un frame). En el borde, la señal salta de
"original en n" a "procesado y retrasado 256 muestras". Ejemplo real con seno de 1 kHz a −18 dBFS, callback
de 4410 (archivo `artifacts/bench/sine1k_p2_b4410.raw`):

```
in [4408..4412] = [-1160, -586, 0, 586, 1160]     (slew natural máximo: 588 LSB)
out[4408..4412] = [-1160, -586, -2802, -2351, -1852]
                                 ^ salto de 2216 LSB en la muestra 4410
```

| señal | slew natural del input | salto máx. en borde de callback | salto máx. global del output |
|---|---:|---:|---:|
| seno 1 kHz, SSB_VOICE | 588 | **2216** (3.8×) | 3527 (6.0×) |
| multisine, SSB_VOICE | 715 | **1963** (2.7×) | 2994 (4.2×) |
| ruido blanco, SSB_VOICE | 3587 | 1932 (0.54×) | 3540 (0.99×) |
| ruido blanco, AM_BROADCAST | 3587 | **4528** (1.26×) | 8149 (2.3×) |

Lectura correcta: con señales tonales el borde del callback introduce un salto **mayor que cualquier salto
natural de la señal**; con ruido el salto queda por debajo del slew natural del ruido, así que ahí no se
distingue del material. El salto global máximo (3527 en el seno) **no** está en un borde: aparece en la
muestra 74912, dentro de un frame, y es un artefacto del denoiser/WOLA. Ninguno de los dos se probó audible
(`HYPOTHESIS`), y no hay WAV de voz para hacerlo.

### 4.2 Clipping (`MEASURED-HOST`)

`clip_count = 0` en los 17 casos medidos, incluido `AM_BROADCAST` (nivelador AF con target −18 dB) sobre
ruido a −30 dBFS. La protección de rango de `audio_enh_process()` (`src/audio_enhancement.c:371-374`) no se
activó en ninguna señal sintética.

### 4.3 Determinismo (`MEASURED-HOST`)

`noise_p2_b4410` corrido dos veces da el mismo sha256 (`6cd89dc900ac2bd38f1514b1c8156cb4a2e2bd9161abb320d081ffe50bb67795`)
y `cmp` idéntico. El OFF es bit-exacto (0 muestras distintas en los 4 archivos).

---

## 5. Qué miden y qué no miden los tests actuales

`tests/native/test_audio_enhancement.c` se ejecuta desde `tests/native/test_main.c:66` y corre contra
**stubs** de Speex, LVGL y liquid (`tests/native/run_native_tests.sh:98-100`). Lo que comprueba:

| test (`test_audio_enhancement.c`) | qué prueba | qué **no** prueba |
|---|---|---|
| `test_off_is_passthrough` | OFF no toca el buffer (memcmp) | nada de Speex |
| `test_preset_out_of_range_is_clamped` | preset 99 → OFF | nada de Speex |
| `test_full_block_is_processed` | un buffer de 4410 atraviesa la cadena (nivelador, NR off) | el troceado del NR (NR apagado en ese preset) |
| `test_deterministic` | dos corridas iguales | calidad |
| `test_nr_only_full_frames` | 300 muestras → **1** frame al stub; el stub atenúa 6 dB (`in/2 ± 2`); el tail queda intacto | atenuación real, fase, retardo |
| `test_frame_boundaries` | 256 → 1 frame; 257 → 1 frame + `out[256] == in[256]` | idem |
| `test_leveler_gain_is_bounded` | la ganancia del nivelador queda en +30 dB y no desborda | comportamiento con Speex real |
| `test_notches` | API de notch con liquid stub (passthrough) | filtro real |

El stub (`tests/native/stubs/stubs.c:70-116`):

```
100  int speex_preprocess_run(SpeexPreprocessState *st, int16_t *x) {
...
107      /* Deterministic: attenuate 6 dB so tests can detect that it ran. */
108      for (int i = 0; i < st->frame_size; i++) {
109          x[i] = (int16_t) (x[i] / 2);
```

Además, el header del stub define **ids de request que no son los de la API real**:

| macro | stub (`tests/native/stubs/speex/speex_preprocess.h:15-16`) | real 1.2.1 (`/usr/include/speex/speex_preprocess.h:99,101,144,146`) |
|---|---:|---:|
| `SPEEX_PREPROCESS_SET_DENOISE` | 0 | 0 |
| `SPEEX_PREPROCESS_SET_NOISE_SUPPRESS` | **1** | **18** |
| — | (1 es, en la lib real, `GET_DENOISE`) | `GET_NOISE_SUPPRESS` = 19 |

El código de producción usa el **nombre** del macro, así que compila y funciona con el header real; pero el
test **no podría detectar** un id equivocado ni un parámetro mal tipado, porque su stub acepta cualquier cosa.

> **Errata 2026-10-29 (#351).** La tabla de arriba es la medición histórica del 2026-10-28 y se conserva tal
> cual. Después de este informe, #351 alineó el header del stub con el ABI real de libspeexdsp 1.2.1:
> `tests/native/stubs/speex/speex_preprocess.h` ahora declara `SPEEX_PREPROCESS_SET_NOISE_SUPPRESS` = 18,
> de modo que un id equivocado en el módulo ya no queda tapado por el stub. El control
> `scripts/check_no_stub_claims.py` punto (c) dejó de verificar la discrepancia "stub = 1 / real = 18" y
> ahora verifica la **congruencia** "stub = real = 18": falla si el header del stub declara cualquier otro
> valor, y su `--self-test` incluye el caso "id del stub cambiado ⇒ FAIL". Sigue vigente lo demás observado
> en esta sección: la atenuación fija de 6 dB del stub, los parámetros sin tipar y el hecho de que un PASS
> de la suite host no sustituye una corrida contra la lib real.

Un PASS de la suite host (medido: `44837 comprobaciones, 0 fallos`, exit 0) prueba entonces: troceado,
tail, clamps, presets, no-crash. **No** prueba atenuación, calidad, CPU, latencia, retardo ni hilos. Cualquier
frase del tipo "NR verificado" basada en ese PASS es un no sequitur.

---

## 6. Memoria, asignaciones, callback e hilos

### 6.1 Memoria y asignaciones

| qué | tamaño / evidencia | clase |
|---|---|---|
| `g_audio_enh` (estado global) | `audio_enh_state_t`: 4 notches + nivelador + NR + `float float_buffer[256]` (1 KB) | `CODE` `src/audio_enhancement.h:71-81` |
| `temp_buffer` por frame | `int16_t temp_buffer[256]` = 512 B **en el stack** del callback | `CODE` `src/audio_enhancement.c:301` |
| estado de Speex | **≈0.32–0.40 MB** de RSS: 2.5–2.7 MB antes de `audio_enh_init()` → 2.9–3.1 MB después (delta medido, incluye el arena del allocator) | `MEASURED-HOST` (`rss_preinit_kb`/`rss_start_kb` en `bench-results.json`) |
| asignaciones por frame | la cadena no hace `malloc`: sólo `speex_preprocess_run()` sobre buffers del estado | `CODE` `src/audio_enhancement.c:293-317` + `CODE` de la lib (buffers en `preprocess.c:467`) |
| crecimiento durante 60 s | RSS final − RSS con estado creado: +80 a +164 kB en 17 casos (buffers de I/O del proceso de bench, no un patrón creciente) | `MEASURED-HOST` |

Límite práctico: el estado es **uno solo** (`g_audio_enh.nr.speex_state`), no por canal ni por stream. No hay
`free` por frame, y `audio_enh_deinit()` destruye el estado una vez (`src/audio_enhancement.c:51-54`).

### 6.2 Callback

`MEASURED-HOST` (x86_64, 3 corridas por caso, 60 s de audio cada una):

| caso | wall mediana | % del tiempo real | máx. por callback de 100 ms |
|---|---:|---:|---:|
| ruido, SSB_VOICE, 4410 | 133–137 ms | 0.22–0.23% | 1.5–1.9 ms (wall, incluye preempción) |
| seno, SSB_VOICE, 4410 | 105–107 ms | 0.18% | 1.2–1.5 ms |
| silencio, SSB_VOICE, 4410 | 104–107 ms | 0.17–0.18% | 1.1–1.5 ms |
| ruido, SSB_VOICE, 8820 (200 ms) | 133–136 ms | 0.22% | 1.9–2.1 ms |
| ruido, SSB_VOICE, 1 | 134–137 ms | 0.22–0.23% | 0.03–0.04 ms |
| cualquier preset OFF | 0.8–2.4 ms | 0.001–0.003% | 0 ms (return inmediato) |

Rangos observados en las corridas de esta sesión (3 corridas por caso); la corrida exacta que quedó
commiteada está en `bench-results.json` (`wall_ms`, `wall_ms_min`, `wall_ms_max`, `max_callback_ms`).

El costo de 60 s de audio es **~0.2% de un núcleo del host**; el callback de 100 ms consume como máximo
~2.1 ms de wall en el peor caso medido. **No es extrapolable al R16** (otra arquitectura, otra carga, sin
medición en la radio: fila `cpu_on_radio` = `BLOCKED`).

### 6.3 Hilos (`CODE` + `HYPOTHESIS`)

- `audio_enh_process()` corre en el **hilo del mainloop de PulseAudio**: `read_callback()`
  (`src/audio.c:57-70`) se registra con `pa_stream_set_read_callback()` (`src/audio.c:231`) y el mainloop
  arranca en `pa_threaded_mainloop_start()` (`src/audio.c:170`).
- Los setters corren en **otros hilos**: `audio_enh_preset_load()` desde el diálogo de Settings
  (`src/dialog_settings.c:431`), `audio_enh_apply_mode_preset()` desde el observer de modo
  (`src/dsp.cpp:293`) y al arranque (`src/main.c:602`).
- `src/audio_enhancement.c` **no tiene ningún mutex ni sección crítica** (`CODE`: no aparece `pthread` ni
  `lv_lock` en el archivo). `audio_enh_process()` usa estado global y **no es reentrante** (`CODE`).
- La API de Speex documenta que el estado no es seguro para uso concurrente (cita del issue:
  speex-api-reference, `SpeexPreprocessState`).

⇒ `HYPOTHESIS`: `speex_preprocess_ctl()` desde el hilo de UI puede ejecutarse mientras
`speex_preprocess_run()` procesa el mismo `SpeexPreprocessState` en el hilo de audio. **No se probó** una
carrera ni un efecto observable; sólo se registra que no hay sincronización y que el estado no es thread-safe.

---

## 7. Banco reproducible

- **Señales**: `scripts/gen_pcm.py`, stdlib pura, semilla 6200, 10 s mono S16 44100 Hz. Cuatro WAV:
  silencio, seno 1 kHz −18 dBFS, multisine 300/1000/2400 Hz, ruido blanco uniforme −30 dBFS. Manifiesto
  `pcm-manifest.json` con sha256, RMS, peak, clipping y duración. Dos corridas dan hashes idénticos (§8).
- **Banco**: `scripts/bench_speex.py` construye un contenedor (`scripts/bench/Dockerfile`,
  `debian:bookworm-slim@sha256:99c0b2…` + `libspeexdsp-dev=1.2.1-1`), copia `src/audio_enhancement.c`
  **verbatim** (sha256 `acab3aad…`), lo compila con `gcc -O2 -Wall -Wextra -Werror` contra la lib real y
  corre 17 casos × 3 corridas + 12 sondas. Lo único sustituido es la periferia (LVGL/cfg/params/liquid), y
  las funciones de liquid **abortan** si alguien las llama: ningún caso puede "medir" un notch inexistente.
- **Salidas**: `measurements.csv` (una fila por medición, `CODE`/`MEASURED-HOST`/`BLOCKED`), `bench-results.json`
  (métricas completas), `artifacts/bench/*.raw` (PCM procesado, gitignored).

### 7.1 Por qué los WAV no van commiteados

10 s mono S16 44.1k = 882 KB por archivo; los cuatro son 3.5 MB de binarios. El generador es determinista y
está pinneado por semilla, y el manifiesto commiteado tiene el sha256 de cada uno, así que el contenido es
verificable sin arrastrar los blobs: `python3 scripts/gen_pcm.py --verify` regenera en memoria y compara
contra `pcm-manifest.json`. Los `.raw` del banco tampoco se commitean (se regeneran con el contenedor y su
sha256 queda en el CSV).

---

## 8. Reproducir

```sh
cd <worktree>
python3 docs/evidencia/2026-10-28-rx-audio-feasibility/scripts/gen_pcm.py
python3 docs/evidencia/2026-10-28-rx-audio-feasibility/scripts/gen_pcm.py --verify
python3 docs/evidencia/2026-10-28-rx-audio-feasibility/scripts/bench_speex.py
```

Pines de esta corrida:

| qué | valor |
|---|---|
| imagen base | `debian:bookworm-slim@sha256:99c0b25c46d0214d5a2091092e7908907dcaecd6a962cde0ac700acee222b6f3` |
| libspeexdsp | paquete `1.2.1-1`, `pkg-config --modversion speexdsp` = `1.2.1` |
| toolchain | `gcc (Debian 12.2.0-14+deb12u1) 12.2.0` |
| imagen del banco | `x6200-speexdsp-bench:1.2.1-1`, id `sha256:46d344b88a0ccdfda0024461b18080233e023c1b8116657e86db5157ded79e1c` |
| fuente compilada | `src/audio_enhancement.c` sha256 `acab3aad158097cd5a8ab1c6e6eb83e8b37011a0098eb91d39dc1c5d2366b25f` |
| manifiesto PCM | sha256 `ffb1c65ece3db02f08cc1a0013aa360825d075efa9b00a0d27f668afa2cfc0ad` |
| fuente de la lib (sólo lectura, para citar WOLA) | `speexdsp-1.2.1.tar.gz` sha256 `8c777343e4a6399569c72abc38a95b24db56882c83dbdb6c6424a5f4aeb54d3d` |

## 9. Límites explícitos de esta auditoría

1. **Nada de esto es la radio.** CPU, RSS y tiempos son de un x86_64 de host con gcc 12 y libspeexdsp
   1.2.1. El R16 usa otra toolchain (Buildroot) y otra versión de la lib si el paquete difiere: la versión
   que corre en la ROM **no** se verificó (`UNVERIFIED`).
2. **Nada de esto es voz.** Tonos y ruido prueban integridad (atenuación, retardo, clipping, saltos), no
   inteligibilidad.
3. **El retardo de un frame** se midió sobre el ruido blanco con preset SSB_VOICE; con señales periódicas la
   correlación es ambigua y se reportó `UNKNOWN`, no un número inventado.
4. **La carrera ctl/run** entre hilos es `HYPOTHESIS`: no hay medición que la demuestre.
5. **La audibilidad** del salto en el borde del callback y del artefacto del denoiser es `HYPOTHESIS`.
6. No se midió el efecto del desajuste de tasa (48000 declarado vs 44100 real) sobre voz: sólo sobre ruido
   blanco (0.027 dB de diferencia).
