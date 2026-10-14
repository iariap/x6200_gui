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

La parte del webserver usa el python **del target** (ARM, vía binfmt) porque es el que trae
`bottle`; el runner lo omite si no encuentra el checkout o la imagen de build.

Qué lleva exactamente la imagen que se publica (verificado sobre el artefacto, no sobre el
árbol de compilación) está en [`IMAGE_INVENTORY.md`](IMAGE_INVENTORY.md).

Lo que sigue necesitando la radio (🔌) está listado en
[`INTEGRATION_AUDIT.md`](INTEGRATION_AUDIT.md) §3.


### Publicar un release

```bash
cd x6200_gui
sudo tools/release/make_release_assets.sh <imagen.img> <versión>   # imagen + tgz + SHA256SUMS
tools/release/publish_release.sh <tag> <versión>              # ensayo: verifica y muestra el comando
tools/release/publish_release.sh <tag> <versión> --publish    # publica
```

El segundo **verifica antes de publicar**: que existan los tres artefactos y que los checksums
coincidan, que el tgz de actualización traiga **el mismo binario GUI** que la imagen (así una
actualización en el lugar instala exactamente lo que instala la imagen), que el árbol esté limpio y
pusheado, que la suite pase y que las notas del release existan. Sin `--publish` se detiene después
de las verificaciones y **muestra el comando que ejecutaría**, así se puede ensayar.

**El tgz de actualización necesita root** (`sudo`), porque el rootfs trae cinco archivos setuid
(`busybox`, `mount`, `umount`, `dbus-daemon-launch-helper`, `ssh-keysign`) y hay que preservarlos:
extrayéndolo como usuario común se pierden y una actualización en el lugar rompería esos binarios.
`tools/release/make_update_tgz.sh` lo arma y lo comprueba.

Hay una prueba del camino completo, que corre el `S02update` **real** de la imagen dentro de un
chroot sobre una copia del rootfs (marca un archivo, aplica el tgz, y comprueba que el backup
traiga lo reemplazado y que el tgz se haya aplicado):

```bash
sudo x6200_gui/tools/release/test_update.sh [rootfs.tar] [actualización.tgz]
```

**El backup**: el `S02update` del firmware base lo hace con
`zcat $i | tar -tf - | xargs tar -cvf - | gzip > /mnt/backup_...`, y `xargs` parte la lista en
varios `tar` que escriben al mismo stdout: el resultado son varios archivos tar concatenados, de
los que sólo se puede leer el primero (~700 de 5313 archivos), así que no sirve para volver atrás.

Por eso **el tgz lleva su propio `S02update`** (`tools/release/update-overlay/etc/init.d/`): la
lista va a un archivo temporal y se arma **un solo** tar, con lo que el backup queda completo y
legible (verificado con esta prueba: 5313 entradas y el archivo reemplazado adentro). Se instala al
aplicar una actualización, no está en la imagen: una tarjeta recién grabada usa el script del
firmware en su primera actualización y a partir de ahí el corregido. Si el backup llegara a fallar,
la actualización sigue igual.

En resumen: para volver atrás de una tarjeta recién grabada, la imagen de la release anterior; con
el script corregido instalado, `/mnt/backup_x6200_r1cbu_*.tgz`.

**El tgz lleva sólo archivos y enlaces**: ni `./` ni entradas de directorio. No es un capricho, el
`S02update` del firmware hace el backup así:

```sh
zcat $i | tar -tf - | xargs tar -cvf - | gzip > /mnt/backup_...
```

Con una entrada `./` ese `tar` recibe `./` y archiva el sistema **entero** —incluido `/mnt`, donde
está escribiendo el backup—. Con sólo archivos, el backup contiene exactamente los archivos que se
reemplazan (5313 entradas, 0 directorios). Verificado extrayendo el tgz con el **busybox de la
imagen**: extrae, crea las carpetas que falten, conserva dueño, permisos y los cinco setuid, y deja
un binario GUI idéntico al de la imagen.

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
