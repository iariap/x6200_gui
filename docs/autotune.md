# Asistente de sintonía — Auto Tune (CW/CWR, AM/SAM y USB/LSB) — issues #190 y #205

A pedido explícito del operador, **una sola acción** (`ACTION_ATUNE`, APP 6 /
Settings > Actions) ayuda a alinear la señal recibida **según el modo**. No hay
pantallas, settings, atajos, FFT nueva ni segundo hilo: es un solo despachador
(`src/autotune_ui.c`) sobre dos políticas puras.

| Modo | Política cerrada |
|---|---|
| **CW/CWR** | El Auto Tune v2 de [#190](https://github.com/iariap/x6200_gui/issues/190): tres tramas de FFT frescas y estables del decodificador CW, **una única corrección** de hasta ±300 Hz y **verificación** con tres tramas nuevas. Sin cambios de cálculo, umbrales ni UX. |
| **AM/SAM** | Detecta una **portadora RF estrecha, única y dominante** en el espectro que ya existe, la centra **aproximadamente** con **una** corrección acotada (≤1 kHz), verifica con cuatro tramas nuevas y hace **rollback condicionado** si no converge. Sin portadora inequívoca **no mueve el dial**. |
| **USB/LSB** | Estimación **conservadora de alineación gruesa** de la energía ocupada dentro de la banda lateral. **No es afinación de voz ni mide inteligibilidad**: muestra el desplazamiento y exige **una segunda pulsación explícita** para aplicarlo, con **Undo de 5000 ms**. El dial no se mueve hasta la confirmación. |
| **USB-D/LSB-D, FM/NFM/WFM y demás** | Sin retune ni sugerencia: `Auto Tune: mode unsupported`. No se toca el dial de FT8/JS8/WSPR ni las frecuencias nominales de datos. |

Es **estrictamente RX**: nunca transmite, no toca `radio_set_ptt`, interlocks,
ATU ni el band plan, y no escribe en TX, RIT ni modo digital. Con TX, RIT, lock,
SPLIT (en voz) o una app/diálogo abierto, la acción **rechaza con su causa** sin
entrar a la sesión y sin mover la frecuencia.

> **Hardware RF validation pending.** Todo lo que sigue está probado en el host
> (módulos puros, políticas con espectros sintéticos, banco de UI sin RF y
> binario ARM sin radio); la precisión con señales reales se verifica en el X6200
> (ver *Pendientes*).

## Qué cambió respecto de v1 (issue #100)

El Auto Tune v1 **barrido a 14.074.500** (zigzag de ±5 kHz en pasos de 500 Hz,
20 probes, 4 muestras por probe) elegía la frecuencia por **RSSI más alto**
(`meter_get_raw_db()`). Ese algoritmo **no existe**: el S-meter no participa de
ninguna decisión (ni en CW ni en AM/SAM ni en USB/LSB), no hay barrido, ni
progreso, ni cambio de modo, ni filtros, ni settings nuevos, ni wizard.

## De dónde sale la información, por modo

| modo | fuente | resolución | qué NO puede afirmar |
|---|---|---|---|
| CW/CWR | `cw_get_tune_sample()` (`src/cw.cpp`): offset del tono y relación marca/ruido de la FFT del decodificador, **ya normalizados** contra el pitch manual | la del estimador de la FFT de CW (±344 Hz de rango) | nada de voz ni de portadora AM |
| AM/SAM, USB/LSB | `dsp_autotune_get_spectrum()` (`src/dsp.cpp`): la **misma PSD promediada** que alimenta el espectro, en 800 bins de display que **interpolan 512 bins físicos** | `width_hz / 512`: a 100 kHz ≈ 195 Hz; a 96 kHz = 187,5 Hz | precisión de portadora de ±10 Hz, pitch de voz, inteligibilidad |
| USB-D/LSB-D, FM/NFM/WFM | — | — | todo: no hay Auto Tune |

**La resolución es la física, no la visual.** El frame de 800 bins sale de
`AveragedPSD<RADIO_SAMPLES, SPECTRUM_NFFT>` (`src/dsp.cpp`), que **interpola** 512
bins hacia 800: por eso `src/autotune_spectrum.h` fija
`AUTOTUNE_SPECTRUM_PHYSICAL_BINS = 512`, la resolución conservadora se calcula
`width_hz / 512` y **todos** los candidatos se cuantizan a
`step_hz = max(1, round(resolution_hz))`. Nunca se infiere `width_hz / 800`, y no
hay interpolación subbin artificial: prometer 125 bins RF independientes sería
mentir sobre el equipo.

## La señal CW: la instantánea del decodificador (`cw_get_tune_sample`)

`src/cw.cpp` publica una copia coherente (`autotune_sample_t`) con `sequence`
(monotonía **por trama de FFT**), `timestamp_ms`, `error_hz` (`> 0` = tono RX por
encima del **pitch configurado**, ya compensado el offset del DDS), `ratio`
(marca/ruido), `dds_offset_hz` y `valid` (marca CW + `ratio >= 4` + finitud +
rango, calculado en el hilo del DSP). Tres detalles que importan:

1. **El signo se normaliza UNA vez, en el DSP.** El crudo de `peak_freq` es el
   **negativo** del corrimiento del tono (`docs/HALLAZGOS.md` 52); la conversión
   vive en `cw.h` y el núcleo nunca ve una coordenada cruda de FFT.
2. **Se compensa el offset del DDS de la misma trama**, así que el error se mide
   contra el pitch configurado y no contra el centro que haya desplazado el
   **AUTO del decodificador (#139)**. Los dos AUTOs coexisten.
3. **Frescura real.** Cada trama avanza secuencia y tiempo (también las
   inválidas); al escribir el dial, `cw_tune_note_dial_write()` hace que el DSP
   descarte su primera ventana posterior.

## La señal espectral: la instantánea del DSP (`dsp_autotune_get_spectrum`)

`src/dsp.cpp::dsp_samples()` publica, **dentro del hilo del DSP y sin tocar
LVGL**, una copia coherente de la PSD promediada (`dsp_autotune_spectrum_t`,
`src/dsp.h` reexporta el tipo desde `src/autotune_spectrum.h`):

| campo | qué es |
|---|---|
| `sequence` | monotonía por publicación |
| `timestamp_ms` | `get_time()` de la publicación |
| `epoch` | sube en cada invalidación: escritura propia, VFO/frecuencia, modo, ancho de FFT, TX/RX |
| `center_freq_hz`, `mode`, `width_hz` | el contexto **vigente** (ancho real de `cfg_cur.fft_width`, nunca una suposición de 100 kHz) |
| `tx`, `valid` | la publicación es de TX, o el ancho no es positivo, o hay un valor no finito |
| `db[800]` | la PSD en dB, **antes** del suavizado visual |

- **Coherencia:** un solo mutex guarda la copia, la secuencia, el epoch y el
  contexto; los observers del GUI escriben el contexto y el hilo del DSP solo
  lee esos campos para publicar. El mutex se retiene para copiar ~3.2 KB, nunca
  para procesar ni para dibujar, y **no se accede a ningún `Subject` desde el
  hilo del DSP**. La cadencia del espectro/waterfall y la política de batería no
  cambian.
- **Invalidación:** `dsp_autotune_invalidate()` (escritura propia, cambios de
  VFO/frecuencia/modo/ancho y TX/RX) sube el epoch y descarta la publicación
  vigente: ninguna trama anterior al cambio puede mezclarse con las posteriores.
- **Coordenadas:** `offset_hz(i) = (i - 400) * width_hz / 800`.

## Política AM/SAM (única corrección automática gruesa)

Sólo se acepta con `width_hz <= 100000` y `width_hz/512 <= 250 Hz` (la vista
habitual de ~100 kHz entra; por encima de eso no alcanza ni para un centrado
aproximado). Por **cada** trama:

1. **piso de ruido** = mediana de los bins finitos con `2500 <= |offset| <= 5000`
   en ambos lados (mínimo 12 bins, si no es inválida);
2. **pico máximo** dentro de `|offset| <= 1000`, máximo local frente a bins a
   distancia ≥ `resolution` (un empate a distancia no es un pico) y
   `pico - ruido >= 12 dB`;
3. **segundo pico** fuera de una guarda `±2*resolution` alrededor del ganador:
   `pico - segundo >= 6 dB` (sin segundo pico, la referencia es el propio ruido);
   empates, QRM y valores no finitos se rechazan;
4. **candidato** = `offset_hz(pico)` cuantizado al múltiplo más cercano de
   `step_hz`, sin interpolación subbin;
5. **cuatro tramas** válidas con candidatos del **mismo signo** (el cero se
   admite) y `max - min <= resolution`; la mediana decide: `|mediana| <=
   resolución/2` ⇒ **`carrier centered` sin escribir**; `|mediana| > 1000 Hz` o
   destino inválido ⇒ `no reliable signal`;
6. **una única escritura provisional** a `start + round(mediana)`, con
   `radio_check_freq` y la misma banda del snapshot (`resolve_band_range`);
7. **verificación**: guarda de **300 ms** desde la escritura, descarte de las
   **tres primeras** publicaciones posteriores y **cuatro tramas nuevas** con la
   misma detección y `|mediana del offset| <= resolution` (tope 2500 ms). Si
   verifica, se conserva el destino y se abre la **ventana de Undo de 5000 ms**
   (`carrier centered - press again to undo`); si no, **rollback condicionado**
   (sólo si todavía es dueño del mismo dial/VFO, en RX).

El estado **nunca** se decide por intensidad absoluta del S-meter (`meter_get_raw_db`).

## Política USB/LSB (asistencia gruesa, confirmación obligatoria)

**La banda lateral de voz no transmite portadora.** Esta política **no encuentra
el carrier verdadero ni mide inteligibilidad**: sólo sugiere un pequeño
desplazamiento para que la energía dominante caiga dentro de la banda RX
configurada. **Nunca se aplica automáticamente.** Se rechaza con
`no reliable signal` si `width_hz > 100000`, `width_hz/512 > 250 Hz`, el ancho del
filtro RX (`to - from`) está fuera de `[1800, 3500] Hz` o el filtro no tiene el
lado esperado (USB `from >= 0 && to > from`; LSB `from < to && to <= 0`). **No se
modifican filtros.**

Por **cada** trama:

1. `noise_db` = mediana de los bins con `3500 <= |offset| <= 5000` (mínimo 12
   bins; no finitos o ausencia ⇒ se rechaza);
2. para cada candidato discreto `d = k*step_hz` con `|d| <= 800 Hz` (incluido
   `d = 0`) se promedia el score de energía ocupada
   `clamp(db - noise_db - 3, 0, 24)` sobre la banda RX **desplazada** (USB
   `[d + max(from,300), d + min(to,2700)]`; LSB `[d + max(from,-2700),
   d + min(to,-300)]`), rechazando ventanas con menos de 8 bins de display;
3. se exige `best >= 5.0`, `best >= 1.7 * score(d=0)` y
   `best >= 1.25 * second` (el máximo separado del ganador por al menos
   `2*resolution`; sin segundo candidato, 0). Empates ⇒ sin sugerencia;
4. **nunca** se sugiere `|d| < max(200 Hz, resolution)` ni `|d| > 800`; el signo
   es el del dial (positivo sube el VFO);
5. **cinco tramas** distintas en máximo 3000 ms, todas con propuesta válida del
   **mismo signo** y `max - min <= max(200 Hz, resolution)`; la sugerencia es la
   mediana redondeada, validada contra la banda y `radio_check_freq`;
6. en vez de escribir, se muestra **literalmente**
   `Auto Tune: suggested %+d Hz - press again`; estado **PREVIEW**, que vence a
   los **5000 ms** desde la sugerencia;
7. sólo una **segunda pulsación** dentro de PREVIEW y con dial, filtros, modo,
   banda, VFO, RX y epoch/contexto **aún idénticos** aplica **una** escritura al
   destino; no hay verificación automática de inteligibilidad: se muestra
   `applied - press again to undo` y se abre el **Undo de 5000 ms**. Si el
   operador no confirma, el dial queda **exactamente igual**; una pulsación
   durante COLLECT cancela, girar el dial durante PREVIEW cancela sin retune y el
   vencimiento vuelve a IDLE;
8. en **UNDO**, una nueva pulsación restaura la frecuencia inicial si sigue
   siendo propietario del dial/contexto y en RX (`Auto Tune: restored`). Si la
   ventana vence, **el resultado queda y no hay auto-undo**; un nuevo trigger
   arranca otra sesión. Durante UNDO, dial/CAT/banda/VFO/TTX/app/filtros cancelan
   la ventana **sin escribir**.

El score es un **indicador de energía ocupada**, no un score de calidad de voz.
Nunca se barre el VFO ni se sondea RSSI.

## Contrato numérico

Constantes internas, no settings (`src/autotune.h` y `src/autotune_spectrum.h`):

| constante | valor | qué fija |
|---|---|---|
| `AUTOTUNE_PHASE_SAMPLES` / `..._TIMEOUT_MS` | 3 / 3000 | tramas CW distintas, frescas y válidas por fase y su plazo |
| `AUTOTUNE_MIN_SNR_RATIO` | 4.0 | `sum_signal/noise` mínimo por trama CW (≈6 dB) |
| `AUTOTUNE_SAMPLE_ERROR_LIMIT_HZ` | 344 | rango del estimador de la FFT de CW |
| `AUTOTUNE_MAX_SPREAD_HZ` / `..._DEADBAND_HZ` / `..._MAX_DELTA_HZ` | 20 / 10 / 300 | estabilidad, "ya sintonizado" y tope de la corrección CW |
| `AUTOTUNE_SETTLE_GUARD_MS` | 300 | espera mínima tras **cualquier** escritura antes de aceptar tramas de verificación |
| `AUTOTUNE_SESSION_TIMEOUT_MS` | 7000 | tope total de la sesión desde el trigger |
| `AUTOTUNE_SPECTRUM_MAX_WIDTH_HZ` / `..._MAX_RESOLUTION_HZ` | 100000 / 250 | ancho y resolución que las políticas espectrales aceptan |
| `AUTOTUNE_SPECTRUM_PHYSICAL_BINS` | 512 | bins físicos que el frame interpola (nunca 800) |
| `AUTOTUNE_AM_FRAMES` / `..._PEAK_MIN_DB` / `..._DOMINANCE_MIN_DB` / `..._MAX_DELTA_HZ` | 4 / 12 / 6 / 1000 | tramas, pico sobre el ruido, dominancia y tope AM |
| `AUTOTUNE_SSB_FRAMES` / `..._MIN_SCORE` / `..._RATIO_VS_ZERO` / `..._RATIO_VS_SECOND` / `..._MAX_DELTA_HZ` / `..._MIN_DELTA_HZ` | 5 / 5.0 / 1.7 / 1.25 / 800 / 200 | tramas y los cuatro umbrales de la sugerencia SSB |
| `AUTOTUNE_COLLECT_MS` / `..._VERIFY_MS` / `..._PREVIEW_MS` / `..._UNDO_MS` / `..._POST_WRITE_DROP` | 3000 / 2500 / 5000 / 5000 / 3 | plazos de cada fase y publicaciones descartadas tras escribir |

## Máquina de estados

```
IDLE --trigger--> COLLECT --(3 tramas válidas y estables)--> [decisión CW]
                              TUNED (sin write) --> IDLE
                              NO_ADJUST         --> IDLE
                              CORRECT --> una escritura --> VERIFY --(3 tramas NUEVAS)--> TUNED | rollback

IDLE --trigger(AM)--> COLLECT --(4 tramas con portadora consistente)-->
                              CENTERED (sin write) --> IDLE
                              NONE                 --> IDLE
                              MOVE --> una escritura --> VERIFY (guarda 300 ms + 3 descartes +
                                       4 tramas nuevas) --verifica--> UNDO (5000 ms)
                                                        --no verifica--> rollback --> IDLE

IDLE --trigger(SSB)--> COLLECT --(5 tramas con la misma propuesta)--> PREVIEW (5000 ms)
                              NONE --> IDLE
                              PREVIEW --vence--> IDLE (dial intacto)
                                      --segunda pulsación--> UNA escritura --> UNDO (5000 ms)
                              UNDO --pulsación--> restaura el origen --> IDLE
                                   --vence--> el resultado queda --> IDLE
```

- **Una sola corrección por invocación** en CW y en AM; en SSB la única escritura
  es la confirmada por el operador (y el Undo es la única escritura de vuelta).
- El estado terminal **libera el timer y descarta las muestras**: el segundo
  trigger arranca limpio.
- El vencimiento de un timer **no hace válida** ninguna muestra: siempre se exige
  secuencia nueva y timestamp posterior a la escritura.

## Propiedad del dial y cancelación

Snapshot al iniciar: banda (`cfg.band_id`), VFO A/B, modo, frecuencia exacta,
pitch, la frecuencia que **espera** haber dejado, estado RX, lock y RIT (más el
contexto de medición de la sesión espectral: ancho de FFT, filtro real,
ATT/PRE/AGC y SPLIT). La regla vive en el núcleo (`autotune_owns`,
`autotune_context_broken`) y el coordinador sólo junta los campos y pregunta:

> Auto Tune puede escribir **solo** si la sesión está viva, el contexto es
> idéntico al snapshot, la radio está en `RADIO_RX`, no hay lock ni RIT y el dial
> sigue **donde lo dejó él** (`freq == expected_freq`).

| evento | comportamiento |
|---|---|
| giro del dial (con aceleración #186), teclas +/−/HKEY, CB | **aborta ANTES** de aplicar la orden manual, sin restaurar |
| CAT, memoria, app, cambio externo de frecuencia | observa el subject y aborta **sin escribir** |
| banda, modo, VFO, pitch, lock, RIT, RX→otro | aborta sin escribir (el contexto ya cambió) |
| ancho de FFT, filtro real, ATT/PRE/AGC, SPLIT durante una medición | cancela **sin escribir** (la baseline y la señal sólo son comparables bajo el mismo contexto) |
| abrir app/diálogo | cancela y **restaura solo si** sigue el contexto original, en RX y con el dial propio |
| PTT/keyer/ATU antes de activar TX (todavía en RX) | CW y AM en VERIFY: rollback condicionado. SSB en PREVIEW (no escribió) y UNDO (valor ya confirmado): sólo termina la ventana |
| TX ya activo | aborta **sin sintonizar jamás** |
| segundo `ACTION_ATUNE` | CW/AM en medición: cancela (con rollback si había una escritura sin verificar). SSB en PREVIEW: **aplica**. UNDO: **restaura** |
| timeout / verificación negativa | rollback condicionado (AM/CW) o simplemente IDLE (SSB) |

**Invariante**: no existe ruta por la cual un rollback de Auto Tune sobrescriba
una frecuencia puesta por el dial, CAT, una app, la memoria, un VFO nuevo o el
usuario. Si ya no se puede restaurar de forma segura, se conserva el estado nuevo
y se registra la cancelación.

## UX y acciones (textos exactos, en inglés)

Nada de widgets nuevos: los mensajes van por la barra existente
(`msg_update_text_fmt`). **Sin texto en español.**

| situación | texto |
|---|---|
| arranque aceptado (todos los modos) | `Auto Tune: tuning...` |
| CW/CWR: corrección verificada, incluido "ya estaba dentro de ±10 Hz" | `Auto Tune: tuned` |
| CW/CWR: sin señal, desviación > 300, inestable, destino rechazado, timeout o verificación negativa | `Auto Tune: no reliable adjustment` |
| AM/SAM: portadora centrada (ya estaba, o `no reliable signal` si no hay) | `Auto Tune: carrier centered` |
| AM/SAM: escritura verificada, ventana de Undo abierta | `Auto Tune: carrier centered - press again to undo` |
| AM/SAM/USB/LSB: sin detección fiable; también filtro/ancho fuera del contrato | `Auto Tune: no reliable signal` |
| USB/LSB: propuesta, sin mover el dial | `Auto Tune: suggested %+d Hz - press again` |
| USB/LSB: aplicada tras la segunda pulsación | `Auto Tune: applied - press again to undo` |
| Undo ejecutado | `Auto Tune: restored` |
| cancelación manual, segundo trigger en medición, cambio externo | `Auto Tune: cancelled` |
| modo sin Auto Tune (USB-D/LSB-D, FM/NFM/WFM) | `Auto Tune: mode unsupported` |
| RX no disponible / TX / ATU / SWR | `Auto Tune: RX only` |
| bloqueo de frecuencia activo | `Auto Tune: frequency locked` |
| RIT distinto de cero | `Auto Tune: disable RIT` |
| app/diálogo activo | `Auto Tune: close current app` |
| SPLIT en AM/SAM y USB/LSB | `Auto Tune: disable split` |

El botón **"Auto Tune"** de la página **APP 6** sigue igual (misma acción,
`ACTION_ATUNE == 129`, sin renumerar ids persistidos y sin cambiar ningún
default). La acción también es asignable a las pulsaciones largas físicas desde
**Settings > Actions**; no se toca HMic F1/F2, la rueda VOL, los presets DFL ni
los defaults existentes. La edición DFL y el volumen quedan intactos.

## Pruebas

**Núcleo CW/CWR** (`tests/native/test_autotune.c`, sin hardware, reloj y muestras
falsos; **169 comprobaciones**, intactas y pineadas por md5 en
`tests/check_autotune_v2.py`): gate de la trama, mediana robusta y estabilidad,
±10/±11/±300/±301 y redondeos, signo CW/CWR, convergencia cerrada del tono a
±35 Hz, limpieza de la fase al cambiar el centro del DDS, guarda de 300 ms y
tramas nuevas en VERIFY, rollback condicionado, una sola corrección, timeouts,
dos triggers, cancelaciones antes y después de la provisional, dial manual y CAT
durante COLLECT y VERIFY, lock, modo/banda/VFO, RIT, TX y frescura.

**Políticas multimodo** (`tests/native/test_autotune_spectrum.c`, **225
comprobaciones**, espectros sintéticos con valores exactos en dB): PSD plano y
silencio, NaN/Inf, empate de portadoras, AM a ±390/±780 Hz (y ±975 en el borde),
portadora ya centrada, picos superpuestos vs. empate, umbral de 12 dB,
dominancia de 6 dB (y 3 dB rechazada), portadora fuera de la ventana, ancho de
384 kHz no medible, colección de 4 tramas y su consistencia, veredictos y topes,
`verified`, gate de frescura (secuencia/timestamp/piso de tiempo/descartes/epoch
exigido) y de contexto (epoch, centro, modo, ancho, TX, inválida), filtros USB/LSB
y lado esperado, sugerencia USB (+752 Hz) y su espejo LSB (−752 Hz), abstenciones
(voz dentro del pasante, lejos, dos estaciones, score bajo), condiciones de
medición, 5 tramas consistentes, ventanas (3000/2500/5000/5000 y descarte 3),
semántica de la pulsación, QSY externo durante PREVIEW y UNDO, y una sesión AM de
punta a punta con guarda, tramas nuevas y rollback.

**Signo del tono con la FFT real** (`tests/native/test_cw_tune_sign.c`, enlaza
liquid-dsp): genera un tono complejo sintético y corre la MISMA cadena que
`src/cw.cpp`. El runner nativo lo salta con razón explícita si el host no tiene
liquid.

**Contrato estático** (`tests/check_autotune_v2.py`): los 16 textos exactos, la
entrada de Settings, el id 129, los defaults intactos, la pureza de los dos
núcleos, las prohibiciones de la UI (no TX, no pitch, no RIT, no S-meter), los
pines md5 del núcleo CW de #190, el snapshot (800/512), el publicador en
`dsp_samples()`, la única vía de escritura del dial, la invalidación del epoch, el
rechazo del digital/FM y la existencia/armado de los 9 fixtures de la familia.

**Banco de UI** (fixtures, sin RF; la señal entra por inyección **solo en tests**
de la instantánea del DSP, `mocks/mock_cw_tune.c` y `mocks/mock_dsp.c`):
`autotune` (CW cancela sin mover), `autotune-progress` (la única corrección
provisional), `autotune-found` (verificada de punta a punta), `autotune-digital`
(rechazo del modo digital), `autotune-am` (portadora a 376 Hz: una escritura
verificada y ventana de Undo viva), `autotune-usb` y `autotune-lsb` (propuesta
que **no mueve** el dial), `autotune-usb-applied` (confirmación + Undo) y
`autotune-usb-timeout` (vencimiento sin escritura). Los fixtures de la familia se
renderizan dos veces en el gate de determinismo (PNG idénticos).

## Pendientes (honestos)

**Hardware RF validation pending** — el banco no tiene RF ni DSP, así que la
precisión real se valida en el X6200:

1. **AM/SAM**: portadora estable a ±390 Hz y a ±780 Hz del dial (centrado
   aproximado y verificación), y **portadora débil o con QRM** — sin QSY dudoso.
2. **USB/LSB**: voz real con desplazamiento de ~±500 Hz (propuesta **no
   automática**, confirmación y Undo), y **dos QSO cercanos** (abstención).
3. **DFL y anchos de FFT**: la política exige `width/512 <= 250 Hz`; medir en la
   vista de 96 kHz y en una más angosta, y confirmar el rechazo a 384 kHz.
4. **PTT, freq lock, CAT y rueda**: durante la medición y durante el Undo, el
   operador siempre gana; nada se escribe en TX.
5. **Modo digital fijo**: FT8/JS8/WSPR no mueven el dial (`mode unsupported`).
6. **CW/CWR original**: portadora a ±35 Hz del pitch, con el AUTO del
   decodificador #139 ON y OFF, y la coexistencia de los dos AUTOs.
7. **Encoder y waterfall** fluido durante la sesión (el DSP no cambia su
   cadencia).
