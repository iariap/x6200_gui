# Validacion en la radio de UI-05 (tokens + flex de la barra) — 2026-10-14

Validado en la radio: PRs **#25** (`41532e7`, tokens de la geometria de la barra) y **#27** (`19aa618`,
la rejilla de la barra pasa a flex real), ambos en `src/info.c` + `src/ui_tokens.h`.

Build probado: `x6200_gui` md5 **`42d79b7d1f0e`** (rama `lu2dsx/x6200-rom` en `19aa618`).

## Lo que sigue bien (sin regresiones)

| Captura | Que se verifico |
|---|---|
| `01-material3.png` | barra completa con el tema por defecto: VFO, modo, PRE/ATU1, iconos de bateria/enchufe/WiFi, y la fila de frecuencia legible |
| `02-vision.png` | con **VISION**: los **iconos** vuelven (bateria, WiFi, enchufe: fix UI-011 ✓) y la celda **`VFO-B`** entra completa (fix UI-012 ✓) |
| `02-outdoor.png` | con **Outdoor**: la fila de frecuencia se ve (fix UI-010 ✓) y las celdas de la barra quedan separadas |

Chequeos: WCAG, handlers, apps en el menu y nativos, todos verdes. `verifica-radio.sh`: TODO OK.

## Defecto encontrado (preexistente, NO de UI-05)

Con **VISION** y un modo de **4 caracteres** (`CW-R`), la celda del modo se **pega a la de `AUTO`** y en
pantalla se lee `CW-RAUTO` (captura `02-vision.png`). Con `CW` (3 caracteres) no pasa, y con temas de
fuente normal tampoco (`02-outdoor.png`).

**A/B para saber si era de UI-05**: se recompilo con `src/info.c` del estado **previo** a los dos PRs
(`41532e7^`, binario md5 **`6f8275c7e099`**, el mismo que estaba en la radio antes de esta validacion)
y con el **mismo tema (VISION) y el mismo modo (`CW-R`)**: la barra muestra el **mismo `CW-RAUTO`**
(captura `03-vision-SIN-ui05.png`). O sea: **no** es una regresion de UI-05, es un defecto que ya
estaba, hermano del UI-012 (que arreglo solo la celda del VFO para VISION).

Reportado como issue aparte con esta evidencia.
