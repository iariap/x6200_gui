# X6200 LU2DSX ROM — v0.1.9-lu2dsx (pre-release)

**Fecha**: 27/10/2026 · **GUI**: main `5c933767` · **Buildroot**: `b44eb81`

## Qué trae esta versión

### Credenciales de logs online desde la UI
Nueva app **Log uploads** (y fila **Ajustes → Operation → Log uploads → Configure**), con
la **misma grilla y estilo que Ajustes**:

- Interruptor global **Upload** y uno por proveedor (**eQSL**, **QRZ**, **Clublog**).
- Campos de **usuario, password/App password, API keys y email**; los passwords se muestran
  enmascarados y se editan tocando la fila (teclado en pantalla).
- Guarda solo en `/mnt/upload.conf` (atómico, permisos 0600); el daemon lo relee en ≤30 s, sin
  reiniciar nada.
- `upload_conf.c` es LVGL-free y tiene roundtrip probado en la PC (passwords con espacios).

### Resto
Igual que v0.1.8: uploader multi-proveedor (eQSL verificado de punta a punta; Clublog
implementado pero bloqueado por 403 desde la red del proyecto; QRZ sin suscripción) y TLS
en la imagen.

## Artefactos
- Imagen SD completa `x6200_<fecha>.img`
- Actualización en el lugar `x6200_r1cbu_v0.1.9-lu2dsx.tgz` + sidecar + `SHA256SUMS`
