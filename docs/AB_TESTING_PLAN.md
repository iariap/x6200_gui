# Plan: arnés de testing UI con A/B para la ROM del X6200

Spec maestro de la infraestructura (rama `lu2dsx/ui-ab-harness`). Fuente: brief del owner del
2026-10-13. Todo lo que implemente tiene que poder verificarse con comandos reproducibles; los
milestones no se declaran cerrados hasta que sus propias pruebas lo demuestren.

## Principio: A ≠ calidad

Tres piezas conceptualmente distintas:

- **A — baseline**: versión actual o última aceptada, *con sus defectos incluidos*.
- **B — candidate**: versión con un cambio; debe demostrar que corrige lo previsto y no agrega
  regresiones.
- **C — quality contract**: reglas independientes de A (geometría, legibilidad, recortes,
  fuentes, contraste, navegación, rendimiento, presupuesto de recursos).

La evaluación compara **B contra A** (regresiones) **y B contra C** (calidad objetiva). Una diff
de píxeles no es un error por sí misma; imágenes idénticas tampoco implican UI correcta.

## Stack

- Harness nativo: C/C++, LVGL 8 (submódulo del repo, 8.3.11 stock, SHA
  `74d0a816a440eea53e030c4f1af842a94f7ce3d3`), CMake, libpng, componentes reales de la GUI,
  mocks explícitos de hardware/radio.
- Framework de evaluación: Python 3.11+, `uv`, pytest, ruff, Pillow, NumPy, PyYAML; tipado.
- CI: GitHub Actions. El framework Python vive **en el host**, nunca en la ROM.

## Arquitectura (separación estricta)

git commits → revision manager → builds aislados → LVGL harness A/B → scenario runner →
(screenshots, metadata, performance) → quality evaluator → (A/B diff, contracts, known issues) →
quality gate → PASS / FAIL / BLOCKED / INCONCLUSIVE → reportes + CI.

Construcción, simulación, render, inspección, captura, benchmark, comparación, evaluación y
reporte son fases separadas, no un script único.

## Estructura objetivo

```text
tools/ui_harness/   C harness (CMake, src/, mocks/, fixtures/, Dockerfile)
tools/ab_testing/   framework Python (uv: src/x6200_ab/, scenarios/, quality/, tests/)
docs/AB_TESTING.md, UI_KNOWN_ISSUES.md, UI_QUALITY_STANDARDS.md
.github/workflows/ab-testing.yml
```

## Milestones y gates

| M | Contenido | Gate |
|---|---|---|
| 1 | Build desde checkout limpio, framebuffer virtual 800×480, PNG, primera pantalla/componente real, tiempo virtual | dos corridas equivalentes → capturas reproducibles |
| 2 | Fixtures declarativos, tiempo controlado, inspección estructural, inventario de defectos existentes (`UI_KNOWN_ISSUES.md`) | se documenta al menos un defecto real reproducible; los checks sintéticos detectan fallos deliberados |
| 3 | A/B entre commits (worktrees aislados), pixel diff, quality contracts, known issues, reportes | distingue corrección / regresión / cambio esperado |
| 4 | Benchmark de render (p50/p95/p99, FPS, flushes, área redibujada, RSS/PSS), repeticiones y warm-up | detecta una regresión de rendimiento controlada |
| 5 | Evaluador central PASS/FAIL/BLOCKED/INCONCLUSIVE (+REVIEW REQUIRED), presupuestos, CLI completa, reportes MD/HTML | las autopruebas verifican todos los estados |
| 6 | Workflow `ab-testing.yml` (corridas manuales), artifacts, checks separados, resumen de la corrida | una corrida manual o la matriz local del harness con una regresión deliberada falla con evidencia; no hay gate automático en PRs |
| 7 | Captura física del X6200, importación de mediciones energéticas, comparación en ARM | lo simulado se distingue claramente de lo medido |

## Reglas fijas

1. Cobertura por componente: `REAL` / `PARTIAL` / `SYNTHETIC` / `UNSUPPORTED`, declarada en la
   metadata de cada escenario. No declarar `REAL` cuando es `PARTIAL`.
2. Los mocks tienen comportamiento explícito y registran llamadas; nada de stubs que devuelven 0
   sin validar el efecto sobre la UI. SQLite temporal con el esquema real (`sql/`) para cfg.
3. Determinismo: tiempo virtual controlado (nada de sleeps para el estado visual); reloj
   monotónico separado, sólo para medir duraciones reales.
4. Known issues con IDs estables (`UI-001`, …), severidad, reproducción y evidencia; un defecto
   corregido pasa a test permanente; no se reintroduce; exclusiones con fecha y justificación,
   nunca exclusiones globales.
5. Presupuestos versionados (`budgets.yaml`) fijados **antes** de ejecutar la comparación.
6. El benchmark x86 detecta regresiones relativas; no demuestra rendimiento del Cortex-A7 ni
   autonomía de batería. CPU% nunca se convierte en watts: la evidencia energética es física.
7. Hardware gate: sin medición física, Energy/Hardware quedan BLOCKED, nunca PASS por default.
8. Sin TX físico; los scripts de captura por red no repiten el patrón de credenciales
   hardcodeadas ni SSH sin verificación.
9. No actualizar LVGL a 9, no introducir motores gráficos nuevos, no modificar `main`.
10. Prioridad del repo (AGENTS.md): batería > performance > UX; todo trade-off queda
    documentado.

## Estado de avance

Ver docs/AB_TESTING.md (se va escribiendo por milestone). Sin acceso al X6200 físico, todo lo
verificable en host se completa y las validaciones de hardware quedan explícitamente PENDIENTE.
