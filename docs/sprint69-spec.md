# Spec: theme-sprint-69 — tema Sprint · concursos (THEMES v2)

**Fecha**: 2026-10-15 · **Corrida**: `factory/theme-sprint-69` · **Base**: trunk `122bea3`
**Issues**: #69 (el tema con su JSON de partida) + #66 (requisitos comunes) — leelos AMBOS
con `gh issue view 69/66 --repo iariap/x6200_gui` antes de diseñar.

## Nota de paralelización
Sunlit (#67) y Redshift (#68) están en vuelo en OTROS worktrees: todos tocarán `src/theme.c`
(fila del tema) y `docs/THEMES.md` (sección) AL FINAL. **Appendeá tus entradas al FINAL** de
las mismas estructuras (orden de append preferido: Sunlit → Redshift → Sprint), con el merge
serializado y el fixer de rebase resolviendo conflictos triviales.

## Qué
Tema **Sprint**: navy profundo + focos/botones ámbar vibrante + lecturas cian hielo;
**Roboto Condensed** (`robotocondensed`) y **DSEG14** (`dseg14`) para la frecuencia; cantos
angulares, jerarquización crítica rápida. El issue trae el JSON base — refinalo con la
cobertura de roles completa sin romper la identidad (marino/ámbar/cian hielo).

## Checklist duro (del epic)
1. JSON sólo con el esquema real; `font_digits`: `dseg14` (verificá que el slot del panel de
   frecuencia use font_digits — mirá los candidatos `FONT_SEGMENTED_SLOTS 30,38` y el
   render real DSEG de frecuencia).
2. Cobertura COMPLETA de roles (`theme.h`): frecuencia, modo, selección, foco, botones/
   disabled, S-meter + `meter_over`, RX/TX, warning/error.
3. WCAG 0 fallas (el cian hielo sobre marino y el ámbar sobre marino deben tener 3:1/4.5:1).
4. Bench vs Classic dentro del compare.
5. Capturas por estado con las fixtures del arnés (freq/mode/foco legibles de un vistazo).
6. **Banco RESERVADO**: sin capturas ARM (se consolidan en #73).

## Fuera de alcance
Selector (#73), remoción viejos (#74), `lvgl/` pinned, DSP/RF/TX.

## Gates
```bash
cd /home/pablo/desa/asf/state/worktrees/x6200/theme-sprint-69
tests/check_harness_determinism.sh
cd tools/ab_testing
uv run x6200-ab compare --a 122bea3 --b HEAD --experiment-id AB-SPRINT69
uv run x6200-ab contracts --revision HEAD && uv run x6200-ab audit --revision HEAD --suite visual
uv run x6200-ab themes
uv run pytest -m "not e2e" -q && uv run pytest -m e2e -q
tests/native/run_native_tests.sh
estáticos 7/7
```

## Invariantes
- Appenda al FINAL, commits `Pablo Iaria <iariap@gmail.com>`, **sin push/PR**.
- NO tocar el selector/default, NO remover viejos, NO tocar temas de otras corridas.

## Reporte final
1. JSON committado con las decisiones justificadas.
2. Capturas por estado + WCAG/contraste con números.
3. Bench vs Classic.
4. Gates completos. 5. PENDIENTES honestos (captura ARM en #73).
