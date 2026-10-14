# Spec: ui05-flex — issue #10, incremento 2: Flex real en la barra de info

**Fecha**: 2026-10-14 · **Corrida**: `factory/ui05-flex` · **Base**: `41532e7` (tip con incremento 1)

## Outcome
Primera conversión de layout real a **LVGL flex** (LVGL 8, sin LVGL9): la rejilla 3×2 de `src/info.c` — que hoy posiciona cada celda con `x*58+15, y*22+5` — pasa a dos contenedores flex (uno por fila) alimentados por los **tokens** del incremento 1. Objetivo: misma salida pixel a pixel; el flex deja de ser aritmética manual y pasa a ser la fuente de la disposición.

## Diseño propuesto (verificable, sin magia)
- Un `lv_obj` contenedor por fila, con `LV_FLEX_ROW`, `flex_grow/track` fijos y **gap = `UI_INFO_CELL_GAP`**; items de `UI_INFO_CELL_W×H`.
- El **overlap de filas (pitch 22 < alto 26)** se conserva posicionando los contenedores en el origen exacto de cada fila (`UI_INFO_ROW_PITCH`) — flex gobierna el eje X; el Y de cada fila sigue a los tokens. No se introducen márgenes negativos.
- Celda WiFi/BT (`UI_INFO_WIFI_*`) y la runa BT siguen su camino actual (la runa es una forma, no layout).

## Criterios de aceptación
```bash
tests/check_harness_determinism.sh                       # PASS 16×2
uv run x6200-ab compare --a 41532e7 --b HEAD --experiment-id AB-UI05-T2
#  CASO A (esperado): 16/16 changed_pixels 0  → PASS puro (mismo estándar que incremento 1)
#  CASO B (si flex mueve algún pixel): cada fixture afectada debe quedar DENTRO de
#    barra_superior, y se declara expected_changes scoped (motivo, autor, until
#    2027-03-31) + capturas A/B/diff/overlay en reports/AB-UI05-T2/ → REVIEW REQUIRED
#    justificado en el PR. Prohibido: waivers de regiones no documentadas.
uv run x6200-ab contracts --revision HEAD                # PASS (160/0/0 o con expected_changes nuevos)
uv run x6200-ab audit --revision HEAD --suite visual     # PASS 0 findings
uv run x6200-ab benchmark --a 41532e7 --b HEAD           # flushes/área delta 0
pytest -m "not e2e" · native · estáticos · ruff/ty       # verdes
```
Sensibilidad: una prueba que demuestre que el layout sale del flex (p. ej. cambiar
temporalmente el gap del contenedor mueve la celda USB y revertir lo devuelve),
documentada en el reporte — anti-falso-verde como en el incremento 1.

## Fuera de alcance
- Otras regiones (band_plan, botones, settings rows) → incrementos 3+.
- Cambiar tokens/valores efectivos; tocar `main_screen.c`, `styles.c`, `clock.c`;
  spectrum/waterfall (prohibido "blindly convert" per issue).
- LVGL9, estilos nuevos, JSON de tema.

## Riesgo
`src/info.c` (flex containers = +2 objetos al árbol → INSPECCIÓN ESTRUCTURAL:
el audit cuenta 1350 objetos; con contenedores extra el conteo y los PATHS
`0/N/i` de los labels CAMBIARÍAN ⇒ detectar si los detectores/inyecciones del
framework dependen de paths; adaptar tests del framework si hace falta, sin
debilitarlos; `test_e2e.py::_inject_structural_defects` busca textos/colores,
verificar que sigue apuntando). Este es el riesgo REAL del incremento: si los
paths estructurales se mueven, el PR debe actualizar los consumidores (framework
tests) y declararlo en el body con evidencia.
