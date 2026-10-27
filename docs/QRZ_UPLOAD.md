# Subida automática de QSO al QRZ Logbook

Implementado el 2026-10-27 (v0.1.6-lu2dsx). Los QSO que guarda el FT8/FT4 en
`/mnt/qso_log.db` se suben solos al **QRZ Logbook** cuando la radio tiene WiFi.
Pensado para extenderse a otros proveedores (Cloudlog, eQSL, Clublog) sin tocar el
resto del sistema.

## Cómo funciona

- Un daemon (`/etc/init.d/S55qrz-upload`, arranca en el boot antes de la GUI) corre
  `/usr/share/x6200/qrz-upload.py --loop`.
- Cada `interval` segundos (30 por defecto): si `wlan0` está `up` y hay `api_key`,
  busca en `/mnt/qso_log.db` los QSO con `rowid` mayor al último subido, arma el
  **ADIF** y hace un `POST https://logbook.qrz.com/api` con
  `KEY=<api_key>&ACTION=INSERT&ADIF=<adif>` (User-Agent identificable).
- Si QRZ responde `RESULT=OK` o `RESULT=REPLACE`, guarda el `rowid` en
  `/mnt/qrz_upload.state` y sigue con el próximo. Si responde otra cosa
  (`RESULT=AUTH`, `RESULT=FAIL`, `REASON=...`), lo deja pendiente y reintenta en la
  próxima pasada; queda registrado en `/mnt/qrz-upload.log`.
- No sube nada si no hay `api_key` (la plantilla se crea vacía en el primer arranque).

## Configuración

Archivo `/mnt/qrz.conf` (se crea solo la primera vez; se puede editar por SSH o
desde la PC):

```
enabled=1
api_key=XXXX-XXXX-XXXX-XXXX     # API key del QRZ Logbook
interval=30
```

La API key está en <https://www.qrz.com/logbook> (botón *API key*). **INSERT por API
requiere suscripción QRZ** (es limitación de QRZ, no de la radio).

Comandos útiles por SSH en la radio:

```sh
python3 /usr/share/x6200/qrz-upload.py --status   # config, último rowid, pendientes
python3 /usr/share/x6200/qrz-upload.py --once     # forzar una pasada ahora
cat /mnt/qrz-upload.log                           # qué subió / qué rechazó
/etc/init.d/S55qrz-upload restart                 # reiniciar el daemon
```

## Requisitos de la imagen

La subida usa HTTPS real (validación de certificados): la imagen trae
`ca-certificates` y el módulo `_ssl` de python3 (Buildroot:
`BR2_PACKAGE_CA_CERTIFICATES=y` + `BR2_PACKAGE_PYTHON3_SSL=y`). Sin eso no hay TLS
utilizable (libcurl sigue en `TLS_NONE`, busybox-wget sin HTTPS).

## Lo que NO hace

- **LoTW**: no se puede firmar TQSL en la radio; exportá el ADIF (ver
  `qso_log.db`) y firmalo en la PC.
- No sube eQSL/Clublog todavía: el script deja el punto de extensión
  (`MODE_TO_ADIF` y `run_once`) para agregarlos.

## Evidencia de la puesta a punto

- Prueba en la PC del POST real con key de prueba:
  `STATUS=AUTH&RESULT=AUTH&REASON=invalid api key` → TLS, User-Agent y POST
  correctos; no se subió nada.
- Pendiente en la radio: validación con la API key real del dueño.
