# Spec: theme-trail-71 — tema Trail · campo/expedición (THEMES v2)

**Fecha**: 2026-10-15 · **Corrida**: `factory/theme-trail-71` (a crear)
**Issues**: #71 (JSON de partida) + #66 (requisitos comunes) — leelos con gh antes de diseñar.

## Qué
Tema **Trail**: verde bosque, oliva, arena y ámbar tierra; superficies PLANAS, bordes suaves
moderados (radius ~5/5/6, border 1) y **IBM Plex Sans**. Distinto de Sunlit (muy luminoso) y
Cathode (neón). Uso prolongado: contraste claro, pocas decoraciones.

## Checklist duro (mismo estándar del epic #66)
1. JSON real (schema de src/theme.c/THEMES.md) — partida del issue #71, refinar roles restantes
   sin romper la identidad (verdes/oliva/arena/ámbar tierra; nada de neón ni berenjena).
2. Cobertura completa de roles (frecuencia, modo, selección, foco, botones/disabled, S-meter
   + meter_over, RX/TX, warning/error).
3. WCAG 0 fallas (checker) · 4. Bench vs Classic en el compare.
5. **Appendeá la fila del tema en theme.c y la sección de THEMES.md AL FINAL** (los otros
   temas en vuelo usan el mismo patrón — el merge serializado rebasa).
6. **NO usar el banco** (capturas ARM consolidadas en #73).

## Gates
```bash
cd state/worktrees/x6200/theme-trail-71
tests/check_harness_determinism.sh
cd tools/ab_testing
uv run x6200-ab compare --a <base> --b HEAD --experiment-id AB-TRAIL71
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
