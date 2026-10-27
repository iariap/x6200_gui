# X6200 LU2DSX ROM — v0.1.7-lu2dsx (pre-release)

**Fecha**: 27/10/2026 · **GUI**: main (sin cambios de código) · **Buildroot**: `bce0f27`

## Qué trae esta versión

### QRZ Logbook automático al conectarse a WiFi (corrige la v0.1.6)
- Daemon `S55qrz-upload` que sube los QSO nuevos de `/mnt/qso_log.db` al **QRZ Logbook**
  (`POST https://logbook.qrz.com/api`, `ACTION=INSERT`, ADIF) cuando hay WiFi.
- Config `/mnt/qrz.conf` (API key), estado `/mnt/qrz_upload.state`, log
  `/mnt/qrz-upload.log`; sin API key no sube nada.
- **Nuevo en esta versión**: ante `RESULT=AUTH` (key válida sin suscripción QRZ para
  INSERT) guarda cooldown de **1 h** en `/mnt/qrz_upload.cooldown` y no insiste cada 30 s;
  opción **`--adif`** para exportar todos los QSO y subirlos a mano.
- La key se cargó en la radio y el POST quedó validado; **QRZ exige suscripción** para
  `INSERT` por API (limitación de QRZ).

### TLS en la imagen (igual que v0.1.6)
`BR2_PACKAGE_CA_CERTIFICATES=y` + `BR2_PACKAGE_PYTHON3_SSL=y` (bundle en `/etc/ssl/certs`,
módulo `_ssl` de python3). De paso habilita el aviso de firmware nuevo por HTTPS.

## Artefactos
- Imagen SD completa `x6200_<fecha>.img`
- Actualización en el lugar `x6200_r1cbu_v0.1.7-lu2dsx.tgz` + sidecar + `SHA256SUMS`

## Requisito de actualización
Cambio de **rootfs**: hay que actualizar la ROM completa (tgz del `S02update` o flashear la
SD); el deploy por SSH del binario GUI no alcanza.
