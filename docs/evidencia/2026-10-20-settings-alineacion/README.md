# Settings: la columna de widgets de la derecha no esta alineada

Reporte del dueño (20/10 09:07): "la pantalla de settings se vio media rara, creo que la columna de
los widgets de la derecha no estaba alineada".

## Medido (build `57b11ddc058a`, radio real, tema Aurora, 800x480)

Capturas de las 5 paginas de Settings (`settings-01..05.png`), con el borde izquierdo de cada widget
detectado por pixeles (columnas de borde vertical claras):

| Pagina | Fila (ejemplo) | Borde izq. del widget |
|---|---|---|
| Operation | `Freq acceleration` | **499** |
| Operation | `Bluetooth` / `Discoverable (CAT)` / `Charger` | **643-646** |
| Audio | `Line-in, Line-out` (toggle + spinbox) | **500** + **644** |
| Audio | `Audio enhance` → boton "SSB voice" | **501** ✗ |
| Audio | `Speaker mode` (toggle) | **645** ✗ |
| Spectrum | filas con toggle chico + toggle ancho | **500** + **644** |
| Spectrum | `Waterfall smooth scroll` / `battery mode` | **645** |
| Clock | fila de 3 widgets | **500 / 596 / 692** |

**El patron**: el borde izquierdo del widget de la derecha cae en **x≈499** en unas filas y en **x≈643**
en otras. En la misma pagina (Audio) "SSB voice" arranca en 501 y "Speaker mode" en 645.

## Causa (codigo)

`src/dialog_settings.c`: la grilla es `col_dsc = { LV_GRID_FR(1), 45,45,45,45,45,45 }` (columna de
etiquetas flexible + 6 columnas de 45 px). **Cada fila elige columnas y span a mano**:

- filas con un widget solo, span 6 → arranca en la columna 0 de la zona chica → **x≈499**;
- filas con "widget chico + widget ancho" → el chico en col 0 (**x≈500**) y el ancho en col 3 →
  **x≈643**;
- filas de 3 widgets (Clock) → 500 / 596 / 692.

No hay una columna derecha unica: hay **dos o tres orígenes** segun la fila.

## Que se verifico y que se descarto

- **NO es regresion del PR #174 (DFL)**: ese merge toco solo `src/spectrum.c/h` y `src/waterfall.c/h`
  (346 lineas), **nada** de `dialog_settings.c`.
- **Ya existia antes**: en la captura del 19/10 (`docs/evidencia/2026-10-19-opt4-150/banco-settings.png`)
  los bordes verticales caen en los mismos dos grupos (499 / 644).
- **Tema**: pasa con el tema del dueño (Aurora); no es un tema en particular.
- **No es un bug de render**: los widgets estan donde el codigo los pone; es el layout.

## Extra observado (cosmetico)

El dialogo de Settings es **semitransparente** y deja ver la pantalla principal: en las capturas se ve
el "3.3k" del texto del filtro DFL y la linea del VFO **a traves** del fondo ✗. Puede sumar a la
sensacion de "raro".

## No lo resuelvo

Regla del rol de QA: solo monitoreo, pruebas, triage y reporte. Esto es la evidencia para el que haga
el rediseno (issue #173 o uno propio de Settings).
