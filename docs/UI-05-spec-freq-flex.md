# Spec: ui05-freq-flex — issue #10, incremento 4: fila de frecuencia a flex real

**Fecha**: 2026-10-14 · **Corrida**: `factory/ui05-freq-flex` · **Base**: tip post-merge del incremento 3 (PR #29)

## Outcome
Convertir la fila de frecuencia (3 labels que teselan [150][500][150], tokens del incremento 3) a **flex real** como se hizo con la barra en el incremento 2: un contenedor `LV_FLEX_ROW` de 800px cuyos hijos crecen/posicionan por flex (`flex_grow`: side=0, main=0 con widths fijos de `freq_style`/`freq_main_style`).

## Expectativa honesta: CASO B probable
A diferencia de la barra (celdas uniformes), acá flex introduce **sub-píxeles de redondeo** en la distribución (800 = 150+500+150 exactos: ojalá caso A). El criterio:
- **CASO A** (0 px en las 16): PASS puro, mismo estándar que incr 1–3.
- **CASO B** (diffs): deben quedar **confinados a la región `frecuencia`** (rect `[0,160,800,36]`), con `expected_change` scoped (`frecuencia`, motivo, `until 2027-03-31`) + capturas A/B/diff/overlay + **declaración REVIEW REQUIRED explícita en el reporte** para decisión humana del orquestador/owner. Prohibido: tocar presupuesto/contratos para que pase, o diffs fuera de la región.

## Riesgos específicos (inventariar antes de codear)
- La geometría Y de la fila es cursor apilado (`y += spectrum_height`) — el contenedor flex NO puede tocar el Y (fuera de scope: espectro prohibido). Solo X.
- `freq[1]` central tiene el markup de recolor del EXP-3 (`#%06X`+`theme_hex`) — preservar literal.
- `home-frecuencia-extrema` (frecuencia máxima) y el multiplicador UI-001 (path `0/18`…): los paths estructurales pueden cambiar (+1/−3 hijos netos bajo el padre nuevo) → repetir el inventario de consumers del incremento 2 (audit/paths/injections/e2e). El conteo de objetos 1382→1383 (o −2+1): verificar.
- El detector `region_content_above` (UI-001) y `region_min_content:frecuencia` miden rectángulos de píxel → unaffected por flex si no cambia el render.

## Gates (idénticos a incrementos 1–3, host)
determinismo 16×2 · compare --a <base> --b HEAD --experiment-id AB-UI05-T4 · contracts · audit · benchmark (flushes/área Δ0) · pytest incl. `-m e2e -k audit` · native · estáticos · **sensibilidad** (gap/width del contenedor mueve la fila → revert exacto).

## Post-merge (orquestador)
Gate QEMU banco: rebuild tip + home smoke + VISION/CW-R (evidencia #28) — ya ritualizado.

## Fuera de alcance
band_plan strip (forma dibujada, no layout), styles (ya roles), freq_strip.c (sin caller — cuando tenga fixture), Y cursor.
