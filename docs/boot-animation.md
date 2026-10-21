# Arranque animado neo-retro del X6200 (`x6200_bootanim`) — issue #192

Desde el primer momento seguro en userspace, la pantalla de arranque es una
animación textual neo-retro inspirada en el screensaver ASCII de Omarchy. Cede
la pantalla **justo antes del primer dibujo de LVGL** (sin esperas cosméticas) y
no toca nada de la operación: radio, audio, DSP, RF, TX/PTT/ATU, VFO, filtros,
waterfall, ajustes ni persistencia quedan igual.

## 1. Qué se ve

| | |
|---|---|
| Fondo | `#0B1012` (negro casi absoluto), pantalla completa |
| Texto primario | `#85E9B2` (verde fósforo): logo y pie |
| Texto secundario | `#729784` (verde apagado): encabezado y subtítulo |
| Encabezado | `X6200 / SYSTEM STARTUP` (escala 3: glifo 15×21 px) |
| Logo | `X6200` (escala 10: glifo 50×70 px, separación 10 px) |
| Subtítulo | `HF TRANSCEIVER` (escala 3) |
| Pie | `> INITIALIZING_` → a los 30 s `> STARTUP DELAYED_` |
| Fuente | bitmap monoespaciada 5×7 definida en el código (sin TTF, sin asset, sin licencia extra) |

Sin gradientes, imágenes, antialiasing, sonido, logo PNG, porcentajes, barra de
progreso ni indicadores ficticios. Todos los textos en inglés. La paleta es la
del contrato redondeada a RGB565 (`0x0882`, `0x8756`, `0x74B0`).

### Layout (coordenadas lógicas 800×480, las mismas que la GUI)

El ancho de cada texto se calcula desde su largo (`n` glifos de `5×escala` + `n-1`
separaciones de `escala`) y se centra en `x=400`; no hay posiciones mágicas por
letra.

| Elemento | Caja lógica | Anclaje |
|---|---|---|
| Logo | x 255..544, y 168..254 | glifo de 70 px centrado en la caja (y 176..245) |
| Encabezado | y 49..69 | baseline en y=70 |
| Subtítulo | y 287..307 | centro vertical en y=297 |
| Pie | y 380..400 (el `_` en x 518..532) | centro vertical en y=390 |

El pie dibuja siempre el texto completo y, cuando el cursor está apagado, borra
**solo la última celda**: la posición no se mueve al parpadear. `STARTUP DELAYED`
(18 caracteres, 321 px) tampoco desborda: la caja del pie es la unión de las dos
variantes (x 240..560).

### Animación (determinista, sin PRNG)

8 fps máximo (125 ms por cuadro, reloj monotónico, `clock_nanosleep` absoluto sin
deriva). Aparición del logo en cinco posiciones de carácter:

* mientras una posición está sin fijar muestra
  `alphabet[(frame_index * 7 + posición * 11) % 18]` con
  `#*+?0123456789ABCDEF`: el mismo cuadro da siempre el mismo dibujo;
* las cinco posiciones se fijan de izquierda a derecha en 250/375/500/625/750 ms
  desde el primer cuadro (`X`, `6`, `2`, `0`, `0`); a los 875 ms el logo está
  completo;
* desde ahí el logo queda fijo y el `_` del pie parpadea 500 ms encendido / 500 ms
  apagado;
* a los 30.000 ms se reemplaza **solo** el pie por `> STARTUP DELAYED_`, se dibuja
  ese cuadro una vez y el proceso sale: no hay animación ni consumo indefinido, y
  el dibujo estático queda en pantalla hasta que la GUI pinte.

No hay espera mínima: si la GUI está lista antes de los 875 ms, el logo puede
quedar a medio descifrar y se corta igual.

## 2. Arquitectura: un proceso, un dueño del framebuffer por fase

```
S01boot_time → S01bootanim → S20gui (x6200_daemon → x6200_gui)
                    │                              │
             x6200_bootanim  ── SIGUSR1/SIGTERM ──┘ boot_splash_stop()
             (único escritor de /dev/fb0            (LVGL es desde acá el
              durante el arranque)                    único escritor)
```

* **`src/bootanim/`** → ejecutable **`x6200_bootanim`** (instalado en
  `/usr/sbin/`). Sin LVGL, sin radio, sin red, sin shell, sin GPIO, sin
  `/mnt/params.db`, sin threads ni subprocesos, sin heap en el render:
  `main.c` (ciclo/reloj/señales), `renderer.c/h` (modelo + raster puro a un
  buffer lógico RGB565) y `fb.c/h` (ioctl/mmap/stride/formato/rotación).
* **`rootfs/etc/init.d/S01bootanim`** → arranca **una** instancia en segundo
  plano y vuelve enseguida; no bloquea `rcS` ni espera por la GUI.
* **`src/boot_splash.c/h`** → lo único de la GUI que habla con el animador.

### Cesión (contrato exacto)

`boot_splash_stop()` lee `/var/run/x6200_bootanim.pid` y:

1. sin PIDFILE, con PID inválido/corrupto o con el proceso ya terminado → no hace
   nada (no espera);
2. verifica identidad con `/proc/<pid>/exe` == `/usr/sbin/x6200_bootanim`: un PID
   reciclado por otro proceso **nunca** recibe una señal;
3. `SIGUSR1` + hasta **100 ms** en sondeos de 10 ms;
4. si sigue vivo: re-verifica identidad, `SIGTERM` + hasta **20 ms**;
5. último recurso: re-verifica identidad, `SIGKILL` + 20 ms de confirmación (no
   bloquea otros 100 ms).

El animador instala handlers de `SIGUSR1`/`SIGTERM` que solo escriben una bandera
`volatile sig_atomic_t`; el ciclo la mira antes de cada cuadro y sale cerrando
mmap/fd **sin borrar el último cuadro**. Si no se puede confirmar la liberación,
la GUI registra el error y sigue igual (nunca dos escritores por diseño).

En `src/main.c` la cesión queda:

* en el camino `X6200_VOICE_TEST` (sale sin pintar pantalla) → antes de `exit()`;
* **antes de `drm_init()`** si se pidió DRM (`/mnt/use_drm` o `X6200_DRM`): DRM
  puede tomar el display, así que primero se suelta el framebuffer. Si DRM falla y
  se cae a fbdev, el animador **no** se reinicia (la GUI sigue igual, con la
  pantalla estática hasta su primer cuadro);
* en el camino fbdev, **justo antes de la primera iteración de
  `lv_timer_handler()`**, después de `lv_scr_load(main_obj)`.

Marcas nuevas en `boot.log`: `bootanim stop` y `bootanim handoff` (no se modificó
ninguna marca existente). No hizo falta `lv_obj_invalidate(main_obj)`:
`lv_scr_load()` → `scr_load_internal()` termina con `lv_obj_invalidate(scr)`
(`lvgl/src/core/lv_disp.c`), así que el primer `lv_timer_handler` repinta la
pantalla completa y no queda nada del splash. El callback `screen_rec_flush` y su
orden quedaron intactos.

### Framebuffer (`fb.c`)

Abre `/dev/fb0` en `O_RDWR`, lee `FBIOGET_FSCREENINFO`/`FBIOGET_VSCREENINFO` y
**solo dibuja** si el panel visible es 480×800 con 16 bpp (5/6/5) o 32 bpp con
bitfields sanos, `xoffset`/`yoffset` = 0, `line_length` ≥ una fila y
`smem_len` ≥ una pantalla con ese stride (todo en 64 bits, sin overflow).
Cualquier otra cosa → aviso en stderr y `exit 0`: no dibuja ni corrompe nada.

Composición con los offsets/lengths reales de `fb_var_screeninfo` (nunca RGBA ni
BGRA hardcodeados) y rotación 90° idéntica a la GUI (`sw_rotate` +
`LV_DISP_ROT_90`): el píxel lógico `(x, y)` cae en el físico `(X = y, Y = 799 - x)`.
El **primer cuadro** pinta el fondo completo; los siguientes escriben solo las
cajas del logo/cursor que cambiaron, borrando al color de fondo antes de
reimprimir, dentro de rectángulos validados contra el stride y el tamaño mapeado.

### Política de fallos

| Situación | Qué pasa |
|---|---|
| `/dev/fb0` todavía no existe | 20 reintentos de 100 ms **en el proceso hijo** (máx. 2 s), después sale en silencio; el boot sigue |
| `/dev/fb0` denegado / ioctl / formato / mmap | aviso en stderr, `exit 0`, sin tocar el panel; la GUI no espera nada |
| Ejecutable ausente o no arranca | `S01bootanim` avisa en stderr y devuelve 0: no bloquea `rcS` |
| `stop` del sistema | el script mata solo el PID del propio ejecutable y limpia el PIDFILE; el handler cierra mmap/fd sin redibujar |
| Reinicios de `x6200_gui` (crash/restart) | el splash es solo del boot del sistema: no reaparece; si la GUI se reinicia antes de terminar el handoff, su siguiente lanzamiento lo detiene igual |
| Radio BASE ausente | la animación sigue hasta 30 s y después congela `STARTUP DELAYED` y sale (la GUI posterior dibuja el waterfall como siempre) |

## 3. Instalación (esto es lo que hay que leer antes de desplegar)

* `x6200_bootanim` se instala en **`/usr/sbin/`** y `S01bootanim` en
  **`/etc/init.d/`** (permisos de ejecución incluidos, con
  `install(TARGETS … DESTINATION sbin)` + `install(DIRECTORY rootfs/ …)` del
  `CMakeLists.txt` raíz).
* **Actualizar solo el binario de la GUI por WiFi (`tools/compila-gui.sh` +
  deploy del `x6200_gui`) NO instala nada de esto**: ni `S01bootanim` ni
  `x6200_bootanim` llegan a la radio por ese camino. Para tener el arranque
  animado hay que **construir la imagen / correr el buildroot** (o hacer el flash
  con el target completo). Sin el script nuevo, la GUI sigue funcionando igual
  (`boot_splash_stop()` es un no-op inmediato cuando no hay animador).
* El PR #90 (dos GUIs sobre `/dev/fb0` por el `S95gui` heredado del vendor) sigue
  resuelto igual: `buildroot/prune-vendor-rootfs.sh` lo saca en cada instalación y
  la lista no se tocó. El animador es el único escritor del framebuffer antes de
  la GUI y no arranca la GUI.

## 4. Verificación

| Gate | Qué prueba | Dónde |
|---|---|---|
| `tests/check_bootanim.sh` | renderer + backend + cesión (procesos reales) + el **binario real** contra un `/dev/fb0` sintético + init + integración | host y CI (`boot-rootfs.yml`, job `bootanim`) |
| `tests/bootanim/renderer_test.c` | modelo (frames/tiempos/cursor/30 s), layout y centrado, texto exacto, snapshots reproducibles | 172 comprobaciones |
| `tests/bootanim/fb_test.c` | geometry/bpp/bitfields/stride/smem, rotación 90° contra un oráculo independiente de LVGL, 16/32 bpp, padding, offsets, centinelas y casos negativos | 88 comprobaciones |
| `tests/bootanim/handoff_test.c` | PIDFILE ausente/corrupto/ajeno/zombi, 100 ms + 20 ms, SIGTERM y SIGKILL de último recurso | 39 comprobaciones |
| `tests/bootanim/fake_fb.c` + `replay_dump.c` | el binario **real** corre en el host con un framebuffer sintético y su cuadro final tiene que ser byte a byte el del volcado independiente | 4 formatos/stride |

El test **no usa `/dev/fb0` real** (§7 del issue): las estructuras de ioctl son
sintéticas (shim `LD_PRELOAD` para el binario real; estructuras armadas a mano en
los unitarios) y el framebuffer es memoria/archivo.

## 5. Validación física pendiente

No se contó con la radio ni con el banco QEMU del operador (el chroot `.vfix3` no
está en este entorno: `tools/captura-banco.sh --check` lo reporta). Lo que sí se
corrió: el **binario ARM real** bajo `qemu-arm-static` con el framebuffer
sintético, cuyo cuadro final es idéntico al del oracle del host. Queda pendiente
en hardware real:

1. Cinco arranques fríos: el texto tiene que aparecer apenas lo permite el panel,
   derecho y sin flicker (fotos/video desde el encendido);
2. dos reinicios de `x6200_gui` (el splash no reaparece);
3. una prueba con la radio BASE ausente (llegar a `STARTUP DELAYED`);
4. una prueba con backend DRM (`/mnt/use_drm`) y una sin framebuffer;
5. un apagado (que `stop` cierre limpio y no impida el apagado);
6. mediana de cinco tiempos `power-on → primera home` antes/después con la misma
   tarjeta: objetivo de aumento ≤150 ms;
7. con la radio andando: un solo `x6200_bootanim` antes de la cesión, cero después,
   un solo `x6200_gui`, RX, PTT/TX, botones, VFO, waterfall, grabación y backlight
   intactos; logs `/mnt/app_logs/boot.log` y `boot_scripts.log` (marcas
   `bootanim stop` / `bootanim handoff`).

## 6. Archivos

```
src/bootanim/{main.c,renderer.c,renderer.h,fb.c,fb.h,CMakeLists.txt}
src/boot_splash.{c,h}
rootfs/etc/init.d/S01bootanim
tests/bootanim/{renderer_test.c,fb_test.c,handoff_test.c,fake_animator.c,fake_fb.c,replay_dump.c,start-stop-daemon.sh,test_util.h}
tests/check_bootanim.sh
docs/boot-animation.md
docs/evidencia/2026-10-21-bootanim192/     (evidencia cruda de esta corrida)
```
