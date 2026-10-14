# Spec: ui05-freq-panel — issue #10, incremento 3: tokens del panel de frecuencia

**Fecha**: 2026-10-14 · **Corrida**: `factory/ui05-freq-panel` · **Repo app**: `iariap/x6200_gui`
**Base**: tip post-`19aa618` (incremento 2 mergeado: barra de info en flex real, pixel-identical)

## Outcome
Extender la capa de tokens a la **fila de frecuencia** (los literales de posición/tamaño de `freq[]` en `src/main_screen.c` y de los extremos en `src/freq_strip.c`), con la misma disciplina del incremento 1: **cero cambio de valor efectivo**, pixel-identical en las 16 fixtures. La conversión a flex de esta fila queda como incremento 4 evaluado *después* (acá hay markup de recolor del EXP-3 y tamaños mixtos sony_20/30: el diff de flex no sería trivialmente cero).

## Alcance
1. `src/ui_tokens.h`: familia `UI_FREQ_*` (origen/ancho/alto de la fila central, pitch de extremos, tamaños relativos) tomada de los literales reales de `main_screen.c`/`freq_strip.c` (leer el código, no inventar métricas).
2. `main_screen.c` + `freq_strip.c`: consumir los tokens; **no** cambiar valores, markup, roles de color ni lógica de freq_lock.
3. `docs/AB_TESTING.md`: sección incremento 3.

## Fuera de alcance
- Flex en la fila de frecuencia (incremento 4, caso B probable → decision aparte).
- `spectrum/waterfall/band_plan` (prohibido por el issue); `styles.c`; otras regiones.
- Cualquier cambio visual: si un pixel se mueve, está mal (repetir/limpiar).

## Criterios (iguales que incremento 1 — el estándar de los refactors token)
```bash
tests/check_harness_determinism.sh                      # 16×2, hashes = los del tip base
uv run x6200-ab compare --a <base> --b HEAD --experiment-id AB-UI05-T3   # 16/16 changed_pixels 0
uv run x6200-ab contracts --revision HEAD               # PASS 160/0/0
uv run x6200-ab audit --revision HEAD --suite visual    # PASS 0 findings
uv run x6200-ab benchmark --a <base> --b HEAD           # delta 0 en flushes/área
pytest -m "not e2e" · pytest -m e2e -k audit · native · estáticos · ruff/ty
```
Sensibilidad anti-falso-verde: alterar temporalmente un token `UI_FREQ_*` debe mover la fila en las capturas y revertido restaurar los 16 hashes exactos.

## Riesgos específicos
- `main_screen.c` tocó escena en EXP-3 (markup `theme_hex`+`%06X`): preservar literalmente esa lógica.
- `_inject_structural_defects` de `test_e2e.py` parchea strings/colores del frecuencia: verificar intactos y correr el e2e.
- Si la geometría de la fila está repartida entre varios `set_pos` con aritmética de layout anidado (no literales puros), tokenizar SOLO lo que sea métrica estable y declarar el resto como no-tokenizable con motivo (como la runa BT en el incr. 1).
