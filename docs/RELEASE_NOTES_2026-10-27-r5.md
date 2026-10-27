# X6200 LU2DSX ROM — v0.1.10-lu2dsx (pre-release)

**Fecha**: 27/10/2026 · **GUI**: main `c83fdd70` · **Buildroot**: `b44eb81`

## Qué trae esta versión

### Fix: las credenciales de logs ahora se ven en Ajustes
La fila "Log uploads" de la v0.1.9 **nunca se había agregado** al grupo Operation y, aun
agregada, quedaba fuera de la primera pantalla. Ahora:

- Tab **Logs** en la **primera página** de tabs de Ajustes
  (`Operation | Audio | Spectrum | Logs | More >`): salta directo al grupo.
- El grupo trae **todas las opciones inline**, con los helpers de Ajustes: switch
  **Upload**, un switch por proveedor (**eQSL/QRZ/Clublog**) y campos de
  **usuario/email/password/API key** en cajas con borde; los passwords se ven
  enmascarados y al tocar el campo se abre el teclado.
- Guarda en `/mnt/upload.conf` al cambiar un switch o confirmar un campo; el daemon lo
  relee en ≤30 s.
- El tab **Info** (una sola fila) pasó al grupo **Display**; la app **Logs** sigue
  existiendo como acceso alternativo (control remoto/acciones).

### Resto
Igual que v0.1.9: uploader multi-proveedor (eQSL verificado), TLS en la imagen, Auto CQ.

## Artefactos
- Imagen SD completa `x6200_<fecha>.img`
- Actualización en el lugar `x6200_r1cbu_v0.1.10-lu2dsx.tgz` + sidecar + `SHA256SUMS`
