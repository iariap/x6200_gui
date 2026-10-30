# Fase A — procedencia y custodia del paquete oficial X6200 1.0.8 · **STOP por integridad**

- **Issue**: [iariap/x6200_gui#375](https://github.com/iariap/x6200_gui/issues/375) — Fase A del [Epic #374](https://github.com/iariap/x6200_gui/issues/374).
- **Estado final de la fase: `ARTIFACT_UNAVAILABLE` (STOP)** — causa **`INTEGRITY_MISMATCH_VS_VENDOR_PUBLISHED_HASH`**.
- **Base real de `main`**: `6850d671d430a1693dbcaf593579a3d98b037995` («docs(audio): physical instrument plan and MUX fail-safe contract (#372)»), verificada con `git ls-remote fork refs/heads/main` y la API de GitHub el 2026-10-30. Worktree `/home/pablo/desa/x6200/x6200-fw108-artifacts`, rama `audit/x6200-fw108-artifacts`.
- **Qué es**: la evidencia de la descarga del paquete oficial `X6200_Update_1.0.8` desde `xiegu.eu`, su verificación estática, el inventario APP/BASE, la comparación con el paquete histórico 1.0.6 local y un checklist de actualización/rollback para el **dueño**, sin ejecutar nada.
- **Qué NO es**: no se instaló, flasheó ni ejecutó ningún artefacto; no se accedió a la radio (WiFi/SSH/USB), ni I²C/UART, ni se montó ninguna imagen escribible. No hay PASS de autenticidad: **el paquete descargado no supera la verificación contra el hash que el propio fabricante publica dentro del paquete**.
- **Consecuencia**: Fases B (#376), C (#377) y D (#378) quedan **bloqueadas** hasta que el fabricante aclare o reemita el paquete, o el dueño aporte una copia auténtica con hash coincidente. Nada de este directorio habilita producción ni instalación.

## 0. Resumen ejecutivo

| Verificación | Resultado | Evidencia |
|---|---|---|
| Cadena de procedencia página oficial → link → ZIP | **PASS** — `https://xiegu.eu/downloads/` → `?sdm_process_download=1&download_id=10216` → 302 → `https://xiegu.eu/wp-content/uploads/sites/4/2026/10/x6200udimg-260820001.zip` | §1 |
| Tamaño real == `content-length` (371 310 845 B) | **PASS** | §1.4 |
| SHA256 calculado del ZIP | `ca0226c7ffbfa38a5655bd9fc4bbe4e7995055cb3e2b35433a2ad5d39b4100a1` | §1.4 |
| Reproducibilidad del servidor (ETag 304; range 206 idénticos) | **PASS** | §1.5 |
| ZIP íntegro (`unzip -t`, `7z t`, CRC32 por entrada) | **PASS** | §2.1 |
| Validación de entradas (sin traversal, symlinks, bomba, paths absolutos) | **PASS** | §2.1 |
| **Hash oficial publicado en el release note == hash del `sdcard.img`** | **FAIL** — declarado `29620674…e5228698` ≠ calculado `a968d278…d0bc3f` | §2.2 |
| Control positivo: paquete histórico 1.0.6, mismo procedimiento | **PASS** — declarado `e507e7ad…7734514` == calculado, verificado de forma independiente | §2.3 |
| Atribución de versión APP / BASE | **parcial** — release note declara APP 1.0.8 (ago 17 2026) / BASE 1.0.8 (ago 19 2026); GUI contiene la constante de versión `1.0.8`; el `.xgf` del BASE es opaco y **no** se puede verificar internamente | §4 |
| Inventario estático APP/BASE y comparables | **completo para lo extraíble sin ejecutar/montar** | §4, `inventory.csv` |
| Checklist de actualización/rollback documentado y **no ejecutado** | **completo** — estado `UPGRADE_NOT_READY` | `upgrade-owner-checklist.md` |
| Paquete apto para instalar / autenticar 1.0.8 oficial | **NO — no instalar** hasta aclaración del fabricante | §2.4 |

**Bottom line:** el ZIP oficial está íntegro y es reproducible, pero **no autentica su propio payload**: el fabricante declara un SHA256 de `sdcard.img` que no corresponde al `sdcard.img` que distribuye en ese mismo ZIP. Como el issue exige «si falla integridad no marcar `READY_FOR_RE`», la fase cierra en STOP documentado con el paquete en custodia.

## 1. Fuente y cadena de procedencia

### 1.1 Página oficial

- URL: <https://xiegu.eu/downloads/> — descargada el **2026-10-30T13:09:24Z**.
- HTTP 200, `content-type: text/html`, 152 173 B; SHA256 de la captura:
  `9fe2294142c4568ac18aa71823319ad889409a0a069873d96af19b5cc0b5f5bd` (`vendor/downloads_page_20261030.html` en scratch).
- Entrada de descarga en el HTML: `<a href="https://xiegu.eu/?sdm_process_download=1&download_id=10216" class="sdm_download green" title="X6200 Firmware Upgrade v1.0.8">`; el texto de la entrada dice **«X6200_Update_1.0.8 (2026-08-20) — Release Notes [EN]»**.
- La misma página lista 1.0.7 («X6200_Update_1.0.7 (251012)») con la nota **«Changed the baseband uplink data to use SLIP encoding»** (contexto para Fase B; no se descargó 1.0.7).
- La página **no** publica hash para X6200 1.0.8. Sí publica «File – Filename/SHA256» para versiones de X6100, lo que confirma que el fabricante conoce la práctica; para X6200 el hash aparece **dentro del release note del paquete**.

### 1.2 Anuncio oficial (contexto de versión)

- URL: <https://xiegu.eu/2026/08/29/xiegu-x6200-firmware-1-0-8-cw-macros-keyer-fixes/> — descargado el **2026-10-30T13:11:38Z**.
- HTTP 200, 113 470 B; SHA256 `a9db66b258097b8fb0d2b4687d42942a421f9c19b681eda9f0d4a6d58a6a3442`.
- Publicado el 2026-08-29. Declara literalmente: **«Firmware Versions APP: 1.0.8 – 17 August 2026 / BASE: 1.0.8 – 19 August 2026»**.
- El mismo post enlaza una copia alternativa en Google Drive («right here», `drive.google.com/…id=1QF9dwOuxPF-Y62iWwF_TaUUZeEdnjWn8`). **No se usó como sustituto**: sin sesión de Google el link responde 303 → `accounts.google.com` (login), por lo que no fue verificable desde este entorno (§7).

### 1.3 Encabezados de la descarga oficial

`HEAD`/`GET` sobre `https://xiegu.eu/?sdm_process_download=1&download_id=10216`, el 2026-10-30T13:09:33Z:

```
HTTP/2 302
location: https://xiegu.eu/wp-content/uploads/sites/4/2026/10/x6200udimg-260820001.zip

HTTP/2 200
content-type: application/zip
content-length: 371310845
last-modified: Wed, 14 Oct 2026 12:19:32 GMT
etag: "1621c0fd-65dcbf1858513"
accept-ranges: bytes
```

### 1.4 Descarga completa y hash calculado

- Comando: `curl -L --retry 5 --fail --retry-delay 5 --connect-timeout 30 --max-time 3600 -o x6200udimg-260820001.zip -D dl10216_get_headers.txt -w '<métricas>' "https://xiegu.eu/?sdm_process_download=1&download_id=10216"`.
- Inicio **2026-10-30T13:09:39Z**; fin **2026-10-30T13:21:44Z**; `curl_exit=0`; HTTP 200; `size_download=371310845`; `time_total=724.674239 s`; velocidad media 512 383 B/s.
- Archivo en disco: 371 310 845 B (exacto al `content-length`).
- **SHA256 calculado del ZIP: `ca0226c7ffbfa38a5655bd9fc4bbe4e7995055cb3e2b35433a2ad5d39b4100a1`** (dos pasadas `sha256sum`, coincidentes).
- El fabricante **no publica** hash del ZIP: este SHA256 es **calculado por nosotros**, no oficial.

### 1.5 Reproducibilidad del servidor

- `If-None-Match: "1621c0fd-65dcbf1858513"` → **HTTP 304** (el recurso servido no cambió).
- `Range: bytes=0-1048575` → **206**, SHA256 `ade1fd2085acfec721fe5157b185652da1b68fc7c221fb24193fd7c02b215d21`, idéntico al primer MiB local.
- `Range: bytes=370262269-371310844` → **206**, SHA256 `7d5bb6371e16297f78ce87e08110b5fa5418a2d709a25b560f29564e5e636973`, idéntico al último MiB local.
- Conclusión: el servidor entrega bytes estables; la discrepancia de §2.2 **no** es un artefacto de esta descarga.

## 2. Integridad: el ZIP está íntegro, el paquete no autentica su payload

### 2.1 Validación del contenedor ZIP (todo PASS)

- `unzip -t`: **exit 0**, «No errors detected in compressed data» (CRC de las 3 entradas verificado).
- `7z t`: **exit 0**, «Everything is Ok».
- Contenido exacto (3 entradas, 856 687 351 B descomprimidos, factor 2,307× — sin bomba):
  - `x6200udimg-260820001/` (directorio, modo 40777)
  - `x6200udimg-260820001/ReleaseNote-260820001(EN_CHS).txt` — 759 B, CRC32 `53F0FA11`
  - `x6200udimg-260820001/sdcard.img` — 856 686 592 B, CRC32 `366043DC`
- Chequeo de seguridad de entradas (Python `zipfile`): sin paths absolutos, sin `..`, sin backslashes, sin symlinks, sin relaciones de compresión > 1000, 0 bytes extra tras el EOCD, sin comentario de archivo, 3 entradas en directorio central.
- CRC32 del `sdcard.img` extraído = `366043dc` = CRC32 declarado por el ZIP ⇒ extracción bit-a-bit correcta.
- La imagen extraída con dos descompresores independientes (`unzip` y `7z x -so`) da el **mismo** SHA256 (§2.2). **La discrepancia no proviene de la descarga ni de la extracción.**

### 2.2 El hallazgo: hash oficial declarado ≠ hash del payload

El release note incluido en el paquete (en sus dos secciones, EN y CHS) declara:

```
Image File SHA256: 29620674020e748ffd70173f0b2920dc0174d2733232d92a2a512111e5228698
APP:  1.0.8  Aug 17 2026,16:21:06
BASE: 1.0.8 Aug 19 2026,16:13:08
```

| Artefacto | SHA256 | Tamaño | ¿Coincide? |
|---|---|---|---|
| `sdcard.img` declarado en `ReleaseNote-260820001(EN_CHS).txt` | `29620674020e748ffd70173f0b2920dc0174d2733232d92a2a512111e5228698` | no declarado | — |
| `sdcard.img` extraído del ZIP oficial | `a968d2786d5003012bcc8485d287fcde93739e018b22aeb93a89dbc695d0bc3f` | 856 686 592 B | **NO** |

- El hash declarado aparece **dos veces** (EN y CHS) en el release note y **no** aparece como bytes dentro del ZIP ni dentro de la imagen (búsqueda binaria exhaustiva: negativo).
- El ZIP que contiene ese release note **es** el ZIP que sirve el link de descarga de la página oficial (mismo nombre de recurso `x6200udimg-260820001`, mismo `download_id=10216`).
- Por tanto la inconsistencia es **interna al paquete publicado**: el par release note ↔ imagen no cierra. No se puede acreditar que el `sdcard.img` distribuido sea el build exacto que el fabricante quiso publicar con ese hash.

### 2.3 Control positivo 1.0.6: el procedimiento de hash del fabricante es real

Se verificó el mismo mecanismo sobre el paquete histórico local (sólo lectura, fuera del workspace):

| Artefacto 1.0.6 | SHA256 | Tamaño |
|---|---|---|
| declarado en `ReleaseNote-250114001(EN_CHS).txt` | `e507e7ad98b3ca4e30407177f579717318e9718b1542c48c7532556627734514` | — |
| `sdcard.img` del ZIP `2025-01-15_Xiegu_X6200_update_V1.0.6.zip` | `e507e7ad98b3ca4e30407177f579717318e9718b1542c48c7532556627734514` | 856 686 592 B |
| **Coincidencia** | **SÍ** | |

El ZIP histórico 1.0.6 tiene SHA256 `8c270b3130cb68349da062be7384c43221ebf64146ab2e8b2ae4cbff26712e17` (371 909 329 B), también calculado por nosotros. El release note 1.0.6 declara APP 1.0.6 `Jan 14 2025,14:13:28` y BASE 1.0.6 `Jan 11 2025,13:46:22`.

Conclusión: el fallo de verificación de 1.0.8 es **específico del paquete 1.0.8 / de su publicación**, no un error metodológico nuestro. No se puede determinar la causa (¿imagen regenerada y note no actualizado? ¿re-subida del ZIP? ¿note copiado de otro build?) sin información del fabricante.

### 2.4 Advertencia de seguridad (bloqueante)

> **NO instalar este paquete hasta que Xiegu/Radioddity aclare o reemita el paquete.**
>
> - El `sdcard.img` del ZIP descargado **no** coincide con el `Image File SHA256` que el propio fabricante declara en el release note del mismo paquete. Escribir esa imagen en una microSD y arrancar la radio con ella es una operación que **no está autenticada** por el fabricante.
> - El dueño debe **verificar el hash de cualquier paquete antes de flashear**. En el 1.0.6 el hash oficial coincide y el procedimiento es válido; en el 1.0.8 descargado, no.
> - Ningún agente de este epic instala, flashea, resetea ni ejecuta nada del paquete (política del Epic #374).

## 3. Contenido del paquete 1.0.8

| Entrada | Tipo | Tamaño (B) | CRC32 | Fecha en ZIP | SHA256 (calculado) |
|---|---|---|---|---|---|
| `x6200udimg-260820001/` | directorio | 0 | — | 2026-08-21 02:38:23 | — |
| `x6200udimg-260820001/ReleaseNote-260820001(EN_CHS).txt` | texto EN+CHS | 759 | `53F0FA11` | 2026-08-21 02:34:42 | `2e16486969366bb42c6e9665a7f6f8981911343c723f2087e60ec0e505f9504c` |
| `x6200udimg-260820001/sdcard.img` | imagen MBR de microSD | 856 686 592 | `366043DC` | 2026-08-20 03:15:26 | `a968d2786d5003012bcc8485d287fcde93739e018b22aeb93a89dbc695d0bc3f` |

- **El paquete 1.0.8 no incluye PDF ni `readme.1st`** (a diferencia del 1.0.6, que incluía el PDF de Radioddity y el `readme.1st`). Sólo release note + imagen. Las instrucciones de instalación vigentes están **fuera del paquete** (§6).
- El release note completo (EN) dice: (1) fix de precisión de WPM del keyer; (2) fix del S-meter que quedaba en −255 dBm; (3) macro de keyer CW (`[APP]` → `[F3]–[CALL]`), y la sección CHS equivalente. Declara APP 1.0.8 (17-ago-2026 16:21:06) y BASE 1.0.8 (19-ago-2026 16:13:08).

## 4. Estructura APP / BASE del payload (inventario estático)

### 4.1 Geometría de la microSD (`sdcard.img`)

`file`, `7z l`, `sfdisk -d`, `fdisk -l` (sólo lectura, sin montar): MBR `label: dos`, `label-id: 0x00000000`, sector 512 B, 1 673 216 sectores (856 686 592 B exactos). Layout **idéntico al del 1.0.6**:

| Partición | Sectores | Tamaño | Tipo | Rol aparente | SHA256 (calculado por rango) |
|---|---|---|---|---|---|
| p1 `sdcard.img1` | 2048–34815 (32 768) | 16 MiB | `0x0c` FAT32 LBA, **activa** | boot (zImage/DTB/boot.scr/logo) | `a37a9a448007651ca39a5d5b231b7a900575ab96f0db9732fb1c568e4dc2af90` |
| p2 `sdcard.img2` | 34816–854015 (819 200) | 400 MiB | `0x83` Linux | rootfs ext4 (APP Linux) | `3c475b16d48acb23f727cbc3d6d3b62f103bbc26bd3ce7f2e8f1b3be27ff4764` |
| p3 `sdcard.img3` | 854016–1673215 (819 200) | 400 MiB | `0x0c` (BPB lo formatea **FAT16**) | DATA/update: `rootfs.tar` | `c7131966845ffd9761702adfe87c56b2d2b93aabe07119a089fa762da70d5bc5` |

- p2 es un ext4 limpio (`dumpe2fs`): UUID `8c499e1c-a022-41dc-bfec-03969cbd2032`, bloque 1024 B, 102 400 inodes, creado 2026-08-20 03:14:08, último escrito 03:14:14.
- p3 contiene un único archivo `rootfs.tar` de **395 427 840 B** (mtime 2026-08-20 14:14), SHA256 **`f511c99bc9a813ace4d5db94ad4ab29cf2e1d0bac56c36969cad6bda89b5221b`**, con 7 513 entradas.
- El contenido de `rootfs.tar` es coherente con p1/p2: el `./boot/zImage` y el `./boot/sun8i-r16-x6200.dtb` del tar tienen el mismo SHA256 que los de p1, y `./usr/app_qt/x6200_ui_v100` / `./usr/firmware/x6200_mcu_20260820141208.xgf` del tar coinciden con los de p2 (verificado por hash en streaming, sin extraer el tar a disco).

### 4.2 APP Linux (payload no autenticado)

| Ruta en el rootfs | Tipo | Tamaño (B) | SHA256 | Atribución de versión |
|---|---|---|---|---|
| `/usr/app_qt/x6200_ui_v100` | ELF 32-bit LSB ARM, EABI5, hard-float, dinámico (`/lib/ld-linux-armhf.so.3`), GNU/Linux 5.4.0, **not stripped**; entry `0x2a188` | 1 684 984 | `26623266bfcd3ed4d2e08b03b9f023cef809579f5ed497899670fc01f66e9b2b` | **atribución 1.0.8** por evidencia interna (§4.4) — paquete no autenticado |
| `/usr/app_qt/x6100_ui_v100` | ELF 32-bit LSB ARM, EABI5, dinámico, not stripped | 1 211 056 | `b972698abd235280aface4ec15285d2e87e46c238bc4658bd249b48c0a50b057` | `UNKNOWN` (no declarada por el fabricante; contenido actualizado respecto de 1.0.6) |
| `/usr/firmware/x6200_mcu_20260820141208.xgf` | `file` = **data** (opaco); entropía 7,4704 bits/byte; primeros 16 B `2cb6ce74a1837bae18eed02aff7c1b82`; modo `-rw-------` | 524 288 | `9a19f97a5ad9e141a3663d222f984073bf0b18b38d4662604372a0036f9daa23` | **BASE**: versión *declarada* 1.0.8 en el release note; **no verificable internamente** (`UNKNOWN` por inspección) |
| `/usr/share/emmc_sources/u-boot-sunxi-with-spl.bin` | bootloader (blob) | 548 864 | `ccf63f9a558c725eb644b3246f786628b6c1c7cbecfb7f1a627fb54fafa2053e` | `UNKNOWN` |
| `/usr/share/emmc_sources/zImage` | kernel ARM (zImage) | 4 956 144 | `a838ec5ef84cdf2b8a9c3ccd75ddd489b82e6f8911b8e4470eb76bdc86df42b3` | `UNKNOWN` (idéntico al kernel del 1.0.6, §5) |
| `/usr/share/emmc_sources/sun8i-r16-x6200.dtb` | DTB | 26 413 | `7c73c0b0dc2a2691e2560f51927dab1bd06e3cc44bff3afb6a2c089ee8629f8b` | `UNKNOWN` |
| `/usr/share/emmc_sources/boot.scr`, `boot_emmcblk1_zimage.scr` | scripts U-Boot (`uImage` script) | 595 | `8fe77effa6228ae9cff2aefcce9a7a614c6742da0abeb8811abb68658a44329e` | `UNKNOWN` |
| `/usr/share/emmc_sources/boot_emmcblk2.scr` | script U-Boot | 424 | `35c58abbf16e154aefd71539c75cd3dda03abda5af366f645a6245bb624a0ac1` | `UNKNOWN` |
| `/usr/share/emmc_sources/boot_without_splash.scr` | script U-Boot | 443 | `ac5506c0c2e16d3108d199973fcb98236f5408fa2fb6dcc4320c833d2e272322` | `UNKNOWN` |
| `/usr/share/emmc_sources/install_emmc.sh` | shell script (leído, **no ejecutado**) | 2 737 | `25b8f41e01e916c5dd7966d33613ce951536ac96f35b22c174a8608f2b2dd401` | `UNKNOWN` |
| `/usr/share/emmc_sources/fmt_emmc.sh` | shell script (leído, **no ejecutado**) | 570 | `e36ab377cd4e40389e4df987858b1793e5b4e91eab04d523f943beff493bca88` | `UNKNOWN` |

Los archivos de la partición de boot p1 son los mismos que los de `/usr/share/emmc_sources` usados por el instalador (por hash): `zImage` `a838ec5e…`, DTB `7c73c0b0…`, `boot.scr` `f60550743a48e4967fe974b5c9b1fd7d7446e0531236c78666b2c826ad197719` (502 B), `logo.bmp` `5461cd6e5ba5a65cbbaffc76e771912f1bea5f2fa78123f46a638d7a39c09756` (25 654 B).

Lectura estática de `install_emmc.sh` (2 737 B, sólo lectura, **no se ejecutó**): el mecanismo de actualización de APP escribe el bootloader en la eMMC, formatea/copia p1 (FAT, 48 MiB según `fmt_emmc.sh`) y p2 (ext4, 7,2 GB) de la eMMC y descomprime `rootfs.tar` desde la partición de la microSD; preserva `/usr/app_qt/xparam.db`, `/usr/app_qt/voice`, `/etc/xgradio` y `/etc/NetworkManager` (backup/restore dentro del script). Es evidencia del mecanismo, **no** instrucciones oficiales al operador ni algo que este agente haya ejecutado.

### 4.3 BASE (MCU/FPGA)

- El único artefacto de BASE identificable es `x6200_mcu_20260820141208.xgf` (524 288 B, opaco: `file`=data, alta entropía, sin magic ELF/DTB/MBR/FAT). No se desencriptó ni ejecutó.
- **No se encontró un artefacto separable de FPGA** en el paquete ni en el rootfs (búsqueda por nombres/magic en el listado completo del rootfs: no hay `.rbf`/`.bin` de FPGA). Si el `.xgf` empaqueta MCU y FPGA a la vez, es `UNKNOWN`: no se puede afirmar ni negar sin desencriptar/ejecutar, y eso está fuera de alcance.
- El PDF histórico 1.0.6 (Radioddity) afirma en §2.3 paso 18 que «HARDWARE and FPGA versions will not change during this process»; es evidencia de que **en 1.0.6** no había actualización de FPGA por esta vía. No se extrapola a 1.0.8.
- El nombre `x6200_mcu_20260820141208.xgf` codifica una fecha 2026-08-20 14:12:08, posterior en ~22 h a la fecha declarada del BASE 1.0.8 (2026-08-19 16:13:08): **relación temporal `UNKNOWN`** (puede ser empaquetado posterior). No se usa el nombre como prueba de versión.

### 4.4 Evidencia de atribución APP 1.0.8 (sin ejecutar)

1. El release note del paquete declara `APP: 1.0.8 Aug 17 2026,16:21:06` y `BASE: 1.0.8 Aug 19 2026,16:13:08`.
2. `x6200_ui_v100` 1.0.8 contiene **una única** cadena suelta `1.0.8` en rodata (offset de archivo `0x11b018`), embebida en el mismo patrón/vecindad que la cadena suelta `1.0.6` del `x6200_ui_v100` del paquete 1.0.6 (offset `0x114ac7`). Es la constante de versión de la APP.
3. mtime del binario en el rootfs: 2026-08-17 23:07; rootfs creado 2026-08-20 03:14; `rootfs.tar` del 2026-08-20 14:14 — coherentes con un build de APP de agosto 2026.
4. **Límite explícito**: esta atribución es por evidencia interna; **no** acredita que el payload sea «el 1.0.8 oficial» porque la autenticidad falla en §2.2. En este documento se lo llama «atribución 1.0.8 por evidencia interna / paquete no autenticado», nunca «APP oficial 1.0.8 acreditada».

### 4.5 Señales estáticas relevantes para las fases siguientes (sin conclusión de RE)

En `x6200_ui_v100` 1.0.8 (ELF con `.symtab`/`.strtab`, sin DWARF) aparecen, entre otras, las cadenas/símbolos: `slip.c`, `slip_encode`, `slip_decode`, `[ERR] SLIP Packet Too Long, length = `, `[ERR] Wrong SLIP Packet, size = `, `BaseCommIIC`, `BaseComm`, `basecomm_slave_info_pack`, `basecommiic_pack`, `basecommiic_param_t`, `startBaseComm`, `startBaseCommIic`. El diff de símbolos contra 1.0.6 da 6 144 vs 6 041 símbolos (FUNC/OBJECT), con `slip_encode`/`slip_decode` presentes sólo en 1.0.8. **Es material de partida para #376, no una conclusión de protocolo**: no se demostró el formato de payload SLIP ni la equivalencia con `x6200_flow_t`, que es exactamente lo que el Epic prohíbe asumir.

## 5. Comparación con el material histórico (1.0.6 y `refs/vendor-fw`)

### 5.1 Material histórico local (sólo lectura, fuera del workspace)

- ZIP: `/home/pablo/lenovo_vieja/Downloads/radio/x6200/2025-01-15_Xiegu_X6200_update_V1.0.6.zip` — 371 909 329 B, SHA256 `8c270b3130cb68349da062be7384c43221ebf64146ab2e8b2ae4cbff26712e17`; contiene 5 entradas (dir + PDF Radioddity 901 623 B + `readme.1st` 355 B + `ReleaseNote-250114001(EN_CHS).txt` 2 336 B + `sdcard.img` 856 686 592 B).
- Hashes de documentos 1.0.6: release note `1695dac424390864d3222f427a6af960f868a142d19c777205da1587486d450a`; readme `0ac390aabb1772108d5fb044c55005707cdf8b05b03b6d2227e6e7808433ab6b`; PDF `86589c152d16876d014eb1d34e94cf5d07dda41520b4efdd18459a88c45a44d3`.
- `sdcard.img` 1.0.6: SHA256 `e507e7ad98b3ca4e30407177f579717318e9718b1542c48c7532556627734514` (**== hash declarado**, §2.3); MBR con el mismo layout (p1 2048/32 768 activa FAT32; p2 34816/819 200 Linux; p3 854016/819 200 FAT); hashes de particiones p1 `366b82ba…`, p2 `fc330714…`, p3 `ffdca08b…`; ext4 UUID `653a142c-a3e4-40df-8050-0fdb3285d601`.
- Dentro del rootfs 1.0.6: `/usr/app_qt/x6200_ui_v100` 1 649 256 B SHA256 `b76331c5aa859ae98dc1819b1079a3e40489e9986c9656e0dc2c553aeb28871b`; `/usr/app_qt/x6100_ui_v100` 929 312 B SHA256 `a3d13459fb209bce32e4dde8fcbfc08d7c758ec69a15f77f3a5bb21e99d8b5eb`; `/usr/firmware/x6200_mcu_20250111134652.xgf` 524 288 B SHA256 `dca90387b70d9550448006310a69ea65625bec87f402c10df789deeb13ff12f3`; `rootfs.tar` 395 028 480 B SHA256 `ad2f7ec3ec6493cc0f44bf6da35971f563c75d75e2ae45be48603f0bd6bff3d8` (7 504 entradas).
- Boot 1.0.6: `zImage` 4 956 144 B `a838ec5e…`; DTB 26 472 B `f422787a9b50b6f526099eb4871dd58c4c2628a41c9428bbd6d9a537b9aa8dac`; `boot.scr` 493 B `776ba0295d690371ec64531173330ba69748c9e2f039b1e48bec9466e9cd5258`; `logo.bmp` 25 654 B `46baf4e04d5a18509ba0bd7d89980dff00816218c37a63702d5fa2c935e0ccd7`.

### 5.2 `refs/vendor-fw` — procedencia demostrada por hash

- `/home/pablo/desa/x6200/refs/vendor-fw/x6200_ui_v100` (1 649 256 B, SHA256 `b76331c5…`) es **byte-idéntico** al `/usr/app_qt/x6200_ui_v100` del rootfs del **paquete oficial 1.0.6**.
- `/home/pablo/desa/x6200/refs/vendor-fw/x6200_mcu_20250111134652.xgf` (524 288 B, SHA256 `dca90387…`) es **byte-idéntico** al `/usr/firmware/x6200_mcu_20250111134652.xgf` del **paquete oficial 1.0.6**.
- Por lo tanto, ese material local queda **atribuido a 1.0.6** (APP 1.0.6 / BASE 1.0.6) con evidencia de hash; **no** se le atribuye 1.0.8. Es el comparable correcto «GUI ELF con GUI ELF» y «BASE con BASE» para #376.

### 5.3 Tabla comparativa 1.0.6 → 1.0.8

| Artefacto | 1.0.6 | 1.0.8 (payload no autenticado) | Cambia |
|---|---|---|---|
| Layout MBR (p1/p2/p3) | 2048/32768 · 34816/819200 · 854016/819200 | idéntico | no |
| `zImage` | `a838ec5e…` (4 956 144 B) | `a838ec5e…` (4 956 144 B) | **no (idéntico)** |
| `sun8i-r16-x6200.dtb` | `f422787a…` (26 472 B) | `7c73c0b0…` (26 413 B) | sí |
| `boot.scr` (p1) | `776ba029…` (493 B) | `f6055074…` (502 B) | sí |
| `logo.bmp` | `46baf4e0…` (25 654 B) | `5461cd6e…` (25 654 B) | sí |
| `x6200_ui_v100` | `b76331c5…` (1 649 256 B), cadena `1.0.6` | `26623266…` (1 684 984 B), cadena `1.0.8` | sí |
| `x6100_ui_v100` | `a3d13459…` (929 312 B) | `b972698a…` (1 211 056 B) | sí |
| `x6200_mcu_*.xgf` | `x6200_mcu_20250111134652.xgf`, `dca90387…` | `x6200_mcu_20260820141208.xgf`, `9a19f97a…` | sí |
| `rootfs.tar` | 395 028 480 B, `ad2f7ec3…`, 7 504 entradas | 395 427 840 B, `f511c99b…`, 7 513 entradas | sí |
| Release note | `e507e7ad…` **coincide** con su imagen | `29620674…` **NO coincide** con su imagen | — |

## 6. Instrucciones oficiales y checklist del dueño

- El **paquete 1.0.8 no trae PDF ni readme**. Las instrucciones vigentes están en la web oficial:
  - «X6200 Firmware Upgrade Tutorial» (`download_id=9241`) → PDF `X6200FirmwareUpgradeTutorialWS-JSZL-2407-01ENRevised.pdf`, 300 348 B, SHA256 `68ecc5a434e12080473807f5a6312fcf1f317960dd6d41a284e300d4bc877fbf`, XIEGU WS-JSZL-2407-01 Rev 1.0, fecha 2024-07-20.
  - Nota: ese tutorial menciona un archivo `x6200udimg.img` en la raíz del ZIP; el paquete oficial actual contiene `sdcard.img`. Es un **anacronismo documental** a advertir al operador (no se deduce nada más).
- El **PDF histórico 1.0.6** de Radioddity (incluido en el paquete 1.0.6 local) documenta el mismo procedimiento en dos pasos (APP por microSD; BASE por menú `FIRMWARE UPGRADE`) con más detalle, y añade `FACTORY RESET` al final. Se usó sólo como documentación citada (texto extraído con `pdftotext`, **nada ejecutado**).
- Procedimiento documentado y riesgos consolidados en **`upgrade-owner-checklist.md`**, con estado **`UPGRADE_NOT_READY`** (no hay backup verificado del dueño y, además, el paquete de 1.0.8 descargado **no es apto para instalar** por §2.4).

## 7. Limitaciones, `UNKNOWN`s y lo que NO se acredita

1. **No se acredita autenticidad del payload** de 1.0.8 (hash oficial ≠ hash calculado). Tampoco la equivalencia con «el 1.0.8 oficial»: sólo hay atribución 1.0.8 por evidencia interna y declaración del fabricante en un paquete que falla su propia verificación.
2. **No se acredita la versión del BASE por inspección**: el `.xgf` es opaco; su versión 1.0.8 es sólo la declaración del release note. Relación entre el nombre `20260820141208` y la fecha declarada del BASE: `UNKNOWN`.
3. **FPGA**: no se identificó artefacto separable; si el `.xgf` incluye FPGA, `UNKNOWN`.
4. **Google Drive** (segunda vía oficial enlazada desde el blog): inaccesible sin login de Google (303 → `accounts.google.com`); no se usó como sustituto ni se pudo comparar.
5. **Paquete 1.0.7** (`download_id=10215`): **HTTP 500** el 2026-10-30T13:26:27Z; no disponible para comparar/atribuir cambios de 1.0.8 respecto de 1.0.7.
6. **Causa de la discrepancia**: `UNKNOWN`. No se puede distinguir entre note desactualizado, imagen regenerada, re-subida del ZIP u otra causa sin información del fabricante.
7. **Inventario**: se hashearon los artefactos clave (contenedor, nota, imagen, particiones, binarios APP/BASE, payload de actualización) y se listó el rootfs completo (7 516 paths con `7z l`); **no** se hasheó cada archivo del rootfs (no era necesario para el gate y multiplicaría el alcance). El listado completo derivado está en scratch, fuera de Git.
8. **Extracción**: lectura de particiones por extracción a scratch con `7z` y `debugfs`/`dumpe2fs`/`mtools` (sólo lectura); **sin** montar, sin `sudo`, sin `dd` a dispositivos. Los metadatos de archivos internos provienen de `debugfs`/`tar -t` y pueden diferir de la semántica de un montaje real.
9. **Instrucciones**: el tutorial oficial es de 2024-07 (genérico) y el PDF detallado es de 1.0.6; ninguno es específico de 1.0.8. Cualquier diferencia operativa de 1.0.8 no documentada es `UNKNOWN`.

## 8. Handoff para la Fase B (#376) — condicionado al desbloqueo

**La Fase B no debe iniciar** mientras el estado sea `ARTIFACT_UNAVAILABLE`. Si el orquestador/dueño desbloquea el gate (paquete reemitido con hash coincidente o copia auténtica aportada por el dueño), este es el material ya custodiado:

Scratch (fuera de Git, no publicado): `/home/pablo/desa/x6200/fw108-scratch/`

| Ruta | Qué es |
|---|---|
| `vendor/x6200udimg-260820001.zip` | ZIP oficial descargado (371 310 845 B, `ca0226c7…`) |
| `vendor/x6200-fw108-vendor/x6200udimg-260820001/sdcard.img` | imagen extraída (856 686 592 B, `a968d278…`) |
| `vendor/x6200-fw108-vendor/x6200udimg-260820001/ReleaseNote-260820001(EN_CHS).txt` | release note (759 B, `2e164869…`) |
| `ext108/parts/1.img` | partición rootfs ext4 extraída (419 430 400 B, `3c475b16…`) |
| `ext108/fs108/x6200_ui_v100` | GUI vendor APP (1 684 984 B, `26623266…`), ELF ARM con símbolos |
| `ext108/fs108/x6100_ui_v100` | GUI X6100 legacy (1 211 056 B, `b972698a…`) |
| `ext108/fs108/x6200_mcu_20260820141208.xgf` | BASE opaco (524 288 B, `9a19f97a…`) |
| `ext108/fs108/emmc_sources/` | zImage, DTB, boot\*.scr, u-boot, install_emmc.sh (leídos, no ejecutados) |
| `ref106/` | material histórico 1.0.6 extraído: `parts/1.img`, `fs106/{x6200_ui_v100,x6100_ui_v100,x6200_mcu_20250111134652.xgf}`, listados, PDF |
| `comparisons/` | listados `rootfs.tar` 1.0.6/1.0.8, dumps de `strings`, listas de símbolos, hashes de `rootfs.tar`, listado completo del rootfs 1.0.8 |
| `vendor/` (capturas) | HTML de la página y blog, headers, log de descarga, tutorial PDF oficial |

Datos mínimos para #376: ELFs `x6200_ui_v100` (payload nuevo, atribución 1.0.8 interna) y `x6200_ui_v100` 1.0.6 (comparable auténtico, `b76331c5…`), ambos ELF ARM 32-bit not stripped; BASE `.xgf` nuevo `9a19f97a…` vs 1.0.6 `dca90387…` (ambos opacos); señales `slip_encode`/`slip_decode` en el binario nuevo (§4.5) como hipótesis de trabajo, **no** como formato demostrado. La regla del Epic sigue vigente: no se puede declarar `SLIP_PAYLOAD_PROVEN` por analogía ni por notas de release.

## 9. Reproducibilidad y logs

- `commands.log`: comandos reales, salidas relevantes y **exit codes** (descarga, hashes, validación, extracción, inventario, comparación, tutorial, checks).
- `extraction-method.md`: método de validación/extracción seguro (sin montar, sin `dd`, sin ejecutar) y criterios de riesgo.
- `manifest.json`: datos legibles por máquina (fuente, hashes calculados y declarados, gate, causa, custodia, rutas scratch).
- `inventory.csv`: inventario por artefacto (tamaño, SHA256, tipo, atribución, ruta scratch).

## 10. Condiciones de desbloqueo (gate A)

El gate pasa a `ARTIFACT_READY_FOR_RE` **sólo** si:

1. **El fabricante reemite/aclara el paquete 1.0.8** y el `sdcard.img` de la nueva descarga coincide con el `Image File SHA256` declarado en su release note; o
2. **El dueño aporta una copia auténtica** (p. ej. del canal oficial que le entregue Xiegu/Radioddity) cuyo `sdcard.img` coincida con el hash declarado; o
3. Existe una **aclaración escrita del fabricante** que documente el hash correcto del payload publicado y permita re-verificarlo.

Sin (1), (2) o (3): `ARTIFACT_UNAVAILABLE` (STOP) y Fases B/C/D bloqueadas. No hay waiver en esta fase.

## 11. Cumplimiento de seguridad (Epic #374)

- Cero acceso a la radio: sin WiFi/SSH/USB, sin I²C/UART, sin TX/PTT/ATU, sin flash/actualización/factory reset.
- Cero ejecución de scripts/binarios del paquete o del firmware (la GUI vendor y el `.xgf` no se ejecutaron; `install_emmc.sh`/`fmt_emmc.sh` se **leyeron**).
- Cero `sudo`, `mount`, `losetup`, `dd` a dispositivos. Las particiones se leyeron por extracción a scratch con `7z` y por herramientas de sólo lectura (`debugfs`, `dumpe2fs`, `sfdisk`, `mdir`, `mtype`, `pdfinfo`, `pdftotext`).
- Ningún binario propietario, ZIP/IMG, credencial, serial, IP, calibración ni audio fue subido a Git: este PR es **docs-only** (metadatos y hashes).
- El material `refs/vendor-fw` y el paquete 1.0.6 se hashearon en su ubicación original (sólo lectura), sin copiarlos ni modificarlos.
