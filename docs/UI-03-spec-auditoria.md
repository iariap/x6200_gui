# Spec: ui03-audit-defectos — issue #8, auditoría y triage de defectos heredados

**Fecha**: 2026-10-14 · **Corrida**: `factory/ui03-audit-defectos` · **Repo app**: `iariap/x6200_gui`
**Base**: tip mergeado de `lu2dsx/x6200-rom` INCLUYENDO el merge de PR #17 (los fixtures nuevos son parte del backdrop de auditoría)
**Baseline A/B**: `d0825830` (registrado en UI-01) — a partir del merge de #17 el tip avanza: actualizar SHA en el reporte.

## Qué se quiere y por qué (outcome)

Convertir los hallazgos superficiales en un **backlog de defectos fundados** (issue
#8): correr `x6200-ab audit --revision <tip> --suite visual` contra el firmware
aceptado, inspeccionar capturas + árbol, y triagear cada hallazgo a
reproducido / unverified / no-fundado **con detector y fixture** para lo
confirmado. Este backlog es el input de priorización de UI-04..UI-07.

## Alcance (en orden)

1. **Auditoría estructural completa** sobre el tip: `audit --revision --suite visual`,
   evaluando también los fixtures nuevos de #17 (About, temas, foco).
2. **Fundación de los 2 hallazgos surfaced de UI-02** (si los fixtures/detectores
   lo permiten): (a) truncamiento de `params.theme_name` char[16] → tema
   inválido silencioso; (b) tipografía VISION (glifo de reloj ausente, VFO-A
   desbordado). Detéctor+fixture o calificación honesta de por qué no.
3. **Triage de UI-007 (riel del switch en Ajustes)**: el detector actual no cubre
   ese par sin fixture de Ajustes — evaluar cheque de contraste sobre los JSON
   de tema directamente (rootfs themes, parse + verificación de pares texto/fondo)
   como detector estático complementario; si viable y fundado, cargarlo.
4. **Backlog priorizado** en `docs/UI_KNOWN_ISSUES.md` (sin duplicar IDs;
   cada defecto confirmado con fixture + detector + captura A; unverified
   explícito). El registro versionado (`known_issues.yaml`) se mantiene.
5. **Contraste sobre temas JSON de rootfs** como chequeo estático nuevo
   (complemento al structural); con tests del framework.

## Fuera de alcance

- No fixear defectos acá (los fixes son PRs de UI-04+); solo fundar y triagear.
- No tocar `src/` salvo el scaffolding mínimo imprescindible para un detector
  NUEVO (reportar antes en el PR, y en tal caso riesgo sube).
- No LVGL9, no FreeType runtime, no supresiones.

## Criterios de aceptación (finish condition verificable)

```bash
cd tools/ab_testing
uv run x6200-ab audit --revision <tip> --suite visual   # verdicto real (PASS sólo si no hay hallazgos no-waiveados)
uv run pytest -m "not e2e" -q                           # 0 fallos
tests/check_harness_determinism.sh                      # PASS (set completo)
# + backlog actualizado: cada entrada nueva con fixture, detector y captura A
```

Sin X6200: Hardware/Energy BLOCKED; no se redacta backlog de bugs de hardware.
