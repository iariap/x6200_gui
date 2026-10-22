# Spec: theme-aurora-72 — tema Aurora · uso diario premium (THEMES v2)

**Fecha**: 2026-10-15 · **Corrida**: `factory/theme-aurora-72` (a crear)
**Issues**: #72 (JSON de partida) + #66 (requisitos comunes) — leelos con gh antes de diseñar.

## Qué
Tema **Aurora**: berenjena profundo + lavanda + coral (notas turquesas), radii grandes
(12/16/14) y gradientes sutiles, font **Inter**. Premium de uso diario; distinto de Material 3
azul/gris y del angular Sprint. NOTA: #74 fija **default Aurora** tras la migración.

## Checklist duro (mismo estándar del epic #66)
1. JSON real — la partida del issue #72 usa `base: "IU1WXY"` (tema que se REMUEVE en #74:
   la base config de TODOS los temas v2 se re-checkeará en #74; dejalo documentado en la
   ficha si algo pinta).
2. Cobertura completa de roles (frecuencia, modo, selección, foco, botones/disabled,
   S-meter + meter_over, RX/TX, warning/error) — lavanda/coral sin sacrificar jerarquía.
3. WCAG 0 fallas (checker) · 4. Bench vs Classic en el compare.
5. **Appendeá la fila del tema en theme.c y la sección de THEMES.md AL FINAL** (patrón de
   los otros temas en vuelo; merge serializado rebasa el resto).
6. **NO usar el banco** (capturas ARM consolidadas en #73).

## Gates
```bash
cd state/worktrees/x6200/theme-aurora-72
tests/check_harness_determinism.sh
cd tools/ab_testing
uv run x6200-ab compare --a <base> --b HEAD --experiment-id AB-AURORA72
uv run x6200-ab contracts --revision HEAD && uv run x6200-ab audit --revision HEAD --suite visual
uv run x6200-ab themes
uv run pytest -m "not e2e" -q && uv run pytest -m e2e -q
tests/native/run_native_tests.sh / estáticos 7/7
```

## Invariantes
- Commits `Pablo Iaria <iariap@gmail.com>`, sin push/PR (el orquestador publica).
- NO tocar selector (#73)/remoción (#74)/temas de otras corridas/lvgl/DSP.

## Reporte final
JSON + decisiones; capturas por estado + WCAG; bench; gates; PENDIENTES honestos.
