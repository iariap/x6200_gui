# Spec: theme-cathode-70 — tema Cathode · CRT sepia/ámbar (THEMES v2)

**Fecha**: 2026-10-15 · **Corrida**: `factory/theme-cathode-70` · **Base**: trunk `122bea3`
**Issues**: #70 (el tema con su JSON de partida) + #66 (requisitos comunes) — leelos AMBOS
con `gh issue view 70/66 --repo iariap/x6200_gui` antes de diseñar.

## Nota de paralelización
Sunlit (#67), Redshift (#68) y Sprint (#69) están en vuelo en OTROS worktrees: todos tocarán
`src/theme.c` (fila del tema) y `docs/THEMES.md` (sección) AL FINAL. **Appendeá tus entradas
al FINAL** de las mismas estructuras (orden preferido de theses: Sunlit → Redshift →
Sprint → Cathode — el merge serializado rebasa cada uno sobre el anterior), y tomatoes
arreglar los conflictos triviales con fixer.

## Qué
Tema **Cathode**: CRT sepia/ámbar AUTHENTIC de instrumentación (no videojuego ni skeuomorphic
TV): dark brown-black glasslike backdrop, texto ámbar/crema luminiferous. Font **IBM Plex
Sans** (`ibmplexsans`) + **DSEG14** (`dseg14`) para frecuencia y reloj; cantos casi cuadrados
(1/2/3 px), sin gradients/glow de default, SIN scanlines ni trails (la identidad sale de
tipografía/color/jerarquía). El issue trae el JSON base con los hex exactos — completá la
cobertura del resto de roles conservando ese esquema de colores brown/espresso/copper.

## Checklist duro (del epic)
1. JSON real sólo con el esquema permitido; `font_digits`: `dseg14` con fallback/puntuación
   como los demás DSEG del día (ver door de SLOT 30/38 y el render real).
2. Cobertura COMPLETA de roles (`theme.h`): frecuencia, modo, selección, foco, botones/
   disabled, S-meter + `meter_over`, RX/TX, warning/error.
3. WCAG 0 fallas (el ámbar/crema sobre coffee-dark debe tener el ratio honesto).
4. **Waterfall térmico ámbar monocromático**: negro → espresso → burnt umber → copper →
   ámbar → parchment crema, con señáls visiblemente distintas entre niveles.
5. Bench vs Classic dentro del compare.
6. Capturas por estado con fixtures (dim-room/señal, evening, etc. — los fixtures actuales).
7. **Banco RESERVADO**: sin capturas ARM (se consolidan en #73).

## Fuera de alcance
Selector (#73), remoción de viejos (#74), `lvgl/` pinned, DSP/RF/TX, shaders/phosphor effects.

## Gates
```bash
cd /home/pablo/desa/asf/state/worktrees/x6200/theme-cathode-70
tests/check_harness_determinism.sh
cd tools/ab_testing
uv run x6200-ab compare --a 122bea3 --b HEAD --experiment-id AB-CATHODE70
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
1. JSON committado con las decisiones justificadas (con el hex de partida del issue).
2. Capturas por estado + WCAG/contraste con números.
3. Bench vs Classic.
4. Gates completos. 5. PENDIENTES honestos (captura ARM en #73).
