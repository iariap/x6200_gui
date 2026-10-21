# Asistente de sintonía fina CW/CWR — Auto Tune v2 (issue #190)

A pedido explícito del operador, **una sola acción** (`ACTION_ATUNE`) centra el
tono CW recibido respecto del pitch configurado: mide tres tramas de FFT
frescas y estables del propio decodificador CW, aplica **una única corrección**
de hasta ±300 Hz y la **verifica** con otras tres tramas nuevas. Sin señal
confiable **no cambia el dial**: es preferible no ajustar que ajustar mal.

Es **estrictamente RX** (nunca transmite, no toca `radio_set_ptt`, interlocks,
ATU ni el band plan) y **solo CW y CWR**. En cualquier otro modo, o con TX, RIT,
lock, o una app/diálogo abierto, la acción **rechaza con su causa** sin entrar a
la sesión y sin mover la frecuencia.

> **Hardware RF validation pending.** Todo lo que sigue está probado en el host
> (núcleo puro, DSP con señal sintética y banco sin RF); la precisión con señales
> reales se verifica en el X6200 (ver *Pendientes*).

## Qué cambió respecto de v1 (issue #100)

El Auto Tune v1 **barrido a 14.074.500** (zigzag de ±5 kHz en pasos de 500 Hz,
20 probes, 4 muestras por probe) elegía la frecuencia por **RSSI más alto**
(`meter_get_raw_db()`). Ese algoritmo **ya no existe como política de sintonía**:

- se eliminaron del núcleo el plan zigzag, las macros de ±5 kHz/500 Hz, el
  umbral de ganancia en dB y el comparador `>` del margen;
- `meter_get_raw_db()` **no se usa** para Auto Tune (el S-meter sigue igual,
  como indicador visual);
- no hay barrido, ni progreso de barrido, ni barra de progreso ficticia, ni
  cambio de modo, ni filtros, ni settings nuevos, ni wizard.

## La señal: la instantánea del DSP (`cw_get_tune_sample`)

El decodificador CW ya calcula, **una vez por trama de FFT de 128 muestras**
(~185 ms: 128/689,0625 Hz), el offset del tono recibido
(`update_peak_freq`) y la relación marca/ruido de esa misma trama
(`update_thresholds`). Auto Tune consume **eso mismo**, no una segunda FFT ni un
pipeline DSP nuevo.

`src/cw.cpp` publica una copia coherente (`autotune_sample_t` de src/autotune.h, protegida por un
mutex chico, sin LVGL y sin `scheduler_put`) con:

| campo | qué es |
|---|---|
| `sequence` | monotonía **por trama de FFT** (no por tick de LVGL) |
| `timestamp_ms` | `get_time()` de la trama |
| `error_hz` | error **normalizado** respecto del **pitch manual**: `> 0` = tono RX por encima del pitch |
| `ratio` | `sum_signal/noise` de esa trama (lineal) |
| `dds_offset_hz` | corrimiento del DDS del decodificador vigente en esa trama (`rx_tone_offset`) |
| `valid` | marca CW + `ratio >= 4` + finitud + rango del estimador, **calculado en el hilo del DSP** |

Tres detalles que importan:

1. **El signo se normaliza UNA vez, en el DSP.** La fórmula de `peak_freq`
   devuelve el **negativo** del corrimiento del tono respecto del centro del DDS
   (`docs/HALLAZGOS.md` 52). La conversión vive en `cw.h`
   (`cw_peak_freq_from_bin`, `cw_tune_error_hz`) y es la misma que usan el lazo de
   audio y la prueba de signo con señal sintética:
   `error_hz = rx_tone_offset - peak_freq = (tono de audio) - key_tone`.
   El núcleo de Auto Tune **nunca ve una coordenada cruda de la FFT**.
2. **Se compensa el offset del DDS de la MISMA trama**, así que el error se mide
   contra el **pitch configurado** (`cfg.key_tone`) y no contra el centro que el
   **AUTO del decodificador (#139)** haya desplazado. Los dos AUTOs **coexisten**:
   Auto Tune v2 no escribe ningún parámetro de #139 (`cw_decoder_snr`,
   `..._gist`, betas, `rx_tone_offset`) y #139 no mueve el dial.
3. **Frescura real.** Cada trama avanza secuencia y tiempo; una trama inválida
   también (invalida el pico anterior). Al escribir el dial, el GUI avisa
   (`cw_tune_note_dial_write()`) y el DSP **descarta su primera ventana**
   posterior (puede traer audio de la frecuencia anterior).

Coexistencia con el widget de sintonía (`src/cw_tune_ui.c`): su estado **no es
fuente de medida** (no tiene edad, secuencia ni validez). El widget sigue
funcionando exactamente como antes.

## Contrato numérico (`src/autotune.h`)

Constantes internas, no settings:

| constante | valor | qué fija |
|---|---|---|
| `AUTOTUNE_PHASE_SAMPLES` | 3 | tramas **distintas, frescas y válidas** por fase |
| `AUTOTUNE_PHASE_TIMEOUT_MS` | 3000 | plazo por fase para juntar esas tres; corre **también** si no llega ninguna trama (sin señal el fallo aparece a los 3 s, no recién al tope de la sesión) |
| `AUTOTUNE_MIN_SNR_RATIO` | 4.0 | `sum_signal/noise` mínimo por trama (≈6 dB de potencia); ruido 0, no finitos y silencios son inválidos |
| `AUTOTUNE_SAMPLE_ERROR_LIMIT_HZ` | 344 | rango del estimador de la FFT (fuera de eso no es un tono) |
| `AUTOTUNE_MAX_SPREAD_HZ` | 20 | `max-min` de las tres tramas usadas (estabilidad) |
| `AUTOTUNE_DEADBAND_HZ` | 10 | `abs(mediana) <= 10` ⇒ ya está sintonizado, **sin escritura** |
| `AUTOTUNE_MAX_DELTA_HZ` | 300 | tope de la corrección: por encima **no se recorta**, no se mueve el dial |
| `AUTOTUNE_SETTLE_GUARD_MS` | 300 | espera mínima tras la escritura para aceptar tramas de verificación |
| `AUTOTUNE_SESSION_TIMEOUT_MS` | 7000 | tope total de la sesión desde el trigger |

Política de corrección (sobre el error ya normalizado):

```
CW  (banda lateral normal)   : target = start + round(error_hz)
CWR (banda lateral invertida): target = start - round(error_hz)
```

o sea: un tono 35 Hz por encima del pitch da **+35 Hz de dial en CW** y
**−35 Hz en CWR**; tras el movimiento el error medido vuelve a ~0. La
mediana de las tres tramas hace robusta la decisión a una muestra atípica.

No se corrige si: el redondeo da 0, hay overflow, `radio_check_freq(target)`
falla, el destino no pertenece al rango de la banda configurada
(`band_info_t`/`cfg_band_read_all_bands`) o `abs(mediana) > 300`.

## Máquina de estados

```
IDLE --trigger--> COLLECT --(3 tramas válidas y estables)--> [decisión]
                                     |                             |
                                     |                    TUNED (sin write) --> IDLE
                                     |                    NO_ADJUST            --> IDLE
                                     |                    CORRECT --> una escritura provisional --> VERIFY
COLLECT --> IDLE : ya sintonizado, sin señal confiable, timeout de fase/sesión o cancelación
VERIFY  --> IDLE : éxito (|mediana| <= 10 con 3 tramas NUEVAS), fracaso (rollback condicionado) o interrupción
```

- **Una sola corrección por invocación** (`abs(delta_hz) <= 300`), y **un solo
  rollback** cuando corresponde.
- Después de escribir: **mínimo 300 ms + 3 tramas nuevas, frescas y estables**
  (la sequía no prueba frescura: se exige secuencia nueva y `timestamp` posterior
  a la escritura).
- El estado terminal **libera el timer y descarta las muestras**: el segundo
  trigger arranca limpio.

## Propiedad del dial y cancelación (sección 5 del issue)

Snapshot al iniciar: banda (`cfg.band_id`), VFO A/B, modo, frecuencia exacta,
pitch, la frecuencia que **espera** haber dejado, estado RX, lock y RIT. La
regla vive en el núcleo (`autotune_owns`, `autotune_context_broken`) y el
coordinador solo junta los campos y pregunta:

> Auto Tune puede escribir **solo** si la sesión está viva, el contexto es
> idéntico al snapshot, la radio está en `RADIO_RX`, no hay lock ni RIT y el dial
> sigue **donde lo dejó él** (`freq == expected_freq`).

Eventos y qué hace:

| evento | comportamiento |
|---|---|
| giro del dial (con aceleración #186), teclas +/−/HKEY, CB | **aborta ANTES** de aplicar la orden manual, sin restaurar (`freq_shift`, `main_screen_rotary_enhanced_cb`) |
| CAT, memoria, app, cambio externo de frecuencia | observa el subject y aborta **sin escribir** |
| banda, modo, VFO, pitch, lock, RIT, RX→otro | aborta sin escribir (el contexto ya cambió) |
| abrir app/diálogo (`dialog_construct`, `main_screen_start_app`) | cancela y **restaura solo si** sigue el contexto original, en RX y con el dial propio |
| PTT/keyer/ATU (antes de activar TX, todavía en RX) | cancela y restaura condicionado |
| TX ya activo (`EVENT_RADIO_TX`) | aborta **sin sintonizar jamás** |
| segundo `ACTION_ATUNE` | cancela; restaura solo si había escrito provisional y conserva la propiedad |
| timeout / verificación negativa | rollback condicionado |

**Invariante**: no existe ruta por la cual un rollback de Auto Tune sobrescriba
una frecuencia puesta por el dial, CAT, una app, la memoria, un VFO nuevo o el
usuario. Si ya no se puede restaurar de forma segura, se conserva el estado nuevo
y se registra la cancelación.

## UX y acciones (textos exactos)

Nada de widgets nuevos: los mensajes van por la barra de mensajes
(`msg_update_text_fmt`) y la pantalla principal se mantiene. **Todo en inglés**:

| situación | texto |
|---|---|
| arranque aceptado | `Auto Tune: tuning...` |
| corrección verificada (incluye "ya estaba dentro de ±10 Hz") | `Auto Tune: tuned` |
| sin señal válida, desviación > 300, medición inestable, destino rechazado, timeout o fallo de verificación | `Auto Tune: no reliable adjustment` |
| cancelación explícita (segundo pulsado) | `Auto Tune: cancelled` |
| modo distinto de CW/CWR | `Auto Tune: CW/CWR only` |
| RX no disponible / TX / ATU / SWR | `Auto Tune: RX only` |
| bloqueo de frecuencia activo | `Auto Tune: frequency locked` |
| RIT distinto de cero | `Auto Tune: disable RIT` |
| diálogo/app activo | `Auto Tune: close current app` |

El botón **"Auto Tune"** de la página **APP 6** sigue igual (misma acción,
`ACTION_ATUNE == 129`, sin renumerar ids persistidos y sin cambiar ningún
default). Además la acción es asignable a las pulsaciones largas físicas
(GEN/APP/KEY/MSG/DFN/DFL) desde **Settings > Actions**
(`{ .label = " Auto Tune ", .action = ACTION_ATUNE }`); no se toca HMic F1/F2,
la rueda VOL, los presets DFL ni los defaults existentes.

## Pruebas

**Núcleo puro** (`tests/native/test_autotune.c`, sin hardware, reloj y muestras
falsos; 169 comprobaciones): gate de la trama (inválidas, NaN/inf, ruido 0,
SNR < 4, fuera de rango), mediana robusta y estabilidad (spread 20 acepta, 21
rechaza), ±10/±11/±300/±301 y redondeos, signo CW/CWR, convergencia cerrada del
tono a ±35 Hz en ambos modos, limpieza de la fase al cambiar el centro del DDS,
guarda de 300 ms y tramas nuevas en VERIFY, rollback condicionado, una sola
corrección, timeouts (3000/7000 ms), dos triggers, cancelaciones antes y después
de la provisional, dial manual y CAT durante COLLECT y VERIFY, lock,
modo/banda/VFO, RIT, TX (antes y durante) y que una trama vieja **nunca** se
reutilice.

**Signo del tono con la FFT real** (`tests/native/test_cw_tune_sign.c`, enlaza
liquid-dsp como el firmware): genera un tono complejo sintético y corre la MISMA
cadena que `src/cw.cpp` (DDS + decimación + ventana Hann + FFT 128 + argmax +
`cw_peak_freq_from_bin` + `cw_tune_error_hz`). Comprueba que un tono **por
encima** del pitch da error **positivo** (y por debajo, negativo), que el offset
del DDS no cambia ni el valor ni el signo, que el pitch objetivo es el
configurado (no 700 Hz fijos) y el ciclo cerrado CW/CWR con el modelo de banda
lateral. **No** es un test que inyecte un error ya normalizado: mide la
orientación real. El runner nativo lo salta con razón explícita si el host no
tiene liquid (el contenedor `x6200-uiharness` y el job *functional* del CI sí).

**Banco de UI** (fixtures, sin RF):
- `autotune.scn` — CW: la acción arranca la sesión, el dial **no se mueve**
  durante COLLECT (v2 no barre) y el segundo pulsado cancela.
- `autotune-progress.scn` — la **única corrección provisional** visible a mitad
  de sesión, con el dial exactamente en la corrección y la sesión viva.
- `autotune-found.scn` — la corrección **verificada** de punta a punta (si la
  verificación fallara, el rollback devolvería el dial al arranque).
- `autotune-notcw.scn` — el **rechazo fuera de CW/CWR**: no abre sesión, no pide
  muestras y no toca el dial.

El banco no tiene DSP (`src/cw.cpp` no se compila: necesita liquid y el hilo de
audio), así que los fixtures con tono usan una **inyección solo en tests** de la
instantánea del DSP (`tools/ui_harness/mocks/mock_cw_tune.c`), que modela el
receptor (mover el dial cambia el error medido). No se fabrica RSSI ni se falsea
el flujo de producción. La barra de mensajes no se dibuja en el host
(`event_send()` es un no-op documentado), así que los **textos exactos** los fija
`tests/check_autotune_v2.py` sobre el fuente, junto con el id 129, la entrada de
Settings y las prohibiciones (no TX, no pitch, no RIT, no S-meter).

## Pendientes (honestos)

**Hardware RF validation pending** — el banco no tiene RF ni DSP, así que la
precisión real se valida en el X6200:

1. CW con una portadora estable a **+35 Hz** y a **−35 Hz** del pitch: corrige
   según el signo y queda ≤10 Hz si el RF acompaña.
2. CWR igual, con el sentido **inverso** de la frecuencia del dial.
3. CW Decoder **AUTO #139 ON y OFF**, con el mismo pitch configurado, y la
   coexistencia (que ninguno de los dos AUTOs persiga al otro).
4. CW débil/intermitente y QRM cercano: **no** hace QSY cuando la medición no es
   confiable.
5. Giro de dial, banda/VFO, APP/diálogo, RIT y PTT durante la operación: el
   usuario siempre gana.
6. La pantalla CW tuner y la aceleración de rueda (#186) siguen operativas.
