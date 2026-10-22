# Spec: ui04-exp5-mode-cell — issue #28 (UI-014): celda del modo se pega a AUTO con VISION

**Fecha**: 2026-10-14 · **Corrida**: `factory/ui04-exp5-mode-cell` · **Repo app**: `iariap/x6200_gui`
**Base**: tip post-merge del incremento 4 (PR #31) · **Issue**: #28 (evidencia radio `e4c329c` + banco en el comment 6171480639)

## Outcome
UI-013 quedó en el registro como "verificado"; **#28 está fundado visualmente (radio + banco) pero NO registrado con fixture/detector en el harness**. Esta corrida cierra las dos mitades: (1) registrar UI-014 con fixture + detector, (2) arreglarlo con la técnica ya probada del EXP-2 (fuente de tamaño normal solo para esa celda bajo VISION).

## Estado del defecto (medido en banco, tema VISION)
`CW-R` (4 chars, ~66px con la fuente escalada) en la celda de 56px del modo: desborda y se pega a `AUTO` → `CW-RAUTO`. `CW` (3) no pasa; IU1WXY/otros temas no pasan. Hermano exacto de UI-012 (allá `VFO-A`; acá el modo), mismo `info.c`, mis 6 celdas.

## Pasos
1. **Extender el escenario con campo `mode`** (o el mecanismo mínimo equivalente que ya exista en `scenario.{h,c}`/`screens.c` — investigar primero; hoy `home-usb-20m` fija USB). Es la pieza que falta para fundar #28: fixture `main-tema-vision-cwr.scn` (VISION + CW-R).
2. **Repro**: `audit`/`contracts` sobre el fixture con `typography_text_overflow` para la celda del modo (path según inventario post-incr.4: barra bajo `0/36/…`) — debe FAIL en la base.
3. **Fix**: `info.c`, celda del modo → fuente de tamaño normal bajo `theme_shape.vision` (patrón literal del EXP-2; solo esa celda).
4. **Registro**: UI-014 nuevo en `known_issues.yaml` + `docs/UI_KNOWN_ISSUES.md` (`fixed` post-fix, con evidencia A/B; sin excepción si el fix entra en el mismo PR — patrón EXP-1..4: repro pre-fix documentado, retiro de excepciones donde aplique).
5. **A/B completo**: compare --a <base> --b HEAD --experiment-id AB-UI04-EXP5; determinismo; contracts; audit; benchmark; native; pytest (unit+e2e); sensibilidad del fixture nuevo (cambiar el modo del escenario mueve la celda).
6. **Nuevos fixtures al set**: check_harness_determinism.sh los cubre por glob (verificar como en UI-02).

## Riesgos/conteciones
- Extender el parser de escenarios toca `tools/ui_harness/src/scenario.{h,c}` (medium, arnés host — no ROM).
- `mode_names[]` del firmware es la lista canónica (CW-R incluido); reutilizar, no inventar strings.
- No tocar: la fila de frecuencia (flex, incr.4), `main_screen.c`, `ui_tokens.h`, markup EXP-3, `lvgl/`.
- El fix visual debe quedar confinado a la celda del modo de `main-tema-vision-cwr` (y VISION fixtures existentes: verificar que USB no cambia — solo celdas >56px se ven afectadas).

## Finish condition
`audit --revision HEAD` PASS con el fixture nuevo; `typography_text_overflow` pasa en CW-R/VISION sin waiver; resto del set byte-idéntico (o diffs confinados justificados); PR con screenshots A/B del banco del defecto ya commiteados + del fix.
