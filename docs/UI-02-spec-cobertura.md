# Spec: ui02-cobertura-real — issue #7, extender la cobertura real de A/B

**Fecha**: 2026-10-14 · **Corrida**: `factory/ui02-cobertura-real` · **Repo app**: `iariap/x6200_gui`
**Baseline validado**: `d08258304ef979320f6f8f5f5ee53e0c9d3e41e2` (tip de `lu2dsx/x6200-rom`; CI 5/5, run 38510204457)

## Qué se quiere y por qué (outcome)

**Extender la cobertura del arnés ya existente** (PR #1 mergeado:
`tools/ui_harness/`, `tools/ab_testing/`) con fixtures nuevos **de GUI real**
para desbloquear los issues UI-03..UI-07: sin Settings, diálogos, temas
completos y estados de foco, la modernización no tiene escenarios reproducibles
dónde verificarse. La cobertura mal clasificada (PARTIAL/SYNTHETIC vendida como
REAL) invalidaría todo el pipeline A/B posterior. Entrega la **expansión mínima
útil**; no es un reharmado del arnés ni una reescritura de reportes.

## Alcance (mínimo útil, orden por valor)

1. **Un diálogo real del firmware**: agregar fixture(s) del camino de diálogo
   disponible en el arnés (los mocks `mock_main_screen.c` ya exponen los
   hooks de diálogos/scheduler). Preferir el diálogo de mayor uso para los
   issues siguientes (Settings/tipografía). Clasificación honesta por
   componente en el JSON.
2. **Matriz de temas completa**: hoy existe `main-otro-tema` (Outdoor).
   Terminar la matriz de los temas envueltos con fixture
   `main-tema-<nombre>.scn` reutilizando la misma pantalla principal (REAL):
   prioridad RS y Outdoor-Negative para la auditoría de contraste del issue
   #8; los demás si el costo de fixtures lo permite.
3. **Estado de foco/encoder**: fixture de foco simulado sobre la pantalla
   principal real (`lv_group_focus_next()` con el grupo ya registrado por el
   main_screen real) para probar la cadena de foco visible sin hardware.
4. **Determinismo**: gate `tests/check_harness_determinism.sh` extendido a los
   fixtures nuevos (dos corridas, sha256 idénticos).
5. **Contracts opcionales por fixture**: solo si generan señal real (p. ej.
   `region_min_content` en el diálogo); si un fixture no agrega contratos, se
   declara y nada se infla.
6. **Known issues UI-003/004/005/007**: reproducir cada una **solo si** el
   fixture correspondiente la cubre; lo no reproducible queda UNVERIFIED con
   evidencia explícita (regla: no se funda lo que no se reproduce).
7. **Matriz de cobertura** documentada en `docs/AB_TESTING.md`
   (pantalla/tema/fuente/estado × REAL/PARTIAL/SYNTHETIC/UNSUPPORTED).

## Fuera de alcance

- No tocar `src/` del firmware (única excepción: línea de scaffolding si un
  diálogo lo exige — reportar antes, preferir NO).
- No tocar `lvgl/` (pinned v8.3.11) ni habilitar FreeType runtime.
- No reescribir CLI/reportes del framework; los fixtures nuevos se integran al
  pipeline existente (el workflow `ab-testing.yml` ya cubre los paths).

## Criterios de aceptación (finish condition verificable)

```bash
# 1. Determinismo sobre el set completo (8 preexistentes + los nuevos)
tests/check_harness_determinism.sh                       # exit 0

# 2. Cada fixture nuevo produce PNG 800x480 + JSON con cobertura honesta
tools/ui_harness/build/ui_harness --scenario <nuevo>.scn --png x.png --json x.json

# 3. Suite completa del framework (sin e2e)
cd tools/ab_testing && uv run pytest -m "not e2e" -q     # 0 fallos

# 4. Prueba de detección: inyección deliberada de una regresión en UN fixchart
#    nuevo, detector la marca (FAIL), revertida antes del commit final.

# 5. Matriz de cobertura documentada y honesta en docs/AB_TESTING.md.
```

Sin X6200 físico: Hardware y Energy quedan **BLOCKED**, nunca PASS.
