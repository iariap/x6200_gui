# Cómo probar el firmware X6200 sin el equipo físico

Documento de la revisión de ROM del 2026-10-11, actualizado con el estado del programa de port. Complementa a [`ROM_REVIEW.md`](ROM_REVIEW.md) (auditoría de la imagen) y [`TRD.md`](TRD.md) (detalle técnico).

## Resumen: qué se puede y qué no

| Nivel | Qué valida | Necesita hardware | Estado |
|---|---|---|---|
| 1. Verificación offline de la imagen | Integridad, particiones, rootfs, dependencias, strings | No | ✅ hecho |
| 2. Tests unitarios nativos | Lógica de `adaptive_tuning` y `audio_enhancement` | No | ✅ hecho (55 checks) |
| 3. Tests en la ISA del objetivo | Lo mismo, compilado con el toolchain ARM y ejecutado con `qemu-arm-static` | No | ✅ hecho (55 checks) |
| 4. Validación del binario real | Cierre de dependencias del `x6200_gui` embebido con el `ld.so` del rootfs | No | ✅ hecho (41 libs, 0 faltantes) |
| 5. Arranque completo (QEMU system) | Kernel, init, servicios, ausencia de crashes del rootfs | No | 🔜 factible, no ejecutado |
| 6. Simulador LVGL en el escritorio | UI real (About, Ajustes, waterfall, timers) con clics y screenshots | No | 🔜 requiere portar la app a un target "sim" |
| — | Consumo real, RF, audio, LCD/táctil, térmica, cat (serial) | **Sí** | ❌ imposible sin equipo |


---

## Pruebas y capturas sin radio

El procedimiento completo (tests ARM con `qemu-arm-static`, el **banco** con framebuffer falso para
capturas, y como **comandar la app desde afuera** por su socket) esta en el skill
[`.agents/pruebas-sin-radio/SKILL.md`](../.agents/pruebas-sin-radio/SKILL.md), con las garantias que
da `tools/captura-banco.sh`: prueba la **ultima version** (reconstruye si el fuente es mas nuevo e
imprime commit + md5) y usa el **socket correcto** (limpia socket/framebuffer viejos y hace handshake
`{"cmd":"get"}` antes de mandar nada).

### Barrido QEMU de las 26 apps con recursos privados (issue #222)

`tests/qemu/run_apps_qemu.sh` corre las **26 apps** de `remote_server.cpp::app_names[]` (sin
`off`) en **3 rondas** contra el **binario ARM real** en un chroot propio, con el framebuffer
falso y el socket de control del banco. No es un harness nuevo: reusa el binario, `libfakefb`,
`x6200-remote`, el socket y las primitivas de captura; lo que cambia es el **aislamiento**.

```bash
tests/qemu/run_apps_qemu.sh --check          # preflight: dice exactamente que falta (no toca nada)
RONDAS=3 DWELL=2 tests/qemu/run_apps_qemu.sh # el contrato completo del issue #222
```

- **Recursos privados**: contenedor, volumen de build y laboratorio propios de la corrida
  (`CONT`/`VOL`/`LAB`, por defecto `x6200-next222-qemu` / `x6200-next222-build` /
  `/tmp/x6200-next222-qemu`). El script **se frena** si le apuntan `banco-apps`, `banco-audio`,
  `x6200-build-output` o un worktree de otra corrida: el aislamiento es un chequeo, no un
  comentario.
- **Capa**: `QEMU_USER` (userland armv7 por `binfmt_misc`, kernel del host). No hay kernel/init
  real, no hay radio, no hay RF y no se toca la SD del owner.
- **Por app** (`tests/qemu/barrido_qemu.py`): `get` del estado inicial, `app <nombre>`,
  confirmacion de que la app quedo abierta, PNG real 800x480, permanencia, `app off`,
  confirmacion de la home y estabilidad de PID, `boot_id`, `logrev`, freq/mode/band y de la
  cuenta de fds/hilos. Un paso de mas de 5 s, un `CRASHLOG`, un `SIGSEGV`, una TX no solicitada,
  un estado persistido cambiado o una fuga de fds/hilos son **FAIL**.
- **Nunca PASS sin salida real**: sin handshake, sin captura con dibujo o sin la app abierta el
  resultado es `FAIL` o `BLOCKED`, y el runner sale 1/77 en vez de 0. Las apps que necesitan un
  periferico o servicio ausente (GPS, WiFi, Bluetooth, red) se declaran `PARTIAL`: foto real del
  mensaje controlado, **nunca** PASS del dispositivo.
- **Sin el entorno privado** el runner declara `SKIP` (exit 77) con la lista exacta de lo que
  falta: no corre a medias ni cambia un SKIP por un verde. El banco del owner
  (`tools/captura-banco.sh`) sigue siendo el camino canonico cuando sus recursos estan libres.

### Cobertura de los 23 dialogos inertes (issue #222)

El arnes de host declara **23** constructores de dialogo como no-op
(`tools/ui_harness/mocks/mock_main_screen.c`), asi que esas ventanas no se renderizan en el
host. La tabla `tests/qemu/dialog-coverage.csv` registra, por dialogo, la fuente de produccion,
el fixture de host que llega a su camino (si hay), la app del barrido que lo abre y el resultado
QEMU esperado/medido, con la captura o la razon exacta de bloqueo. El gate
`tests/check_dialog_coverage.py` (en `tests/run_all_tests.sh` y en el CI) falla si el mock, las
fuentes o `app_names[]` cambian sin actualizar la tabla, y si una fila declara cobertura real
sin captura. El harness de host **sigue siendo sintetico** para esos 23: su etiqueta no se
promueve a REAL.

### Baseline reproducible del release (issue #222)

`tests/release/baseline_manifest.py` emite el manifiesto de identidad (`GUI_SHA`,
`Buildroot_SHA`, `LVGL_gitlink`, `fuente-md5`, md5/sha256 del binario, hashes de
`sdcard`/`rootfs`, `QEMU_CPU`, imagen/contenedor, seed) y `--verify` comprueba que el manifiesto
es **reproducible** (dos corridas sobre el mismo arbol dan el mismo documento). Lo que no esta
disponible se declara `PENDING` con su razon; un campo ausente nunca se lee como cero.

```bash
tests/release/baseline_manifest.py --txt
tests/release/baseline_manifest.py --verify
WT=<worktree> BR=<clon Buildroot> OUT=<salida> ARTEFACTOS=<dir> tests/release/baseline_manifest.py > baseline.json
```

## QA en la radio: monitoreo, pruebas, triage, issue y evidencia

El ciclo completo —monitorear la rama del port, compilar limpio, desplegar por WiFi, probar en el
equipo, hacer el **triage** de lo que falla, cargar el **issue** (buscando antes si ya existe) y dejar
la **evidencia** en `docs/evidencia/` con el **registro** en [`PRUEBAS.md`](PRUEBAS.md)— esta en el
skill [`.agents/qa-radio/SKILL.md`](../.agents/qa-radio/SKILL.md). Incluye la tabla de errores ya
cometidos (build incremental stale, IP fija, issue duplicado, escribir en el arbol compartido,
`grep` sin `-a` sobre un binario) para no repetirlos, y la politica de transmision: solo la banda de la
antena, en QRP, con autorizacion del dueño.

## Estado actual (2026-10-12): todo en un comando

```bash
cd x6200_gui
tests/run_all_tests.sh            # todo
tests/run_all_tests.sh --quick    # sin la suite de JS8 (que es la más lenta)
```

Corre y resume, dejando el log de cada suite en `/tmp/<nombre>.log`:

| Suite | Qué cubre | Resultado |
|---|---|---|
| `host` | arnés nativo: aguja/band plan/AVI, UDP de FT8, aviso de versión, wfview (UDP real), cluster, Field Ops, temas JSON, teclado, Bluetooth, países y banderas, WSPR | **469 comprobaciones, 0 fallos** |
| `wspr` | round-trip del motor con la herramienta del X6100 (decodifica a −20 y −25 dB) | **158 comprobaciones, 0 fallos** |
| `sstv` | los 7 modos, correlación 0,92–0,98 | **all OK** |
| `wefax` | 300/300 líneas, correlación 0,995 | **PASS** |
| `navtex` | SITOR-B a +10 y +5 dB (el caso de 0 dB es informativo) | **all OK** |
| `js8` | **la suite completa del X6100** contra nuestro port | **1994 aserciones en 88 casos** |
| `webserver` | las 19 páginas/endpoints del web configurator, con el python del propio equipo | **19 comprobaciones, 0 fallos** |

> **Conteos al 2026-10-22** (tip `f825fe4b`, corrida de #222; la tabla de arriba es el estado
> del 2026-10-12 y se deja como historico): `tests/native/run_native_tests.sh` = **52.946
> comprobaciones, 0 fallos** en 24 etapas (incluye Propagacion #145 con 44.738 y SWR Scan #125
> con 4.851); `--ci-parity` da lo mismo con `lvgl/` oculto. `tests/run_all_tests.sh --quick` =
> **20 OK, 0 fallos** con 6 omitidos (`wspr`, `sstv`, `wefax` y `js8` por las deps de codec de la
> imagen `ci`, `webserver` sin checkout, `qemu_apps` sin banco privado). Los numeros viejos de
> la tabla son de su fecha y **no** se reemplazan: cada uno vale para el SHA que midio.

La parte del webserver usa el python **del target** (ARM, vía binfmt) porque es el que trae
`bottle`; el runner lo omite si no encuentra el checkout o la imagen de build.

Qué lleva exactamente la imagen que se publica (verificado sobre el artefacto, no sobre el
árbol de compilación) está en [`IMAGE_INVENTORY.md`](IMAGE_INVENTORY.md).

Lo que sigue necesitando la radio (🔌) está listado en
[`INTEGRATION_AUDIT.md`](INTEGRATION_AUDIT.md) §3.


### Publicar un release

```bash
cd x6200_gui
sudo tools/release/make_release_assets.sh <imagen.img> <versión>   # imagen + tgz + tgz.sha256 + SHA256SUMS
tools/release/publish_release.sh <tag> <versión>              # ensayo: verifica y muestra el comando
tools/release/publish_release.sh <tag> <versión> --publish    # publica
```

El segundo **verifica antes de publicar**: que existan la imagen, el tgz y su sidecar, que los
checksums coincidan, que el tgz de actualización traiga **el mismo binario GUI** que la imagen (así
una actualización en el lugar instala exactamente lo que instala la imagen), que el árbol esté
limpio y pusheado, que la suite pase y que las notas del release existan. Sin `--publish` se detiene
después de las verificaciones y **muestra el comando que ejecutaría**, así se puede ensayar.

`make_release_assets.sh` corre además el **gate de paridad del updater**
([`check_updater_parity.sh`](../tools/release/check_updater_parity.sh)): el `S02update` del overlay
de este repo y el de la imagen de `AetherX6200Buildroot` tienen que ser **el mismo archivo**. Si no
hay checkout de Buildroot a mano lo informa como `SKIP` (visible, nunca como pase) y la integración
lo corre con la ruta explícita.

**El tgz de actualización necesita root** (`sudo`), porque el rootfs trae archivos setuid y hay que
preservarlos: extrayéndolo como usuario común se pierden y una actualización en el lugar rompería
esos binarios. `tools/release/make_update_tgz.sh` lo arma y comprueba el **inventario setuid exacto
de la imagen auditada** (3): `bin/busybox` 4755 root:root, `usr/libexec/dbus-daemon-launch-helper`
4750 root:101 y `usr/libexec/ssh-keysign` 4711 root:root. `bin/mount` y `bin/umount` son **0755**:
la aserción vieja de "5 bits setuid" era falsa y abortaba la herramienta sobre la imagen buena.

### Actualización en el lugar: qué viaja y qué se verifica

Cada paquete viaja con **su sidecar** `<tgz>.sha256` (una línea `<hash>  <nombre>`), y los dos
archivos van juntos a `/mnt` de la radio:

```bash
cp x6200_r1cbu_<versión>.tgz x6200_r1cbu_<versión>.tgz.sha256 /mnt/
```

`etc/init.d/S02update` es **el mismo script en los dos puntos de instalación** (el overlay de este
repo, que viaja dentro del tgz, y `rootfs-overlay` de `AetherX6200Buildroot`, que es el que corre
una tarjeta recién grabada). Antes de tocar `/`, para cada `/mnt/x6200_r1cbu_*.tgz`:

1. exige `/mnt` como **montaje propio y escribible** (si no, el backup caería en el rootfs);
2. exige el **sidecar** y que su SHA256 sea el del paquete, que el gzip esté completo (`gzip -t`) y
   que el tar se recorra entero;
3. hace un **preflight de la lista del propio paquete**: nombres relativos, sin `.` ni `..`, sin
   duplicados, sólo archivos, symlinks y hard links (nada de directorios, nodos ni fifos), sin poder
   reemplazar **el punto de montaje** `/mnt`, `/dev`, `/proc`, `/sys`, `/run` ni `/tmp` (los hijos sí
   viajan: la rootfs auditada trae symlinks bajo `/dev`), con un listado que salga **limpio** (el tar
   del target avisa por stderr cuando tiene que recortar un `/` inicial o un `../` de un nombre, que
   es justo lo que el contrato prohíbe), los enlaces relativos que puedan salirse del rootfs
   (un `..` se admite mientras el destino quede adentro: la imagen auditada trae
   `dev/fd -> ../proc/self/fd`), el inventario setuid exacto de la imagen y el propio `S02update`
   como `root:root 0755`;
4. hace **UN** backup con el tar del target (nunca `xargs`, que parte la lista y deja un archivo con
   varios tar concatenados de los que sólo se lee el primero): va a
   `/mnt/backup_<tgz>.partial`, se verifica (mismas rutas, y el contenido de cada archivo igual al
   de disco) y recién ahí se renombra a `/mnt/backup_<tgz>`. Las rutas que el paquete **agrega**
   quedan listadas en `/mnt/new_<tgz>.list` para poder sacarlas a mano;
5. recién entonces descomprime sobre `/` con `gzip -dc | tar -x` y `pipefail`, así el exit de los
   dos lados es real. Si falla: `Update failed: package retained; restore backup manually`, no borra
   nada y no sigue con otro paquete;
6. después compara el árbol instalado con el paquete (mismo listado y mismo hash del contenido
   concatenado), exige el updater en `root:root 0755` y que la GUI sea un ELF de al menos 1 MiB;
   **sólo entonces** borra el tgz y el sidecar. El backup verificado queda.

Los mensajes al operador son cuatro, en inglés: `Update applied`, `Update verification failed`,
`Update backup failed` y `Update failed: package retained; restore backup manually`.

**Lo que esto no es**: no hay A/B, no hay firma (el SHA256 con sidecar detecta corrupción
accidental, no prueba autenticidad) y no hay rollback automático. Un corte de energía **en medio**
de descomprimir sobre el único rootfs **no** está garantizado como recuperable: para volver atrás
hay que restaurar a mano desde `/mnt/backup_<tgz>` (o reimagenar la SD con la imagen anterior).

### El primer update de una imagen histórica

Una imagen que todavía corre el `S02update` **viejo** (el stock de Buildroot, `xargs`) ejecuta **su
propio** script en el primer update en el lugar: ese script no mira el sidecar, deja el backup
truncado y borra el tgz sin comprobarlo. Por eso **el primer update de una imagen histórica NO queda
protegido por este contrato** y se reporta `UNSUPPORTED/UNSAFE_LEGACY`, nunca como pase de
seguridad. La recomendación es **reimagenar** una tarjeta de laboratorio (con backup y restauración
manual) en vez de migrar por update en el lugar. Después de ese primer update la imagen queda con el
updater nuevo y **las actualizaciones siguientes sí quedan protegidas**.

### Las pruebas del camino de actualización

En un chroot sobre una copia del rootfs, con el bash y el BusyBox del target:

```bash
sudo x6200_gui/tools/release/test_update.sh <rootfs.tar> <paquete.tgz> [directorio de trabajo]
```

Corre los dos escenarios (A: imagen ya compatible → contrato completo; B: imagen histórica →
`UNSUPPORTED/UNSAFE_LEGACY`) y exige el sidecar junto al paquete. No borra nada por su cuenta: si le
pasás un directorio de trabajo, tiene que estar vacío. El directorio no puede tener datos reales.

El banco **hermético y privado** de la matriz de casos (contenedor desechable con qemu-user, chroot
con el rootfs real, sin tocar la radio ni los volúmenes compartidos) está en
[`tests/release/test_safe_update.sh`](../tests/release/test_safe_update.sh) con su
[README](../tests/release/README.md): paquete bueno, segunda aplicación, rutas nuevas, backup
verificado y restaurado a mano, sidecar faltante / con hash malo / con otro nombre, tgz truncado,
entradas absolutas / `..` / `./` / duplicadas / bajo `/mnt` `/dev` `/tmp`, nodos, setuid de más,
updater con uid 1000, tar que falla al extraer, reuso del backup idéntico, conflicto de backup, sin
espacio, `/mnt` read-only, `/mnt` sin montar y el primer update de una imagen histórica.

**El tgz lleva sólo archivos y enlaces**: ni `./` ni entradas de directorio. Con una entrada `./` el
backup del script stock recibe `./` y archiva el sistema **entero** —incluido `/mnt`, donde está
escribiendo el backup—. Con sólo archivos, el backup contiene exactamente los archivos que se
reemplazan (0 directorios), y el preflight del updater nuevo rechaza cualquier paquete que se salga
de eso.

### Análisis estático

El arnés corre además `gcc -fanalyzer` sobre los módulos que compilan con los *stubs* del host
(`adaptive_tuning`, `audio_enhancement`, `ft8_udp`, `update_check`, `band_plan_data`, `avi_writer`,
`bt_devices`, `kbd_rollover`). **`wfview_server` queda afuera a propósito**: abre los tres sockets
del servidor y los mantiene mientras vive el proceso, y el verificador de fugas de descriptores lo
lee como una fuga. Ese análisis encontró una fuga real y cuatro truncamientos posibles (ver
`NEW_FEATURES.md` §30).

---

## Nivel 2: tests unitarios nativos

Arranque sin red, sin Catch2 y sin hardware: LVGL, liquid-dsp y SpeexDSP se sustituyen por *stubs* deterministas.

```bash
cd x6200_gui
tests/native/run_native_tests.sh          # x86 (usa ASan/UBSan si el toolchain los tiene)
```

Cubre:

- `adaptive_tuning`: init, curva velocidad→multiplicador monótona, un solo clic = ×0.1, rotación lenta < ×1, rotación rápida ≥ ×20, timeout de 500 ms, *decay* y el *gating* por el ajuste **Freq acceleration** (none = 1×, lite ≤ 10×, strong ≤ 100×).
- `audio_enhancement`: preset OFF bit-exacto, *clamping* de presets fuera de rango, **procesado de bloques completos** (regresión del bug que descartaba >256 muestras), determinismo, NR sólo en tramas completas con cola intacta, límites exactos de trama (256/257), ganancia del leveler acotada y API de notches.

### Los tests detectan realmente los bugs (prueba de mutación)

| Mutación | Fallos detectados |
|---|---|
| Reintroducir la guarda vieja `nsamples > AUDIO_ENH_FRAME_SIZE → return` | 5 |
| Anular el *gating* del multiplicador | 3 |

### Bug latente encontrado por los tests

`audio_enh_preset_get_current()` estaba **declarada en el header pero sin implementar**: cualquier llamador habría fallado al enlazar. Se implementó. Además se corrigió un aviso `-Wswitch` en `audio_enh_preset_load()`.

---

## Nivel 3: misma lógica en la ISA del X6200

Compila los tests con el compilador cruzado real (Buildroot, gcc 12.3.0, `armv7-a` + NEON) y los ejecuta con emulación de usuario:

```bash
docker run --rm --entrypoint="" -v "$(pwd):/src" -v x6200-build-output:/b -w /src x6200-buildroot bash -c '
  /b/host/bin/arm-buildroot-linux-gnueabihf-gcc -std=gnu11 -O1 -static \
    -I x6200_gui/tests/native/stubs -I x6200_gui/src \
    x6200_gui/src/adaptive_tuning.c x6200_gui/src/audio_enhancement.c \
    x6200_gui/tests/native/stubs/stubs.c x6200_gui/tests/native/test_main.c \
    x6200_gui/tests/native/test_adaptive_tuning.c x6200_gui/tests/native/test_audio_enhancement.c \
    -lm -o /src/x6200_gui/tests/native/build/arm_tests'

qemu-arm-static x6200_gui/tests/native/build/arm_tests
```

Sirve para detectar diferencias de comportamiento en ARM (tipos, *casts*, redondeo, alineación).

---

## Nivel 4: validar el binario que va en la ROM

Extrae el rootfs de la imagen y usa su propio cargador para resolver todas las dependencias:

```bash
dd if=images/x6200_YYYYMMDD_HHMMSS.img of=/tmp/rootfs.ext4 bs=1M skip=33 count=512
mkdir -p /tmp/root && 7z x -o/tmp/root /tmp/rootfs.ext4
LD_TRACE_LOADED_OBJECTS=1 qemu-arm-static -L /tmp/root /tmp/root/usr/sbin/x6200_gui
```

Resultado con la imagen `x6200_20261011_201456.img`: **41 bibliotecas resueltas, 0 faltantes**.

También se puede arrancar el binario (`qemu-arm-static -strace -L /tmp/root …`): llega a la inicialización de LVGL y aborta al no existir `/dev/fb0` ni `/dev/input/event*`, que es lo esperable sin hardware. **No sirve para probar la UI**, sólo el arranque y la carga de librerías.

---

## Nivel 5 (propuesto): arranque completo en QEMU system

El R16 del X6200 es `sun8i`; QEMU emula máquinas de la misma familia (H3): `orangepi-pc`, `bananapi-m2u`, `bananapi-m2m`. El kernel 6.1.82 del firmware es un kernel sunxi multiplataforma, así que se puede intentar:

```bash
# en un contenedor con qemu-system-arm instalado (requiere red)
qemu-system-arm -M orangepi-pc -m 512 -nographic \
  -kernel zImage -dtb <dtb-de-qemu> \
  -drive if=sd,format=raw,file=rootfs.ext4 \
  -append "console=ttyS0 root=/dev/mmcblk0p2 rootwait"
```

Qué validaría: arranque del kernel, montaje del ext4, `init` de BusyBox, scripts `S01..S96` (red, bluetooth, alsa, `S95gui`) y ausencia de crashes/`not found` al iniciar servicios.
Qué **no**: LCD, audio, CAT/serial con la radio, GPS, consumo.

Alternativa más simple y también útil: `qemu-system-arm -M virt` con bus virtio, si el kernel trae esos drivers.

---

## Nivel 6 (propuesto): simulador LVGL en el escritorio

El repo ya incluye `lv_drivers` con soporte SDL y `lv_drv_conf.h` con conmutadores. Con un target nativo `sim` (LVGL + driver SDL/MONITOR + *stubs* de `libaether_x6200_control`, PulseAudio y CAT) se podría:

- abrir el **About**, la pantalla principal y **Ajustes** con las filas nuevas ("Waterfall battery mode", "Audio enhance");
- girar el encoder virtual y comprobar el multiplicador adaptativo, el *timeout* de la etiqueta `x%.1f` y el efecto de "Freq acceleration";
- capturar screenshots y compararlos en cada build.

Es el mayor esfuerzo de la lista (hay que aislar las dependencias de radio/audio), pero es la única forma de probar la UI sin el equipo.

---

## Lo que sólo se puede medir con la radio

- Consumo en RX/TX y autonomía real (los 25–35 % del TRD siguen siendo estimaciones).
- Comportamiento de RF, sensibilidad, DSP en condiciones reales de señal.
- Audio (codec ALSA/PulseAudio, latencia, calidad del NR y del leveler).
- Pantalla RGB666, táctil/encoder físico, backlight.
- CAT/serial con la radio conectada y estabilidad térmica en uso prolongado.

**Alternativa práctica** sin comprar nada: pedirle a otro operador con X6200 que flashee la imagen en una SD y reporte `x6200_log.txt` (el daemon lo guarda en `/mnt/app_logs/`), que ya cubriría arranque real y errores en campo.
