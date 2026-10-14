# Spec: ui05-tokens — issue #10, design tokens (incremento 1: capa de tokens + info.c)

**Fecha**: 2026-10-14 · **Corrida**: `factory/ui05-tokens` · **Repo app**: `iariap/x6200_gui`
**Base**: `135f297` (tip de `lu2dsx/x6200-rom`; incluye UI-04 completo + skill QEMU #24)
**Issue**: #10 ([UI-05] Design tokens, reusable widgets and Flex/Grid layouts)

## Qué se quiere y por qué (outcome)

El issue pide centralizar spacing/typography/widget metrics y reemplazar
posiciones hardcodeadas **progresivamente** ("small reviewable PRs, not single
massive rewrite"). Este es el **incremento 1**: la capa de tokens + la primera
conversión, elegida por ser el nido de números mágicos que ya causó dos defectos
(UI-011/UI-012 vivían en la geometría fija de `info.c`): las 6 celdas de la
barra superior. El resto de regiones (botones, band_plan, settings rows) sigue
en incrementos posteriores.

## Alcance incremento 1

1. **`src/ui_tokens.h`** (nuevo): constantes nombradas de geometría/roles para
   la barra de info (ancho/alto de celda, pitch, gap, márgenes; fuente por
   defecto de celda). Sin macros oscuras: nombres de dominio
   (`UI_INFO_CELL_W`, `UI_INFO_CELL_PITCH`, …).
2. **`src/info.c`**: reemplazar los literales de geometría por los tokens
   (incluidos los 56×26/72/2 que quedaron documentados en el fix EXP-2) **sin
   cambiar ningún valor efectivo** — refactor puro. Conservar la lógica de
   fuente VFO/`theme_shape.vision` de EXP-2 y el resto del comportamiento.
3. **No convertir a Flex/Grid todavía**: la conversión de layout es el
   incremento 2 (otra corrida, otro PR) para que este diff sea verificable
   pixel-por-pixel de cero cambio.

## Fuera de alcance (este PR)

- Flex/Grid en cualquier región; spectrum/waterfall/band_plan (áreas de dibujo
  fijo — el issue lo prohíbe explícitamente "do not blindly convert");
  conversiones en `main_screen.c`/`styles.c`; widgets reutilizables nuevos.
- No tocar `lvgl/`, frameworks, workflows.

## Criterios de aceptación (finish condition verificable)

```bash
# 1. CERO cambio visual — la promesa del refactor:
uv run x6200-ab compare --a <base> --b HEAD --experiment-id AB-UI05-T1
#   → Visual Regression: pixel-identical en las 16 fixtures (changed 0)
#   → Quality/Functional/Performance PASS; Hardware/Energy BLOCKED

# 2. Gates estándar:
tests/check_harness_determinism.sh      # PASS, hashes INALTERADOS vs base
cd tools/ab_testing && uv run x6200-ab contracts --revision HEAD   # PASS 160/0/0
uv run x6200-ab audit --revision HEAD --suite visual               # PASS
uv run pytest -m "not e2e" -q · run_native_tests.sh · ruff/ty
```

3. **Gate QEMU (post-merge, lo corro yo como orquestador)**: banco cortex-a7,
   ruta funcional de humo (arranque + Status + freq) sobre el binario del tip —
   un refactor sin cambio visual no exige batería de temas completa, pero SÍ
   arranque + comandos.

## Riesgo y reversibilidad

`src/info.c` + header nuevo; riesgo bajo-medio (compilación en 47 temas/estados
= mismos valores); revert = 1 commit. La garantía fuerte es el criterio 1: si
un solo pixel cambia, el refactor está mal y se descarta el intento (disciplina
1 cambio + 1 check).
