# Spec: roe-parabola-113 — el chart de ROE dibuja una parábola (issue #113)

**Fecha**: 2026-10-16 · Corrida: `factory/roe-parabola-113` · Base trunk `3d6c022`
**Issue**: #113 — orden del owner: "la medición de la ROE tiene que ser una parábola".

## Estado actual (por qué existe esta corrida)

`#104` (cierre de `#99`) dibuja una **curva Catmull-Rom de 393 puntos** (`src/roe_curve.[ch]`,
`dialog_swrscan.c`): cada muestra del barrido es un nudo exacto → la curva sigue cada muestra,
incluidas las ruidosas. El owner miró el resultado y pide **parábola**: una sola forma limpia de
valle (un ajuste cuadrático de las muestras), no un spline que se adhiere a cada punto.

## Qué implementar

1. **Ajuste cuadrático de las muestras del barrido** (least squares: `y = a·x² + b·x + c`)
   en `src/roe_curve.[ch]` — **reemplaza** la interpolación Catmull-Rom. La curva renderizada
   (`curve[]`, ~2 px/segmento como hoy) sale del fit, no de los nudos. (Con el barrido automático
   de más abajo el conteo es 41..51 según el span: el fit es sobre las muestras del barrido
   activo, no sobre un 50 fijo.)
   - Justificación del método en el código: least squares sobre todas las muestras = el fit
     estable con ruido; si encontrás una razón sólida para otra variante (p. ej. robusto a
     outliers), podés elegirla, pero **documentá la razón con números** en el reporte.
   - Sin `math.h` si el archivo hoy lo evita (compila en el stage nativo tal como está —
     mirá cómo hace `roe_curve.c` hoy; si necesitás `pow`, verificá el link del firmware y de
     los tests nativos, y del harness si aplica).
2. **El readout (ROE mínimo y frecuencia del mínimo) sigue saliendo de las MUESTRAS medidas**
   — la data manda, no el fit. Reportá el delta **vértice del fit vs mínimo medido** (frecuencia
   y valor) con las muestras sintéticas de tus tests: si el delta en un barrido real típico
   fuera grande (p. ej. > 2 muestras de separación), declaralo como PENDIENTE para revisión
   estética del owner en radio (él pidió parábola; la decisión visual es suya).
3. **Clamp conservado**: el techo ya declarado (`ROE_CURVE_MAX_VALUE`, >5 al borde superior)
   aplica a la parábola por segmento — la parábola se va a infinito en los extremos; debe
   recortarse al borde superior del chart sin dibujar fuera del área.
4. **Marcadores de muestra**: si el chart hoy marca/dibuja de algún modo las muestras, reportá
   qué pasa con ellas tras el cambio (un parábola + puntos sueltos puede ser la vista correcta
   para el operador: medida vs ajuste — no los elimines sin reportarlo; si no hay marcadores,
   no agregues).
5. **Determinismo puro**: mismo barrido → misma curva (memcmp en el test).

## TDD (RED/GREEN obligatorios — pegá las salidas)

Actualizá/adaptá `tests/native/test_roe_curve.c` (los tests actuales de #99 verifican propiedades
de spline que YA NO apliquen — reescribilos a las de parábola, manteniendo lo que sigue vigente):
- Fit correcto: muestras sintéticas de una parábola conocida (ruido cero) → `a,b,c` recuperados
  dentro de epsilon → y la curva pasa ~por esos valores.
- Con ruido: el fit no se va a los extremos (estabilidad), el mínimo de la curva queda en el
  valle esperado.
- Vértice: cálculo del vértice (`-b/2a`) coherente con la curva dibujada.
- Clamp: extremos de la parábola recortados al techo, sin coordenadas fuera de rango.
- Determinismo: mismo input → buffer idéntico (memcmp).
- El readout mínimo: sigue apuntando a la muestra medida, no al vértice del fit.
RED real: primero rompí el fit (o dejá el Catmull-Rom) y pegá la salida roja; luego GREEN.

Para las **ampliaciones** (barrido automático + piso de potencia), el TDD vive en
`tests/native/test_swrscan.c::test_swrscan_sweep()`: tabla span → puntos → paso exacto →
duración, extremos en los límites del span, determinismo (memcmp) y piso de potencia. RED real:
los tests nuevos contra la implementación de HEAD (50 puntos fijos, paso `span/50`) → 111
comprobaciones, 73 fallos; GREEN: 102 comprobaciones, 0 fallos.

## Gates (pegá la salida real)

```bash
cd /home/pablo/desa/asf/state/worktrees/x6200/roe-parabola-113
tests/native/run_native_tests.sh
tests/check_harness_determinism.sh
cd tools/ab_testing
uv run pytest -m "not e2e" -q && uv run pytest -m e2e -q
uv run x6200-ab compare --a origin/lu2dsx/x6200-rom --b HEAD --experiment-id AB-ROEPARAB
uv run x6200-ab contracts --revision HEAD && uv run x6200-ab audit --revision HEAD --suite visual
uv run x6200-ab themes
uv run ruff check . && uv run ruff format --check . && uv run ty check
cd ../.. && estáticos 7/7 (8/8 si audita-pruebas aplica por docs)
```
(Ojo: `dialog_swrscan` está mockeado en el arnés UI — el A/B probablemente salga pixel-idéntico;
el peso lo cargan los tests nativos. Reportá qué cambió realmente.)

## Banco QEMU (regla del owner: cierre con prueba QEMU salvo requiere-radio)

El diálogo de SWR Scan **sí corrió en banco antes** (la repro de #84/#85 corrió 5 scans ×2 ahí):
- Intentá capturar el diálogo con la parábola dibujada (seguí el método de navegación de esa
  evidencia — `docs/evidencia/` de #84/#85 o el README que quede, y el gate #86 + `docker ps`
  antes; banco libre ahora).
- Si el stub del banco no trae datos de ROE y el chart sale vacío → **declará requires-radio**
  explícito con la razón en el reporte, junto con la evidencia que sí se rindió (el diálogo
  abre/cierra sin crashes, TDD con datos sintéticos).

## Ampliación de alcance 1 (owner, 2026-10-16): "aumentar la frecuencia de barrido como el NanoVNA"

### Medición del barrido ANTES de esta ampliación (números del código)

| Dato | Valor | Fuente |
|---|---|---|
| Puntos por pasada | **50 fijos** | `SWRSCAN_POINTS` (`src/dialog_swrscan.h`, eliminado por esta ampliación) |
| Paso | `span / 50` → **1 kHz** (span 50 kHz); 2 / 4 / 10 kHz (100 / 200 / 500 kHz) | `dialog_swrscan_sweep_freq()` |
| Dwell por punto | **~35 ms** = un paquete del flow de la radio | `flow.h` ("Usually a packet arrives every 35ms"); `do_step` corre una vez por paquete (`RADIO_SWRSCAN`) |
| Duración del barrido | 50 × 35 ms = **~1,75 s** | `SWRSCAN_POINTS × SWRSCAN_DWELL_MS` |
| Potencia del barrido | **3 W** en LSB (TX continuo mientras mide) | `radio_start_swrscan()` → `x6200_control_txpwr_set(3.0f)` |

El dwell **no es un parámetro de la GUI**: el barrido avanza un punto por cada paquete del flow
(`dialog_swrscan_update` ← `radio_tick` ← `RADIO_SWRSCAN`) y `aether_control` sólo expone
`x6200_control_swrscan_set(on/off)`, sin cadencia ni cantidad de puntos. Por eso **más puntos =
más tiempo al aire**, proporcionalmente, y bajar el dwell no está en la mano del software.

### Decisión 1: barrido AUTOMÁTICO por span

El conteo de puntos deja de ser fijo y **sale del span que se está mirando** (como el NanoVNA:
denso en spans chicos, acotado en los anchos). `dialog_swrscan_points_for_span(span_hz)`:

1. `objetivo = span / SWRSCAN_STEP_TARGET_HZ` (1250 Hz), acotado a
   `[SWRSCAN_MIN_POINTS-1, SWRSCAN_MAX_POINTS-1]` = `[40, 50]` intervalos.
2. `intervalos` = el **mayor divisor de `span` ≤ objetivo** (el paso nunca queda más fino que el
   objetivo; si el span no tiene divisores en el rango, manda la exactitud y el conteo baja hasta
   2 puntos).
3. `puntos = intervalos + 1`.

La geometría es **borde a borde**: `paso = span / (puntos - 1)` **exacto** (sin resto), `index 0` =
borde bajo del span, `index puntos-1` = borde alto. Esto **cierra la nota pendiente** de la primera
ampliación (antes la última muestra quedaba un paso abajo del borde alto).

| Span | Puntos | Paso (Hz) | Duración (puntos × 35 ms) |
|---|---|---|---|
| 50 kHz | **41** | 1.250 | 1.435 ms |
| 100 kHz | **51** | 2.000 | 1.785 ms |
| 200 kHz | **51** | 4.000 | 1.785 ms |
| 500 kHz | **51** | 10.000 | 1.785 ms |

Inventario de spans del diálogo (los **únicos** que ofrece): `dialog_swrscan_span_cb()` cicla
50000 → 100000 → 200000 → 500000 → 50000 (default 200000 en `src/cfg/cfg.c`); no hay comando
remoto de span (`remote_server.cpp` no expone span).

**Techo explícito**: `SWRSCAN_TX_BUDGET_MS = 1785` = `SWRSCAN_MAX_POINTS (51) × SWRSCAN_DWELL_MS
(35)`, o sea el barrido más largo de la tabla. Queda **≤ 2 s** → se implementa directo (regla del
owner). `_Static_assert(SWRSCAN_MAX_POINTS * SWRSCAN_DWELL_MS <= SWRSCAN_TX_BUDGET_MS)` en
`dialog_swrscan.c`: subir los puntos sin decisión del owner **no compila**.

**Reevaluación pedida por el owner con TX mínima** (ver Decisión 2: el PA dejó de ser la
restricción; lo que acota es la espera del operador). La alternativa que el presupuesto deja
afuera es `SWRSCAN_MAX_POINTS = 81` (81 × 35 = 2835 ms):

| Span | 51 puntos (implementado) | 81 puntos (opción, 2,8 s) |
|---|---|---|
| 50 kHz | 41 pts / 1.250 Hz | 41 pts / 1.250 Hz — **idéntico** |
| 100 kHz | 51 pts / 2.000 Hz | 81 pts / 1.250 Hz |
| 200 kHz | 51 pts / 4.000 Hz | 81 pts / 2.500 Hz |
| 500 kHz | 51 pts / 10.000 Hz | 81 pts / 6.250 Hz |

El span de **zoom** (50 kHz, el que el operador usa para sintonizar la antena) queda idéntico: los
81 puntos sólo mejoran los spans de panorama, a cambio de un barrido 58 % más largo (1,8 s → 2,8 s).
Supera los ~2 s de la regla del owner → queda como **decisión pendiente**, no implementada.

**Buffers**: `data[SWRSCAN_MAX_POINTS]`, `data_filtered[SWRSCAN_MAX_POINTS]`,
`curve[(SWRSCAN_MAX_POINTS-1)*ROE_CURVE_OVERSAMPLE+1]` = ~2 KB estáticos (no stack).

## Ampliación de alcance 2 (owner, 2026-10-16): el barrido transmite al piso de potencia

### Medición del piso (fuente en el código, no supuesto)

| Dato | Valor | Fuente |
|---|---|---|
| Piso del control del **panel** | **0.1 W** | `src/vol.c`, `case VOL_PWR`: `f = LV_MIN(8.0f, f); f = LV_MAX(0.1f, f);` |
| Rango del control **remoto** | 0.1 .. 10.0 W | `src/remote_server.cpp`: `{"pwr", CMD_PWR, 1, 100}` (décimas de watt) |
| Unidad del registro | décimas de watt | `x6200_control_txpwr_set`: `uint8_t p = pwr * 10.0f` → byte bajo de `rfg_txpwr` |
| Potencia del barrido (antes) | **3 W fijos** | `radio_start_swrscan()` → `x6200_control_txpwr_set(3.0f)` |
| Potencia del barrido (ahora) | **0.1 W** | `SWRSCAN_TX_POWER_DW = 1` (`src/dialog_swrscan.h`) |

El NanoVNA mide a ~0 dBm (1 mW): el X6200 **no llega ahí** (su control no declara nada por debajo
de 0.1 W), así que "mínima posible" = el **piso del equipo**. 0.1 W = 20 dBm = **14,8 dB menos**
que los 3 W de antes y 20 dB por encima del NanoVNA.

**Restore (ya existía, patrón #84/#102)**: `radio_stop_swrscan()` sigue devolviendo la potencia del
operador (`x6200_control_txpwr_set(subject_get_float(cfg.pwr.val))`) y el modo guardado al arrancar;
además el dispatcher de apps tiene el backup genérico (#94, `radio_state_backup_t` incluye `pwr`).
La potencia se pone en `radio_start_swrscan()` y **no** en el diálogo a propósito: tiene que estar
en el piso **antes** de `x6200_control_swrscan_set(true)` (lo que arranca el TX) y las dos
escrituras van juntas bajo el lock de radio.

**¿Alcanza para medir ROE?** El medidor es del firmware: el paquete del flow trae el campo `vswr`
(`radio.c` → `dialog_swrscan_update(pack->vswr * 0.1f)`) y la GUI no tiene ningún umbral de
potencia. En el código **no hay evidencia de un umbral mínimo de medición**; que el bridge siga
midiendo con la misma calidad a 0.1 W es **requires-radio** (declarado como PENDIENTE en el reporte,
no asumido).

### Codificación (lo que hace fallar el build/el test si alguien lo cambia)

- `SWRSCAN_TX_POWER_DW` / `SWRSCAN_PWR_FLOOR_DW` / `SWRSCAN_PWR_CEILING_DW` y `SWRSCAN_TX_POWER_W`
  en `src/dialog_swrscan.h` (con la fuente del piso en el comentario).
- `_Static_assert(SWRSCAN_TX_POWER_DW >= SWRSCAN_PWR_FLOOR_DW && <= SWRSCAN_PWR_CEILING_DW)` en
  `dialog_swrscan.c`: una potencia fuera del control del equipo no compila.
- `tests/native/test_swrscan.c`: el barrido usa **exactamente** el piso (`== SWRSCAN_PWR_FLOOR_DW`,
  `== 0.1f`) y menos que los 3 W de antes.

## Invariantes

- **Display-only** para la parábola: NO tocar el pipeline de medición (RF/DSP/medidor) — sólo la
  curva dibujada y su fit. Las dos ampliaciones del owner **sí** cambian la *configuración* del
  barrido (conteo/geometría, Decisión 1) y la *potencia* del barrido (0.1 W mientras mide,
  Decisión 2), y nada más: el medidor, el DSP y el modo LSB del barrido quedan igual.
- El **restore** de potencia y modo al salir del diálogo sigue siendo el del patrón
  #84/#102/#94 (`radio_stop_swrscan()` + el backup del dispatcher de apps).
- No tocar `lvgl/`, temas, ni los runs en vuelo (#109 settings corre en paralelo en OTRO
  worktree — no mezcles).
- Commits `Pablo Iaria <iariap@gmail.com>`; **NO push ni PR** (publica el orquestador);
  `.asf-active` sin commitear.

## Reporte final

1. Método elegido + razón con números (y el delta vértice vs mínimo medido).
2. TDD RED → GREEN con salidas (parábola en `test_roe_curve.c`; barrido automático y piso de
   potencia en `test_swrscan.c`).
3. Gates con salida real. 4. Banco (captura o requires-radio declarado). 5. HEAD + base.
6. PENDIENTES honestos (revisión estética en radio del owner, decisión de los 81 puntos, y la
   medición de ROE a 0.1 W).
