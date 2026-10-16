# Spec: roe-curve-smooth — el chart de ROE dibuja la curva suavizada (issue #99, feedback owner)

**Fecha**: 2026-10-15 · Corrida: `factory/roe-curve-smooth` · Base: trunk actual
**Issue**: #99 — el chart de ROE se ve "sierra" (zigzag de muestras) y no es representativo;
el owner pide curva suavizada (Bezier o similar).

## Diseño
1. **Encontrar el render del chart de ROE**: `grep` en `src/dialog_swrscan.c` (o donde dibuja
   el chart — puede ser un LVGL chart con `ser盛` de puntos o una polyline/line). Medir el
   estado actual (captura banco o el arnés si el dialogo REAL se renderiza — recordar que el
   `dialog_swrscan` está mockeado en el harness UI (mock_main_screen.c), así que la validación
   visual es del **banco** (o test nativo del resampler si el chart no se ve a no el banco).
2. **El suavizado determinista**: resampler Catmull-Rom (o bezier cubico equivalente) que
   toma N muestras y renderiza la curva con paso fijo (resample 4×/8× o por pixel del chart);
   **clamp identidad del valor (SWR number) al rangos del chart** (1..20 o el que use; sin
   overshoot encima del máximo/donbajo).
3. **SALVAGUARDA de producto**: los **minimos locales se preservan como ANCLA de la curva**
   (son las resonancias — donde toca meter una antena). Los picos de ruido son lo que se
   suave. Implementar como filtro (muralla de la curva en los mínimos; interpolación sólo
   entro stationary) y TESTS del comportamiento (nativo).
4. Toggle de render (si es trivial añadir al UI: no. Simplest: siempre suavizada).
5. Test nativo del resampler (direct unit test, sin visual): puntos de endpoints garantizados
   (la primera y última muestra quedan), mínimos conservados (tolerancia), suavizado idempotente.
   Si el renderer es engine-specific (`lv_line`/`lv_chart`): la unidad testea la Fuente de
   puntos (entrada N muestras → salida M puntos para el chart).

## Gates
```bash
cd /home/pablo/desa/asf/state/worktrees/x6200/roe-curve-smooth
tests/check_harness_determinism.sh
cd tools/ab_testing
uv run x6200-ab compare --a origin/lu2dsx/x6200-rom --b HEAD --experiment-id AB-ROECURVE
uv run x6200-ab contracts --revision HEAD && uv run x6200-ab audit --revision HEAD --suite visual
uv run x6200-ab themes
uv run pytest -m "not e2e" -q && uv run pytest -m e2e -q
tests/native/run_native_tests.sh
estáticos 7/7
+ banco: captura del dialogo SWRScan (md5 del binario) — el chart suavizado visible.
```

## Invariantes
- NO tocar RF/DSP (el scan/la medida sigue exacto: el smoothing es SOLO render).
- No commitear `.asf-active`; NO tocar runs de otros (#74/#94/#92/#95).
- Commits `Pablo Iaria <iariap@gmail.com>` — **NO push/PR**.

## Reporte final
1. El render actual (quien pinta el chart) y el cambio (diff).
2. El resampler (números: entrada/salida de puntos, sink de anclas de mínimos).
3. Tests TDD (RED/GREEN).
4. Captura banco before/after (md5) o la falta declarada.
5. Gates completos. PENDIENTES honestos (la verificación en radio con bandas reales queda
   para el QA/owner, como siempre).