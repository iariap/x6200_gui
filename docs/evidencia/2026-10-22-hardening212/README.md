# #212 — DATA ext4 segura en SD nueva y conservación de DATA existente

Hijo 1/4 de la épica [iariap/x6200_gui#211](https://github.com/iariap/x6200_gui/issues/211)
(hardening P1 sobre el assessment #194, hallazgos **F-01** y F-18). Contrato
cerrado: [iariap/x6200_gui#212](https://github.com/iariap/x6200_gui/issues/212).

| | |
| --- | --- |
| Fecha | 2026-10-22 |
| Repos | `iariap/x6200_gui` (evidencia, este commit) + `iariap/AetherX6200Buildroot` |
| Ramas | GUI `factory/hardening-212` · Buildroot `fix/rom-data-firstboot` |
| Base verificada | GUI `d4fb57f8` = `origin/lu2dsx/x6200-rom` · Buildroot `f23d14f9` = `lu2dsx/x6200-rom` |
| Commit Buildroot | `486ae98fc99babb9c6c75b0d6f4e2498ebfa4676` (`486ae98`) — S01create_data + banco de tests (ver §8) |
| Estado | implementado y verificado en HOST + contenedor ARM real (qemu-user); **sin fusionar**; QA en radio `PENDING_HARDWARE` |

## 1. Qué cambia (y qué no)

| Archivo | Repo | Cambio |
| --- | --- | --- |
| `br2_external/board/X6200/linux/rootfs-overlay/etc/init.d/S01create_data` | Buildroot | **reescrito** (única pieza de runtime que cambia) |
| `br2_external/tests/test_S01create_data.sh` | Buildroot | **nuevo**: banco hermético (no se instala en la rootfs: no está bajo `rootfs-overlay/`) |
| `br2_external/tests/README.md` | Buildroot | **nuevo**: cómo se corre el banco, qué aísla y qué no prueba |
| `docs/evidencia/2026-10-22-hardening212/README.md` | GUI | **nuevo**: esta evidencia (no toca runtime ni tests de la GUI) |

No se toca: `etc/fstab` (sigue declarando `/dev/mmcblk0p3 /mnt ext4 rw 0 2`),
`genimage.cfg` (sigue con p1 BOOT + p2 rootfs y **sin** p3), ningún otro
`S*`/init, ni la GUI. No se agregan servicios, ni selector de filesystem, ni
fsck desatendido, ni migrador FAT.

Mecanismo del arreglo (contrato §1–§5, mapeado al código):

| Cláusula del contrato | Dónde está |
| --- | --- |
| §1 `blkid -L DATA` antes de particionar; sólo se acepta `/dev/mmcblk0p3` con `ext4`\|`vfat`; si ya está montada se verifica fuente/tipo/`rw`; si no, `mount -t "$data_type"`; se anuncia `DATA ready (ext4)`/`DATA ready (vfat legacy)` | `start()` líneas 120-165 (rama "existing DATA") |
| §2 p3 sin label / label en otro disco / filesystem desconocido → abortar con `DATA unavailable: existing partition requires inspection`, sin `mkfs`/`mkpart`/otra partición | líneas 114-190 |
| §3 SD nueva: MBR `msdos` + p1/p2 presentes y sin p3/p4; sólo el tramo libre final, alineado a 1 MiB, ≥ 64 MiB; nuevo índice 3; `partprobe`; espera del nodo (5×200 ms); sin firma previa; `mkfs.ext4 -F -m 0 -L DATA`; `blkid` tipo+label; `mount -t ext4`; verificación `rw` | líneas 192-300 |
| §4 fuera el `grep fat32` global y el `mkfs.vfat`; `fstab` ext4 intacto | verificado como check estático del banco |
| §5 toda falla → `exit != 0` + `DATA unavailable: <razón>` en stderr, nunca `DATA ready`; rc de `parted`/`partprobe`/`mkfs`/`mount`/`blkid`/`blockdev` validados | `fail()` + `trap on_exit EXIT` (cubre fallas inesperadas) |

Extra de robustez respecto del contrato (no lo contradice, y es lo que hace
falsable "sólo se formatea la p3 recién creada por este proceso"): antes del
`mkfs` se exige `blockdev --getsize64` == tamaño pedido en sectores, y se
verifica que p1/p2 de la tabla quedaron idénticos tras el `mkpart`.

## 2. Método y laboratorio

Todo se midió en **HOST** y en un **contenedor privado y desechable** con
userland ARM real (`armv7l`/`armhf` bajo `qemu-user`/binfmt del host):

```
uname -m: armv7l        dpkg arch: armhf      bash: GNU bash 5.2.15 (arm-unknown-linux-gnueabihf)
/bin/bash: ELF 32-bit LSB pie executable, ARM, EABI5 ... interpreter /lib/ld-linux-armhf.so.3
parted (GNU parted) 3.5          blkid from util-linux 2.38.1      mke2fs 1.47.0
```

Las versiones de `parted` (3.5) y util-linux (2.38.1) son **las mismas** que las
del target (Buildroot 2022.11 del repo), y las herramientas que usa el script
(`blkid`, `findmnt`, `mount`, `blockdev`, `parted`, `partprobe`, `mkfs.ext4`)
están todas en la rootfs real de la ROM — verificado contra el listado del
artefacto del assessment: `./sbin/blkid`, `./bin/findmnt`, `./bin/mount`,
`./sbin/blockdev`, `./usr/sbin/parted`, `./usr/sbin/partprobe`,
`./sbin/mkfs.ext4` son binarios de util-linux/e2fsprogs (no applets busybox) y
`./bin/sh -> bash` (por eso el shell objetivo es bash en modo POSIX).

Aislamiento del banco (nunca un dispositivo del host ni la SD del owner):

* reemplaza `/dev` por un tmpfs privado del contenedor (sin nodo de SD);
* cada caso arma una **copia** de una imagen de tarjeta (`/lab/card.img`, misma
  geometría medida del artefacto: p1 BOOT vfat @2048 +65536 sectores, p2 rootfs
  ext4 @67584 +1048576 sectores, disco de 1116160 sectores = 545 MiB) sobre un
  loop device propio;
* publica `/dev/mmcblk0`/`pN` con loops con `--offset`/`--sizelimit` sobre esa
  copia (equivalente determinista de lo que publica udev) y **oculta** los nodos
  `/dev/loopN`, así `blkid -L`/`parted`/`mount` sólo pueden ver la copia;
* al cerrar cada caso (y en el `trap EXIT`) desmonta, hace `losetup -d` y borra
  nodos; el resumen falla si queda algún loop asociado;
* las herramientas son las reales y un PATH con shims transparentes registra cada
  invocación en un *journal* por caso (los shims sólo loguean y `exec`ean el
  binario real);
* el `fstab` de la ROM se instala en `/etc/fstab` del contenedor, para que el
  banco pruebe el camino integrado script+fstab (es donde vivía F-01).

**No es** un arranque real de kernel/init, ni la radio, ni la rootfs Buildroot
objetivo (ver §6/§7).

## 3. Matriz de casos (condición → comandos emitidos → efecto)

Corrida final del banco contra el script del PR: **17/17 casos OK, 0 checks con
FAIL, `RESULTADO: PASS`, exit 0**. Los "comandos emitidos" salen del journal real
de cada caso (no de la lectura del código).

| Caso | Condición | Comandos emitidos (journal) | Efecto medido |
| --- | --- | --- | --- |
| `estaticos` | el archivo del PR | — (grep/sintaxis) | sin `fat32`, sin mkfs vfat, `mkfs.ext4 -F -m 0 -L DATA "$DATA"` exacto, fstab ext4 intacto, init script 0755, `sh -n` (bash POSIX) y `bash -n` OK |
| `fresh-first-boot` | SD nueva, p1+p2 + 256 MiB libres | `blkid -L DATA`; `parted -m -s /dev/mmcblk0 unit s print free`; `parted -s ... print`; `blkid -o value -s TYPE` p1/p2; `findmnt … /mnt`; `parted -m -s -a none … mkpart primary ext4 1116160s 1640447s`; `partprobe /dev/mmcblk0`; `parted … print free`; `blockdev --getsize64 /dev/mmcblk0p3`; `blkid /dev/mmcblk0p3`; `mkfs.ext4 -F -m 0 -L DATA /dev/mmcblk0p3`; `blkid -o value -s TYPE/LABEL`; `mount -t ext4 /dev/mmcblk0p3 /mnt`; `findmnt … /mnt` | `rc=0`, stdout **exactamente** `DATA ready (ext4)`; p3 `ext4`+label `DATA`; `/mnt` montado `rw` desde `/dev/mmcblk0p3`; tabla `1 2 3`; **regiones p1 y p2 idénticas** (SHA256) |
| `second-boot-idempotent` | misma tarjeta, segundo arranque | `blkid -L DATA`; `blkid -o value -s TYPE /dev/mmcblk0p3`; `findmnt … /mnt`; `mount -t ext4 /dev/mmcblk0p3 /mnt` | `rc=0`, `DATA ready (ext4)`; **sin `parted`, sin `partprobe`, sin `mkfs`**; el archivo escrito en el primer arranque sigue legible; p1/p2 idénticas |
| `existing-ext4-content` | p3 ext4 con contenido, symlink e ids | `blkid -L DATA`; `blkid -o value -s TYPE`; `findmnt`; `mount -t ext4 …` | `rc=0`, `DATA ready (ext4)`; sin `parted`/`mkfs`; `/mnt/params.db` intacto, symlink `link-params -> params.db` intacto, `keep.bin` con `1234:5678:640`, `backups/backup.tgz` presente; p3 sigue `ext4`+`DATA`; p1/p2 idénticas |
| `legacy-vfat-preserved` | p3 vfat legacy (tarjeta anterior a la conversión) | `blkid -L DATA`; `blkid -o value -s TYPE`; `findmnt`; `mount -t vfat /dev/mmcblk0p3 /mnt` | `rc=0`, `DATA ready (vfat legacy)`; sin `parted`/`mkfs`; contenido legible; **región de p3 byte-idéntica** (SHA256) |
| `p3-without-label` | p3 existe sin label DATA | `blkid -L DATA`; `parted … print free` | `rc=1`, `DATA unavailable: existing partition requires inspection (partition 3 already exists on /dev/mmcblk0)`; sin `mkpart`/`mkfs`/`partprobe`; región de p3 intacta |
| `p3-unsupported-fs` | p3 con label DATA pero `ext3` | `blkid -L DATA` | `rc=1`, `… requires inspection (the filesystem on /dev/mmcblk0p3 is 'ext3')`; sin `mkfs` |
| `label-on-other-device` | label DATA en `/dev/mmcblk1p1` (segundo disco) | `blkid -L DATA` | `rc=1`, `… requires inspection (the DATA label is on /dev/mmcblk1p1, not on /dev/mmcblk0p3)`; no particiona la tarjeta del ROM; p1/p2/tramo libre intactos |
| `p3-mounted-readonly` | p3 con DATA montada `ro` en `/mnt` | `blkid`; `findmnt … /mnt` | `rc=1`, `DATA unavailable: /mnt is mounted read-only`; sin `mount`/`mkfs`; sigue `ro`; contenido intacto |
| `mount-fails` | `/mnt` no es un directorio | `blkid`; `findmnt`; `mount -t ext4 …` | `rc=1`, `DATA unavailable: cannot mount /dev/mmcblk0p3 (ext4) on /mnt (mount exited 32)`; sin `parted`/`mkfs`; p3 sigue `ext4`+`DATA`; `/mnt` no queda montado |
| `parted-fails` | disco sin tabla de particiones; y sin nodo `/dev/mmcblk0` | `blkid -L DATA`; `parted … print free` | `rc=1`, `DATA unavailable: cannot read the partition table of /dev/mmcblk0 (parted exited 1)`; sin `mkpart`/`mkfs`; primer MiB/p1 intactos |
| `p3-node-never-appears` | el kernel nunca publica `/dev/mmcblk0p3` | `blkid`; `parted … print free`; `parted … print`; `blkid` p1/p2; `findmnt`; `mkpart … 1116160s 1640447s`; `partprobe` | `rc=1`, `DATA unavailable: /dev/mmcblk0p3 did not appear after creating partition 3`; **sin `mkfs` ni `blockdev`**; p1/p2 intactas y la región de p3 sigue en cero (partición creada, sin filesystem) |
| `free-under-64mib` | sólo 32 MiB libres al final | `blkid`; `parted … print free`; `parted … print`; `blkid` p1/p2 | `rc=1`, `DATA unavailable: only 32 MiB of aligned free space after /dev/mmcblk0p2, need 64 MiB`; **aborta antes de tocar la tabla**; p1/p2/tramo intactos |
| `no-free-region` | tarjeta del tamaño exacto de la imagen (545 MiB) | idem | `rc=1`, `DATA unavailable: no free region reaches the end of /dev/mmcblk0: no room for /dev/mmcblk0p3 without touching p1/p2`; nada escrito |
| `exact-64mib` | 64 MiB justos al final | como `fresh-first-boot` con `mkpart … 1116160s 1247231s` | `rc=0`, `DATA ready (ext4)` (el mínimo del contrato alcanza y es inclusivo) |
| `p4-instead-of-p3` | tabla `1,2,4` (índice distinto de 3) | `blkid`; `parted … print free` | `rc=1`, `… requires inspection (partition 4 already exists on /dev/mmcblk0)`; sin `mkpart`/`mkfs`; región de p4 intacta |
| `data-on-p3-with-p4` | DATA válida en p3 y p4 presente | `blkid`; `blkid TYPE`; `findmnt`; `mount -t vfat …` | `rc=0`, `DATA ready (vfat legacy)`; sin `parted`/`mkfs`; p3 legible; **p3 y p4 byte-idénticas** (el script jamás toca p4) |

Negativos, mensaje exacto de stderr (todos exit≠0 y sin `DATA ready` en stdout):

```
p3-without-label       DATA unavailable: existing partition requires inspection (partition 3 already exists on /dev/mmcblk0)
p3-unsupported-fs      DATA unavailable: existing partition requires inspection (the filesystem on /dev/mmcblk0p3 is 'ext3')
label-on-other-device  DATA unavailable: existing partition requires inspection (the DATA label is on /dev/mmcblk1p1, not on /dev/mmcblk0p3)
p3-mounted-readonly    DATA unavailable: /mnt is mounted read-only
mount-fails            DATA unavailable: cannot mount /dev/mmcblk0p3 (ext4) on /mnt (mount exited 32)
p3-node-never-appears  DATA unavailable: /dev/mmcblk0p3 did not appear after creating partition 3
free-under-64mib       DATA unavailable: only 32 MiB of aligned free space after /dev/mmcblk0p2, need 64 MiB
no-free-region         DATA unavailable: no free region reaches the end of /dev/mmcblk0: no room for /dev/mmcblk0p3 without touching p1/p2
p4-instead-of-p3       DATA unavailable: existing partition requires inspection (partition 4 already exists on /dev/mmcblk0)
```

Tabla de la copia del caso `fresh-first-boot` después del arranque simulado
(`parted -m -s card.img unit s print free`, evidencia cruda en
`cases/fresh-first-boot/parted.txt`):

```
BYT;
/lab/card.img:1640448s:file:512:512:msdos::;
1:32s:2047s:2016s:free;
1:2048s:67583s:65536s:fat16::lba;      <- p1 BOOT (región idéntica antes/después)
2:67584s:1116159s:1048576s:ext4::;    <- p2 rootfs (región idéntica antes/después)
3:1116160s:1640447s:524288s:ext4::;   <- p3 DATA creada por el script
```

### El banco discrimina (no es un PASS vacío)

El mismo banco, corrido contra el **S01create_data de la base**
(`f23d14f9`, 1810 bytes, `md5 b8856cad02d057801ae148bc24288dd0`): **17/17 casos
FAIL, 70 checks con FAIL**. En particular el primer arranque de SD nueva, con el
script viejo, emite `mkfs`/`mkfs.vfat` y deja `rc=32` sin montar `/mnt`
(`blkid,mkfs,mkfs.vfat,mount,parted,partprobe`) — que es exactamente el hallazgo
**F-01** ("DATA nace vfat, el primer boot no monta") que este PR cierra.
```
$ docker run --rm --privileged -e LAB=/labbase -e SCRIPT=/base/S01create_data … \
      h212-x6200-data-arm bash /repo/br2_external/tests/test_S01create_data.sh
== resumen por caso
estaticos                FAIL  rc=rc=0
fresh-first-boot         FAIL  rc=rc=32 blkid,mkfs,mkfs.vfat,mount,parted,partprobe
second-boot-idempotent   FAIL  rc=rc=0 blkid,parted
existing-ext4-content    FAIL  rc=rc=0 blkid,mount
legacy-vfat-preserved    FAIL  rc=rc=0 blkid,mount
p3-without-label         FAIL  rc=rc=32 blkid,mkfs,mkfs.vfat,mount,parted,partprobe
p3-unsupported-fs        FAIL  rc=rc=0 blkid,mount
label-on-other-device    FAIL  rc=rc=0 blkid,mount
p3-mounted-readonly      FAIL  rc=rc=0 blkid
mount-fails              FAIL  rc=rc=32 blkid,mount
parted-fails             FAIL  rc=rc=0 blkid,parted
p3-node-never-appears    FAIL  rc=rc=32 blkid,mkfs,mkfs.vfat,mount,parted,partprobe
free-under-64mib         FAIL  rc=rc=32 blkid,mkfs,mkfs.vfat,mount,parted,partprobe
no-free-region           FAIL  rc=rc=0 blkid,parted
exact-64mib              FAIL  rc=rc=32 blkid,mkfs,mkfs.vfat,mount,parted,partprobe
p4-instead-of-p3         FAIL  rc=rc=32 blkid,mkfs,mkfs.vfat,mount,parted,partprobe
data-on-p3-with-p4       FAIL  rc=rc=0 blkid,mount

casos: 17, con fallas: 17, checks con FAIL: 70
RESULTADO: FAIL
```

(El contraste importa: con el script del PR el mismo banco da `casos: 17, con
fallas: 0, checks con FAIL: 0, RESULTADO: PASS`.)

## 4. Identidad del artefacto (overlay → rootfs) y contra el artefacto real

* Aplicando el overlay como lo hace `target-finalize` (`rsync -a` del
  `rootfs-overlay/` sobre el target): el `S01create_data` instalado y el `fstab`
  son **idénticos** a los del repo, con modo preservado (0755 el init script,
  0644 el fstab):

```
23bb37020d1adc9df24786193d406a713a958fb91e292faa69f2cbeb5d465fa2  S01create_data  (fuente)
23bb37020d1adc9df24786193d406a713a958fb91e292faa69f2cbeb5d465fa2  S01create_data  (instalado en el target)
31d158a78754aef665a5c86315c19d79286acaf64951c50be5eebb19714a2ad8  fstab           (fuente)
31d158a78754aef665a5c86315c19d79286acaf64951c50be5eebb19714a2ad8  fstab           (instalado en el target)
```

* Contra el **artefacto real** del assessment #194 (rootfs de la ROM): su
  `/etc/init.d/S01create_data` mide **1810 bytes** con modo 0755 y su `/etc/fstab`
  **368 bytes**, que son exactamente el tamaño y el modo del script y del fstab
  del tip base en el repo (`git show f23d14f9:… | wc -c` = 1810 y 368). Es decir:
  lo que se instala en la rootfs es el archivo del repo (verificado en el
  artefacto), y el PR cambia ese mismo archivo. No se construyó la rootfs
  completa: ver §7 (BLOCKED con su razón).
* Sello de fuentes de la GUI (`tools/fuente-md5.sh`): `18f5458db571b3056efdfa54c0013a05`
  (idéntico al de la base: este PR no toca `src/` ni `lvgl/`).

## 5. Gates (salida real)

| Gate | Resultado |
| --- | --- |
| banco focalizado `br2_external/tests/test_S01create_data.sh` (contenedor ARM privado) | **PASS: 17/17 casos, 0 FAIL**, `RESULTADO: PASS`, exit 0 |
| banco contra la base (discriminación) | **FAIL 17/17, 71 checks** (F-01 reproducido) |
| `sh tests/native/run_native_tests.sh` (GUI) | `138 comprobaciones, 0 fallos` (rc 0) |
| `sh tests/run_all_tests.sh --quick` (GUI) | `17 OK, 0 fallos` (exit 0). SKIP: `js8` (por `--quick`), `wspr`/`sstv`/`wefax` (la imagen `x6200-uiharness` del host no trae `fftw3.h`/`sndfile.h`, SKIP del propio runner), `webserver` (ver nota) |
| `python3 tests/audita-pruebas.py` (GUI) | `OK: estructura y evidencia en orden` (rc 0; 31 puntos "para revisar a mano" informativos) |
| `scripts/apps/x6200/check-matrix.sh origin/lu2dsx/x6200-rom HEAD <worktree>` | `rc=0`, todo SKIP (cambio sólo de `docs/**`): firmware/nativo, identidad del build, temas, ruff/pytest del arnés A/B, gates `x6200-ab` (themes+contracts), `bash -n` de .sh tocados, determinismo del arnés y banco QEMU — cada uno con su motivo ("sin cambios en …"). Salida completa: `==> [SKIP] …` × 9, sin ningún FAIL |
| CI de `AetherX6200Buildroot` sobre el PR | **no aplica**: `.github/workflows/main.yml` corre sólo en `tags` y `workflow_dispatch`, no en PRs (el gate real es el banco privado) |
| gates de UI (no aplican) | `tests/check_harness_determinism.sh`, `check_harness_natural.sh`, `check_waterfall_incremental.sh`, `tests/check_bootanim.sh`, `check_drawbuf_partial.sh`: **SKIP** — este PR no toca `tools/ui_harness/**`, `src/**`, `lv_conf*.h` ni los temas de la imagen (es lo mismo que reporta `check-matrix` por paths) |
| `tools/verifica-firmware-port.py` (no aplica) | **SKIP** — no se toca el port ni el artefacto de firmware; el rearmado reproducible GUI+Buildroot es gate de integración del orquestador (épica #211) |

Nota sobre `webserver`: `run_all_tests.sh` corre ese paso sólo si existe la
imagen `x6200-buildroot`, y lo hace montando el volumen **`x6200-build-output`
del owner**. La política de laboratorio de la épica prohíbe usar recursos de
build de otros, así que se corrió la suite con un shim de PATH que hace fallar
sólo `docker image inspect x6200-buildroot`, de modo que el propio runner reporta
`webserver` como omitido. El shim no puede fabricar ningún PASS (sólo deniega).

## 6. Limitaciones de esta evidencia

* No es un boot real de kernel/init: los loop devices, los montajes y la
  publicación de `/dev/mmcblk0pN` los hace el banco (en la radio los hace el
  kernel + udev). No hay RF, ni SD física, ni radio.
* El userland es ARM real (Debian armhf, glibc, bash 5.2) con las **mismas
  versiones upstream** que el target para `parted`/util-linux, pero no es la
  rootfs Buildroot del `X6200_defconfig`: no reemplaza la verificación de la
  rootfs generada (§7).
* En algunos casos la tabla se editó con `parted` sobre el archivo (no vía el
  script) para armar la condición inicial: equivalente a una tarjeta ya escrita,
  no idéntico a un `dd` de la imagen.
* Validación en radio con **SD virgen**: `PENDING_HARDWARE` (pipeline de QA
  remoto separado de la épica).

## 7. Prohibiciones respetadas y BLOCKED

* No se hizo SSH a la radio, ni flash, ni deploy, ni TX/RF, ni formateo de la SD
  del owner, ni releases, ni scripts de QA físico. No se usó `sudo` interactivo
  (el banco necesita root **dentro** de su contenedor privilegiado propio, no en
  el host). No se usaron ni compartieron `banco-apps`, `banco-audio`,
  `x6200-build-output`, rootfs ni directorios de compilación de otros; el
  contenedor, la imagen docker y los directorios de trabajo son privados de esta
  corrida. No se pararon ni reutilizaron contenedores ajenos.
* **BLOCKED — rootfs completa desde `X6200_defconfig`**: no se generó el rootfs
  completo. Razón: requiere inicializar el submódulo `buildroot` (2022.11.x,
  cc34b5b2) y construir toolchain + kernel + u-boot + ~200 paquetes (horas de
  cómputo y varios GB) en un directorio de build **propio**; la épica prohíbe
  explícitamente reutilizar el build output de otros y hay 4 hijos corriendo en
  paralelo. Sustituto usado: la verificación de aplicación del overlay (§4) más
  la comparación contra el listado del artefacto real de la ROM, que sí prueban
  que el archivo instalado es el del repo con su modo, y que las herramientas que
  el script invoca existen en la rootfs objetivo con el mismo upstream.

## 8. Commits

* **Buildroot** (`fix/rom-data-firstboot`, base `f23d14f9`): `486ae98fc99babb9c6c75b0d6f4e2498ebfa4676`
  — `br2_external/board/X6200/linux/rootfs-overlay/etc/init.d/S01create_data`
  (reescrito), `br2_external/tests/test_S01create_data.sh` y
  `br2_external/tests/README.md` (nuevos). Sin push.
* **GUI** (`factory/hardening-212`, base `d4fb57f8`): este commit — sólo
  `docs/evidencia/2026-10-22-hardening212/README.md`. Sin push.
* Par GUI↔Buildroot: la GUI **no** embebe ninguna fuente del script y el
  Buildroot no embebe fuentes de la GUI, así que el par no tiene un hash
  compartido: la identidad que sí se verifica es sello de fuentes de la GUI
  (`18f5458db571b3056efdfa54c0013a05`, sin cambios) + SHA256 del
  `S01create_data`/`fstab` del PR (§4). El par de SHA se informa al orquestador.

## 9. Riesgos residuales y pendientes

1. `partprobe`/nodo a mitad de camino: si la partición se crea pero el nodo no
   aparece (o `mkfs` falla), el script aborta **sin formatear**; en el arranque
   siguiente la p3 existe sin label y el script aborta con `existing partition
   requires inspection`, que es el comportamiento pedido por el contrato (nada de
   auto-recuperación destructiva) pero deja la radio sin `/mnt` hasta que un
   humano inspeccione. Documentado en el encabezado del script.
2. Requiere que la tarjeta tenga **más de 545 MiB** (el tamaño de `sdcard.img`) y
   al menos 64 MiB alineados al final; si no, no crea DATA (aborta explicando).
3. `etc/fstab` sigue declarando `/mnt` como `ext4`: una DATA **vfat legacy** la
   monta este script con `-t vfat`, pero un `mount -a` (que hoy nadie ejecuta en
   el arranque) fallaría para ese caso. No se cambió el fstab porque el contrato
   lo mantiene; queda anotado para el hijo que toque update/backup.
4. `DATA ready (ext4)`/`(vfat legacy)` es el contrato de stdout de este script; la
   GUI/daemons no dependen de ese texto (el estado real es el montaje de `/mnt`).
5. QA en radio con SD virgen y con la SD del owner: `PENDING_HARDWARE` (pipeline
   separado, con autorización y ventana del owner). Este PR **no** es `GO` de
   release.
