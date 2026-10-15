# AB-FONTS — migración a fuentes libres (issue #9, decisión 1)

Evidencia de la corrida del cambio de tipografía, medida con el arnés A/B sobre el
árbol migrado.

| archivo | qué es |
|---|---|
| `report.md` / `report.html` / `results.json` | el veredicto de `x6200-ab compare --a 02c1311 --b HEAD --experiment-id AB-FONTS`: **PASS** (Functional, Visual Regression, Quality Contract y Performance; Hardware y Energy BLOCKED por no haber radio) |
| `diffs/`, `screenshots/`, `metrics/`, `logs/` | el diff por fixture (cajas + `diff.png`/`overlay.png`), las capturas y las métricas del framework |
| `font-matrix.md` | la matriz de candidatas medida (`tools/fonts/matrix.py report`): cajas de texto por etiqueta, tinta dibujada, defectos de legibilidad y el `score` que elige el default |
| `font-matrix-digits.md` | lo mismo para las tres familias segmentadas, sobre `main-frecuencia-dseg` (fila de frecuencia y reloj) |
| `gallery.png` | la galería para el ojo del owner: barra superior + fila de frecuencia + botones de las 15 familias completas y de las tres de dígitos |

Cómo se reproduce:

```sh
tools/fonts/fonts.py fetch --licenses && tools/fonts/fonts.py generate && tools/fonts/fonts.py verify
tools/fonts/matrix.py capture --out /tmp/fontmatrix            # 8 fixtures × 15 familias (--font)
tools/fonts/matrix.py report --baseline <render pre-migración> --captures /tmp/fontmatrix
```

La línea base es el render del árbol **antes** de la migración (la familia
propietaria) con el mismo arnés: `--a 02c1311` en el `compare`, y para la matriz un
`capture` sobre `02c1311` con los mismos 8 fixtures.
