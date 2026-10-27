# Subida automática de QSO a logs online (QRZ, eQSL, Clublog)

Implementado el 2026-10-27 (v0.1.6 QRZ; **v0.1.8 agrega eQSL y Clublog**). Los QSO que
guarda el FT8/FT4 en `/mnt/qso_log.db` se suben solos a los logs online que configures
cuando la radio tiene WiFi.

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
- Si un proveedor responde error de **credenciales/permiso** (QRZ `RESULT=AUTH`, eQSL
  `No match on eQSL_User/eQSL_Pswd`, Clublog HTTP 401/403) guarda
  `/mnt/<prov>_upload.cooldown` y reintenta recién en **1 h** en vez de insistir cada 30 s.
  `--status` muestra estado/pendientes/cooldown por proveedor y `--adif` sirve para subir
  el log a mano mientras tanto.

## Configuración

### Desde la UI (v0.1.9)

En **Ajustes** hay un tab **Logs** en la primera página (Operation | Audio | Spectrum |
**Logs** | More >) que salta directo a las opciones, embebidas con la misma grilla y estilo
de Ajustes (también se abre como app **Logs** por control remoto/acciones):

- **Upload** (interruptor global) y un interruptor por proveedor (eQSL / QRZ / Clublog).
- Campos de credenciales: al tocar la fila se abre el teclado; los passwords se muestran
  enmascarados (`********`). Cada cambio se guarda solo en `/mnt/upload.conf` y el daemon
  lo toma en ≤30 s.

### Archivo

Archivo `/mnt/upload.conf` (se crea solo la primera vez; migra la `/mnt/qrz.conf` de
v0.1.6 si existe). Cada proveedor se habilita por separado:

```
enabled=1
interval=30

# QRZ Logbook (INSERT por API requiere suscripcion QRZ)
qrz_enabled=0
qrz_api_key=

# eQSL (gratis; usuario y clave de eqsl.cc)
eqsl_enabled=1
eqsl_user=
eqsl_pswd=

# Clublog (gratis; la API key y el "App password" se generan en clublog.org)
clublog_enabled=1
clublog_callsign=LU2DSX
clublog_email=
clublog_password=
clublog_api=
```

- **QRZ**: API key en <https://www.qrz.com/logbook> (botón *API key*). **INSERT por API
  requiere suscripción QRZ** (limitación de QRZ, no de la radio).
- **eQSL**: usuario y clave de <https://www.eqsl.cc>. **`EQSL_USER` es el nombre de
  usuario de eQSL (el indicativo, p. ej. `LU2DSX`), no el email** (probado: con el email da
  `No match on eQSL_User/eQSL_Pswd`). Usa el método documentado en
  `https://www.eqsl.cc/qslcard/ImportADIF.txt` (`ADIFData` + `EQSL_USER` + `EQSL_PSWD`).
  **Verificado con la cuenta real del dueño: 4/4 QSO aceptados**.
- **Clublog**: email de la cuenta, **App password** y **API key** (se generan en
  <https://clublog.org/api.php>). Usa `putlogs.php` (multipart). **Medido 27/10 desde la
  red del proyecto: `putlogs.php` responde 403 de nginx** (incluso con GET; las otras
  páginas del sitio dan 200), así que desde esa red no se pudo validar en vivo; el 403 se
  trata como auth y el daemon reintenta en 1 h.

Comandos útiles por SSH en la radio:

```sh
python3 /usr/share/x6200/qrz-upload.py --status   # config, último rowid, pendientes
python3 /usr/share/x6200/qrz-upload.py --once     # forzar una pasada ahora
python3 /usr/share/x6200/qrz-upload.py --adif     # imprime el ADIF de todos los QSO
                                                  # (para subirlos a mano, p. ej. a QRZ)
cat /mnt/qrz-upload.log                           # qué subió / qué rechazó
/etc/init.d/S55qrz-upload restart                 # reiniciar el daemon
```

## Requisitos de la imagen

La subida usa HTTPS real (validación de certificados): la imagen trae
`ca-certificates` y el módulo `_ssl` de python3 (Buildroot:
`BR2_PACKAGE_CA_CERTIFICATES=y` + `BR2_PACKAGE_PYTHON3_SSL=y`). Sin eso no hay TLS
utilizable (libcurl sigue en `TLS_NONE`, busybox-wget sin HTTPS).

## Lo que NO hace

- **LoTW**: no se puede firmar TQSL en la radio; exportá el ADIF y firmalo en la PC.
- **Cloudlog**: no implementado todavía (el script deja el punto de extensión
  `PROVIDERS`).

## Evidencia de la puesta a punto

- **eQSL (verificado con la cuenta real)**: el lote de 4 QSO dio
  `Result: 4 out of 4 records added` en la cuenta `LU2DSX` (usuario = indicativo; con el
  email como `EQSL_USER` eQSL responde `No match`). Antes se había validado el circuito
  con la cuenta de prueba documentada (`TEST-SWL/Testpswd1`). La radio mostró
  `Caution: ProgramID or Logger not found` (eQSL no tiene registrado `X6200GUI`), que no
  impide la importación.
- **Clublog (no validado en vivo)**: `putlogs.php` devuelve HTTP 403 de nginx desde la red
  del proyecto, tanto desde la PC como desde la radio, con GET y con POST.
- **QRZ**: key válida, pero `RESULT=AUTH` por falta de suscripción QRZ; el dueño no la va a
  pagar, así que QRZ queda deshabilitado y el ADIF se sube a mano por la web si hace falta.

## Exportar el ADIF a mano (sin suscripción QRZ)

La radio guarda todo en `/mnt/qso_log.db`; el uploader sabe volcarlo:

```sh
# desde la PC, por SSH (agrega la cabecera ADIF y guarda el archivo)
{ printf '<adif_ver:5>3.1.0 <programid:8>X6200GUI <EOH>\n'
  sshpass -p 123 ssh root@192.168.0.236 \
    'python3 /usr/share/x6200/qrz-upload.py --adif'
} > qso.adi
```

El archivo `qso.adi` se sube a mano:

- **QRZ Logbook** (gratis por web): qrz.com → *Logbook* → **Import / Upload ADIF** → elegir `qso.adi`.
- **LoTW**: firmar el ADIF con **TQSL** en la PC (*File → Sign a log file*) y subir el `.tq8`
  resultante a LoTW (la radio no puede firmar TQSL).
- **eQSL / Clublog**: importar el mismo ADIF desde sus páginas de carga.

El daemon QRZ queda **apagado** (`enabled=0` en `/mnt/qrz.conf`) mientras no haya
suscripción: no reintenta ni llena el log. Para re-activarlo, `enabled=1` y
`/etc/init.d/S55qrz-upload start`.

## Seguridad de las credenciales

- Las credenciales viven **solo en la radio**, en `/mnt/upload.conf` (permisos `0600`):
  no están en este repositorio, ni en las imágenes/tgz publicadas (la partición DATA no
  forma parte de la imagen).
- En este repo **no hay** API keys ni passwords: el template de `/mnt/upload.conf` que
  crea el daemon está vacío, y el `.gitignore` excluye `upload.conf`, `qrz.conf`,
  `assessment-194/`, `backups/`, `*.adi` y `qso_log.db`.
- Si una credencial se compartió fuera de la radio (por ejemplo en un chat), conviene
  **regenerarla** en el servicio y cargar la nueva desde **Ajustes → Logs**.
