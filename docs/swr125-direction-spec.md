# Spec — #125-fix: la parábola sale invertida en la radio (orden del owner: base abajo, puntas a infinito positivo)

Fecha: 2026-10-17. Rama: `factory/swr125-direction` (base `37de9937` = trunk tip).
Creado por: orquestador (spec stage, con la regla nueva del owner: el setup define TODO el plan de prueba).

## Orden del owner (2026-10-17)

> "Me parece que la parábola está invertida en la radio: tiene que tener la **base abajo** y las
> **puntas tienen que tender a infinito positivo**."

## Evidencia ya fundada (por el orquestador, con ojos propios)

- **Radio re-flasheada** (captura `docs/evidencia/2026-10-15-validacion-arreglos/26-roe-multipasada-final.png`,
  commiteada en trunk — la del owner): curva en **domo ∩**: sube desde la esquina inferior-izquierda
  hasta ~4.0 cerca del 60 % del chart y cae a 1.0 al final. Además:
  - readout `min 1.0 @ 6.891.334 kHz` — **fuera del span** del eje (labels 6.941.334 / 6.991.334 / 7.041.334);
  - banner `Error: timeout de RF` y `n=44/51` (parcial);
  - `Passes: 1`, `Span: 200 kHz`, `Scale: Log`.
- **Arnés** (`03-swrscan-valle.png`, backend fake): valle ∪ correcto (base al piso a la izquierda del
  centro, readout `1.0 @ 7.019.925` con tick dentro del rango) — con el **mismo** render (`calc_y` leído
  en trunk: vswr alto → arriba, 1.0 → piso; el draw no puede dar ∩).
- **Conclusion fundada**: el bug no está en el render — está en el **camino de datos real**
  (radio.c ↔ swr_session/diálogo). El arnés no lo vio porque su fake verifica contra el índice del
  PROPIO diálogo (tautología) — lección registrada en asf#41: el oráculo debe modelar el ORDEN REAL
  del productor, desacoplado del código bajo test.

## Investigación a fundar (con líneas, antes de tocar)

1. ¿Quién hace el **stepping de TX** en la radio real: `radio.c` (su estado `RADIO_SWRSCAN`, un step
   por paquete) o el diálogo (`dialog_swrscan_sweep_freq()`)? ¿Hay **dos contadores** (el del radio y
   el de la sesión nueva del #125) que pueden desencajarse — en dirección (pasada 1 descendente según
   el diseño del owner) y/o velocidad (el radio auto-step puede seguir **más allá del span** — el
   `6.891.334` del readout huele exacto a eso: `6.991.334 − 100 kHz`)?
2. ¿Con qué índice y con qué frecuencia el paquete entra al buffer de la sesión? Si la sesión guarda
   `sample[i]` con su propio do_step pero el TX real está en otra frecuencia (stale/otro índice), el
   agregado sale espejado/rotado → puede mirar como domo.
3. ¿El `Error: timeout de RF` con n=44/51 se produce por la misma desincronización (la sesión espera
   packs de una freq y el radio entrega otra, hasta el timeout)? Fundado o refutado.
4. ¿Qué pasa con Sweep_Freq de radio.c (si controla algo) vs `swr_grid_build`?

## Criterios de aceptación (pinneados por el owner)

- **Forma**: con el dataset del owner, el dibujo es **U**: base (mínimo) abajo, puntas creciendo hacia
  los bordes ("a infinito positivo"). Aserción determinística de la imagen (legibilidad): en la
  banda del chart (y≈69..399), el segmento mínimo de la curva queda en el **cuarto inferior** de la
  banda EN la columna del mínimo, y los bordes del chart muestran curva **subiendo** respecto al mínimo.
- **Coherencia índice↔frecuencia**: para CADA muestra, index == posición de la frecuencia pedida con
  MONO-dirección por pasada según la spec del owner (pasada 1 ascendente o descendente — si el
  diseño impone descendente, la sesión tiene que saberlo; NUNCA dos contadores).
- **El readout del mínimo** sale de la cada de la sessión (freq dentro del span SIEMPRE).
- **El fix es single-source**: un único dueño del índice/dirección (si la sesión es dueña, radio.c
  deja de auto-steppear en SWRSCAN — o viceversa según lo que fundes; el menor blast radius gana).
- Sin regressión del #125 (`error_hz=0`, 51/51, budgets del #117, 3/5 gated).

## Plan de prueba (por la regla del setup — completo desde acá)

1. **Dataset de aceptación**: el CSV real del owner (commit `b39d6014`,
   `docs/evidencia/2026-10-15-validacion-arreglos/roe-muestras-extraidas.csv`) + la captura del owner
   (`26-roe-multipasada-final.png`) como RED documental del domo.
2. **RED en el arnés replicando el orden real**: el fake de radio re-escrito para modelar el
   **orden de paquetes del radio.c real** (no derivar de la sesión — independiente): pasadas y step
   según lo que funde el punto 1 (descendentes en TX si el radio.stepper hace eso), packs por paquete
   con reloj virtual, y la ventana conducida por las APIs reales. El test debe **reproducir el domo**
   (aserción de forma: mínimo arriba / fuera de span) con el código actual = RED.
3. **GREEN**: con el fix — el mismo modelo de radio real → forma U (aserciones de los criterios de
   arriba + `error_hz=0` + index/freq coherentes).
4. **Capas**: nativos (RED→GREEN como 2-3), arnés (+ aserción de legibilidad PIL sobre el PNG del
   render nuevo), A/B (`AB-SWR125DIR-…`), contracts/audit/themes, determinismo, ruff/ty/pytest,
   cross-compile ARM. Banco (humo + captura, requires-radio para el verdicto final en radio).
5. **requires-radio** declarado a priori: la confirmación final del U en la radio es del owner —
   proponé el procedimiento exacto (1 pasada con carga ficticia + captura remota read-only).

## Invariantes

- Sin RF/TX/DSP más allá del camino SWRScan afectado; nada de `lvgl/` (pinned). El presupuesto de
  #117 intocado (no subir TX budget); 3/5 pasadas siguen gated.
- Una solita escritor; commits `Pablo Iaria <iariap@gmail.com>`; sin push; `.asf-active` sin
  commitear; TODO en foreground.

## Entregables

Reporte final con: la fundación del punto 1-3 (líneas y mecanismos exactos), el fix elegido y el
blast radius, TDD RED→GREEN con el modelo de radio real, la aserción de legibilidad con números
(bordes de px), gates, banco con capturas, spec actualizada (`docs/swr125-spec.md` de la corrida
swr125-direction o parche sobre docs/roe119-spec.md → decidí con el patrón de las corridas previas),
HEAD+base, archivos cambiados, PENDIENTES (el procedimiento en radio para confirmar).

---

## UPDATE (etapa de código, 2026-10-17): la premisa del camino de datos queda REFUTADA

La investigación de los puntos 1-4 **refuta** la hipótesis del setup ("el bug está en el camino de
datos real: dos contadores radio.c ↔ swr_session"). Fundado con líneas:

1. **No hay dos contadores.** `src/radio.c:245-246` (caso `RADIO_SWRSCAN`) **solo reenvía** el paquete
   (`dialog_swrscan_update(pack->vswr * 0.1f)`) y `src/radio.c:269-270` solo llama al watchdog
   (`dialog_swrscan_poll()`). `radio_start_swrscan()` (`src/radio.c:686-699`) pone el estado y la
   potencia; **no auto-steppea el VFO ni lleva un contador de puntos**. El índice y la dirección son
   **de la sesión y de nadie más**: `src/swr_session.c:229-242` ("the storage index IS the grid
   index"), con `limits.alternate_direction` decidiendo la dirección por pasada, y el diálogo pide la
   frecuencia con `swr_session_freq()` (`src/swr_session.c:221-227`). Un único dueño → el punto 3 del
   criterio "single-source" **ya se cumple** en el código de base; no hay nada que reasignar.
2. **El `6.891.334` del readout NO está fuera del span**: es el **borde inferior** de la grilla.
   `swr_grid_build()` (`src/swr_session.c:50-73`) la arma `center ± span/2` = `6.991.334 ± 100.000` =
   `6.891.334 .. 7.091.334` (las tres etiquetas del eje son ticks interiores cada 50 kHz). El `min`
   sale de `swr_session_minimum()` sobre las muestras medidas (`src/dialog_swrscan.c:358-380`), nunca
   del ajuste dibujado, y vale 1.0 en el primer punto de la grilla: la **base del domo**. El `n=44/51`
   es el **timeout de RF** (los últimos 7 paquetes no llegaron; `abort_text(SWR_ABORT_TIMEOUT)` =
   `"timeout de RF"`, `src/dialog_swrscan.c:313`), no una desincronización de índices.
3. **La causa real es el RENDER**: `roe_curve_resample()` (`src/roe_curve.c`) ajusta una **cuadrática
   libre** por mínimos cuadrados y la dibuja tal cual; la orientación no está restringida, así que un
   barrido con forma de domo (`a < 0`) se dibuja como domo. El mapeo vertical es correcto y no puede
   dar un domo: `calc_y()` (`src/dialog_swrscan.c:295-307`) devuelve `(1-x)*h` con `x` creciente con
   la ROE (log o lineal). El arnés no lo vio porque su dataset es un valle limpio (el ajuste libre ya
   salía con `a > 0`): el bug solo aparece con un barrido medido que no sea un valle limpio.

**Fix elegido (blast radius mínimo, render-only)**: en `roe_curve_resample()`, tras el ajuste libre,
si `a < 0` se lo espeja al valle con la **misma columna del vértice** y su base en el **mínimo
medido** (`src/roe_curve.c:166-185`): `xv = -b/(2a)` (clamp a `[0, n-1]`), `a = -a`,
`b = -2*a*xv`, `c = ymin + a*xv*xv`. Un ajuste que ya abre hacia arriba se dibuja igual que antes:
los casos que funcionaban no cambian. No se toca el camino de datos, ni el barrido, ni el RF/DSP.

**TDD RED→GREEN** (ver `docs/HALLAZGOS.md` 50 para las salidas completas):

- Nativo (`tests/native/test_roe_curve.c`): RED `18522 comprobaciones, 156 fallos`; GREEN
  `18522 comprobaciones, 0 fallos`, con el print `domo (fit a=-0.0061) -> valle en x=20.00, base
  1.00, extremos 3.43 / 4.19`. El test del #119 queda igual (`valle dibujado en la muestra 48.88`).
- Arnés (`tests/check_harness_swrscan_curve.sh`, fixture nueva `swrscan-owner-40m-domo` con el domo
  del owner embebido): RED `3 FAIL` (valle en el borde `x=10`, bordes suben 0 px); GREEN valle
  `y=398 en x=284`, bordes suben `207 / 277 px`; el valle real del owner sigue valle (`20 / 12 px`).

**`requires-radio`**: el mecanismo en hardware que produjo un domo (por qué la ROE medida subió a
~3.8 en el medio) no se puede fundar desde el repo — el host no transmite. El verdicto final del U en
la radio es del owner (1 pasada con carga ficticia + captura read-only).
