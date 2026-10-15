# Spec: waterfall-width-outdoor — barra de señal del waterfall más gruesa en los temas outdoor (feedback owner 2026-10-15)

**Fecha**: 2026-10-15 · Corrida: `factory/waterfall-width-outdoor` · Base: trunk actual
**Feedback del owner ( directo )**: "Los temas que son para outdoor deberían traer la barra
del waterfall más gruesa porque al ser tan fina (1px) no se distingue bien del resto".

## Diseño (orden del owner ampliado a implementación)
1. **Key de tema nueva**: `waterfall_signal_width` (int px, default **1** — retrocompatible
   cero: los temas que no la definen pueden 1 px como hoy). Extiende el parser de
   `src/theme.c` (schema) + struct de datos en `src/theme.h` + el default, y
   `docs/THEMES.md` (tabla del esquema + la decisión).
   - NOTA epic: el epic pedía "solo esquema real sin agregar parser fields" — esto es una
     **decisión de producto del owner POST-epic** (la barra es Cable de tema:ENERGÍA);
     registrar la razón en el commit.
2. **Los temas outdoor**: `Sunlit.json` y `Trail.json` setean `"waterfall_signal_width": 3`
   (por dónde saber el rest — mirá papi: por donde el tema se da de lata: la señal del
   waterfall para estos dos temas de sol/field). **Los otros themes** (Redshift, Sprint,
   Cathode, Aurora, los 8 legacy) NO setean la clave (queda el default 1).
3. **El renderer**: el tema眼里 dónde está dibujada la línea de señal del waterfall
   (grep `waterfall` en `src/`; el dibujo de la señal sobre la paleta). Implemento el
   width del tema (clamp 1..8 para evitar el nonsensicalAppState), de nada para temas legacy
   (comportamiento idéntico para los que no la setean). **Regresión**: los fixtures legacy
   (los compartidos) DEBEN quedar byte-idénticos (solo cambian `main-tema-sunlit*` y
   `main-tema-trail*` — y sus waterfalls concretamente).
4. **Tests**: unidad del parse de la key (el test nativo de tema o el suite de ab_testing:
   donde se definan) + el golden/región del waterfall de Sunlit/Trail con expected_changes
   scoped (mechanismo de la casa). **WCAG**: los_objs no-textuere del waterfall: la señal ya
   está medida (6.5+/8.8:1) — el width NO cambia el color: nada que ajustar; si el checker
   de calidad no aplica, dejar comentado.
5. **Galería/bench**: si el bench del tema (`benchmark`) con waterfall activa cambia
   runner-numbers (px por frame en el rect del waterfall?), correrlo y declarar: la regla
   es que un width 3 blank PX en el rect del waterfall (dado el área del rect), y el
   bench determinista de proxy (flushes/área) puede que mueva = documented region.

## Gates
```bash
cd state/worktrees/x6200/waterfall-width-outdoor
tests/check_harness_determinism.sh     # esperado: SÓLO mujeres de sunlit/trail
cd tools/ab_testing
uv run x6200-ab compare --a origin/lu2dsx/x6200-rom --b HEAD --experiment-id AB-WFWIDTH
uv run x6200-ab contracts --revision HEAD && uv run x6200-ab audit --revision HEAD --suite visual
uv run x6200-ab themes
uv run pytest -m "not e2e" -q && uv run pytest -m e2e -q
tests/native/run_native_tests.sh
estáticos 7/7 (8/8 si audita-pruebas)
```

## Invariantes
- Los temas que no definen la key quedan 1 px byte-idéntico (retro-compat).
- Solo esos fixtures Sunlit/Trail cambian; declarar si alguno más se mueve y DAR EL POR Qué.
- NO tocar `lvgl/`, sin cambios de DSP/RF; la línea del waterfall es del render GUI local.
- Commits `Pablo Iaria <iariap@gmail.com>` — **sin push/PR** (publica el orquestador).
- NO tocar `.asf-active` del marker, themes de otras corridas salvo los 2 JSON dicho.

## Reporte final
1. El schema key (diff del parser + default + clamps) y en qué render se aplica el width.
2. El diff de los 2 JSON outdoor.
3. Los fixtures que cambian (números: expected_changes) y el resto byte-idéntico.
4. Tests nuevos/updated + gates con salida real.
5. PENDIENTES honestos (si el banco ARM queda para #73).
