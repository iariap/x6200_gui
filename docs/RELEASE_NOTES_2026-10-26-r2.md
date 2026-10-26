# X6200 LU2DSX ROM — v0.1.5-lu2dsx (pre-release)

**Fecha**: 26/10/2026 · **GUI**: main `7cd4a6df` · **Buildroot**: `de27194c`

## Qué trae esta versión

### FT8: **Auto CQ por defecto** (como FT8CN)
Con `Auto` habilitado (es el default de fábrica) y **sin un QSO en curso**, al decodificar un
CQ de una estación **no trabajada** en esa banda/modo, la radio **la llama sola** en el slot
alternado — el equivalente a *Auto track CQ* + *Auto call tracked* de FT8CN.
Se desactiva apagando `Auto` (junto con el auto-avance del QSO), tocando una fila, o con
`TX CQ`. Guardas: no llama si ya hay QSO activo, si estás llamando CQ, si la estación figura
trabajada en `/mnt/qso_log.db`, ni sin indicativo cargado; respeta *Hold Freq*.

**Verificado en la radio**: con FT8 abierto en 40 m y sin tocar nada, llamó sola a `PY5QW`
(`Auto CQ: llamando a PY5QW` en pantalla, TX en slots alternados). Evidencia:
`docs/PRUEBAS.md` (26/10 22:32Z) y `assessment-194/scratch/cw244/fb-autocq.png`.

### Resto
Sin cambios respecto de la v0.1.4-lu2dsx (que era de herramientas/gates).

## Artefactos
- **Imagen SD completa**: `x6200_<fecha>.img`.
- **Actualización en el lugar**: `x6200_r1cbu_v0.1.5-lu2dsx.tgz` + sidecar sha256.
- `SHA256SUMS` con ambos.

## Estado verificado en la radio
GUI `8b534333…` (misma que la imagen y el tgz): `CRASHLOG` 0, audio de RX sano, TX detenido y
app cerrada al terminar la prueba. La ROM original de fábrica (eMMC) no se tocó.
