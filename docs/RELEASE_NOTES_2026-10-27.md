# X6200 LU2DSX ROM — v0.1.6-lu2dsx (pre-release)

**Fecha**: 27/10/2026 · **GUI**: main (sin cambios de código) · **Buildroot**: `1c1334f`

## Qué trae esta versión

### QSO → QRZ Logbook automático al conectarse a WiFi
- Daemon `S55qrz-upload` (arranca en el boot) que sube los QSO nuevos de
  `/mnt/qso_log.db` al **QRZ Logbook** con la API oficial
  (`POST https://logbook.qrz.com/api`, `ACTION=INSERT`, ADIF con fecha/hora/banda/modo/
  frecuencia/RST/grillas/indicativo).
- Configuración en `/mnt/qrz.conf` (plantilla creada en el primer arranque) con la
  **API key** del QRZ Logbook (`enabled=1`, `interval=30`); estado en
  `/mnt/qrz_upload.state`; log en `/mnt/qrz-upload.log`.
- No sube nada sin API key. **INSERT por API requiere suscripción QRZ**.
- Extensible a Cloudlog/eQSL/Clublog (queda el punto de extensión en el script).

### TLS utilizable en la imagen (requisito)
Hasta ahora la ROM no podía hacer HTTPS: python3 sin módulo `_ssl`, libcurl con
`TLS_NONE`, busybox wget sin HTTPS y sin CA. Ahora:
- `BR2_PACKAGE_CA_CERTIFICATES=y` (bundle en `/etc/ssl/certs`)
- `BR2_PACKAGE_PYTHON3_SSL=y` (módulo `_ssl` de python3)

**Efecto colateral bueno**: el **aviso de firmware nuevo** (que usa `python3 urllib`
por HTTPS) pasa a poder funcionar en la radio.

## Artefactos
- Imagen SD completa `x6200_<fecha>.img`
- Actualización en el lugar `x6200_r1cbu_v0.1.6-lu2dsx.tgz` + sidecar + `SHA256SUMS`

## Requisito de actualización
Este cambio es de **rootfs** (paquetes nuevos), no del binario GUI: hay que
actualizar la ROM completa (tgz del `S02update` o flasheando la SD). El deploy por SSH
del binario no alcanza.
