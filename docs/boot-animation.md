# Arranque animado neo-retro del X6200 (`x6200_bootanim`) — issues #192 y #204

Desde el primer momento seguro en userspace, la pantalla de arranque es una
animación textual neo-retro. Cede la pantalla **justo antes del primer dibujo de
LVGL** (sin esperas cosméticas) y no toca nada de la operación: radio, audio,
DSP, RF, TX/PTT/ATU, VFO, filtros, waterfall, ajustes ni persistencia quedan
igual.

Desde el issue #204 la animación es **continua**: la intro, la lluvia, el barrido
(BEAMS) y el descifrado periódico (DECRYPT) se repiten hasta que la GUI pide la
cesión. A los 30 s **solo cambia el texto del pie**: no hay salida, ni congelado,
ni pantalla rígida mientras termina el arranque. La inspiración es el screensaver
ASCII de Omarchy (efectos `decrypt`, `beams` y `rain` de `ttfx`); es una
adaptación local a la grilla bitmap de 800×480: **no se porta código ni se declara
paridad con esa herramienta** ni se usa Rust, terminal, ANSI ni configuración.

## 1. Qué se ve

| | |
|---|---|
| Fondo | `#0B1012` (negro casi absoluto), pantalla completa |
| Texto primario | `#85E9B2` (verde fósforo): logo, pie y líder de la lluvia |
| Texto secundario | `#729784` (verde apagado): encabezado, subtítulo y estelas |
| Destello | `#D2FFE4`: **solo** la celda que atraviesa el haz de BEAMS |
| Encabezado | `X6200 / SYSTEM STARTUP` (escala 3: glifo 15×21 px) |
| Logo | `X6200` (escala 10: glifo 50×70 px, separación 10 px), siempre centrado en x=400 |
| Subtítulo | `HF TRANSCEIVER` (escala 3) |
| Pie | `> INITIALIZING_` → a los 30 s `> STARTUP DELAYED_` |
| Fuente | bitmap monoespaciada 5×7 definida en el código (sin TTF, sin asset, sin licencia extra) |

Sin gradientes, imágenes, antialiasing, sonido, logo PNG, porcentajes, barra de
progreso ni indicadores ficticios. Todos los textos en inglés. La paleta es la
del contrato redondeada a RGB565 (`0x0882`, `0x8756`, `0x74B0`, destello
`0xD7FC`).

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
| Lluvia lane 0 | x 116..125, y 112..347 | dos columnas a la izquierda del logo |
| Lluvia lane 1 | x 182..191, y 112..347 | ídem |
| Lluvia lane 2 | x 608..617, y 112..347 | dos columnas a la derecha del logo |
| Lluvia lane 3 | x 676..685, y 112..347 | ídem |

El pie dibuja siempre el texto completo y, cuando el cursor está apagado, borra
**solo la última celda**: la posición no se mueve al parpadear. `STARTUP DELAYED`
(18 caracteres, 321 px) tampoco desborda: la caja del pie es la unión de las dos
variantes (x 240..560). Con `STARTUP DELAYED` el subrayado final queda fijo —no
parpadea— así que nunca se genera un rectángulo sucio en la celda del cursor de
`INITIALIZING_`.

### Animación (determinista, sin PRNG, sin hilos)

**Cadencia:** 84 ms por cuadro (`BA_FRAME_MS`, ≈11,9 fps, nunca más de 12), reloj
monotónico con espera absoluta (`clock_nanosleep` `TIMER_ABSTIME`, sin deriva) y
sin `pthread` ni busy-wait. Si el reloj se atrasa, el cuadro perdido **se saltea**:
no se acumula deuda ni se frena la GUI.

**Intro (0..874 ms):** las cinco posiciones del logo muestran
`alphabet[(frame_index * 7 + posición * 11) % 18]` con
`#*+?0123456789ABCDEF` y se fijan de izquierda a derecha en 250/375/500/625/750 ms
(`X`, `6`, `2`, `0`, `0`); a los 875 ms el logo está completo.

**Ciclo continuo desde 875 ms** (`cycle_ms = (elapsed_ms - 875) % 6000`, se repite
para siempre, sin reejecutar la intro):

| Fase | Qué hace |
|---|---|
| 0..1199 | logo fijo y legible; lluvia ASCII activa |
| 1200..2599 | **BEAMS**: un haz recorre las cinco celdas de izquierda a derecha (`min(4, (cycle-1200)/280)`); el carácter no cambia, solo su tinta pasa al destello y la celda anterior vuelve al verde normal |
| 2600..3599 | **DECRYPT**: **exactamente una** posición a la vez vuelve al glifo provisional del alfabeto (`active = (cycle-2600)/200`); las otras cuatro siguen siendo `X6200`, y a los 3600 vuelven las cinco |
| 3600..5999 | logo fijo y legible; lluvia activa |

Las transiciones no dejan pantalla en blanco y el logo nunca desaparece entero.

**RAIN desde 875 ms y para siempre:** cuatro columnas con hasta 3 glifos 5×7 a
escala 2 (10×14 px) separados 22 px (un líder en verde principal y dos estelas en
verde apagado; **nunca más de 12 glifos por cuadro**). La posición del líder es
`head_y = 112 + ((step * 6 + lane * 41) % 236)` con
`step = ((elapsed_ms - 875) / 84) % 40`, el carácter sale de
`alphabet[(frame_index * 7 + lane * 11 + trail * 5) % 18]` y un glifo se dibuja
**solo si su caja entera `[y, y+13]` cabe en la lane**: nada de recortes ni
escrituras fuera de la caja.

**30 s:** `delayed` es **solo** el texto del pie (`> STARTUP DELAYED_`, una vez).
No termina el proceso, no congela logo/lluvia/haz y no suelta `/dev/fb0`.

**Salida:** llega `SIGUSR1`/`SIGTERM` desde la GUI (o se apaga el sistema): el
proceso sale en la iteración inmediata, sin completar la fase ni esperar el ciclo
siguiente y **sin borrar el último cuadro**. Si la GUI nunca llega a pintar, el
splash vive indefinidamente con el límite de CPU que impone el máximo de 12 fps.

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
  `/mnt/params.db`, sin threads ni subprocesos, sin heap ni allocations por
  cuadro (un único buffer lógico fijo de 800×480 en BSS):
  `main.c` (ciclo/reloj/señales), `renderer.c/h` (modelo puro + raster a ese
  buffer RGB565) y `fb.c/h` (ioctl/mmap/stride/formato/rotación).
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
El **primer cuadro** pinta el fondo completo (un único rectángulo sucio); los
siguientes escriben solo lo que cambió —celdas del logo, pie/cursor y las **cuatro
cajas fijas** de lluvia, sin superponerse entre sí ni con el pie— dentro de
rectángulos validados contra el stride y el tamaño mapeado. `BA_MAX_DIRTY` es 12
(siete como máximo real en el ciclo: cinco celdas de logo + pie/cursor + cuatro
lanes); si el límite se alcanzara, el renderer lo marca como error en vez de
perder un rectángulo en silencio.

### Política de fallos

| Situación | Qué pasa |
|---|---|
| `/dev/fb0` todavía no existe | 20 reintentos de 100 ms **en el proceso hijo** (máx. 2 s), después sale en silencio; el boot sigue |
| `/dev/fb0` denegado / ioctl / formato / mmap | aviso en stderr, `exit 0`, sin tocar el panel; la GUI no espera nada |
| Ejecutable ausente o no arranca | `S01bootanim` avisa en stderr y devuelve 0: no bloquea `rcS` |
| `stop` del sistema | el script mata solo el PID del propio ejecutable y limpia el PIDFILE; el handler cierra mmap/fd sin redibujar |
| Reinicios de `x6200_gui` (crash/restart) | el splash es solo del boot del sistema: no reaparece; si la GUI se reinicia antes de terminar el handoff, su siguiente lanzamiento lo detiene igual |
| Radio BASE ausente o arranque lento | la animación **sigue** (lluvia, BEAMS y DECRYPT) y a los 30 s solo agrega `> STARTUP DELAYED_` al pie; no hay salida propia |
| La GUI nunca pide la cesión | el splash vive indefinidamente a ≤12 fps con esperas absolutas (sin busy-wait ni CPU de más); no reintenta el boot de la GUI |

## 3. Instalación (esto es lo que hay que leer antes de desplegar)

* `x6200_bootanim` se instala en **`/usr/sbin/`** y `S01bootanim` en
  **`/etc/init.d/`** (permisos de ejecución incluidos, con
  `install(TARGETS … DESTINATION sbin)` + `install(DIRECTORY rootfs/ …)` del
  `CMakeLists.txt` raíz).
* **Actualizar solo el binario de la GUI por WiFi (`tools/compila-gui.sh` +
  deploy del `x6200_gui`) NO instala nada de esto**: ni `S01bootanim` ni
  `x6200_bootanim` llegan a la radio por ese camino. Para tener el ciclo continuo
  del issue #204 hay que **construir y desplegar el binario ARM nuevo de
  `x6200_bootanim`** (imagen completa o paquete que lo incluya): actualizar la GUI
  no cambia el proceso del splash, que sigue siendo el binario viejo (con la
  animación que terminaba a los 30 s) hasta reemplazarlo. Sin el script nuevo, la
  GUI sigue funcionando igual (`boot_splash_stop()` es un no-op inmediato cuando no
  hay animador).
* El PR #90 (dos GUIs sobre `/dev/fb0` por el `S95gui` heredado del vendor) sigue
  resuelto igual: `buildroot/prune-vendor-rootfs.sh` lo saca en cada instalación y
  la lista no se tocó. El animador es el único escritor del framebuffer antes de
  la GUI y no arranca la GUI.

## 4. Verificación

| Gate | Qué prueba | Dónde |
|---|---|---|
| `tests/check_bootanim.sh` | renderer + backend + cesión (procesos reales) + el **binario real** determinista contra el volcado byte a byte (intro, BEAMS, DECRYPT, 30 s y después) + la versión de producción viva más allá de 30 s virtuales, parada por señal, sin busy-wait + caminos degradados + init + integración | host y CI (`boot-rootfs.yml`, job `bootanim`) |
| `tests/bootanim/renderer_test.c` | modelo (intro, cursor, lluvia, BEAMS, DECRYPT, bordes de fase, continuidad en 1/3/5/7/15/29/30/31/45/120 s), layout, lanes, higiene de `dirty` (≤12%, ≤12% de píxeles, nada fuera de las cajas), secuencias con cuadros salteados y determinismo | ~15.000 comprobaciones |
| `tests/bootanim/fb_test.c` | geometry/bpp/bitfields/stride/smem, rotación 90° contra un oráculo independiente de LVGL, 16/32 bpp, padding, offsets, centinelas, casos negativos y el fixture de las cuatro lanes de lluvia | 132 comprobaciones |
| `tests/bootanim/handoff_test.c` | PIDFILE ausente/corrupto/ajeno/zombi, 100 ms + 20 ms, SIGTERM y SIGKILL de último recurso, juzgado por **sondeos** de la espera (no por tiempo de pared) | 45 comprobaciones |
| `tests/bootanim/fake_fb.c` + `replay_dump.c` | el binario **real** corre en el host con un framebuffer sintético y sus cuadros tienen que ser byte a byte los del volcado independiente | 4 formatos/stride + 5 cuadros del ciclo |
| `tests/bootanim/render_bench.c` | mediciones reales en host (píxeles sucios y bytes por cuadro por fase) | informativo |

El test **no usa `/dev/fb0` real** (§7 del issue): las estructuras de ioctl son
sintéticas (shim `LD_PRELOAD` para el binario real; estructuras armadas a mano en
los unitarios) y el framebuffer es memoria/archivo.

### Instrumentación del banco

`src/bootanim/main.c` incluye un camino determinista (reloj sintético que avanza un
cuadro por iteración + límite de cuadros) **solo** bajo `-DBOOTANIM_TEST`, macro
que únicamente define el gate del host: permite comparar el proceso real byte a
byte contra el volcado. Ni CMake ni la ROM lo definen, y el gate lo verifica sobre
el preprocesado: el binario de producción no tiene **ninguna** condición que lo
detenga antes de la cesión (solo `SIGUSR1`/`SIGTERM`).

La **cesión** (`src/boot_splash.c`) tiene la misma instrumentación bajo
`-DBOOTANIM_TEST`: tres contadores de los sondeos de la espera
(`boot_splash_test_waits`/`waited_ms`/`reset_waits`). El test afirma **cuántas
veces esperó** —y que **no** esperó cuando no corresponde (PIDFILE ausente,
corrupto, ajeno, PID muerto o zombi)—, nunca **cuánto tardó**: un runner de CI
cargado puede tardar >50 ms en cualquier trabajo sin que el código espere nada.
El gate comprueba sobre el preprocesado que en producción no queda NADA de esa
instrumentación.

## 5. Validación física pendiente

No hay radio ni banco QEMU del operador en el entorno de esta corrida (el chroot
`.vfix3` no está: `tools/captura-banco.sh --check` lo reporta). Queda pendiente en
hardware real:

1. cinco arranques fríos: capturar video desde el encendido y comprobar que a 1, 5
   y 10 s hay movimiento y que, si el arranque se prolonga, **pasados 30 s también
   sigue** (lluvia, BEAMS y DECRYPT visibles);
2. que el waterfall aparezca exactamente al terminar la inicialización, sin
   retardo cosmético, sin tearing/flicker y sin dos escritores;
3. dos reinicios de `x6200_gui` (el splash no reaparece);
4. una prueba en backend DRM (`/mnt/use_drm`), donde el splash cede antes de
   `drm_init()` — el ciclo puede cortarse antes del waterfall: es la limitación
   real documentada, nunca dos escritores — y una sin framebuffer;
5. un apagado (que `S01bootanim stop` cierre limpio y no impida el apagado);
6. logs reales `/mnt/app_logs/boot.log` y `boot_scripts.log` (marcas
   `bootanim stop` / `bootanim handoff`), PID del animador y consumo de CPU
   durante el arranque;
7. mediana de cinco tiempos `power-on → primera home` antes/después con la misma
   imagen y condiciones: objetivo de aumento ≤150 ms;
8. con la radio andando: un solo `x6200_bootanim` antes de la cesión, cero después,
   un solo `x6200_gui`, RX, PTT/TX, botones, VFO, waterfall, grabación y backlight
   intactos.

## 6. Archivos

```
src/bootanim/{main.c,renderer.c,renderer.h,fb.c,fb.h,CMakeLists.txt}
src/boot_splash.{c,h}
rootfs/etc/init.d/S01bootanim
tests/bootanim/{renderer_test.c,fb_test.c,handoff_test.c,fake_animator.c,fake_fb.c,replay_dump.c,render_bench.c,start-stop-daemon.sh,test_util.h}
tests/check_bootanim.sh
docs/boot-animation.md
docs/evidencia/2026-10-21-bootanim204/     (evidencia cruda de esta corrida)
docs/evidencia/2026-10-21-bootanim192/     (corrida del issue #192)
```
