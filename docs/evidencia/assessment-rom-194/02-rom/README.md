# Unidad #196 — Integridad de ROM, persistencia y confiabilidad ARM/QEMU

- **Campaña:** assessment pre-release ROM #194 (`iariap/x6200_gui`), unidad 2/5.
- **Ejecutada:** 2026-10-21 (UTC) por un agente ejecutor de evidencia; no se implementó ni
  corrigió firmware, tests, CI, budgets, golden ni excepciones.
- **Carpeta:** `docs/evidencia/assessment-rom-194/02-rom/` (rama `assessment/196-evidence` → PR a
  `assessment/rom-194`). Medición separada en `assessment-194/clone-gui` (detached en el candidato)
  y copias privadas en `assessment-194/scratch-196/`.
- **Alcance real:** auditoría estática del artefacto de #195, ejecución del binario ARM del
  candidato en un **QEMU_USER privado** (no el banco compartido), 100 ciclos de estrés RX, 1 h de
  reposo y negativas de protocolo/DB. Los **tres barridos completos de apps del banco quedaron
  BLOCKED** porque `banco-apps` está ocupado por otra sesión (ver §2). No se tocó la radio
  (192.168.0.236), no se desplegó, flasheó, transmitió ni se cortó energía.

## 0. Resumen de estados

| Escenario | Estado | Capa | Qué se midió |
|---|---|---|---|
| ROM-01 procedencia/layout/rootfs/dependencias | **PASS** | HOST | hashes, sfdisk, U-Boot, boot.vfat, `e2fsck -fn`, inodos/bloques, `readelf`/`ld.so --list` |
| ROM-02 init/servicios/prune offline | **PASS** (prune + 26/26 parsean) | HOST/QEMU_USER | 26 init scripts, intérprete real `/bin/sh` (bash 5.2.15), workflow de prune ejecutado literal |
| ROM-03 DATA y persistencia | **FAIL** (tarjeta nueva: DATA vfat y primer boot sin montar); **PASS** (DATA ext4/vfat existente) | HOST (código) | fstab, `S01create_data`, `genimage.cfg`, DB por defecto |
| ROM-04 update/rollback | **FAIL** (empaquetador y owner del overlay) + **PASS** (aplicación post-overlay) | HOST/QEMU_USER | scripts de release ejecutados como root en contenedor privado, tgz, backup, `test_update.sh` |
| ROM-05 banco sano | **PASS** en QEMU_USER privado; **BLOCKED** el banco compartido | QEMU_USER | identidad, handshake, captura, DB sintética |
| ROM-06 tres barridos completos (25 sin `off`, FT4 alias) | **BLOCKED** | — | dependencia exacta: `banco-apps` ocupado / scripts con nombre y volumen fijos |
| ROM-07 100 ciclos RX | **PASS** (QEMU_USER, parcial) | QEMU_USER | 700 comandos, 0 divergencias, 0 crashes |
| ROM-07 1 h de reposo | **PASS** en proceso nuevo (intento A interrumpido externamente a los 31 min) | QEMU_USER | 3.605 s, 768 `get`, RSS/fds planos, 0 CRASHLOG/overflow |
| ROM-08 negativas de protocolo | **PASS** | QEMU_USER | 12 casos, `err`/sin cambio, 0 crashes |
| ROM-08 DB corrupta/read-only/faltante | **FAIL** | QEMU_USER | SIGSEGV determinista (mismo PC) en los tres casos |
| ROM-08 DB llena / periféricos reales | **BLOCKED** (DB llena) / **PENDING_HARDWARE** (RF, red, GPS, BT, audio) | — | camino de código / hardware |

**No hay aprobación de release**: el informe contiene FAIL y BLOCKED. Los defectos se documentan,
no se reparan, y no se abrió ningún ticket.

## 1. Base, procedencia y artefacto medido

| Elemento | Valor |
|---|---|
| GUI candidato (medido) | `c888308eede71205605c06383432ee7e165f202e` |
| Rama de desarrollo | `lu2dsx/x6200-rom` |
| Gitlink LVGL | `74d0a816440eea53e030c4f1af842a94f7ce3d3` (v8.3.11) |
| Buildroot | `d084e2870a00b64d9c57474e7eceeef80f43f25b` (`iariap/AetherX6200Buildroot`) |
| Submódulo `buildroot` | `cc34b5b2abf2588f0ef55e46a4fe85a60c19ba1f` (2023.02.11) |
| `fuente-md5` del candidato | `964193ae9306b5127845cec44a57d52c` = checkout = staged = embebido |
| Artefacto | `assessment-194/clone-buildroot/build/images/` (copia durable local, fuera de Git) |
| GUI en la rootfs | 13.407.624 B, md5 `1f998c46909307397525c975a712adfd`, sha256 `7e2e9331c4cbc51727a098996d98e263c45dd57cb8fff8d1f2aea2964c3849c5` |

Todos los SHA256 del manifiesto de #195 se **revalidaron antes de extraer**:
[`logs/rom01/sha256.log`](logs/rom01/sha256.log). Coinciden exactamente:

| Archivo | Bytes | SHA256 |
|---|---:|---|
| `sdcard.img` | 571.473.920 | `d0ff4435ad1d068e25bf2c357a4e93478463b187e7745a9a54844488e44e33a5` |
| `rootfs.tar` | 242.790.400 | `7c26c76b6fc27bc12dc8d5e012e51d5c2c0568fa72a8e02cad5432dfcd36a2dd` |
| `rootfs.ext4` (→ `rootfs.ext2`) | 536.870.912 | `9365b52f9aef33e0623ae8271aaf415483fe72e1afbc8ef99f93e15437bc620a` |
| `boot.vfat` | 33.554.432 | `914f07eb00d1e5afee56ab795ce6b7b1d4410d774a3534881da494319c30b17d` |
| `zImage` | 5.567.760 | `660c94c6f7231e7dc262fefb295aae1a8dfcf9b9c2b46cdc1eced2c3296bcc14` |
| `u-boot-sunxi-with-spl.bin` | 556.112 | `dd96b0ca1eda2c98b318ed35f284d873e64eca795a51f1d9b944115a77de0381` |
| `sun8i-r16-x6200.dtb` | 26.544 | `4d73f8a5d5f846c21e4ac30550a5811443a01666f9ee57880b5316e78627cc38` |

Se usó el artefacto de #195, **no** el ROM del orquestador (`rom-build`, tip `50bf8ab0`,
GUI `3b22a03a…`): ése no es el candidato.

### 1.1 Divergencia del tip — STALE

Al iniciar la unidad el tip de `lu2dsx/x6200-rom` era `50bf8ab0` (PR #200, boot animation). Al
revalidar antes del PR el tip había avanzado a **`0d00e7fcaad5ad8c1423aaaff41efd2543042af2`**
(2 commits más: `9ed25fa9` borra el u-boot prebuilt del repo; `0d00e7fc` QA de arranque del tip y
reparación de geometría MBR de p3). El candidato `c888308e` sigue siendo ancestro del tip
(`git merge-base --is-ancestor` = 0) y **este dossier mide exactamente `c888308e`**.

Frente al candidato, el tip tiene **24 archivos de producción** fuera de `docs/`:
`src/bootanim/*`, `src/boot_splash.*`, `src/main.c`, `src/CMakeLists.txt`, `CMakeLists.txt`,
`rootfs/etc/init.d/S01bootanim`, tests de `tests/bootanim/`, `.github/workflows/{main,boot-rootfs}.yml`
y `u-boot/u-boot-sunxi-with-spl.bin` (borrado). Detalle: [`logs/rom00/refs.log`](logs/rom00/refs.log).

**Declaración STALE:** todo resultado de este informe es válido para `c888308e`, **no** para
`50bf8ab0` ni `0d00e7fc`. Si se adopta el tip hay que repetir: build ARM/ROM e identidad de
artefactos, layout/MBR y montaje de DATA, prune del rootfs, inventario de init, arranque real,
estrés/reposo (el `main.c` cambia) y empaquetado de update (cambia el workflow `main.yml` y
desaparece el u-boot prebuilt). El hallazgo de hardware del tip (p3 512 KiB corta, `/mnt` sin
montar, corregido con `parted resizepart 3 100%`) es **evidencia externa del tip**, no del
candidato; se cita en §5.2 como contexto.

### 1.2 Reproducibilidad del par congelado (defconfig)

El par congelado **no compila el GUI desde cero** con `X6200_defconfig`: el defconfig fija
`BR2_PACKAGE_PULSEAUDIO_DAEMON=y` sin `BR2_PACKAGE_PULSEAUDIO=y`
(`br2_external/configs/X6200_defconfig`, línea 54) y no
habilita `BR2_PACKAGE_LIBSNDFILE`; kconfig descarta el símbolo y el `configure` del GUI falla por
`sndfile`, `libpulse`, `libpulse-simple`. El artefacto de #195 se construyó con
`BR2_PACKAGE_PULSEAUDIO=y` + `BR2_PACKAGE_LIBSNDFILE=y` sólo en el `.config` privado
(desviación declarada; el defconfig y el `.mk` no se tocaron). Además, el `.config` final terminó
con `# BR2_PACKAGE_PULSEAUDIO_DAEMON is not set` (olddefconfig), lo que confirma la
inconsistencia interna del defconfig. Evidencia:
[`logs/rom00/refs.log`](logs/rom00/refs.log), hashes en `artifact-manifest.json`.
**No se afirma reproducibilidad bit a bit** (no se compiló dos veces el mismo par).

## 2. Aislamiento, recursos y bancos

- Todo el trabajo vive en `assessment-194/`: `clone-gui` (detached `c888308e`, árbol limpio),
  `clone-buildroot` (`d084e287`), `scratch-196/` y `wt-assessment-196/`.
- **No** se usó el volumen compartido `x6200-build-output`/`x6200-build-cache`, ni `x6200_qa`,
  `buildroot_new`, `rom-build`, ni la radio, ni scripts de radio.
- **`banco-apps` estaba ocupado** durante toda la unidad: contenedor `banco-apps` corriendo con
  chroot de otra sesión (`/home/pablo/desa/asf/state/worktrees/x6200/at190-chroot`), imagen
  `x6200-buildroot`. No se usó ni se detuvo. `banco-audio` no existía. Los scripts
  `tools/captura-banco.sh` y `tools/barrido-apps-banco.sh` usan nombre fijo `banco-apps` y el
  volumen compartido `x6200-build-output`, así que **no son ejecutables sin violar el aislamiento**.
- `sudo` **no está disponible** en este entorno (`sudo: The "no new privileges" flag is set`), lo
  que bloquea el camino de update que exige root (§6).
- Contenedor propio para QEMU_USER: `assessment196-qemu` (imagen `x6200-buildroot`,
  `sha256:b3e76013…`, `QEMU_CPU=cortex-a7`), montando sólo `scratch-196/rootfs/tree`. Sin volúmenes
  compartidos. Host: 16 núcleos, 89 GB RAM, `-j8` máximo; qemu-arm 10.2.2, e2fsprogs 1.47.3,
  util-linux 2.41.5. Entorno completo: [`capturas/host-env.json`](capturas/host-env.json).

## 3. ROM-01 — Procedencia, layout, rootfs y dependencias (PASS)

### 3.1 Layout y arranque

`sfdisk --json` y `parted` ([`logs/rom01/layout.log`](logs/rom01/layout.log),
[`logs/rom01/sfdisk-verify.log`](logs/rom01/sfdisk-verify.log),
[`capturas/rom01-layout.json`](capturas/rom01-layout.json)):

| Partición | Inicio | Tamaño | Tipo | Boot | Contenido |
|---|---:|---:|---|---|---|
| p1 BOOT | 1.048.576 B (1 MiB, sector 2048) | 33.554.432 B (32 MiB) | `0x0c` | **sí** | FAT16, label `BOOT` |
| p2 rootfs | 34.603.008 B (33 MiB, sector 67584) | 536.870.912 B (512 MiB) | `0x83` | no | ext4, label `rootfs` |
| — | — | — | — | — | **no hay p3/DATA en la imagen** |

- Imagen total 571.473.920 B = 545 MiB; tabla `dos` sin GPT; `sfdisk --verify`: *No errors detected*.
- Los offsets/ tamaños coinciden **exactamente** con `br2_external/board/X6200/genimage.cfg` y con
  la suposición 33 MiB/512 MiB de `tools/release/make_release_assets.sh` y `publish_release.sh`
  (comprobada, no asumida).
- U-Boot: el binario de 556.112 B está en el offset 8 KiB y coincide byte a byte con
  `u-boot-sunxi-with-spl.bin` (`cmp`); firma `U-Boot SPL 2023.01` / `U-Boot 2023.01 for sunxi`.
- `boot.vfat`: 3 archivos (`zImage` 5.567.760 B, `sun8i-r16-x6200.dtb` 26.544 B, `boot.scr` 334 B),
  `fsck.fat -n` limpio, 2.733/16.343 clusters usados. `boot.scr` fija
  `root=/dev/mmcblk0p2 rootwait panic=10 fbcon=rotate:3 video=VGA:480x800`; U-Boot define
  `fdtfile=sun8i-r16-x6200.dtb`, por lo que `${fdtfile}` del script resuelve.
- Particiones extraídas por offset verificado (`dd bs=512 skip=2048/67584`) y comparadas:
  p1 = `boot.vfat` y p2 = `rootfs.ext2` (SHA256 idénticos), GUI idéntica por `debugfs dump`
  ([`logs/rom01/extract-partitions.log`](logs/rom01/extract-partitions.log)).

### 3.2 Rootfs

`e2fsck -fn` sobre la copia extraída: **limpio, exit 0**, 5.375/32.768 inodos, 65.878/131.072
bloques de 4 KiB ([`logs/rom01/e2fsck.log`](logs/rom01/e2fsck.log)):

| Métrica | Valor |
|---|---|
| UUID / label | `79bb870e-5334-47c8-a825-3f1510b356ed` / `rootfs` |
| Bytes usados / libres | 269.836.288 B (50,3 %) / 267.034.624 B (49,7 %) |
| Inodos usados / libres | 5.375 (16,4 %) / 27.393 (83,6 %) |
| Features | `has_journal ext_attr resize_inode dir_index filetype extent … metadata_csum` |

Nota: `/etc/fstab` declara `/dev/root / ext2 … noauto`; el filesystem real es ext4 con journal.
La entrada es `noauto` y el kernel arranca con `root=/dev/mmcblk0p2`, así que no se afirma impacto;
queda como inconsistencia documental.

Inventario del `rootfs.tar` ([`capturas/rootfs-audit.json`](capturas/rootfs-audit.json),
listado completo en [`capturas/rootfs-tar-listing.txt.gz`](capturas/rootfs-tar-listing.txt.gz)):

| Dimensión | Valor |
|---|---|
| Archivos / directorios / symlinks / hardlinks | 4.053 / 391 / 920 / 493 |
| Setuid / setgid / nodos de dispositivo | **3** / 0 / 0 |
| Setuid | `bin/busybox` 4755, `usr/libexec/dbus-daemon-launch-helper` 4750 (root:101), `usr/libexec/ssh-keysign` 4711 |
| Dueños | 5.855 entradas `0:0`; 1 `0:101` (dbus); 1 `33:33` (www-data) |
| Paquetes instalados (stamps de Buildroot) | **142** ([lista](capturas/packages-installed.txt)) |
| Bibliotecas compartidas | **363** ([lista](capturas/rootfs-libs.txt)) |
| Init scripts | **26** (24 `S*` + `rcS`/`rcK`; [lista](capturas/rootfs-initd.txt)) |
| Módulos del kernel | 410 `.ko`, 15 MB, `6.1.82` |
| Archivos world-writable | ninguno (sólo el directorio `/tmp` 1777) |

Enlaces no resolubles dentro del archivo: 11. Nueve son caminos provistos en runtime
(`/dev/fd`, `/dev/stdin|out|err`, `/dev/log` → `/tmp/log`, `/etc/mtab` → `/proc/self/mounts`,
`/var/lib/dbus` → `/tmp/dbus` creado por `S30dbus`, `/var/lock` → `/run/lock` creado por
`inittab`), `/etc/localtime` resuelve a zona real y dos son symlinks de desarrollo de ICU
(`current/Makefile.inc`). **No se encontró un enlace roto funcional** por inspección estática.

### 3.3 Dependencias del GUI (cierre ARM real)

`readelf -d`: 28 `NEEDED`. `QEMU_LD_PREFIX=<rootfs> qemu-arm-static <rootfs>/lib/ld-linux-armhf.so.3
--list <rootfs>/usr/sbin/x6200_gui`: **exit 0, 47 bibliotecas resueltas, 0 “not found”**
([`logs/rom01/gui-elf.log`](logs/rom01/gui-elf.log),
[`logs/rom01/gui-ldd-qemu.log`](logs/rom01/gui-ldd-qemu.log)). Interpreter
`/lib/ld-linux-armhf.so.3` presente. El binario del `rootfs.tar` y el de la partición ext4 tienen
el mismo md5 `1f998c46…` (ya verificado en #195 y reconfirmado acá).

## 4. ROM-02 — Boot offline, init y servicios

### 4.1 Prune del rootfs (PASS)

Se extrajo el bloque `run:` de `.github/workflows/boot-rootfs.yml` y se ejecutó **literalmente**
([`capturas/boot-rootfs-run-block.sh`](capturas/boot-rootfs-run-block.sh),
[`logs/rom02/prune-workflow.log`](logs/rom02/prune-workflow.log)): `bash -n` OK; `S20gui` queda,
`S95gui` se borra, `S99vendor` queda; los 4 guards fail-closed (comodín, path absoluto, `..`,
path del port) devuelven **rc=2 sin borrar nada**. En la imagen final: `S20gui` presente y
**0 entradas `S95gui`** en el tar ([`logs/rom02/image-services.log`](logs/rom02/image-services.log)).

### 4.2 Sintaxis con el intérprete real de la imagen (PASS: 26/26)

**`/bin/sh` es bash, no BusyBox ash**: el rootfs trae `/bin/sh -> bash` (GNU bash 5.2.15,
`arm-buildroot-linux-gnueabihf`) y `/bin/bash` de 981.728 B. Por lo tanto los `#!/bin/sh` de los
init scripts se ejecutan con **bash**. Con el binario real (`$ROOTFS/bin/sh -n`, bajo
`qemu-arm-static`) **los 26 scripts parsean, rc=0**
([`logs/rom02/sh-syntax-real.log`](logs/rom02/sh-syntax-real.log)). `S04restore_backup_config`
usa `if [[ … ]] then` (bash-style) y **también parsea** con bash; la semántica del `if` se verificó
con un snippet aislado bajo el bash de la imagen. El script **no se ejecutó** contra `/`.

Se conserva además la prueba con el BusyBox de la imagen como **control negativo de portabilidad**
([`logs/rom02/sh-syntax.log`](logs/rom02/sh-syntax.log)): allí `S01create_data` (arrays bash) y
`S04` fallan, lo que demuestra que estos scripts **dependen de bash**. Hoy no es un fallo porque
`/bin/sh` es bash; sí es una dependencia no declarada: si un build quitara bash o cambiara
`/bin/sh` a BusyBox ash, DATA y restore dejarían de funcionar. No se modificó nada.

### 4.3 Orden y servicios (estático)

`rcS` recorre `/etc/init.d/S??*` en orden lexicográfico:
`S01boot_time → S01create_data → S01seedrng → S01syslogd → S02klogd → S02sysctl → S02update →
S03tune_cache → S04restore_backup_config → S10udev → S11radio_daemon → S20gui → S30dbus →
S39btpairings → S40bluetoothd → S40network → S45network-manager → S48sntp → S49ntp → S50alsa →
S50pulseaudio → S50sshd → S94btcat → S96webserver`.

- **GUI:** `S20gui` lanza `/usr/sbin/x6200_daemon`, un wrapper `/bin/sh` con
  `while true; do x6200_gui …; mv log; done` (líneas 14-16), **sin backoff ni límite**. `x6200_gui`
  es el ELF del candidato; el wrapper hace bind de `/usr/share/x6200/images` a `/dev/shm/images` y
  siembra `/mnt/params.db` desde `params.default.db` si falta (S20gui línea 23). El log de la GUI
  va a `/tmp/x6200_log.txt` (tmpfs) y sólo se mueve a `/mnt/app_logs` cuando la GUI termina: **no
  hay rotación mientras el proceso vive** (no se midió crecimiento en uso real).
- **Audio:** `S50alsa` (`alsactl restore`; `/var/lib/alsa/asound.state` presente) y `S50pulseaudio`
  (`pulseaudio --system --daemonize`). La GUI arranca en S20, **antes** de pulseaudio (S50); no se
  ejecutó el arranque real, no se afirma si el contexto de audio se recupera.
- **Red/tiempo:** `S40network` (`ifup -a`) y `S45network-manager` (NetworkManager) conviven;
  `S48sntp` y `S49ntp` (ntpd) están ambos presentes. Sin boot real no se afirma conflicto.
- **Bluetooth:** `S39btpairings` (restaura `/mnt/backups/bluetooth.tgz` y `modprobe hidp/uhid`),
  `S40bluetoothd`, `S94btcat`. `hidp` existe (`CONFIG_BT_HIDP=m`, `hidp.ko`), pero **`uhid` no está
  compilado** (`# CONFIG_UHID is not set`, sin `uhid.ko`): el `modprobe uhid` del script es no-op y
  los teclados Bluetooth **BLE/HOGP no tienen soporte** en este kernel
  ([log](logs/rom02/bt-modules.log)).
- **Web:** `S96webserver` ejecuta `/usr/bin/x6100_webserver` (entry point Python legítimo que
  importa `x6100_webserver` desde `site-packages`) con `--db /mnt/params.db --host 0.0.0.0
  --port 80 --https-port 443 --filebrowser-path /mnt/`.
- **`S11radio_daemon` no arranca ningún demonio**: sólo crea el symlink de `resolv.conf` y, en
  `stop`, escribe `s` en `/dev/ttyS1`. No hay proceso “radio daemon”.
- **Watchdog: no hay servicio de watchdog** en `init.d` ni configuración de `/dev/watchdog`
  (existe el applet `/sbin/watchdog` de BusyBox, sin uso). La única supervisión es el `while true`
  del wrapper de GUI, que ante un crash reinicia sin espera (riesgo de lazo, agravado por §8.2).
- **Montajes/seed:** `inittab` monta `proc`, remonta `/` rw, ejecuta `mount -a` (fstab) y
  `/run/lock/subsys`, y recién después `rcS`; fstab monta `proc`, `devpts`, `/dev/shm`, `/tmp`,
  `/run`, `sysfs` y `/dev/mmcblk0p3` en `/mnt` **como ext4**. `S20gui` monta tmpfs en `/dev/shm` y
  siembra la DB. `S01boot_time` y `rcS` escriben en `/mnt/app_logs` **antes** de que exista DATA,
  así que en una tarjeta sin DATA esos logs caen en la rootfs.
- **Boot de kernel/init/servicios/hardware: NO probado.** No se usó QEMU system ni se montó un
  kernel; esta unidad sólo ejecutó el binario ARM en QEMU_USER. Ver limitaciones.

## 5. ROM-03 — DATA y persistencia

Archivos del Buildroot congelado: `fstab` (sha256 `31d158a7…`), `S01create_data`
(`7708574d…`), `genimage.cfg` (`7e9a57fd…`), copiados tal cual a la imagen (se verificó el script
del rootfs contra el overlay).

### 5.1 Lo que dice el código

`fstab` línea 9: `/dev/mmcblk0p3 /mnt ext4 rw 0 2`. `genimage.cfg` define **sólo 2 particiones**
(u-boot fuera de tabla en 8K, `boot` y `rootfs`): la imagen **no trae p3**. `S01create_data` tiene
tres ramas:

1. `blkid -L DATA` encuentra una DATA existente → la monta con su tipo real y sale.
2. No hay DATA → `parted … | grep fat32`; si hay una partición fat32, sale sin crear nada.
3. No hay DATA ni fat32 → toma el espacio libre, crea p3 `fat32`, `mkfs -t vfat -n DATA` y
   `mount /dev/mmcblk0p3` (un solo argumento: resuelve tipo por fstab, que dice ext4).

### 5.2 Lo que realmente pasa (evidencia ejecutada)

`/bin/sh` es bash 5.2.15 (§4.2), así que `S01create_data` **parsea** y el kernel lo ejecutaría en
el boot real (no se ejecutó acá). El circuito, con las condiciones del artefacto:

1. `inittab` ejecuta `mount -a` **antes** de `rcS`; fstab pide `ext4` para `/dev/mmcblk0p3`.
2. En una tarjeta nueva `blkid -L DATA` no devuelve nada.
3. `parted -m -s /dev/mmcblk0 print | grep fat32`: la p1 del artefacto es **FAT16** (aunque el MBR
   diga tipo `0x0c`), así que **no** matchea y el script sigue. Si la partición de boot fuera
   FAT32, este guard haría que el script saliera **sin crear DATA** (guard frágil).
4. Toma el espacio libre, crea p3 `fat32` con `parted`, `partprobe`, y
   `mkfs -t vfat -n DATA /dev/mmcblk0p3` → **DATA queda vfat**.
5. `mount /dev/mmcblk0p3` va **sin tipo**: libmount resuelve por fstab → `ext4`
   (comprobado con `findmnt --fstab` en host y `mount -f -T` del util-linux 2.38 del target bajo
   QEMU), que falla sobre vfat. El script **no chequea
   el rc** del mount. El paso de formato se comprobó con el `mkfs.fat` de la imagen bajo QEMU sobre
   un archivo privado: `rc=0`, label `DATA`, `blkid TYPE=vfat`
   ([`logs/rom03/mkfs-vfat.log`](logs/rom03/mkfs-vfat.log)).

| Tarjeta | Qué pasa | Estado |
|---|---|---|
| **Nueva** (sin p3) | Se crea y formatea p3 **vfat**; `mount /dev/mmcblk0p3` resuelve ext4 y **falla** → primer boot con `/mnt` en la rootfs (la GUI y `S20gui` escriben ahí). En el **segundo** arranque `blkid -L DATA` encuentra la vfat y `mount -t vfat` (tipo real) sí monta | **FAIL** en el primer boot y en el contrato “DATA ext4” |
| DATA **vfat** existente | `mount -a` intenta ext4 y falla; `S01create_data` rama 1 monta con `-t vfat` | **PASS** (compatibilidad vfat) |
| DATA **ext4** existente | `mount -a` (inittab, antes de `rcS`) monta ext4 en `/mnt`; la GUI usa `/mnt/params.db` | **PASS** (es el caso de la tarjeta del owner) |

Consecuencias verificables: en una tarjeta nueva el primer arranque **no persiste en DATA** (la DB
sembrada y los logs caen en la rootfs); desde el segundo arranque se monta una DATA **vfat**, no
ext4. `docs/RELEASE_NOTES_2026-10-11.md` dice “FAT32” (coincide con lo que crea el script) pero no
menciona el fallo de montaje del primer boot; `docs/PRUEBAS.md:43` afirma que “el próximo ROM nace
con DATA ext4”, garantía que **el artefacto por sí solo no da**: el mecanismo de primer arranque
crea vfat y no monta. El procedimiento de flasheo externo (no inspeccionado) podría pre-crear o
extender p3 ext4 antes de flashear; eso explicaría el caso histórico, pero no está en el repo ni se
midió acá. `parted` y `partprobe` viven en
`/usr/sbin` y el script los invoca sin path; el binario de BusyBox contiene el `PATH` default
`/sbin:/usr/sbin:/bin:/usr/bin`, así que deberían resolver (no verificado en boot real).
No se ejecutó el script ni se creó/formateó ningún dispositivo: la evidencia es el código, el
intérprete bash real y la resolución de fstab en el host. Punto frágil adicional, no medido:
`partprobe /dev/mmcblk0` se ejecuta con p2 (rootfs) montada; si el kernel no recrea el nodo p3,
`mkfs`/`mount` fallan y el script ignora los errores.

**Contexto de hardware externo (tip, no candidato):** el commit `0d00e7fc` registra que en la ROM
del tip `50bf8ab0` `/mnt` no montaba por una **tercera entrada MBR vieja que dejaba p3 512 KiB
corta**; se reparó en vivo con `parted resizepart 3 100%`, sin pérdida de datos, y se actualizó el
script de flasheo. Es evidencia de que la geometría de p3 es frágil y de que el flasheo del
candidato (cuya MBR sólo tiene p1/p2) depende de un procedimiento externo que recomponga p3. Si el
flasheo escribe la MBR de la imagen, la entrada p3 desaparece; si preserva la MBR vieja (como
sugiere el hallazgo del tip), p3 sobrevive con su geometría. El script de flasheo no está en el
repo y no se inspeccionó; se declara como limitación.

### 5.3 Base de datos por defecto

`/usr/share/x6200/params.default.db`: 102.400 B, sha256 `bc18ba10…`, `PRAGMA integrity_check` =
`ok`, journal `delete`, 13 tablas + `sqlite_sequence`, `version(id)=1`
([`logs/rom03/db-integrity.log`](logs/rom03/db-integrity.log)). Con `version=1`, el primer arranque
aplica las migraciones 2..5; la migración 2 (`UPDATE atu SET freq=freq*50000+25000 WHERE
freq<500000`) **no es idempotente** si se pierde la tabla/registro `version`: volvería a multiplicar
frecuencias. No se ejecutó sobre DATA real; es análisis de código (`src/params/migrations.c`).

Observación del arranque QEMU con la DB de fábrica: se registra `cfg_init: Error during loading
params` porque faltan claves (`eq_mic_p2`..`p5` y varios `band_params`); la GUI **sigue con los
defaults** y no crashea (el error es de carga, no de apertura). La DB de fábrica sólo trae 4 filas
en `params`; no se afirma que sea un defecto, se registra como comportamiento observado.

**Persistencia entre reinicios del proceso (QEMU_USER):** con la DB escrita durante los 100 ciclos,
un segundo proceso de la GUI arrancó y restauró `freq=14074000`, `mode=CW`, `band=20m SSB`,
`filter=500`; el hash de la DB no cambió y no hubo CRASHLOG
([`logs/rom03/persist-restart.log`](logs/rom03/persist-restart.log)). **PASS** para persistencia
entre procesos; **no** es una prueba de corte de energía ni de hardware.

## 6. ROM-04 — Update y rollback

**Ejecución en sandbox privado (root de contenedor).** El host no tiene `sudo` (`no new
privileges`), así que los scripts se corrieron **tal cual, como root, dentro de un contenedor
privado** (`assessment196-update`/`assessment196-testupd`, imagen `x6200-buildroot`) con montajes
sólo de `assessment-194`; no se usó sudo del host, no se tocó la SD, la radio ni `rom-build`. Se
preservó el camino de permisos real (extracción como root, owners numéricos), no se reemplazó por
una extracción sin dueño/setuid. Los scripts no se editaron.

- **`make_update_tgz.sh` ejecutado** ([log](logs/rom04/make-update-container.log)):
  `rootfs.tar` → tgz de **102.736.632 B**, sha256 `239b49bba9233585d4b196734e951ac8c5711cf17100f05c99699dcbb60c29d0`,
  **5.466 entradas**, 0 directorios, 0 entradas `./`. Termina **exit 1** en su propia aserción:
  `setuid: 3 (tienen que ser 5)` → **FAIL del empaquetador** (el rootfs tiene 3 setuid; `mount`/
  `umount` no son setuid). La documentación (`docs/TESTING.md:84-86`) repite el número 5.
- **Contenido del tgz:** GUI sha256 `7e2e9331…` = la de la imagen; `etc/init.d/S02update` sha256
  `d3c7d76b…` = el overlay corregido; el resto de los archivos 0/0. **Pero
  `etc/init.d/S02update` viaja con owner `1000/1000`** (uid del checkout de desarrollo; en el
  target **uid 1000 = `pulse`**), no 0/0: la actualización deja el script que corre como root
  propiedad de uid 1000 → **FAIL de integridad/seguridad**. Se verificó extrayendo el tgz con el
  **BusyBox tar del target** en un chroot privado: `owner=1000:1000 mode=755`, y
  `pulse:x:1000:1000` en `/etc/passwd`
  ([log](logs/rom04/overlay-owner-target.log)). El assert del script sólo cuenta setuid, no revisa
  owners.
- **`test_update.sh` ejecutado** ([log](logs/rom04/test-update-container.log)): **exit 0** en el
  chroot privado (binario ARM bajo binfmt). Backup `backup_x6200_r1cbu_196.tgz` de 96.587.048 B y
  **5.466 entradas** (el test preinstala el overlay corregido), backup con la GUI vieja marcada,
  GUI restaurada a `7e2e9331…`, tgz consumido, **3 setuid** tras actualizar, 4.548 archivos. **PASS del
  camino post-overlay**. El test **preinstala el overlay** antes de correr `S02update`, así que no
  prueba la primera aplicación con el script de la imagen.
- **Primera actualización (script de la imagen), ejecutada**: se armó un target con el rootfs de
  la imagen y se corrió **el `S02update` stock** dentro del chroot privado, con el `tgz` en `/mnt`
  ([log](logs/rom04/first-update-exec.log)). Resultado: `rc=0`, actualización aplicada (GUI
  `7e2e9331…`, 3 setuid, `S02update` 1000:1000), pero el backup
  `backup_x6200_r1cbu_196.tgz` (102.431.284 B) sólo deja leer **706/5.466 entradas** por el
  `xargs` del BusyBox del target (varios `tar` concatenados). `docs/TESTING.md` decía ~700/5313:
  coincide. **FAIL de completitud del backup de la primera actualización.** (Una emulación con el
  `xargs` de GNU del host dio 3.464 por otro troceo; no es el del target y no se usa como número
  de firmware, [log](logs/rom04/backup-first-update.log).)
- **Backup corregido** (lista a archivo temporal + un solo tar, el del overlay):
  **5.466/5.466 entradas**, GUI incluida ([mismo log](logs/rom04/backup-first-update.log)).
  **PASS** del arreglo, que sólo aplica desde la segunda actualización.
- **Sin verificación de integridad ni rollback:** el `S02update` (imagen y overlay) hace
  `zcat "$i" | tar -xf -` sobre `/` sin chequear checksum/firma; si `tar` falla igual ejecuta
  `rm "$i"` (no hay `set -e`); si el backup falla, sigue (“Backup failed, continuing”). No hay
  A/B, journal de update ni restauración automática. Un corte de energía durante la extracción deja
  el rootfs a medias y consume el tgz.
- **Offsets del release:** `dd skip=33 count=512` de `make_release_assets.sh` (línea 42) y
  `publish_release.sh` (línea 51) coincide con el layout real (§3.1): **PASS** de esa premisa.
- **Límites del test de update:** `test_update.sh` hace `rm -rf "$T"` al empezar y al terminar (no
  conserva la evidencia), y su comprobación de “tgz borrado” usa un nombre hardcodeado
  (`x6200_r1cbu_v0.1.2-lu2dsx.tgz`, línea 83): con otro nombre la aserción es vacua y puede reportar
  “el firmware borró el tgz” sin haberlo comprobado. Se documenta, no se edita.

## 7. ROM-05/06/07 — Banco, estrés y reposo

### 7.1 Banco compartido: BLOCKED (dependencia exacta)

`banco-apps` estaba **corriendo** (contenedor de otra sesión, chroot
`/home/pablo/desa/asf/state/worktrees/x6200/at190-chroot`). `captura-banco.sh`/`barrido-apps-banco.sh`
tienen nombre fijo `banco-apps`, volumen por defecto `x6200-build-output` y `docker exec banco-apps`
hardcodeado: usarlos implicaría tocar el banco ajeno o el volumen compartido. Por eso:

- **ROM-06 (tres barridos completos): BLOCKED.** No se ejecutó ningún barrido de las 25 entradas
  (`APPS="FT8 FT4 JS8 WSPR SSTV WeFax NavTex RTTY CW Field GPS Bands Settings BT Cluster CwTrainer
  Eq Status QTH Callsign Recorder WiFi SWRScan Propagation DFLEditor"`, sin `off`; FT4 es alias de
  FT8). No hay `barrido.json` ni capturas por app en esta unidad. #197 necesitará el banco o una
  autorización explícita para levantar uno privado con los scripts adaptados (no se hizo: sustituir
  el nombre/volumen habría cambiado el arnés).
- **ROM-05 y ROM-07** se ejecutaron en un **QEMU_USER privado equivalente**, con el binario y las
  libs de la imagen, chroot propio y DATA sintética de fábrica (`params.default.db`), sin volumen
  compartido. Esto es evidencia **PARTIAL**, no el banco contratado.

### 7.2 ROM-05 — GUI ARM real en QEMU_USER (PASS parcial)

Preparación: rootfs extraído del `rootfs.tar` del candidato, `params.default.db` copiada a
`/mnt/params.db`, imágenes a `/dev/shm/images`, libs de banco (`libfakefb`, `libcrash`,
`libx6200trace`) y cliente `x6200-remote` compilado **static** con el toolchain del Buildroot
congelado. Contenedor `assessment196-qemu` (`QEMU_CPU=cortex-a7`). Identidad antes de correr:
GUI md5 `1f998c46…`, sha256 `7e2e9331…`, fuente-md5 `964193ae…` (coinciden con el manifiesto;
[`logs/rom05/handshake.log`](logs/rom05/handshake.log)). El binario pasó el gate #86, re-ejecutado
en esta unidad ([`logs/rom01/gate86.log`](logs/rom01/gate86.log)).

- Socket de control creado a los **17 s** de arrancar el contenedor (wall, QEMU).
- Handshake `{"cmd":"get"}` devuelve `{"t":"state",…,"mode":"USB","freq":10100000,"tx":false}`.
- Captura del framebuffer falso: **6.770 colores**, PNG 800×480
  ([`capturas/rom05-baseline.png`](capturas/rom05-baseline.png)).
- La GUI registra degradación esperada sin radio/i2c/CAT/NetworkManager/audio y **sigue viva**.
- El chroot privado **no monta `/proc`**, así que `boot.log`/`lastbreath` muestran `up_s=0` y el
  tiempo de boot no es interpretable; la métrica de 17 s es wall del contenedor, no boot real.

### 7.3 ROM-07 — 100 ciclos RX (PASS, QEMU_USER, 2 corridas)

Secuencia exacta por ciclo: `app off → mode USB → freq 14074000 → filter 2400 → mode CW → app
Settings → app off`, `get` tras cada paso con espera de estado (poll 100 ms, timeout 5 s).

| Métrica | Corrida A (proceso original) | Corrida B (mismo proceso que el reposo de §7.4) |
|---|---|---|
| Crudo | [`stress-rx-100.jsonl`](capturas/stress-rx-100.jsonl) | [`stress-rx-100b.jsonl`](capturas/stress-rx-100b.jsonl) |
| Ciclos | **100/100** | **100/100** |
| Pasos / comandos | 700, 700 `{"t":"ok"}` | 700, 700 `{"t":"ok"}` |
| Divergencias / timeouts / errores | 0 / 0 / 0 | 0 / 0 / 0 |
| `settle` p50 / p95 / máx | 182 / 428 / 449 ms | 198 / 471 / 1.471 ms |
| Cliente p50 / p95 / máx | 57 / 63 / 75 ms | 62 / 116 / 178 ms |
| Duración | 207 s | 239 s |
| CRASHLOG / Overflow / reinicios | 0 / 0 / 0 | 0 / 0 / 0 |
| RSS / threads / fds antes → después | 60.248 kB · 11 · 19 (sin cambio) | 59.188 → 59.116 kB · 11–12 · 25 (sin tendencia) |

La corrida B se hizo después de que el contenedor de A fuera **detenido externamente** (ver §7.4);
el host estaba más cargado, de ahí el outlier de 1,47 s en `settle` (sigue muy por debajo del
timeout de 5 s y sin divergencias). El primer piloto (3 ciclos) mostró 3 “divergencias” en
`app Settings`: era una **carrera de medición** (el `ok` se responde antes de que el scheduler LVGL
aplique la orden; el `get` inmediato veía `app=""`). Se corrigió la metodología con espera de
estado. El piloto queda preservado ([`logs/rom07/stress-pilot.log`](logs/rom07/stress-pilot.log)) y
**no** se cuenta como defecto de la GUI.

### 7.4 ROM-07 — 1 h de reposo (PASS en proceso nuevo; primer intento interrumpido)

Mismo esquema: sondeo `get` cada 5 s (12/min) y muestreo cada 60 s de PID, RSS, threads, fds,
CRASHLOG, Overflow/cola, bytes de log y socket.

**Intento A (proceso original): interrumpido a los 31 min.** El contenedor
`assessment196-qemu` terminó con **exit 137** a las 21:39:43Z (`OOMKilled=false`, `Error` vacío),
sin `CRASHLOG sig=`, sin línea nueva en el log de la GUI y sin evento de la GUI; la última línea
del log es de las 21:09 (fin del estrés). Fue una **detención externa del contenedor**, no un
crash del binario; se desconoce el actor (no se ejecutó `docker stop` de ese nombre en esta sesión).
Datos parciales: [`capturas/idle-3600-partial-external-stop.jsonl`](capturas/idle-3600-partial-external-stop.jsonl),
31 muestras, 366 `get` OK, 6 errores al final, RSS 60.248 kB/fds 19/threads 10–11 estables hasta la
detención ([postmortem](capturas/qemu-container-postmortem.json)). **No se cuenta como PASS de 1 h.**

**Intento B (proceso nuevo `assessment196-soak`, sí completa 1 h).** Para no perder la ventana se
arrancó un proceso nuevo con la DB persistida (14.074 MHz/CW), se repitieron los **100 ciclos**
(corrida B de §7.3) y se dejó 1 h de reposo: la corrida de 60 muestras dio 3.379 s (720 `get`) y
se agregaron 4 muestras extra (226 s, 48 `get`) para superar los 3.600 s, total **3.605 s / 64
muestras / 768 `get`**. Crudo: [`capturas/idle-3600b.jsonl`](capturas/idle-3600b.jsonl) +
[`capturas/idle-3600b-extra.jsonl`](capturas/idle-3600b-extra.jsonl).

| Métrica | Valor |
|---|---|
| Ventana / muestras | **3.605 s / 64 muestras** |
| `get` OK / errores | **768 / 0** |
| PID estable | 7 (sin reinicios en 64 muestras) |
| RSS | 59.188 → 59.116 kB (rango 59.116–59.188; sin tendencia) |
| threads / fds | 11–12 / **25 constante** |
| CRASHLOG / Overflow / cola | 0 / 0 / 0 (chequeo directo del log real) |
| Crecimiento de log | 296.731 B constante (0 B/min) |
| Latencia máxima de `get` | 771 ms (incluye carga del host; sin timeout) |

**Nota de medición (bug del script, no del firmware):** `idle-3600.sh` leía `log_bytes`, `crash` y
`overflow` de un path hardcodeado `/t/out-banco.txt` (el log del proceso A). En la corrida B el log
real es `/t/out-soak.txt`: se midió directo antes y después (**296.731 B**, **0** `CRASHLOG sig=`,
**0** Overflow, 2.857 líneas; [`logs/rom07/soak-log-check.log`](logs/rom07/soak-log-check.log)). Las
columnas `log_bytes`/`crash`/`overflow` de las 64 muestras B son del archivo viejo y **no se usan**
para la conclusión; RSS/threads/fds/PID sí son del proceso correcto. El script quedó corregido
(`LOG=${LOG:-…}`) para futuras corridas.

**Límites:** el reposo de 1 h es de un **proceso nuevo**, no del proceso de la corrida A (que fue
detenido externamente); sí es el mismo proceso que la corrida B de 100 ciclos. QEMU_USER no es
kernel/init real ni Cortex-A7; no hay FFT, radio, audio ni hardware; una hora plana **no** demuestra
ausencia de fugas ni watchdog OK. El host ejecutó otras tareas de auditoría durante la ventana (no
se aisló el CPU), por lo que la latencia máxima no es una medición limpia de reposo.

## 8. ROM-08 — Negativas

### 8.1 Protocolo y socket (PASS)

Sobre el proceso QEMU privado, con el cliente existente ([`capturas/negatives.jsonl`](capturas/negatives.jsonl),
[`logs/rom08/negatives.log`](logs/rom08/negatives.log)):

| Caso | Respuesta | Estado |
|---|---|---|
| comando inexistente | `{"t":"err","err":"Unknown command"}` | PASS |
| app inexistente | `{"t":"err","err":"Unknown app"}` | PASS |
| mode inválido (texto y número) | `{"t":"err","err":"Invalid mode"}` | PASS |
| freq −1 / pwr 999 / filter 999999 | `{"t":"err","err":"Invalid value"}`, estado sin cambio | PASS |
| JSON malformado / `val` faltante | `{"t":"err","err":"Bad request"}` / `Invalid value` | PASS |
| `off` duplicado | `ok` ×2, `app=""` | PASS |
| desconexión abrupta (JSON parcial + close) | siguiente `get` OK, GUI viva | PASS |
| 10 reconexiones consecutivas | 10/10 `state` | PASS |

CRASHLOG 0 en todos. Los negativos nativos de events/scheduler/spectrum/restore/remote/lastbreath/
params/migrations se **reusan de #195** (138 comprobaciones, 0 fallos; no se repitieron).

### 8.2 DB corrupta / read-only / faltante (FAIL, crash determinista)

Se usaron copias privadas del rootfs (no la DATA del equipo) y contenedores efímeros:

| Caso | Preparación | Observado |
|---|---|---|
| DB corrupta | primeros bytes de `params.db` reemplazados por `CORRUPT` | `file is not a database` → `params_init: Open params.db` → `avisa_subject_nulo` → **SIGSEGV**, exit **139**, 2/2 con **PC 0x42e12c** idéntico |
| DB read-only | `/mnt` montado read-only (`-v …/mnt:/t/mnt:ro`) | migración 2 `attempt to write a readonly database` → mismo `Open params.db` → **SIGSEGV**, exit 139, mismo PC |
| DB faltante | sin `params.db`, `/mnt` escribible | crea DB vacía de 8.192 B; migración 1 falla (`Can't apply 1 migration`) → **SIGSEGV**, exit 139, mismo PC |

Evidencia: [`logs/rom08/db-corrupt-gui.log`](logs/rom08/db-corrupt-gui.log),
[`db-corrupt-gui-run2.log`](logs/rom08/db-corrupt-gui-run2.log),
[`db-readonly-gui.log`](logs/rom08/db-readonly-gui.log),
[`db-missing-gui.log`](logs/rom08/db-missing-gui.log). Camino de código:
`src/params/db.c::database_init()` (si `migrations_apply()` falla devuelve false),
`src/params/params.c:631-674` (si `database_init()` falla **no llama `cfg_init()`** y sigue),
`src/params/migrations.c` (la falta de tablas base en una DB nueva hace fallar la migración 1),
`src/main.c:562-568` (`params_init()` → `cb_hooks_init()`), y `src/radio.c:967-971`
(`cb_hooks_init` lee `cfg.band_id` y después `cfg_cur.band->split.val`; con `cfg_init()` omitido,
`cfg_cur.band` es NULL). La dirección del fallo se simbolizó con el binario no stripped de la
misma build: **PC 0x42e12c = offset 0x2e12c dentro de `cb_hooks_init` (+0x34)**, consistente con la
desreferencia de la línea 971 ([`logs/rom08/crash-symbolize.log`](logs/rom08/crash-symbolize.log)).
El wrapper
`/usr/sbin/x6200_daemon` reinicia la GUI en un `while true` sin espera: en hardware, una DB
corrupta o `/mnt` read-only llevaría a un **lazo de arranque/crash** y a un log por iteración en
`/mnt/app_logs` (o en la rootfs si `/mnt` no está montado).

Se ejecutó además el **wrapper real** (`x6200_daemon`) 10 s con la DB corrupta en el chroot
privado: **34 inicios de GUI, 19 CRASHLOG con la misma firma, 20 archivos de log y `boot.log` de
18 KB** en 10 s (~3,4 vueltas/s); como el nombre del log tiene resolución de segundos, varias
vueltas se pisan y se pierden ([`logs/rom08/crashloop-summary.log`](logs/rom08/crashloop-summary.log),
[`crashloop-boot.log`](logs/rom08/crashloop-boot.log)). Es un lazo de crash real, no una hipótesis.

La DB **llena** no se probó (requeriría un fixture privado adicional; no se inyectó en DATA real);
por código,
`params_write_*` ignoran el rc de `sqlite3_step` (`src/params/db.c`), por lo que un fallo de
escritura se tragaría en silencio. Queda **BLOCKED** con esa dependencia.

### 8.3 Periféricos ausentes (PASS parcial)

Sin i2c, CAT (`/dev/ttyS2`), radio (`La BASE no respondió`), NetworkManager/dbus ni audio, la GUI
registra los errores y **sigue respondiendo** durante 100 ciclos + 1 h. No se inyectaron fallos en
demonios ni en la radio; es el entorno QEMU, no una prueba de hardware.

## 9. Hallazgos (sin abrir tickets)

1. **Tarjeta nueva: DATA nace vfat y el primer boot no la monta.** `S01create_data` crea p3
   `fat32` + `mkfs -t vfat`, pero `mount /dev/mmcblk0p3` resuelve `ext4` por fstab y falla; el
   primer arranque escribe en la rootfs y recién el segundo monta la DATA vfat. Contradice el
   contrato “DATA ext4” de `docs/PRUEBAS.md`; `RELEASE_NOTES` sí dice FAT32 pero omite el fallo de
   montaje. Evidencia: §5.2, [`logs/rom03/fstab-resolve.log`](logs/rom03/fstab-resolve.log),
   [`logs/rom03/target-branch.log`](logs/rom03/target-branch.log),
   [`logs/rom03/mkfs-vfat.log`](logs/rom03/mkfs-vfat.log). Antecedente: #188 / Buildroot PR #1.
2. **DB corrupta, read-only o faltante → SIGSEGV determinista** (mismo PC, exit 139) y el wrapper
   reinicia sin backoff. Evidencia: §8.2. Antecedentes: #121 (lastbreath/DATA), #188.
3. **El empaquetador de update aborta y el overlay viaja con owner 1000**. `make_update_tgz.sh`
   **ejecutado como root** genera un tgz válido de 5.466 entradas pero sale **exit 1** porque
   espera 5 setuid y hay 3; además `etc/init.d/S02update` queda **1000/1000** (uid 1000 = `pulse`
   en el target), no 0/0. Evidencia:
   [`logs/rom04/make-update-container.log`](logs/rom04/make-update-container.log), §6.
4. **Update sin integridad ni rollback, y primera actualización con backup incompleto**: el
   `S02update` de la imagen usa `xargs` → backup medido en el target **706/5.466** entradas; no hay
   checksum/firma; el tgz se borra aunque `tar` falle y el backup fallido no detiene la
   actualización. El overlay corregido da **5.466/5.466** pero sólo aplica desde la segunda.
   Evidencia: §6, [`logs/rom04/first-update-exec.log`](logs/rom04/first-update-exec.log),
   `docs/TESTING.md:97-110`.
5. **No hay watchdog** y la supervisión es un `while true` sin backoff (amplifica cualquier crash,
   p. ej. el de DB). Evidencia: `rootfs/usr/sbin/x6200_daemon:14-16`, §4.3.
6. **Dependencia no declarada de bash**: `/bin/sh -> bash`; los init scripts usan arrays y `[[ ]]`.
   Con BusyBox ash fallarían `S01create_data` y `S04` (§4.2). No es un fallo actual, pero ata el
   arranque a que bash siga en la imagen.
7. **Reproducibilidad del par congelado**: el defconfig no habilita `PULSEAUDIO`/`LIBSNDFILE`; el
   build del candidato usó una desviación local. Evidencia: §1.2.
8. **Divergencia del tip**: `50bf8ab0` → `0d00e7fc` durante la unidad; resultados **STALE** para el
   tip (§1.1).

No se modificó ningún archivo de producción, test, CI ni configuración; no se publicaron issues.

## 10. Capas, cobertura y limitaciones

| Escenario | Capa | Cobertura | Comentario |
|---|---|---|---|
| ROM-01 layout/hashes/e2fsck/readelf | HOST | REAL | artefacto exacto del candidato |
| ROM-01 loader `--list` | QEMU_USER | REAL | ejecuta el loader ARM del rootfs |
| ROM-02 prune | HOST | REAL | workflow ejecutado literal |
| ROM-02 sintaxis init | QEMU_USER | REAL | `/bin/sh` (bash 5.2.15) de la imagen bajo qemu-arm; BusyBox ash como control negativo |
| ROM-02 boot kernel/init/servicios | — | **UNSUPPORTED** | no hay QEMU system ni hardware |
| ROM-03 rutas DATA | HOST + QEMU_USER | PARTIAL | código + bash real + resolución de fstab; no se ejecutó contra `/dev/mmcblk0` |
| ROM-03 DATA ext4 montada | HARDWARE (histórico del owner) | PARTIAL | no medido en esta unidad |
| ROM-03 tarjeta nueva (p3 vfat) | HOST + QEMU_USER | PARTIAL | trazado sobre copia agrandada + parted/mkfs/bash reales; no se creó el dispositivo |
| ROM-04 update | HOST + QEMU_USER | REAL/PARTIAL | scripts y chroot privados ejecutados como root; no SD/hardware |
| ROM-05 GUI QEMU | QEMU_USER | PARTIAL | banco propio, no el compartido |
| ROM-06 barridos | — | **BLOCKED** | `banco-apps` ocupado |
| ROM-07 estrés/reposo | QEMU_USER | PARTIAL | sin radio/FFT/tiempos Cortex-A7 |
| ROM-08 protocolo | QEMU_USER | REAL | comandos reales al binario del candidato |
| ROM-08 DB corrupta/RO/faltante | QEMU_USER | SYNTHETIC | fixture privado; código real |
| ROM-08 DB llena | — | **BLOCKED** | sin fixture adicional; no se inyectó en DATA real |
| ROM-08 periféricos reales | — | **PENDING_HARDWARE** | radio/red/GPS/BT/audio físicos |

Limitaciones principales:

1. **No se arrancó kernel/init real** ni se usó QEMU system; arranque, montajes reales, watchdog y
   servicios quedan `PENDING_HARDWARE`.
2. **Bancos BLOCKED**: no hay barrido de apps; sólo se abrió `Settings` en los 100 ciclos.
3. **Update**: se ejecutó en un contenedor privado como root, **no en SD/hardware**; la primera
   actualización con el script de la imagen no se aplicó (se emuló su comando de backup) y no se
   probó corte de energía durante la extracción.
4. **DB**: los crashes se reprodujeron en QEMU_USER con fixtures privados (2/2 corrupta, 1/1
   read-only, 1/1 faltante); el mismo camino de código aplica a hardware, pero **no se confirmó en
   la radio** (no se tocó).
5. **Sin RF/audio/FFT/energía**: no se transmitió, no se midió consumo ni tiempos reales.
6. **Reproducibilidad**: no bit a bit; build con desviación de `.config` (§1.2).
7. **Tip**: el dossier es STALE para `50bf8ab0`/`0d00e7fc` (§1.1).
8. **Flasheo**: la imagen no trae p3; la instalación en una SD con DATA depende de un procedimiento
   externo (no inspeccionado) que recomponga/valide p3 extremo a extremo.
9. **Reposo**: el primer intento (proceso original) fue detenido externamente a los 31 min (exit
   137, sin CRASHLOG/OOM); la hora completa se midió en un proceso nuevo y el host no se aisló de
   otras tareas de I/O.

## 11. Reproducción y entregables

- Comandos exactos, cwd, variables no secretas, inicio/fin UTC y exit code:
  [`commands.jsonl`](commands.jsonl). Las corridas hechas con `docker run` directo (QEMU, DB,
  lazo de crash) declaran `timestamps_source`; cuando no se capturó el instante quedan `null` con
  el log y el exit conservados. Logs completos: [`logs/`](logs/).
- Scripts locales de captura/análisis (orquestan herramientas existentes, no sustituyen harness):
  [`scripts/run-logged.sh`](scripts/run-logged.sh), [`scripts/rootfs-audit.py`](scripts/rootfs-audit.py),
  [`scripts/stress-rx.sh`](scripts/stress-rx.sh), [`scripts/idle-3600.sh`](scripts/idle-3600.sh),
  [`scripts/negatives.sh`](scripts/negatives.sh).
- Manifiesto con SHAs, configuración, entorno, contenedor y limitaciones:
  [`artifact-manifest.json`](artifact-manifest.json).
- Resultados por métrica: [`results.csv`](results.csv); métricas de confiabilidad consolidadas:
  [`reliability.csv`](reliability.csv).
- Hashes de todos los archivos de esta carpeta: [`SHA256SUMS`](SHA256SUMS).
- Artefactos grandes: fuera de Git en `assessment-194/clone-buildroot/build/images/` (SHA256 y
  tamaño en §1); no se subieron ROM ni binarios a Git y no se depende de `/tmp`.

Comando base del QEMU privado (repetible; el contenedor se elimina al terminar):

```bash
docker run -d --name assessment196-qemu --entrypoint "" -e QEMU_CPU=cortex-a7 \
  -v "$PWD/scratch-196/rootfs/tree:/t" x6200-buildroot \
  sh -c 'chroot /t /bin/busybox env LD_PRELOAD=/lib/libfakefb.so:/lib/libcrash.so:/lib/libx6200trace.so /usr/sbin/x6200_gui > /t/out-banco.txt 2>&1'
docker exec assessment196-qemu chroot /t /tmp/x6200-remote '{"cmd":"get"}'
```

## 12. Estado para el orquestador

- **Artefacto:** confirmado el de #195 por SHA256; GUI md5 `1f998c46…`, fuente-md5 `964193ae…`.
- **Bancos:** `banco-apps` ocupado; `banco-audio` inexistente. ROM-06 BLOCKED; ROM-05/07 sólo en
  QEMU privado.
- **Root:** sin `sudo`; ROM-04 BLOCKED en ejecución.
- **Tip:** `0d00e7fc` al cerrar; STALE declarado para `50bf8ab0` y `0d00e7fc`.
- **Pendiente para #197/#198/#199:** tres barridos completos de apps y capturas por app (banco),
  arranque real/kernel/watchdog (hardware), **primer boot en tarjeta nueva** (creación/montaje de
  DATA), update en SD/hardware real (primera actualización, rollback y corte de energía), DB
  llena/read-only en hardware, y repetir todo si se adopta el tip.
