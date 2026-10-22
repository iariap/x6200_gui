# Hijo #215 — Actualizaciones verificadas y backups completos (GUI + Buildroot)

Evidencia de la corrida del **hijo 4/4** de la épica
[#211](https://github.com/iariap/x6200_gui/issues/211). Contrato cerrado:
[#215](https://github.com/iariap/x6200_gui/issues/215). Fuente de los hallazgos:
assessment [#194](https://github.com/iariap/x6200_gui/issues/194) (F-03/F-04) y los
hijos 1/2 de la épica.

Sin push: los PRs (uno por repositorio) los abre el orquestador. **Este hijo toca
los dos repos**, así que va un par de commits con su matriz.

## Bases y worktrees

| repo | rama | base (HEAD verificado) | worktree/clon privado |
| --- | --- | --- | --- |
| `iariap/x6200_gui` | `factory/hardening-215` | `d4fb57f8ff8ccbcfdef13fc191cc9977511b6845` | `state/worktrees/x6200/hardening-215` |
| `iariap/AetherX6200Buildroot` | `fix/rom-safe-update` | `f0ff620a3052b8cd56bcb33b1374dc24d80d8379` | `state/worktrees/x6200/hardening-215-buildroot` (clon propio) |

La rama de Buildroot **avanzó** respecto de lo que dice la épica
(`f23d14f9…`): el hijo 1 (#212, DATA ext4) ya está mergeado arriba. El diff hasta
el verificado (`f23d14f9..f0ff620a`) toca sólo `S01create_data` y
`br2_external/tests/**`; el `S02update` de la imagen sigue siendo el stock, así que
la base elegida es el tip actual. Nada de este cambio toca `S01create_data` ni
`fstab` (son del #212, ya integrados: `fstab` declara `/mnt` ext4 y el
`S01create_data` nuevo monta la DATA existente con su tipo real -vfat legacy
incluido-; no queda `mount -a` roto por ese par).

## Sello de fuentes: un solo updater, en los dos puntos de instalación

```
4ed95d3f54ceafd8b71e1883e33faa45175f24a95dbb179419b8fad42bf1f025  x6200_gui/tools/release/update-overlay/etc/init.d/S02update
4ed95d3f54ceafd8b71e1883e33faa45175f24a95dbb179419b8fad42bf1f025  AetherX6200Buildroot/br2_external/board/X6200/linux/rootfs-overlay/etc/init.d/S02update
```

`cmp` entre los dos: idénticos (548 líneas). El gate de paridad
(`tools/release/check_updater_parity.sh`) compara los dos archivos y sale 1 si
difieren; `make_release_assets.sh` lo corre con `BUILDROOT_S02UPDATE=<ruta>` y
avisa con un `SKIP` visible cuando no lo tiene. Ver `sello-y-paridad.txt`.

## Cambio por archivo

### `iariap/x6200_gui` (rama `factory/hardening-215`)

| archivo | qué cambia |
| --- | --- |
| `tools/release/update-overlay/etc/init.d/S02update` | **reescrito**: el updater del target. Verificación de sidecar SHA256 + `gzip -t` + recorrido completo del tar, preflight del paquete (nombres, tipos, duplicados, los puntos de montaje /mnt /dev /proc /sys /run /tmp, enlaces relativos que no se salgan del rootfs, inventario setuid exacto, updater `root:root 0755`), `/mnt` como montaje propio y escribible, **un** backup con el tar del target a `.partial` verificado y renombrado, lista de rutas nuevas, extracción con `pipefail`, verificación del árbol instalado (listado + hash del contenido), y recién ahí consumo del paquete y el sidecar. Cuatro mensajes al operador, en inglés. |
| `tools/release/make_update_tgz.sh` | inventario setuid **exacto** de la imagen (3: `bin/busybox` 4755 0:0, `usr/libexec/dbus-daemon-launch-helper` 4750 0:101, `usr/libexec/ssh-keysign` 4711 0:0) en staging y en el tgz; la aserción falsa de "5 bits setuid" se va; normaliza el overlay a `root:root 0755` **en staging** (antes hacía `chown` sobre el checkout); publica `<tgz>.sha256` y lo valida con `sha256sum -c`. |
| `tools/release/test_update.sh` | reescrito: sin nombre de tgz ni rootfs hardcodeados, se niega a borrar un directorio de trabajo con contenido, exige el sidecar, y corre los **dos** escenarios (A: imagen ya compatible; B: imagen histórica → `UNSUPPORTED/UNSAFE_LEGACY`, con el backup truncado medido). Ya no preinstala el overlay para "pasar": la primera actualización se reporta como insegura. |
| `tools/release/check_updater_parity.sh` | **nuevo**: gate de paridad de los dos `S02update`. |
| `tools/release/make_release_assets.sh` | valida el sidecar junto al tgz, llama al gate de paridad (`BUILDROOT_S02UPDATE`, `SKIP` visible si falta) y lista el sidecar entre los artefactos. |
| `tools/release/publish_release.sh` | exige y publica el sidecar junto al tgz (checksum propio y asset del release). |
| `docs/TESTING.md` | actualiza la sección de release/actualización: qué viaja a `/mnt` (los dos archivos), qué se verifica y en qué orden, qué **no** se promete (sin A/B, sin firma, sin rollback automático, corte de energía no garantizado), el primer update de una imagen histórica como `UNSUPPORTED/UNSAFE_LEGACY`, y las pruebas. |
| `tests/release/**` | **nuevo**: banco hermético del camino de actualización (contenedor privado con qemu-user + chroot sobre una copia del rootfs real + el BusyBox/bash del target), el generador de paquetes de fixture y su README. |

### `iariap/AetherX6200Buildroot` (rama `fix/rom-safe-update`)

| archivo | qué cambia |
| --- | --- |
| `br2_external/board/X6200/linux/rootfs-overlay/etc/init.d/S02update` | el mismo archivo, byte a byte (imagen SD nueva). |
| `br2_external/tests/README.md` | nota de dónde vive el banco del contrato y del gate de paridad; aclara que `fstab`/`S01create_data` no son de este cambio. |

## Método (política de laboratorio de la épica)

- **HOST + QEMU_USER/chroot privado** con el binario ARM real y el
  BusyBox/bash **del target**: el rootfs de la prueba sale de un paquete
  publicado de la ROM (copia), se extrae como root en el laboratorio privado y
  el updater corre dentro de un `chroot` con `/mnt` montado como tmpfs.
- **Recursos privados**: contenedor propio y desechable
  (`h215-x6200-update-lab`, construido de `tests/release/Dockerfile`), directorio
  de laboratorio propio y clon propio de Buildroot. **No** se usó
  `banco-apps`, `banco-audio`, `x6200-build-output`, ni rootfs/build dirs ajenos,
  ni se paró ni reutilizó ningún contenedor de otra corrida.
- Nada de SSH a la radio, flasheo, deploy, TX/RF, formateo de la SD del owner,
  releases ni QA físico. Sin `sudo` interactivo (el banco corre como root dentro
  de **su** contenedor, no en el host).
- El fixture es una **copia** de un paquete publicado
  (`x6200_r1cbu_v0.1.2-lu2dsx.tgz`, 102 266 238 B, sha256
  `7d6179dd83e0015a8144cce597c7ffc61c90ce76abbc8c37cf26335fbaf08460`), usado sólo
  como árbol de la "imagen" y contraste histórico. El árbol del fixture se normaliza
  al inventario auditado (3 setuid: `bin/mount` y `bin/umount` quedan 0755) y su
  `etc/init.d/S02update` se reemplaza por el nuevo — así el fixture representa la
  **imagen auditada** con el updater nuevo.

### LIMITACIONES

- El fixture tiene **5483** entradas (no las 5466 del rootfs auditado: el
  número es del artefacto publicado, que es anterior). Las cifras de la matriz
  son del fixture y están indicadas como tales.
- El banco no es un boot de kernel/init ni corre en la radio: es un `chroot`
  con los binarios del target, sin RF ni SD física. La prueba en radio con una
  SD virgen (y el primer update de una imagen histórica) queda como
  `PENDING_HARDWARE`, en el pipeline de QA remoto separado.
- `df`/`mount` dentro de un `chroot` no ven `/proc/mounts` con los caminos del
  target: por eso el chequeo de `/mnt` del updater compara el **filesystem**
  contra `/` (dispositivo, tamaño, usado, punto de montaje) en vez de exigir la
  cadena `/mnt` en la tabla de montajes. Mismo comportamiento en la radio.
- El banco de `br2_external/tests/` del hijo 1 (#212) no se toca.

## Matriz de casos

Los casos con las condiciones, los comandos y el efecto observado están en
`lab/results.tsv` y, caso por caso, en `lab/cases/<caso>/{stdout.txt,stderr.txt,steps.txt}`
(salida cruda del updater y de cada comprobación). Resumen del veredicto del banco:

| caso | condición (qué fuerza) | veredicto |
| --- | --- | --- |
| `types-rule` | la regla de tipos y de targets de enlaces, contra la tabla de `tests/release/check_types_rule.py` (19 casos) | PASS |
| `good-first-apply` | paquete bueno (5483 entradas) con sidecar: preflight, backup, extraccion, verificacion y consumo | PASS (rc=0, `Update applied`, stderr vacio) |
| `good-added-paths` | paquete que agrega dos archivos: se instalan y quedan en `/mnt/new_*.list`, no en el backup | PASS |
| `subsequent-apply` | dos paquetes en `/mnt`: se aplican en orden, con un backup por paquete | PASS |
| `sidecar-missing` | paquete sin sidecar | PASS |
| `sidecar-bad-hash` | sidecar con SHA256 que no es el del paquete | PASS |
| `sidecar-wrong-name` | sidecar que nombra otro archivo | PASS |
| `truncated-tgz` | tgz cortado (con sidecar correcto): lo detecta `gzip -t` | PASS |
| `absolute-entry` | nombre absoluto en el paquete (el tar lo recorta al listar y lo avisa) | PASS |
| `dotdot-entry` | nombre con `..` en el paquete | PASS |
| `dot-entry` | nombre con `./` adelante | PASS |
| `duplicate-entry` | entrada duplicada | PASS |
| `forbidden-mnt / -dev / -tmp` | entradas que reemplazarian el punto de montaje `/mnt`, `/dev`, `/tmp` | PASS (3 casos) |
| `fifo-entry / device-entry` | fifo y nodo de dispositivo en el paquete | PASS (2 casos) |
| `unexpected-setuid` | un setuid de mas (inventario exacto de la imagen) | PASS |
| `symlink-dotdot` | symlink relativo cuyo target se sale del rootfs | PASS |
| `updater-uid-1000` | `S02update` con uid 1000 (el bug del empaquetador viejo) | PASS |
| `symlink-absolute-admitted` | paquete (armado con el empaquetador real) con un symlink de target absoluto, como `etc/alsa/conf.d/*` de la rootfs | **PASS** (rc=0, `Update applied`, el symlink queda con su target) |
| `extract-fails` | tar que falla al extraer **despues** del preflight y del backup (fixture: un archivo bajo `etc/passwd`, que en el sistema es un archivo) | **PASS** (`Update failed: package retained; restore backup manually`, paquete y backup retenidos, nada bajo `/` cambia, reuso del backup identico y conflicto con otro contenido) |
| `entry-over-directory` | paquete que declara un **archivo** en una ruta que en el sistema es un **directorio** (`usr/sbin`) | PASS (fija el comportamiento observado: `Update backup failed`, fail-closed; ver el hallazgo) |
| `no-space` | `/mnt` de 150 MiB: el paquete (100 MiB) entra, el backup (250 MiB) no | **PASS** (`Update backup failed`, no se extrae, el paquete queda) |
| `readonly-mnt` | `/mnt` montado read-only | PASS |
| `mnt-not-mounted` | `/mnt` sin montar | PASS |
| `legacy-first-update` | imagen historica: la corre el `S02update` stock (xargs) | UNSUPPORTED/UNSAFE_LEGACY (backup truncado de ~704 de 5483 entradas, tgz borrado sin verificar, sidecar tirado) |

## Gates

| gate | comando | resultado |
| --- | --- | --- |
| gates repetidos al cierre | nativos + `audita-pruebas` + regla de tipos | ver `gates-repetidos.txt` (138/0, OK, 19/19) |
| matriz de la app (al cierre) | `scripts/apps/x6200/check-matrix.sh origin/lu2dsx/x6200-rom HEAD <wt>` sobre el commit de cierre | **rc=0** (`themes` ok, `contracts` ok, `bash -n` de los 6 scripts ok; SKIPs con motivo; ver `gates-check-matrix-cierre.txt`) |
| sintaxis del target | `busybox sh -n` (BusyBox 1.36.1 del target, vía qemu) y `bash -n` | ver `gates-sintaxis.txt` |
| sintaxis de las herramientas | `bash -n` de los 6 scripts de `tools/release/` | ver `gates-sintaxis.txt` |
| tests nativos | `sh tests/native/run_native_tests.sh` | ver `gates-native.txt` |
| registro de pruebas | `python3 tests/audita-pruebas.py` | ver `gates-audita-pruebas.txt` |
| identidad del firmware | `python3 tools/verifica-firmware-port.py --tar <paquete>` | ver `gates-verifica-firmware-port.txt` |
| paridad del updater | `tools/release/check_updater_parity.sh <buildroot>/…` | ver `sello-y-paridad.txt` |
| matriz de la app | `scripts/apps/x6200/check-matrix.sh origin/lu2dsx/x6200-rom HEAD <worktree>` | ver `gates-check-matrix.txt` |
| suite del repo | `sh tests/run_all_tests.sh --quick` | **rc=0, 17 OK, 0 fallos** (SKIP declarados: wspr/sstv/wefax por la imagen mínima sin `fftw3.h`/`sndfile.h`, js8 por `--quick`, webserver por falta del checkout; ver `gates-run-all-tests.txt`) |
| banco del contrato | `tests/release/test_safe_update.sh <fixture> <lab>` | ver `lab/run.log` + `lab/results.tsv` |

### SKIP / BLOCKED / PENDIENTE razonados

- **Paridad sin ruta**: `check_updater_parity.sh` sin argumento (ni
  `BUILDROOT_S02UPDATE`) sale 2 y dice que no adivina un checkout hermano: hay
  varios worktrees en vuelo y uno ajeno daría un FAIL falso. En el release,
  `make_release_assets.sh` avisa con un `SKIP` visible. En esta corrida el gate
  se corrió con la ruta explícita (`sello-y-paridad.txt`).
- **`sh tests/run_all_tests.sh --quick`**: corrido al cierre → **rc=0, 17 OK, 0
  fallos**. Tres SKIP legítimos de los *codecs*: `wspr` ("la imagen
  x6200-uiharness no trae /usr/include/fftw3.h: build full: docker build -t
  x6200-uiharness tools/ui_harness"), `sstv` y `wefax` (idem por
  `/usr/include/sndfile.h`): son las deps que sólo instala el stage *full* de la
  imagen y el runner los cuenta como omitidos, nunca como pase. Más `js8`
  (omitido por `--quick`) y `webserver` (sin el checkout del configurador).
- **`x6200-ab contracts`** (dentro de `check-matrix.sh`): si falta la imagen
  `x6200-uiharness` y no puede construirla en el runner, la matriz de la app lo
  reporta como `BLOCKED` con su salida real (nunca como pase).
- **Banco QEMU del skill de la app** (`tools/captura-banco.sh`): no corre acá
  porque el diff no toca `src/**` ni `tools/ui_harness/**`; la matriz de la app
  lo informa como SKIP con ese motivo.
- **Radio**: `PENDING_HARDWARE` (pipeline de QA remoto separado).
- **Corrida del banco (cierre)**: la corrida final (D) corrió los **27 casos**
  completos con **exit=0**: **26 PASS, 0 FAIL y 1 `UNSUPPORTED/UNSAFE_LEGACY`**
  (`lab/results.tsv`, `lab/run-final-D.log.gz`, `lab/cases/<caso>/…`). Las dos
  corridas anteriores (9 y 10) habían quedado cortadas por reinicios del host con
  21 casos verdes y cero `FAIL`; sus logs quedan en `corridas/`.
- **`extract-fails` cambió de fixture**: el que usaba antes (un archivo en la
  ruta de un directorio del sistema, `usr/sbin`) no llegaba a la extracción: la
  interceptaba el backup (ver el hallazgo de abajo). Ahora usa un archivo bajo
  `etc/passwd` (un archivo en el sistema): pasa el preflight, pasa el backup y el
  tar de la extracción falla con "Not a directory", que es lo que el contrato
  pide. El fixture viejo quedó como caso propio (`entry-over-directory`).

### Hallazgo del banco: un paquete que declara un archivo donde el sistema tiene un directorio

Fixture: un paquete armado con el empaquetador real que declara `usr/sbin` como
**archivo**, sobre un sistema donde `usr/sbin` es un **directorio**. Salida real
del updater (caso `entry-over-directory`, `lab/cases/entry-over-directory/`):

    stdout: Update backup failed
    stderr:   reason: the backup does not list the same paths it was given

Por qué: la ruta existe, así que entra a la lista del backup; el tar del target
lista un directorio con barra final (`usr/sbin/`) y la comparación de nombres del
backup no coincide, así que el updater corta ahí. Efecto: **fail-closed** — nada
bajo `/` cambia, el paquete y el sidecar quedan, no se crea backup ni `.partial`
— que es lo que el contrato quiere ante un paquete que no se puede aplicar. Lo
mejorable es la **diagnosis**: el preflight no cruza los tipos del paquete contra
el sistema, así que el mensaje apunta al backup y no al paquete (con el contrato
en la mano, lo correcto sería `Update verification failed` en el preflight).

No se cambió el updater por esto: los PRs ya estaban publicados, el contrato fija
los cuatro mensajes y el caso no puede darse con un paquete del empaquetador real
(sólo emite entradas de archivo/enlace de la misma rootfs). Queda como hallazgo
para el orquestador (un chequeo de preflight "esta ruta es un directorio en el
sistema y el paquete la declara como archivo" lo rechazaría antes de armar el
backup) y el comportamiento observado queda **fijado por el caso
`entry-over-directory`**, para que no se degrade sin que se vea.

## Interpretaciones del contrato (lo que la realidad del árbol obligó a precisar)

- **"sin sobreescribir directamente /mnt, /dev, /proc, /sys, /run, /tmp"**: se
  rechaza la **entrada del punto de montaje** (`mnt`, `dev`, ...), no sus hijos.
  Los hijos tienen que poder viajar: la rootfs auditada trae symlinks bajo `/dev`
  (`dev/fd -> ../proc/self/fd`, `dev/stdin`, `dev/log`) y sin ellos la
  actualización no puede restaurar lo que reemplaza. Rechazar todo `/dev` hacía
  que el paquete bueno fuera rechazado (medido, ver `lab/results.tsv` de las
  corridas previas).
- **"sólo admitir ficheros y symlinks relativos bajo rootfs"**: se interpreta
  sobre los **nombres** de las entradas (que es lo que decide dónde escribe la
  extracción). El **target** de un symlink puede ser absoluto: la rootfs auditada
  los trae (`etc/alsa/conf.d/50-arcam-av-ctl.conf -> /usr/share/alsa/...` y
  varios más) y un enlace sólo crea un enlace, que se resuelve dentro del mismo
  rootfs. Lo que sí se rechaza es un target relativo con tantos `..` que se salga
  del rootfs (defensa en profundidad; la rootfs auditada no tiene ninguno).
- **Listado limpio**: el tar del target recorta un `/` inicial o un `../` de un
  nombre *al listar* y lo avisa por stderr, así que el nombre recortado ya no
  está en la lista. El preflight toma ese aviso como paquete inválido (cualquier
  salida de stderr en el listado), además de las reglas sobre los nombres.
- **Modos y dueños del setuid**: se validan exactos en el empaquetador (sobre el
  árbol en staging, con `find -printf '%P %m %U %G'`) y el updater valida el
  inventario exacto (rutas, modo y dueño) del paquete antes de tocar `/`.

## Riesgos residuales y PENDING_HARDWARE

- **Corte de energía en medio de la extracción**: no está garantizado como
  recuperable (rootfs único, sin A/B). El procedimiento manual es restaurar
  `/mnt/backup_<tgz>` (o reimagenar con la imagen anterior). No se ejecutaron
  cortes físicos.
- **Primer update de una imagen histórica**: lo corre el `S02update` viejo de esa
  imagen, que no verifica nada (medido: backup truncado de 704 de 5483 entradas y
  tgz borrado sin comprobar). Se reporta `UNSUPPORTED/UNSAFE_LEGACY`, nunca como
  pase. Recomendación: reimagenar una tarjeta de laboratorio.
- **Sin firma**: el SHA256 con sidecar detecta corrupción accidental, no prueba
  autenticidad.
- `PENDING_HARDWARE`: actualización real en la radio (con SD virgen y con una
  imagen histórica), y el resto de la matriz en el pipeline de QA remoto.

## Pendientes

- Registrar la corrida en `docs/PRUEBAS.md` (el registro de pruebas del repo
  pide el md5 del binario de la GUI de la corrida; acá se usó un paquete
  publicado, no un build de la corrida).
- El post-verify comprueba que la GUI **instalada** sea un ELF de al menos 1 MiB,
  no que el paquete la traiga: un paquete sin GUI pasa esa comprobación (el
  empaquetador siempre la incluye y el gate de release compara la GUI del tgz
  con la de la imagen; queda anotado por si el orquestador quiere endurecerlo).
- `PENDING_HARDWARE`: la actualización real en la radio (SD virgen y una imagen
  histórica) corre en el pipeline de QA remoto separado, con ventana y SHA.
