# Evidencia — boot anim v2: Decrypt + Beams + Rain continuos (issue #204), corrida 2026-10-21

* **Worktree**: `/home/pablo/desa/asf/state/worktrees/x6200/bootanim-v2-204`
* **Rama**: `factory/bootanim-v2-204` (convención del harness; el issue pedía
  `feat/bootanim-continuous-v2`)
* **Base**: `dadb17053542ab6a5a54ea0d9c74dfdf9076b219` = `origin/lu2dsx/x6200-rom`
  (merge-base = base: sin commits nuevos en la rama base durante la corrida)
* **Commits**: `94d004ee` (renderer+main), `b5e9864e` (tests+gate) y el commit de
  documentación de esta carpeta
* **Sello de fuentes de este árbol** (`tools/fuente-md5.sh .`):
  `18f5458db571b3056efdfa54c0013a05` (el `.fuente.md5` del volumen de deploy hoy
  es `964193ae9306b5127845cec44a57d52c`, de otro árbol de fuentes: no se comparan)
* **Sin push**: el PR lo abre el orquestador contra `lu2dsx/x6200-rom`.

## Método

1. Nada se escribió fuera del worktree: los volúmenes del operador
   (`x6200-build-output`) y los clones de Buildroot se usaron **solo como
   lectura** (toolchain/sysroot y `.fuente.md5`); todos los artefactos fueron a
   `/tmp/opencode`.
2. Los tests no usan `/dev/fb0` real: los unitarios arman las estructuras de
   ioctl a mano y el binario real corre con un shim `LD_PRELOAD`
   (`tests/bootanim/fake_fb.c`) que redirige `/dev/fb0` a un archivo y sintetiza
   `FBIOGET_FSCREENINFO/VSCREENINFO`.
3. **Comparación byte a byte del proceso real**: el contrato viejo usaba el fin
   automático a los 30 s como oráculo (y por eso podía comparar contra un volcado
   fijo). Con el ciclo continuo ese oráculo ya no existe, así que el gate compila
   además un binario **determinista** (`-DBOOTANIM_TEST`: reloj sintético que
   avanza un cuadro por iteración + `-DBA_TEST_FRAME_LIMIT=<N>`) y compara su
   framebuffer, byte a byte, contra el volcado independiente
   (`tests/bootanim/replay_dump.c`) en cuadros de la intro, de BEAMS (32), de
   DECRYPT (45), del umbral de los 30 s (357/358) y posteriores (500). El macro
   solo lo define el gate: el preprocesado del binario de producción no lo ve (se
   verifica en el paso 7 del gate) y CMake no lo referencia.
4. La versión de **producción** se prueba por comportamiento: se la arranca con
   reloj acelerado 100x, se espera el pie `> STARTUP DELAYED_` (30 s virtuales)
   **comprobando que el proceso siga vivo**, se verifica que la pantalla siga
   cambiando después de ese umbral y recién ahí se la para con `SIGUSR1`/`SIGTERM`
   midiendo el tiempo de salida. Nunca se usa un `timeout` como oráculo de éxito.
5. El binario **ARM** se compiló con el toolchain de Buildroot y se corrió bajo
   `qemu-arm-static` con el mismo shim linkeado en el binario (LD_PRELOAD no sirve
   porque se aplicaría a qemu, que es x86_64).

## Gates (salida real)

| Gate | Comando | Resultado | Log |
|---|---|---|---|
| sintaxis del init | `bash -n rootfs/etc/init.d/S01bootanim` | **OK** (sin salida) | — |
| gate del arranque (host) | `bash tests/check_bootanim.sh` | **PASS (parcial)**: 15.002 + 132 + 39 comprobaciones + 10 cuadros byte a byte del binario real + proceso vivo más allá de 30 s + 7 caminos degradados + init + integración; **SKIP ASan/UBSan** (el gcc del host no puede linkear `libasan`/`libubsan`) | `gate-bootanim-host.log` |
| gate del arranque (con sanitizers) | `docker run --rm -v $WT:/src -w /src x6200-uiharness bash tests/check_bootanim.sh` | **PASS completo**, incluido **ASan+UBSan**; el caso "sin permisos" se **SKIP**ea con la razón real (uid 0 ignora los permisos del archivo) | `gate-bootanim-docker-asan.log` |
| draw buffer parcial (#147) | `bash tests/check_drawbuf_partial.sh` | **PASS**: framebuffer físico idéntico con 5 tamaños de buffer + contrato de flush | `gate-drawbuf-partial.log` |
| waterfall incremental (#148) | `bash tests/check_waterfall_incremental.sh` | **PASS**: 195 checks, 0 fallas, con ASan/UBSan | `gate-waterfall-incremental.log` |
| suite del repo | `./run_tests.sh` | **PENDING** (exit 2): falla en `src/ft8/worker.h:11: ft8lib/constants.h: No such file or directory` (el header vive solo en el sysroot ARM, `tools/ui_harness/third_party/ft8lib`). **Previo a este cambio**: el diff no toca `src/ft8/**` ni CMake, y la corrida anterior documentó el mismo fallo sobre el contenido exacto de la base (`docs/evidencia/2026-10-21-bootanim192/gate-run_tests-BASE-atribucion.log`, exit 2 con el mismo error) | `gate-run_tests.log` |
| matriz del harness | `scripts/apps/x6200/check-matrix.sh origin/lu2dsx/x6200-rom HEAD $WT` | **EXIT 0**: native tests + 11 chequeos estáticos (`check_build_identity` incluido) + `x6200-ab themes` + `x6200-ab contracts` (materializa el sha y corre el arnés) + `bash -n` del gate. Dos tramos **SKIP** (temas/python A/B: sin cambios en esos paths) y uno **BLOCKED** (nunca PASS): | `matrix.log` |
| ├ banco QEMU | `tools/captura-banco.sh --check` | **BLOCKED** con la razón real: "falta el chroot (`.vfix3`)", "el chroot no tiene /lib", "falta `tools/remote/x6200-remote`". No se corrió el banco porque **escribe en el volumen de deploy del operador** (fuera del alcance del encargo); en su lugar se corrió el binario ARM bajo `qemu-arm-static` (fila de abajo) | `matrix.log` |
| toolchain ARM (target suelto) | `cmake -S src/bootanim -DCMAKE_TOOLCHAIN_FILE=<buildroot> && cmake --build && cmake --install` | **OK**: `ELF 32-bit LSB pie executable, ARM, EABI5`, 36.676 bytes, instalado en `sbin/` con el sello `18f5458d…` embebido (1 ocurrencia) | `arm-standalone-build.log` |
| toolchain ARM (proyecto raíz) | `cmake -S . -DCMAKE_TOOLCHAIN_FILE=<buildroot> && cmake --build --target x6200_bootanim` | **OK**: el `add_subdirectory(bootanim)` real compila para ARM | `arm-root-project-build.log` |
| binario ARM bajo QEMU | `qemu-arm-static -L <sysroot>` (cuadros 240 y 358 + producción con SIGUSR1) | **OK**: cuadros 240 (20.160 ms) y 358 (30.072 ms) **idénticos byte a byte** al oráculo del host (819.200 bytes cada uno); la versión de producción sigue **viva y animando pasado 30 s virtuales** y sale con **código 0 en 3 ms reales** ante `SIGUSR1`, con el cuadro final intacto | `arm-qemu-run.log`, `arm-qemu-run.sh` |
| mediciones de host | `tests/bootanim/render_bench.c` | **OK** (informativo, sin umbrales) | `render-bench-host.log` |
| unitario del renderer | `renderer_test` | 15.002 comprobaciones, **0 fallos**; área sucia máxima tras la intro 17.500 px de 384.000 (**4,56%**) | `renderer-test.log` |
| unitario del backend | `fb_test` | 132 comprobaciones (129 en el contenedor: la de permisos no aplica como root), **0 fallos** | `fb-test.log` |

## Indicadores medidos (host, cuadros reales)

`tests/bootanim/render_bench.c` reproduce 2.201 cuadros (184,8 s de animación):

```
fase                    cuadros dirty px avg dirty px max  dirty % max bytes/cuadro render us/cuadro
intro                        10        10850        17500        4.56%        21700           33.0
primer cuadro (full)          1       384000       384000      100.00%       768000         1723.8
lluvia                      204         9576        12940        3.37%        19153           26.7
beams                       518        13786        16755        4.36%        27573           42.0
decrypt                      59        14476        16755        4.36%        28952           42.2
delayed (lluvia, >30s)     1409        10599        16440        4.28%        21199           30.0
```

* **Píxeles sucios por cuadro ordinario: ≤4,56%** de 800×480 (techo del contrato:
  12%); el **100% ocurre una sola vez**, en el primer cuadro (fondo completo).
* **Bytes al framebuffer a 16 bpp: 19-29 KB por cuadro** ordinario (768 KB el
  primero): ninguna fase repinta la pantalla completa.
* **`dirty.count` máximo observado: 7** (techo `BA_MAX_DIRTY` = 12); `overflow` = 0
  en los 2.201 cuadros (el banco fallaría si se alcanzara el límite).
* **Render puro (modelo + raster, sin blit ni hardware): 27-54 µs por cuadro**
  ordinario (1,7 ms el cuadro completo), contra un presupuesto de 84.000 µs:
  cientos de veces de margen aunque el Cortex-A7 sea mucho más lento que el host.
* **Proceso real de producción sin busy-wait**: 20 ms de CPU en 3.002 ms de reloj
  (0%) en la corrida a reloj real del gate.
* **Salida por señal**: `SIGUSR1` después de los 30 s virtuales → 1 ms; `SIGTERM`
  en el arranque → 8 ms; `SIGTERM` después de los 30 s → 1 ms (contrato <150 ms),
  con el último cuadro conservado (paleta + cajas verificadas por el gate).
* **RAM del raster**: un único buffer estático de 800×480×2 B (768 KB) en BSS; no
  hay heap ni segundo framebuffer por cuadro.

## Lo que prueba el gate nuevo (detalle)

`tests/check_bootanim.sh`:

1. compilación host de todo con `-std=c11 -Wall -Wextra -Werror` (sin avisos),
   incluidos los 6 binarios deterministas y el medidor;
2. unitarios: modelo (intro, cursor, lluvia, BEAMS, DECRYPT, bordes de fase,
   continuidad en 1/3/5/7/15/29/30/31/45/120 s y en 30001/120000 ms), layout y
   centrado, geometría de las cuatro lanes, paleta (incluido el destello
   `#D2FFE4` = `0xD7FC`), higiene de `dirty` (≤12% de píxeles, nada fuera de las
   cajas declaradas, centinelas, `overflow` = 0), determinismo byte a byte y
   **secuencias que saltean cuadros** (para N en {26, 45, 200, 714, 2000}: el
   replay completo cuadro a cuadro, el de dos renders 0→N y un único repintado
   completo del cuadro N dan bytes idénticos), el confinamiento de los cambios de
   píxeles entre cuadros consecutivos a los rectángulos declarados en `dirty`, y
   el backend/rotación/formatos/centinelas más el fixture de las lanes;
3. handoff con **procesos reales**: PIDFILE ausente/vacío/corrupto/ajeno/zombi,
   100 ms + 20 ms, SIGTERM, SIGKILL de último recurso;
4. ASan + UBSan (o SKIP con el motivo exacto del compilador);
5. el **binario real**: 10 cuadros byte a byte (4 formatos/stride en 240; 32 y 45
   en las fases BEAMS/DECRYPT; 357/358/500 alrededor de los 30 s), el
   confinamiento de la diferencia 357→358 al pie + lluvia (prueba de que los 30 s
   son solo texto), la supervivencia más allá de 30 s virtuales con el pie
   `> STARTUP DELAYED_` observable en píxeles, la salida <150 ms por
   `SIGUSR1`/`SIGTERM` (arranque y después del umbral) sin borrar el cuadro, la
   ausencia de busy-wait y 7 caminos degradados + dispositivo ausente + sin
   permisos (todos `exit 0` **sin escribir** en el framebuffer);
6. el script de init: sintaxis, permiso de ejecución, invocación exacta de
   `start-stop-daemon`, singleton, `stop` que mata solo el PID del propio
   ejecutable, PIDFILE ajeno/corrupto y "sin ejecutable no bloquea el boot";
7. integración: `S95gui` ausente y `S20gui` único, el prune sigue sacando el
   `S95gui` sin tocar `S01bootanim`, el handoff queda antes de `drm_init()` y del
   primer `lv_timer_handler()`, `screen_rec_flush` intacto, las marcas de
   `boot_mark`, el target en CMake, el gate en `boot-rootfs.yml`, la lista de
   prohibiciones del contrato y **el macro del banco solo en el banco**
   (preprocesado de producción sin `BOOTANIM_TEST`/`BA_TEST_FRAME_LIMIT`, CMake
   sin el macro).

## Invariantes verificadas

* `src/main.c` (GUI), `src/boot_splash.*`, `src/screen_rec.c`, `src/waterfall.c`,
  `src/bootanim/fb.c`, `rootfs/etc/init.d/*`, LVGL, U-Boot/kernel, persistencia,
  el prune de `S95gui` y los CMake **no se tocaron**: `git diff --name-only`
  lista solo `src/bootanim/{renderer.h,renderer.c,main.c}`,
  `tests/bootanim/{renderer_test.c,fb_test.c,replay_dump.c,render_bench.c}`,
  `tests/check_bootanim.sh` y `docs/boot-animation.md`.
* Un solo escritor de `/dev/fb0` por fase, sin hilos nuevos, sin segundo
  framebuffer y sin heap por cuadro (el gate verifica que no haya
  `pthread`/subprocesos/LVGL/red/SQLite en `src/bootanim/**`).

## SKIP / PENDING razonados

* **ASan/UBSan en el host**: SKIP porque el `cc` de este Fedora no puede linkear
  `libasan`/`libubsan` (`cannot find /usr/lib64/libasan.so.8.0.0`). Se corrió el
  mismo gate completo dentro de `x6200-uiharness`, con **PASS**.
* **`./run_tests.sh`**: PENDING por el header de FT8 ausente (pre-existente, ver
  tabla). No está relacionado con este cambio.
* **Banco QEMU del operador**: BLOCKED por falta del chroot `.vfix3` y del
  binario `tools/remote/x6200-remote`; además escribiría en el volumen de deploy
  del operador (fuera del alcance de esta corrida).
* **`.fuente.md5`**: el volumen de deploy tiene el sello de otro árbol de fuentes
  (`964193ae…` vs `18f5458d…` de este worktree): no se comparan. Lo que sí se
  verificó es que el sello **que se embebe** en el binario ARM es el que calcula
  `tools/fuente-md5.sh` para este árbol.

## Hardware boot validation pending

No hay radio ni banco QEMU con chroot en este entorno, así que **no se ejecutó
ninguna prueba física** del §6 del issue. Lo que sí se corrió es el binario ARM
bajo `qemu-arm-static` con framebuffer sintético (arriba), que no reemplaza la
radio. Queda pendiente en hardware real:

* [ ] cinco arranques fríos con captura de video: movimiento a 1 s, 5 s y 10 s, y
      que pasados 30 s **siga** (lluvia, BEAMS y DECRYPT visibles)
* [ ] el waterfall aparece exactamente al terminar la inicialización, sin retardo
      cosmético, sin tearing/flicker y sin dos escritores
* [ ] dos reinicios de `x6200_gui` (el splash no reaparece)
* [ ] una prueba con backend DRM (`/mnt/use_drm`, cesión antes de `drm_init()`:
      el ciclo puede cortarse antes del waterfall) y una sin framebuffer
* [ ] un apagado (`S01bootanim stop` + handlers del animador, sin impedir el
      apagado)
* [ ] logs reales `/mnt/app_logs/boot.log` y `boot_scripts.log` (marcas
      `bootanim stop` / `bootanim handoff`), PID del animador y consumo de CPU
      durante el arranque
* [ ] mediana de cinco tiempos `power-on → primera home` antes/después con la
      misma imagen (objetivo: aumento ≤150 ms)
* [ ] con radio andando: un solo `x6200_bootanim` antes de la cesión / cero
      después, un solo `x6200_gui`, RX, PTT/TX, botones, VFO, waterfall,
      grabación y backlight intactos
* [ ] **despliegue**: la radio necesita el **binario ARM nuevo de
      `x6200_bootanim`** (imagen/paquete completo); actualizar solo la GUI por
      WiFi no cambia el proceso del splash
