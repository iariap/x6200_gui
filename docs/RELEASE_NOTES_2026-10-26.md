# X6200 LU2DSX ROM — v0.1.4-lu2dsx (pre-release)

**Fecha**: 26/10/2026 · **GUI**: main `f425fa52` · **Buildroot**: `de27194c`

## Qué trae esta versión

Versión de **herramientas y gates de calidad**; el binario de la GUI es el mismo que la
v0.1.3-lu2dsx (los commits nuevos no tocan `src/`).

- **#306/#308 — Banco natural**: el perfil declara la caja nueva del waterfall, así el banco
  de pruebas compara contra la geometría vigente (el ítem del waterfall sube 44 px desde #298).
- **#309 — `expected_changes` por (fixture, region) + reporte con autor**: el gate de calidad ya
  no tapa la pantalla; cada cambio esperado se evalúa por fixture y región y el reporte dice de
  quién es.
- **#310 — CI**: los PRs no corren GitHub Actions; la validación es la matriz local (documentado).
- **Notas de release v0.1.3** (`docs/RELEASE_NOTES_2026-10-24.md`) en main.

## Artefactos
- **Imagen SD completa**: `x6200_<fecha>.img`.
- **Actualización en el lugar**: `x6200_r1cbu_v0.1.4-lu2dsx.tgz` + sidecar sha256.
- `SHA256SUMS` con ambos.

## Estado verificado en la radio
La radio ya ejecuta el binario de esta versión (`063f42f8…`, el mismo que produce `src/` en
main): `CRASHLOG` 0 y audio de RX sano. La ROM original de fábrica (eMMC) no se tocó.
