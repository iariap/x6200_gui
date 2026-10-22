# Spec: codec-statics-47 — los checkers estáticos de codecs en host sano (issue #47)

**Fecha**: 2026-10-15 · **Corrida**: `factory/codec-statics-47` · **Base**: trunk `6cbdbe5`
**Issue**: #47 (`tests: los checkers estáticos de codecs (wspr/sstv/wefax/navtex) fallan en host sano`).

## Contexto heredado
- Reconocido en el PR #43: los checkers estáticos de IMAGE codecs del `run_all_tests.sh`
  (wspr/sstv/wefax/navtex) fallan en host sano; la causa provisional era "imagen/ruta
  hardcodeadas" (el checker busca una imagen en una ruta que no existe en todos los
  checkouts). Repro exacto pendiente, y esa carencia es el defecto de hiermeticidad.
- Los `run_all_tests.sh` contadores se arreglaron en #43 (los `fallos` inexistente);
  el trabajo quedó abierto para este run.

## Tarea
1. **Repro limpio**: clonar/checkout fresco (o simular: sin estado local), correr
   `tests/run_all_tests.sh` y capturar el fallo exacto (línea/ruta del decoder que pide
   la imagen). Contraejemplo real primero (el método de la Casa).
2. **Fix heroin**: rutas relativas importadas del repo (`tools/...`/`fixtures/`...),
   fixtures propios commiteados, y un detalle de salida legible: si el fixture no está,
   el checker reporta `SKIP` con razón (no FAIL) tan sólo si la pieza es realmente
   opcional; si NO es opcional, el fixture se agrega al repo con el fallo fundado.
3. Los 4 codecs (wspr/sstv/wefax/navtex) en verde en host: `run_all_tests.sh` exit 0 en
   un **clone fresco** (verificá con un clone temporario en /tmp con el repo limpio —
   o con la ecología de la CI, que ya corre la matrices sin estos fixtures).
4. Los contadores finales del `run_all_tests.sh`: suman los 5 suites y no cuenta doble.

## Gates (salida real)
```bash
tests/run_all_tests.sh                  # exit 0, conteo correcto en el CLONE FRESCO
tests/check_harness_determinism.sh       # sin cambio: PASS 33 x2
cd tools/ab_testing
uv run x6200-ab compare --a 6cbdbe5 --b HEAD --experiment-id AB-CODECS47  # PASS pixel-idéntico
uv run x6200-ab contracts --revision HEAD && uv run x6200-ab audit --revision HEAD --suite visual
uv run pytest -m "not e2e" -q && uv run pytest -m e2e -q
tests/native/run_native_tests.sh
estáticos 6/6
```
(El arnés/contracts corren si tocás algo que el arnés lee; si el cambio es sólo `tests/`,
igual corré los gates — sin corregir por honestidad.)

## Fuera de alcance
Los decoders en C (wspr/sstv/wefax/navtex) y toda la TX.

## Invariantes
- Commits `Pablo Iaria <iariap@gmail.com>`. **Sin push/PR** (publica el orquestador).
- PENDIENTE fundado > asumido: si una imagen es imposible en host, se declara la
  condición y el skip con razón — no se disfraza de FAIL.

## Reporte final
1. El repro en clone limpio (rutas/exactas del fallo).
2. El fix por checker (4 piezas o el patrón común).
3. `run_all_tests.sh` exit 0 en clone fresco + conteo de suites.
4. Gates con salidas reales.
5. PENDIENTES honestos.

