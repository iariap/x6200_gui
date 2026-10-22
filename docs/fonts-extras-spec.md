# Spec: fonts-extras — el "8 fantasma" DSEG + kerning GPOS→kern + manual + poda de roles muertos

**Fecha**: 2026-10-15 · **Corrida**: `factory/fonts-extras` · **Base**: trunk `11f8d689`
**Orden del owner**: "avanza con todos" sobre el cuadro de pendientes (los 4 ítems de
fuente/tema/manual en una sola corrida porque comparten `src/fonts/` generados).

## Pieza 1 — el "8 fantasma" (segmentos apagados estilo Casio, DSEG)
- **Estado**: DSEG7/14 ya compilan slots 30/38 con `--lv-fallback` a la familia default
  (#60). Falta la capa que dibuja los segmentos **apagados** detrás del dígito (el look
  Casio completo).
- **Diseño mínimo propuesto** (el de la mesa; ajustalo con la lectura del código):
  un **label/objeto interactivo por dígito** NO (demasiados objetos). La vía más ligera:
  los dígitos del panel de frecuencia y del reloj se dibujan **con el glifo `8` de la
  familia digit en alpha bajo detrás del valor real** — implemento como **segundo
  `lv_label` sombra** por panel (2 objetos extra en total, no por dígito: label sombra
  con texto "88:88.888" duplicado alineado de la forma ANTERIOR al valor), reclames
  del riego **render-price en el bench ECO** (predichos: +2 sweeps? Arch: el extra
  raster es casi todo región declarada; MEDELO).
  - MUST protect: al recortar el reloj/la frecuencia cuando hay `_ds` state changes,
    el sombra AHORA sigue; sync del texto (ambos labels se setean juntos: hacer helper
    `freq_shadow_update()` y `clock_shadow_update()` que esperi un solo set).
  - Cambios de tema: la alpha DEL fantasma debe llevar `LV_OPA` fija adaptativa
    (declared token si sale necesaria: `digits_shadow_opa` — si el tema brinda la
    métrica, revisar el `contrast` de los dígitos — el shadow opa NO PISA el valor).
- **Cobertura**: fixture `main-frecuencia-dseg` MEJORA: sombra TRAS el valor. Expected
  regiones + golden si toque. La tecla del panel es el hook que ya está del #60.
- ** alternatives descartadas con **note en el reporte**: pretender que la propia FONT
  renderice (lias sheets) no existe en el formato bitmap-src que LVGL deja; la capa
  de "alpha del dígito" es la forma honesta; cambia de ambos lados.

## Pieza 2 — **kerning GPOS→kern** vía fontTools (el pipeline, un paso previo)
- En `tools/fonts/fonts.py`: con `fontTools` (pip en el venv del propia run; o
  `pip install --user` no: 'usar uv run — mirá cómo el repo env envía python de
  tools') extraer el kerning **GPOS** de cada TTF y convertirlo al formato legacy
  **`kern`** que `lv_font_conv`'s reader (1.5.3) entiende (verify que la subida
  espara el formato `kern` correcto: es el pair de la **table** con bins que
  `lv_font_conv` 1.5.3 lee para kerning; leer el fuente del tool para el formato).
- El resultado **SÓLO si el delta es visible y medible en la matriz** (con los
  Δw de `--inspect` barriendo las fixtures antes/después): el texto con kerning
  suele BAJAR un pelito de ancho en las string de UI; la re-carretera es la MISMA
  de #60 (regiones + expected_changes scoped con razón/").

## Pieza 3 — **manual §7.1**: la línea de `{"cmd":"app","text":"off"}` (ahora cierra
apps/RTTY). Una línea en la sección de comandos útiles. (Era PENDIENTE del #59.)

## Pieza 4 — **poda de roles de tema sin render**: `mode_text`/`mode_bg`/`mode_border`
y la superficie `strip` quedaron sin consumidor tras borrar `freq_strip` (#59). Ellos hoy
viven en `src/theme.h` (tabla) + `check_wcag.py` (pares) y consumen espacio/micró. **Podá**:
- quitar las entradas de la tabla (4 roles × 9 temas) + del checker (`theme_pairs.yaml`),
  con la confirmación de que NINGUNA pantalla los referencia (grep `mode_text|mode_bg|`
  `mode_border|panel.*strip` de src/): si NO queda CRUZADO con algo de UI,
  borrar enteros; si alguna sobrevive (ej, el hairline?), decláralta de la lista
  con razon.
- Los contratos: nuevo conteo de pares NO página; regiones intactas.

## Gates (usual)
```bash
cd /home/pablo/desa/asf/state/worktrees/x6200/fonts-extras
tests/check_harness_determinism.sh          # 35 x2 (sombra + kerning mueven región)
cd tools/ab_testing
uv run x6200-ab compare --a 11f8d689 --b HEAD --experiment-id AB-FONTSE
uv run x6200-ab contracts --revision HEAD && uv run x6200-ab audit --revision HEAD --suite visual
uv run x6200-ab themes
uv run pytest -m "not e2e" -q && uv run pytest -m e2e -q    # 3 sprints si tocás performance.py
tests/native/run_native_tests.sh
estáticos 7/7
ruff/format/ty limpios
```

## Fuera de alcance
- `lvgl/` pinned (74d0a816). DSP/RF/TX/audio; `.rodata` (requires banco — un run propio);
  las fuentes de radio raw ( publicación física del look).
- El re-baseline del corpus otra vez: SÓLO las regiones de `dseg`/sombras y las del
  kerning — delporte-crecento del tiempo con `expected_changes` por región.

## Invariantes
- Commits `Pablo Iaria <iariap@gmail.com>`. **Sin push/PR** (publica el orquestador).
- El bench natural x3 como barra del ghost (si el delta pixel no reproduce, no verification).
- PENDIENTE fundado > supuesto.

## Reporte final
1. Pieza 1: el sombra con su medición del bench (antes/después px/región) y los deltas del
   render en el contexto ECO (reproducible x3).
2. Pieza 2: el kerning → el delta de ancho ANTES/DESPUÉS de la matriz (¿p95 blew? ¿promedio de
   w bytes por font) y la inclusión fontTools del pipeline (si falla el GPOS extractor con
   una familia, declararlo: cada TTF se pega).
3. Pieza 3: el diff del manual.
4. Pieza 4: los roles eliminados (y los checadores alineados) y el conteo antes/después.
5. Gates completos con salida real.
6. PENDIENTES honestos.
