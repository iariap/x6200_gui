# Asistente de sintonía fina (issue #100)

A pedido del operador, el asistente busca alrededor de la frecuencia actual el
punto donde la señal se recibe mejor y deja el dial ahí. Es **RX only**: nunca
transmite, y el dial del operador nunca se pierde (si nadie supera al punto de
partida, vuelve exactamente a la frecuencia de arranque).

## La señal de calidad: el S-meter de la radio (RSSI del paquete DSP)

La señal que se mide es el **nivel del S-meter que reporta la radio**
(`meter_get_raw_db()`, en dBm). La cadena, en el firmware:

- `src/radio.c:191` — el paquete del DSP trae el nivel de RF del equipo
  (`dsp_samples(samples, RADIO_SAMPLES, pack->flag.tx, -(int16_t)pack->dbm)`).
- `src/dsp.cpp:200` (`update_s_meter`) → `src/meter.c` (`meter_update`): el
  nivel crudo queda en `meter_db_raw`, que devuelve `meter_get_raw_db()`.

**Por qué el S-meter y no un nivel de audio RX.** El camino de audio también
expone un escalar, `audio_get_peak_db()` (`src/audio.c:467`), pero es el pico
del stream de **monitor de captura** (`monitor_cb`, sobre `capture_device` con
`PA_STREAM_PEAK_DETECT`): está **después del volumen y de la ganancia de
captura**, así que subir el volumen mueve el número sin que cambie la señal
recibida — no sirve para comparar dos frecuencias. El S-meter, en cambio, es el
nivel de RF que reporta el propio equipo, independiente del audio, y es además
el número que el operador ya ve en pantalla. La correlación fina "se oye mejor"
(audio RX real) es la validación del owner sobre la radio (ver Pendientes).

## Algoritmo (`src/autotune.c`, sin UI, testeado en nativo)

- **Score** (`autotune_score`): mediana de k muestras (k = `AUTOTUNE_SAMPLES`,
  4 por defecto). La mediana rechaza un pico aislado sin promediar la señal.
- **Plan** (`autotune_plan`): zigzag creciente alrededor del arranque —
  `start`, `+paso`, `−paso`, `+2·paso`, `−2·paso`, … mientras el apartamiento
  quede dentro de ±ventana. El arranque es el probe 0 (se mide también, para
  comparar contra la señal real y no contra una suposición). Con la calibración
  por defecto son **21 probes**.
- **Decisión / restore** (`autotune_result`): el mejor probe solo gana si supera
  al arranque por el margen (`AUTOTUNE_MIN_IMPROVE_DB`, 1 dB); si no, devuelve el
  arranque. Empate: gana el primero en el orden del plan (determinista). El dial
  se ancla: nunca queda en un punto peor.

Calibración por defecto: paso 500 Hz (el "1 bin" del pedido), ventana ±5 kHz,
4 muestras por probe.

## Trigger y progreso (`src/autotune_ui.c`)

- **Trigger**: el botón **"Auto Tune"** de la página APP 6 (radio y red, al lado
  de SWR Scan), una acción (`ACTION_ATUNE`) que se despacha en
  `main_screen_action()`; **no** es una app (no abre diálogo, corre sobre la
  pantalla principal). Apretarlo de nuevo cancela y restaura.
- **Progreso**: la fila de frecuencia se mueve probe a probe (el dial barre la
  ventana). El mensaje final ("Auto tune: best …" / "no better spot, restored")
  aparece en la barra de mensajes de la radio: el *marker* breve del spot.
- **Restore / dial nunca perdido**: es el mismo idioma de casa del snapshot de
  estado de las apps (#102, `radio_state_save`/`radio_state_restore` en
  `src/radio.c`), pero el asistente es una acción, no una app: guarda su propia
  **ancla** (la frecuencia de arranque) y la devuelve de forma determinista
  cuando nadie la supera. Además, abrir una app (`main_screen_start_app`) o
  **cualquier diálogo** (`dialog_construct`, el punto por el que pasan todos)
  cancela el barrido y restaura el dial *antes* de que el frame guarde su
  snapshot, para que una ventana abierta a mitad del barrido no deje el dial en
  una frecuencia intermedia.

## Banco (QEMU/host)

Fixtures en `tools/ui_harness/fixtures/`:

- `autotune.scn`: dispara el botón y corre el barrido entero. El S-meter del
  banco es constante, así que ningún probe supera al arranque y el dial queda en
  el punto de partida (14.074.000): el **restore**.
- `autotune-progress.scn`: corta el barrido a mitad y captura el dial en el
  primer probe (14.074.500): el **progreso visible**.
- `autotune-found.scn`: empuja un S-meter sintético con un pico (el banco no
  tiene DSP que actualice el S-meter) y el dial queda en el mejor spot
  (14.072.500): el camino **"spot recogido"**.

El banco **no puede medir la señal RF real** (no hay señal): el S-meter es el
del fixture (`mock_radio.smeter_db` constante, o el pico sintético de
`autotune-found`). El mensaje final no se ve en el host porque `event_send()` es
un no-op documentado del arnés (`tools/ui_harness/mocks/mock_ui_deps.c`), así que
la evidencia del banco es **el dial** (el fixture lo verifica en
`scenario_settle`: falla si el dial no vuelve al arranque o no llega al pico).

## Pendientes (honestos)

- **Requiere radio**: la correlación "se oye mejor" (audio RX real) la valida el
  **owner** en la radio. El algoritmo es determinista y está probado en nativo,
  y el banco ejercita el camino completo (trigger, progreso, restore, spot
  recogido) con el S-meter del fixture, pero el banco no tiene señal RF.
- El marcador del spot final *sobre el waterfall* quedó fuera: no hay hook de
  marcador en `src/spectrum.c`/`src/waterfall.c` (superficie compartida), y el
  mensaje final ya da el aviso barato.
