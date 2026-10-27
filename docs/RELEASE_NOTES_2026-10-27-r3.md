# X6200 LU2DSX ROM — v0.1.8-lu2dsx (pre-release)

**Fecha**: 27/10/2026 · **GUI**: main (sin cambios de código) · **Buildroot**: `b44eb81`

## Qué trae esta versión

### Subida automática de QSO a **eQSL y Clublog** (además de QRZ)
El daemon de subida (`/etc/init.d/S55qrz-upload`) ahora es **multi-proveedor**, con una
config única `/mnt/upload.conf` (migra sola la `/mnt/qrz.conf` anterior):

- **eQSL** (habilitado): `POST https://www.eQSL.cc/qslcard/ImportADIF.cfm` con
  `ADIFData` + `EQSL_USER` + `EQSL_PSWD` (método documentado en `ImportADIF.txt`).
  **Verificado de punta a punta**: 4 QSO aceptados (`Result: 4 out of 4 records added`)
  con la cuenta de prueba documentada, desde la red de la radio.
- **Clublog** (habilitado): `POST multipart https://clublog.org/putlogs.php` con
  `file`/`email`/`callsign`/`password` (App password) / `api` (API key), según la guía.
  **Medido**: `putlogs.php` responde **403 de nginx** desde la red del proyecto (incluso
  un GET; otras páginas del sitio dan 200), así que queda implementado pero **sin validar
  en vivo** desde acá; el 403 se trata como auth (cooldown 1 h).
- **QRZ** (deshabilitado): la cuenta del dueño no tiene suscripción QRZ para `INSERT`
  por API (`RESULT=AUTH`); queda soportado y se activa con `qrz_enabled=1` + API key.
- Estado y cooldown **por proveedor** (`/mnt/<prov>_upload.{state,cooldown}`),
  `--status` por proveedor y `--adif` para exportar el log a mano.

### TLS en la imagen (misma base que v0.1.6)
`BR2_PACKAGE_CA_CERTIFICATES=y` + `BR2_PACKAGE_PYTHON3_SSL=y`.

## Artefactos
- Imagen SD completa `x6200_<fecha>.img`
- Actualización en el lugar `x6200_r1cbu_v0.1.8-lu2dsx.tgz` + sidecar + `SHA256SUMS`

## Requisito de actualización
Cambio de **rootfs**: hay que actualizar la ROM completa (tgz del `S02update` o flashear la
SD); el deploy por SSH del binario GUI no alcanza.
