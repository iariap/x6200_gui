# Spec: ui04-tipografia — issue #9, tipografía y fuentes (primeras mejoras reales)

**Fecha**: 2026-10-14 · **Corrida**: `factory/ui04-tipografia` · **Repo app**: `iariap/x6200_gui`
**Base**: tip de `lu2dsx/x6200-rom` post-merge de PR #18 (auditoría UI-03 con backlog fundado)

## Qué se quiere y por qué (outcome)

PRIMER milestone de modernización visual: tipografía limpia y legible en la
pantalla 800×480, manteniendo LVGL 8 y conservando la funcionalidad. Se entra
por el backlog fundado de UI-03 (no por rediseño global): cada experimento
ataca **un solo defecto fundado con detector y fixture ya existentes**, con el
ciclo A/B completo y evidencia. Los contratos y fixtures de UI-02/UI-03 son el
"C" independiente — escribir el fix primero, no cambiar los contratos.

## Cadencia de experimentos (uno por cambio lógico, en este orden)

1. **EXP-1 · UI-011** (major): iconos de barra/reloj desaparecen en VISION —
   `fonts_scale()` copia structs de fuente sin rangos de icono. Experimento
   aceptable: propagar/mantener los rangos de glyph de iconos en las fuentes
   escaladas (o fallback de fuente con iconos), **sin** diseñar iconos nuevos.
   Detectors: `typography_missing_glyph` debe pasar en `main-tema-vision` con
   el fix; visual diff limitado a la región esperada (`reloj_potencia`/`barra`).
2. **EXP-2 · UI-012** (minor): `VFO-A` overflow en la celda 56×26 con la
   fuente escalada. Reparar `info.c` para seguir la escala (o clampar) con la
   regla de smart contract `typography_text_overflow` pasando. Solo esa celda.
3. **EXP-3 · UI-010** (critical): franja de frecuencia invisible en Outdoor
   (colores del tema). Fix de tema/estilos con los pares del JSON y contraste
   suficiente; el visual diff tocará `frecuencia` + waterfall. Detectors:
   `region_min_content:frecuencia` pasa; el waiver scoped de UI-010 se **retira
   en el mismo PR** (o en el subsecuente si hubo sleeps del gate).
4. **EXP-4 · UI-013** (major): truncamiento de `params.theme_name` — requerirá
   cambio en `src/params.c`/`dialog_settings.c` (encajar todo o invalidar
   valores largos en el menu). Cada experimento es un PR separado con su propio
   A/B; EXP-4 es el único que toca `src/` (risk high: ojo con el orden).

## Reglas de cada experimento (obligatorias)

- **A** = última revisión aceptada del trunk (`lu2dsx/x6200-rom` tip al
  momento); **B** = candidato; **C** = contracts/que los contra (sin mirar la UI).
- Antes de codear: hipótesis + resultado esperado + regiones esperadas del diff.
- A/B con comandos reales del CLI (`compare`, `benchmark --a --b`); veredicto
  por exit code (0/1/2/3/4) sin relajar budget ni contratos; excepcionesNunca
  nuevas para el propio fix.
- Screenshots antes/después + diff visual **embebidos en el PR** (`gh pr
  comment --attach <png>` — requieren PNGs reales de las corridas;
  alt-text `path#descripción`). Sin screenshots no hay evidencia visual.
- `run_native_tests.sh` + `check_*.py` + pytest del framework + determinismo.
- **Repro antes de fix**: el fixture/detector del defecto debe FALLAR en A
  (con el defecto) y PASAR en B; si un contrato ya pasa en A, el experimento
  apunta al defecto equivocado.
- One fix = one PR; un candidato fallido no acumula parches: se revierte y se
  re-hipotetiza.

## Fuera de alcance

- Rediseño del UI / cuidado layout; tokens/roles semánticos (issue #10); FTK
  runtime rasterizer sin benchmark que lo justifique (si se protes, experimento
  separado de solo medición, sin PR mergeable del src).
- Cambios RF/TX/CAT/DSP; touch físico (M7 hardware BLOCKED).

## Criterios de aceptación (finish condition verificable)

```bash
# Que el PR de cada experimento esté verde (CI 5 jobs) y mergeable,
# con un PR de: compare PASS (o REVIEW REQUIRED justificado) con diff visual
# confinado + contract del defecto en PASS en B + 0 regressions en fixtures
# previos (pixel-identical fuera de la región esperada si aplica)
tests/check_harness_determinism.sh          # PASS set completo (16+)
cd tools/ab_testing && uv run pytest -m "not e2e" -q
uv run x6200-ab compare --a <base> --b <head> --experiment-id AB-UI04-<exp>
```

Evidencia de cada experimento pegada en su PR (salidas reales + PNGs). Sin
X6200: hardware/energía quedan BLOCKED; CPU-locals nunca battery claim.
