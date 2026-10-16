# Spec: bench-ci-tolerance — el bench de CI deja el ruido del runner en INCONCLUSIVE, no FAIL

**Fecha**: 2026-10-15 (noche) · Corrida: `factory/bench-ci-tolerance` · Base: trunk `e8376bd`
**Evidencia**: los 4 payloads del job `UI Performance` de hoy (runs 38810720725, 38822204534,
38830907010, 38832600451; asf#41 reincidencia ×5) — el escenario FAILIDO **cambia al azar**
entre re-runs (apps-menu-6 → main-tema-cathode-tx → apps-menu-2 → main-tema-aurora →
main-tema-sunlit-eco), SIEMPRE con `flushes_delta_pct == 0.0` y `redrawn_area_delta_pct ==
0.0` y p95 +35~70%; en host todas esas ramas midieron PASS con cv < 2%.

## Causa
El job de CI corre 71 escenarios × A y B **secuenciales**: si el runner shared se degrada
a mitad de la corrida, medio lado sale inflado y el p95 de UN escenario sobre el budget.
El proxy determinista (0.0 en todo) prueba que el código no cambia el render — es ruido del
runner, no regresión. Los re-runs punteros no resuelven: 3 seguidos fallaron con escenarios
distintos.

## El arreglo (el gate sigue aprobando/fallando por lo REAL)
1. **Distinguir la firma del ruido en el reporte del bench** (en el CLI del arnés,
   `tools/ab_testing/src/x6200_ab/` — el módulo que calcula el gate del compare/benchmark):
   un escenario cuyo único motivo de FAIL sea el p95 con los DOS proxies deterministas en
   0.0 exacto se clasifica **`INCONCLUSIVE` (host noise)** y NO cuenta como FAIL del
   experimento: el experimento en CI sale `INCONCLUSIVE` (exit especial o la salida del
   status; NO 0 y NO 1 → usar exit 3 como hoy hacen los INCONCLUSIVE si existe el
   convención — mirar el código).
   - La condición es estricta: si CUALQUIER otro delta (flushes/área) !=0, o el p50 también
     explota + los proxies mueven, sigue FAIL (una regresión REAL mueve el proxy o el p50
     sostenido).
2. **Y reforzar la robustez del bench de CI**: en el trabajo de CI (workflow
   `ab-testing.yml`, job "UI Performance"): repetir el PAR (A y B) en 2 rondas y usar el
   MEJOR de las dos (el p95 más bajo por escenario, para ambos lados con la misma política —
   determinista y ridículo de implementar: `min(p95_A, p95_A_r2)`), con el total de frames
   documentado. Si las 2 rondas FALLAN con la firma, ahí sí FAIL (3 strikes = código real).
   Si esto es mucho (timing), el mínimo viable es (1) solo: INCONCLUSIVE de la firma.
3. **Tests TDD** del clasificador (suite de ab_testing): fixture con los payloads reales de
   HOY (los json del run): escenario con proxy 0.0 + p95 +35% → INCONCLUSIVE; escenario con
   cualquier delta de proxies → FAIL. Los escenarios existentes (un PASS real y un FAIL de
   proxies reales) siguen su clase.
4. **Documentación**: en `docs/AB_TESTING.md` la clase nueva (el gate del CI con el manejo
   del ruido del runner) + el header del job de CI.
5. El workflow NO re-intenta (it hat del GH ya lo cubrió con el re-run manual 3×): el exit
   code del job si INCONCLUSIVE: **neutro** (exit 0 con el estado en el JSON/summary? NO:
   el CI gate debe REFLEJAR la intelligibility: dejar el exit del proceso correcto (las convenciones de
   INCONCLUSIVE del CLI; mirar cómo sale benchmark/tests cuando INCONCLUSIVE hoy) — la meta
   es que el job NO pinne FAIL por ruido, pero tampoco TAPR un FAIL real.

## Gates
```bash
cd /home/pablo/desa/asf/state/worktrees/x6200/bench-ci-tolerance
cd tools/ab_testing
uv run pytest -m "not e2e" -q     # incluye los tests nuevos del clasificador
uv run pytest -m e2e -q
uv run ruff check . && uv run ruff format --check . && uv run ty check src tests
uv run x6200-ab compare --a origin/lu2dsx/x6200-rom --b HEAD --experiment-id AB-BENCHCI
uv run x6200-ab contracts --revision HEAD && uv run x6200-ab audit --revision HEAD --suite visual
uv run x6200-ab themes
uv run pytest -m "not e2e" -q && uv run pytest -m e2e -q
cd ../.. && tests/check_harness_determinism.sh && tests/native/run_native_tests.sh && estáticos 7/7
```

## Invariantes
- El trabajo real (proxy determinista) sigue siendo la barra: una regresión de flushes/área
  sigue FAILANDO.
- Solo el modo CI (o la firma) aplica la tolerancia: el benchmark de host (que el coder corre
  en la corrida) NO cambia: los FALSEIGHT que ya PASS/INCONCLUSIVE por cv se mantienen.
- no `.asf-active`; NO tocar temas/fixtures; commit `Pablo Iaria <iariap@gmail.com>`; sin push/PR.

## Reporte final
1. El clasificador (diff + la conveyor de test con los payloads de hoy).
2. La robustez de 2 rondas si entra (el diff del workflow).
3. Gates completos. PENDIENTES honestos (el job de CI real lleva la verificación live — la
   siguiente corrida confirma).
