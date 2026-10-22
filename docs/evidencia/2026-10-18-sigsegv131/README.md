# Evidencia — #131: SIGSEGV `fallo=0xf363` tras un cambio de modo

**Fecha**: 2026-10-18 · Rama `factory/sigsegv-131` · base `07321a5b` · HEAD de código `57bfec09`
**Binario histórico**: `md5 b1ba67520079904449b6932a112d3c78` (13.753.408 B)
**ROM**: `c640645d810c` · librerías exactas `libc 412df301…`, `liblvgl cd30ca6c…`

## Acceso al equipo

La radio **no estuvo accesible** durante esta corrida: no respondió en `192.168.0.236` ni en
`192.168.18.67` (`No route to host`) y el barrido de `192.168.0.0/24` no encontró ningún host con
SSH. Por eso **no se pudo traer `/proc/<pid>/maps` ni volver a leer los md5 del equipo**.

No hizo falta inventar nada: los artefactos exactos ya estaban en el volumen privado del #116
(`cw116-build-output`, target del mismo ROM) y en la evidencia del #116:

```
412df301b11eeecfaaa77847fe63372b  libc.so.6
cd30ca6c58ad42f06caad930b65f0ee2  liblvgl.so
```

El `liblvgl` con el que se resolvió es byte a byte el de la ROM (`md5 cd30ca6c…`) y el `x6200_gui`
es el binario que corría la radio (`md5 b1ba67520079`), no un build parecido.

## 1. Reconstrucción exacta del ejecutable

El issue decía “GUI `b1ba67520079` (commit `885f823`)”. **El commit que produce ese md5 es
`63dbf7a4`**, no `885f823`:

```
# rebuild aislado, volumen privado sigsegv131-build-output (no el compartido)
$ docker run ... make x6200-gui-dirclean && make x6200-gui-rebuild
885f823 -> md5 846afac6ed47432ec79ca11d309807be   (NO es el binario del crash)
63dbf7a4 -> md5 b1ba67520079904449b6932a112d3c78   (SÍ: el del issue)
```

`63dbf7a4` es el tip del trunk cuando QA compiló a las 19:27 (después de `885f823`, que es sólo
tests) e incluye el cambio de `src/roe_curve.c` del #130. El binario exacto también había quedado
en `x6200_qa/bin-resol` y en `/tmp/qa-sw125/x6200_gui-build`; los tres md5 coinciden.

## 2. Base de carga y pila resuelta

La base de `liblvgl` surge del propio par `pc`/`lr`: **`0xb6170000`**. Es la única base
`0x1000`-alineada en la que (barrido reproducible en `verifica-base-131.py` → `base-unica.txt`):

- `pc = 0xb6186ae8` cae en `_lv_event_mark_deleted+0x30`;
- `lr = 0xb618d4f0` queda **exactamente 4 bytes después** del
  `bl _lv_event_mark_deleted@plt` de `lv_obj_destructor` (0x1d4ec);
- las palabras altas de la pila caen después de los `bl`/`blx` de `_lv_obj_destruct` y
  `obj_del_core` (el árbol de borrado de objetos de LVGL).

Resultado (`resolucion.txt`, reproducible con `resuelve-pila-131.py`):

```
main link=0x1ea8c  base(runtime)=0x47ea8c  base de carga del ELF=0x460000
liblvgl base (fijada por pc/lr) = 0xb6170000

pc=0xb6186ae8 -> liblvgl+0x16ae8 : _lv_event_mark_deleted+0x30
lr=0xb618d4f0 -> liblvgl+0x1d4f0 : lv_obj_destructor+0x10

0xb618db48 -> liblvgl+0x1db48 : _lv_obj_destruct+0x14
0xb6196128 -> liblvgl+0x26128 : obj_del_core.part.0+0x114
0xb619605c -> liblvgl+0x2605c : obj_del_core.part.0+0x48

0x00510b7c -> enlace 0xb0b7c : apply_cmd+0x0       (puntero a función del scheduler remoto)
0x00612848 -> enlace 0x1b2848 : __func__.0+0x0     (puntero a string de rodata)
```

### El fallo

`pc` es `ldr r2, [r3, #4]` (`_lv_event_mark_deleted`, `lv_event.c:151`), que recorre
`event_head` / `e->prev`. `fallo=0xf363` y el offset 4 implican **`r3 = 0xf35f`**: el puntero de
la lista de eventos de LVGL está corrupto. La cadena de llamadas es
`obj_del_core → _lv_obj_destruct → lv_obj_destructor → _lv_event_mark_deleted`, o sea un borrado
recursivo de objetos, no el camino de `on_vfo_mode_change`.

`desensamblado.txt` tiene el desensamblado alrededor de `pc`, `lr` y de las tres palabras de pila.

## 3. Veredicto

**Librería (LVGL 8.3.11).** El PC/LR y la estructura corrupta (`event_head`/`e->prev = 0xf35f`)
están dentro de `_lv_event_mark_deleted`; el crash es del walker de eventos de LVGL durante un
borrado de objetos. **No hay evidencia que lo atribuya a una línea nuestra**: el contexto del log
(cambio de modo + carga de filtros) termina antes de entrar al borrado, y el crash no se reprodujo
(LB→USB→CW→AM + 4 botones, pid 3931 intacto).

La palabra `apply_cmd` en la pila es un **puntero a función del scheduler del control remoto**, no
un frame: consistente con que el cambio de modo haya llegado por el socket, pero **no prueba
causalidad**. No se toca `lvgl/` (submódulo pineado) y no se inventa un fix.

Queda **no resuelto el origen de la corrupción** (`0xf35f`): haría falta el stack completo de 160
palabras y/o reproducirlo. El punto B hace que el próximo crash traiga esa información sola.

## 4. Punto B: `CRASHLOG-MOD`

Implementado en `src/crashlog_modules.c` (GUI-side, sin tocar RF/DSP):

- al arrancar lee `/proc/self/maps` (si no está, cae a `dl_iterate_phdr`), toma cada mapping
  ejecutable de archivo absoluto, calcula su **md5 sin heap** y guarda `base`, `md5` y `path` en
  una tabla estática (96 entradas, paths de 96 B truncados por la cola con `~`);
- deja el bloque de líneas **ya formateado** en `.bss`; el handler sólo hace `write(2)`, no
  formatea ni aloca;
- salida: una línea `CRASHLOG-MOD <base> <md5> <path>` por módulo, **antes** de
  `CRASHLOG-STACK`;
- al arrancar deja `CRASHLOG-MOD-INIT modulos=<n> exe_base=<base> exe_md5=<md5> exe=<path> (<ms> ms)`
  para medir el costo.

```
CRASHLOG sig=11 fallo=0x4c pc=... lr=... sp=... base=...
CRASHLOG-MOD 00400000 5f5bfb3b9381... /usr/sbin/x6200_gui
CRASHLOG-MOD 40850000 ea0515804c12... /lib/libfakefb.so
...
CRASHLOG-STACK 00000000 ...
```

### TDD

- RED: `tests/native/run_native_tests.sh` → `crashlog_modules.c: No such file` (log en
  `tdd-red.log`).
- GREEN: `tests/native/test_crashlog_modules.c`, 26 comprobaciones, 0 fallos
  (`tdd-green.log`). Fija formato determinista, paths largos truncados por la cola, buffer corto
  sin salida parcial y MD5 contra los vectores estándar.

### Costo medido

- Banco QEMU (`Cortex-A7`, 46 módulos, 33 MB de `.so`): **305 ms** en el arranque normal y
  **523 ms** en la corrida del crash inducido; `boot.log` anota `+0.31 s` (una sola vez).
- Host x86-64 (3 módulos, 2 MB): 10,6 ms. No es comparable con la radio.
- El handler no mide ni aloca: sólo `write(2)` del bloque.

### Validación en ARM real (QEMU)

- Arranque normal: `out-banco.txt` →
  `CRASHLOG-MOD-INIT modulos=46 exe_base=0x00400000 exe_md5=5f5bfb3b9381...`.
- SIGSEGV inducido con `kill -SEGV` (sin `libcrash`, para que corra el handler nuestro):
  **44 líneas `CRASHLOG-MOD`** y después `CRASHLOG-STACK`, en ese orden
  (`banco-crash-selftest.txt`).

## 5. Gates

Ver [`gates.txt`](gates.txt). Resumen: nativos + `-fanalyzer` OK; estáticos 7/7 (+ manual/audita);
determinismo PASS; A/B `AB-SWR131` PASS pixel-idéntico; contracts 873/873, audit 0 hallazgos,
themes OK; ruff/format/ty/pytest 227 + e2e 13; cross-compile ARM OK (`5f5bfb3b9381`); banco QEMU OK.

## Archivos de esta evidencia

| Archivo | Qué es |
|---|---|
| `modulos-rom.txt` | cierre de `NEEDED` del binario exacto con md5; base de `liblvgl` y del ejecutable |
| `resolucion.txt` | pc/lr/pila resueltos |
| `resuelve-pila-131.py` | script reproducible de la resolución |
| `verifica-base-131.py`, `base-unica.txt` | barrido de bases 0x1000: sólo `0xb6170000` cumple toda la evidencia |
| `desensamblado.txt` | desensamblado alrededor de pc, lr y las palabras de pila |
| `banco-crash-selftest.txt` | `CRASHLOG-MOD` + `CRASHLOG-STACK` del SIGSEGV inducido en QEMU |
| `tdd-red.log`, `tdd-green.log` | RED→GREEN del formateador y del MD5 |
| `gates.txt` | salidas reales de los gates |

## Pendientes

- **Radio**: no accesible en esta corrida. Falta confirmar en hardware que `/proc/self/maps` da
  las mismas bases/md5 y medir el costo real del md5 (el número de QEMU no es el de la radio).
- **Causa raíz del #131**: el `event_head` corrupto no se pudo atribuir; no se reprodujo. El
  próximo crash con `CRASHLOG-MOD` permitirá resolver el `.so` exacto y la pila completa.
- **Hipótesis no fundada** (no es hallazgo): llamadas a LVGL desde hilos que no son el de la GUI
  (p. ej. `lv_msg_send` del hilo de radio) podrían corromper la lista de eventos; no se midió y no
  se afirma.
