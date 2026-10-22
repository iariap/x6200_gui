# Evidencia — boot anim neo-retro (issue #192), corrida 2026-10-21

* **Worktree**: `/home/pablo/desa/asf/state/worktrees/x6200/bootanim-192`
* **Rama**: `factory/bootanim-192` (convención del harness; el issue pedía
  `feat/boot-neo-retro-omarchy`)
* **Base**: `ca650ea4` = `origin/lu2dsx/x6200-rom` (merge-base = base; sin
  commits nuevos en la rama base)
* **Commits**: `ee50cfd1` (renderer+backend+tests), `c7938d1c` (init+empaquetado),
  `fb859bb6` (handoff+CI+docs)
* **Sello de fuentes de este árbol** (`tools/fuente-md5.sh .`):
  `5c48c798937da256391f193338d7b8c1` (el sello del volumen de deploy hoy es
  `f31f9f2b06509f496556ae4eaac34912`, de otro árbol de fuentes: no se comparan)

## Método

1. Nada se escribió en el checkout base, en `lvgl/`, en otros worktrees ni en los
   volúmenes/chroot del operador. Los volúmenes `x6200-build-output` /
   `x6200-buildroot` se usaron **solo como lectura** (sysroot del toolchain y
   `.fuente.md5`); el staging del install fue a `/tmp`.
2. Los tests no usan `/dev/fb0` real: los unitarios arman las estructuras de ioctl
   a mano y el binario real corre con un shim `LD_PRELOAD`
   (`tests/bootanim/fake_fb.c`) que redirige `/dev/fb0` a un archivo, sintetiza
   `FBIOGET_FSCREENINFO/VSCREENINFO` y (con `BA_TEST_SPEED`) acelera el reloj
   monotónico para llegar de verdad a los 30 s.
3. El cuadro final del binario real se compara **byte a byte** contra un volcado
   independiente (`tests/bootanim/replay_dump.c`), que reproduce la animación con
   el renderer y el backend del propio proyecto.
4. El binario **ARM** se corrió bajo `qemu-arm-static` en el sysroot de Buildroot
   con el mismo shim, y su framebuffer salió idéntico al del oráculo del host.

## Gates (salida real)

| Gate | Comando | Resultado | Log |
|---|---|---|---|
| sintaxis del init | `bash -n rootfs/etc/init.d/S01bootanim` | **OK** (sin salida) | — |
| gate del arranque (host) | `bash tests/check_bootanim.sh` | **PASS (parcial)**: 172+88+39 comprobaciones + 4 formatos del binario real + init + integración; **SKIP ASan/UBSan** (falta `libasan`/`libubsan` en el host Fedora) | `gate-bootanim-host.log` |
| gate del arranque (con sanitizers) | `docker run … x6200-uiharness bash tests/check_bootanim.sh` | **PASS** completo, incluido **ASan+UBSan** | `gate-bootanim-docker-asan.log` |
| draw buffer parcial (#147) | `bash tests/check_drawbuf_partial.sh` | **PASS** (framebuffer físico idéntico con 5 tamaños de buffer) | `gate-drawbuf-partial.log` |
| waterfall incremental (#148) | `bash tests/check_waterfall_incremental.sh` | **PASS** (195 checks, ASan/UBSan incluidos) | `gate-waterfall-incremental.log` |
| suite del repo | `./run_tests.sh` | **PENDING**: falla en `src/ft8/worker.h:11: ft8lib/constants.h: No such file or directory` (los headers viven solo en el sysroot ARM). **Es previo a este cambio**: el mismo comando sobre un árbol con el contenido exacto de la base (`ca650ea4`, sin este diff) falla igual | `gate-run_tests-HEAD.log`, `gate-run_tests-BASE-atribucion.log` |
| matriz del harness | `scripts/apps/x6200/check-matrix.sh origin/lu2dsx/x6200-rom HEAD <worktree>` | **EXIT 0**: native tests + 10 chequeos estáticos (`check_build_identity` incluido) + `x6200-ab themes` + `bash -n` de los shells nuevos, todo **ok**. Dos tramos **BLOCKED** (nunca PASS), con la razón real: | `matrix.log` |
| ├ `x6200-ab contracts` | materializa el sha en un worktree temporal | **BLOCKED** (exit 2): `git worktree add` hacia `/tmp` falla en este entorno (`unable to write file …`). **Previo a este cambio**: el mismo `git worktree add` falla idénticamente con el commit **base** `ca650ea4`. Con `--workspace` en el disco del repo **sí evalúa**, y ahí aparece el fallo del escenario `dfl-mirror-lsb`, **también idéntico en la base** (`ab-contracts-base-atribucion.log`) | `ab-contracts-bloqueado.log`, `ab-contracts-head.log`, `ab-contracts-base-atribucion.log` |
| ├ banco QEMU | `tools/captura-banco.sh --check` | **BLOCKED**: "falta el chroot (`.vfix3`)", "falta `tools/remote/x6200-remote`". No se corrió el banco porque **escribe en el volumen de deploy del operador** (prohibido por este encargo). En su lugar se corrió el binario ARM bajo `qemu-arm-static` (fila de abajo) | `matrix.log` |
| toolchain ARM (target suelto) | `cmake -S src/bootanim -DCMAKE_TOOLCHAIN_FILE=<buildroot> && cmake --build && cmake --install` | **OK**: ELF 32-bit ARM EABI5, instalado en `sbin/`, sello `5c48c798…` embebido | `arm-standalone-build.log` |
| toolchain ARM (proyecto raíz) | `cmake -S . -DCMAKE_TOOLCHAIN_FILE=<buildroot> && cmake --build --target x6200_bootanim` | **OK**: el `add_subdirectory(bootanim)` real compila para ARM | `arm-root-project-build.log` |
| install completo ARM (DESTDIR) | `cmake --build -j16 && DESTDIR=/stage cmake --install` | **OK (exit 0)**: la GUI completa, `btcat` y `x6200_bootanim` compilan para ARM y se instalan en el `DESTDIR`; en el staging quedaron `/usr/sbin/x6200_bootanim` (ARM, 32 KiB), `/usr/sbin/x6200_gui` (14,4 MiB), `/etc/init.d/S01bootanim` (ejecutable) y `S20gui`, con **`S95gui` ausente**. Es lo más cerca de `target/` que se puede llegar sin el árbol fuente de Buildroot | `arm-full-install.log` |
| binario ARM bajo QEMU | `qemu-arm-static -L <sysroot> x6200_bootanim_arm_test` | **OK**: animación completa (30 s virtuales, `exit 0`), framebuffer final **idéntico byte a byte** al oráculo del host; también probado el corte por `SIGUSR1` (exit 0, revelado a medias) | `qemu-arm-run.log` |

### Contenido del staging de la imagen (equivalente a `target/`)

```
-rwxr-xr-x  2640      /etc/init.d/S01bootanim
-rwxr-xr-x  1623      /etc/init.d/S20gui
-rwxr-xr-x  32380     /usr/sbin/x6200_bootanim     ELF 32-bit LSB ARM EABI5
-rwxr-xr-x  14448300  /usr/sbin/x6200_gui          ELF 32-bit LSB ARM EABI5
[no existe]            /etc/init.d/S95gui          <- el prune lo saca
```

### Lo que el gate nuevo prueba (detalle)

`tests/check_bootanim.sh` (7 secciones):

1. compilación host de todo con `-std=c11 -Wall -Wextra -Werror` (sin avisos);
2. unitarios: modelo (frames/tiempos/cursor/30 s), layout y centrado, texto
   exacto, snapshots reproducibles, `ba_glyph('%') == NULL` (no puede aparecer un
   porcentaje ficticio), rotación contra el oráculo de LVGL, 16/32 bpp con
   bitfields distintos, `line_length` con padding, offsets, centinelas, casos
   negativos de geometry/bpp/bitfields/stride/smem/mmap/permisos;
3. handoff con **procesos reales**: PIDFILE ausente/vacío/corrupto/ajeno/zombi,
   100 ms + 20 ms, SIGTERM, SIGKILL de último recurso y su presupuesto medido;
4. ASan + UBSan (o SKIP con el motivo exacto del compilador);
5. el **binario real**: 4 combinaciones de formato/stride/padding/bitfields
   comparadas byte a byte contra el volcado independiente, el corte real de los
   30 s, `SIGUSR1` (salida en 1-2 ms con código 0 y cuadro final conservado,
   paleta del contrato y cajas respetadas) y 7 caminos degradados + dispositivo
   ausente + sin permisos (todos `exit 0` **sin escribir** en el framebuffer);
6. el script de init: sintaxis, permiso de ejecución, invocación exacta de
   `start-stop-daemon`, singleton (una sola instancia, un solo `-S`), `stop` que
   mata solo el PID del propio ejecutable, PIDFILE ajeno/corrupto y "sin
   ejecutable no bloquea el boot" (con un `start-stop-daemon` doble, porque el
   host Fedora no tiene el real: ver Riesgos);
7. integración: `S95gui` ausente y `S20gui` único, el prune sigue sacando el
   `S95gui` sin tocar `S01bootanim`, el handoff queda antes de `drm_init()` y del
   primer `lv_timer_handler()`, `screen_rec_flush` intacto, las marcas nuevas de
   `boot_mark`, el target en CMake con `install(… DESTINATION sbin)` y el gate en
   `boot-rootfs.yml`.

## Capturas

Del binario **ARM** corriendo bajo qemu (bytes reales del target):

* `frame-000-logico.png`: cuadro 0 — logo descifrándose (`#7+90`, el alfabeto
  provisional con `alphabet[(frame*7 + pos*11) % 20]`) y pie `> INITIALIZING_`;
* `frame-003-logico.png`, `frame-007-logico.png`: bloqueo progresivo y logo
  completo a los 875 ms;
* `frame-revelado-sigusr1-logico.png`: corte por `SIGUSR1` a ~0,6 s (revelado a
  medias, sin esperas cosméticas);
* `frame-final-30s-logico.png` y `frame-final-30s-fisico.png`: el cuadro de los
  30 s (`> STARTUP DELAYED_`) visto en lógico 800×480 y en el panel físico
  480×800 (rotado 90°, la misma orientación que LVGL).

Los PNG se generan del framebuffer crudo deshaciendo la rotación
(`log_x = 799 - Y_fisico`, `log_y = X_fisico`).

## Riesgos y mitigaciones

| Riesgo | Mitigación probada |
|---|---|
| Dos escritores del framebuffer (flicker, doble CPU del issue #89) | Un solo `x6200_bootanim` por PIDFILE+identidad; la GUI lo verifica por `/proc/<pid>/exe` antes de su primer `lv_timer_handler`; el prune del `S95gui` sigue intacto (gate 7). Si no puede confirmar la liberación, registra el error y sigue (nunca queda un segundo escritor por diseño: `SIGKILL` al PID identificado) |
| El splash retrasa la GUI | Cero esperas: `S01bootanim` vuelve enseguida; la cesión es un `SIGUSR1` + hasta 100 ms sólo si el animador no responde, y el test mide 1-2 ms en el caso normal y ≤220 ms+20 en el peor (SIGKILL) |
| El animador queda vivo (CPU/zombi) | Sale solo a los 30 s (probado con reloj acelerado), sale con cualquier señal, cierra mmap/fd sin redibujar, `S01bootanim stop` limpia el PIDFILE |
| Formato/geometría raros en el panel | Validación estricta (480×800, 16/32 bpp, bitfields sanos, offsets 0, stride y smem consistentes) y 7 caminos degradados que salen 0 **sin escribir**; `mmap` fallido no deja trazas en el panel |
| Cambio en superficie compartida (CMake raíz, `src/CMakeLists.txt`, `src/main.c`) | El proyecto raíz completo configura y compila para ARM con el toolchain de Buildroot (`arm-root-project-build.log`, `arm-full-install.log`) y la GUI sigue cargando `lv_scr_load(main_obj)` con `screen_rec_flush` intacto (gate 7) |
| Rotación/formato mal interpretados | El framebuffer físico del binario real es byte a byte el de un oráculo independiente; además coincide el de ARM/qemu con el del host |
| El `stop` del sistema mata un PID reciclado o la GUI | `stop` verifica `/proc/<pid>/exe` antes de señalizar, nunca usa `killall` y el test lo prueba con un PIDFILE ajeno (proceso `sleep` sobrevive) |
| El gate no corre con el `start-stop-daemon` real | Se probó la lógica propia del script con un doble del comando (el host Fedora no trae `start-stop-daemon`) **PENDING** en la imagen |
| Prueba con `/dev/fb0` real | **PENDING**: no hay radio ni chroot del banco en este entorno |

## Hardware boot validation pending

No se ejecutó ninguna de las pruebas físicas del §7 del issue (no hay X6200 ni
banco QEMU con chroot en este entorno: `tools/captura-banco.sh --check` reporta
"falta el chroot (.vfix3)", "falta tools/remote/x6200-remote" y el volumen sí
está pero el banco no se corrió porque **escribiría en el volumen de deploy del
operador**, prohibido por este encargo):

* [ ] cinco arranques fríos reales (fotos/video desde el encendido)
* [ ] dos reinicios de `x6200_gui` (el splash no reaparece)
* [ ] una prueba con la radio BASE ausente (llegar a `STARTUP DELAYED`)
* [ ] una prueba en backend DRM (`/mnt/use_drm`) y una sin framebuffer
* [ ] un apagado (`S01bootanim stop` + handlers del animador)
* [ ] medianas de cinco tiempos `power-on → primera home` antes/después (≤150 ms)
* [ ] con radio: un solo `x6200_bootanim` antes de la cesión / cero después, un
      solo `x6200_gui`, RX, PTT/TX, botones, VFO, waterfall, grabación y backlight
* [ ] logs reales `/mnt/app_logs/boot.log` (marcas `bootanim stop` /
      `bootanim handoff`) y `boot_scripts.log`

Lo que sí se corrió del binario del target es la corrida ARM bajo
`qemu-arm-static` con framebuffer sintético (arriba), que no reemplaza la radio.
