# #219 — Quitar el busy-spin de `radio_thread` (FAIL-SLOW sin paquete)

Hijo 3/6 de la épica [#216](https://github.com/iariap/x6200_gui/issues/216).
Issue: [#219](https://github.com/iariap/x6200_gui/issues/219). Hallazgo base:
F-08 del dossier [#194](https://github.com/iariap/x6200_gui/issues/194)
([#198](https://github.com/iariap/x6200_gui/issues/198)).

- Rama: `factory/next-219`
- Base: `b798f13b` (tip de `origin/lu2dsx/x6200-rom` con el hardening #211 y el
  fix CW idle #217 integrados)
- Worktree: `state/worktrees/x6200/next-219` (harness `asf`)
- Alcance: `src/radio.c` (solo `radio_thread`) + pruebas

## 1. El defecto y el contrato

`src/radio.c::radio_thread()` dormía según el valor de retorno de `radio_tick()`:

```c
if (radio_tick()) {      /* rama sin paquete */
    usleep(15000);
}
```

`radio_tick()` devuelve `false` cuando `x6200_flow_read()` entrega un paquete y
`true` cuando no hay paquete (es el `return true;` de la rama `else`). La rama que
**no** dormía era la que devolvía `false`, y giraba tan rápido como permitiera la
plataforma. F-08 lo midió en QEMU_USER: `TID 16 ~99.8% de un core, ~630k read()/s`
(el proceso total ~101%).

Contrato del #219 (cerrado y mínimo): **dormir en todas las iteraciones**, sin
mirar el retorno, sin cambiar `radio_tick()` (callbacks, flags de SWRScan/RX/TX y
su watchdog), sin dormir con el lock de radio tomado y sin introducir busy wait,
colas, hilos, backoff ni controles nuevos.

### Polaridad (verificada contra el código, base y fix)

| escenario | `x6200_flow_read()` | `radio_tick()` | antes | ahora |
|---|---|---|---|---|
| paquete | `true` | `false` | **0 sleeps -> busy-spin** | 1 sleep de 15 ms |
| sin paquete | `false` | `true` | 1 sleep de 15 ms | 1 sleep de 15 ms |

En las dos rutas el sondeo queda a un `flow_read` cada ~15 ms (con paquetes la
base hacía, tras el paquete, una lectura inmediata extra y después el sleep de
15 ms; el fix invierte el orden, el período de sondeo es el mismo). El ritmo de
paquetes del SDK (~35 ms) es más lento que el lazo (15 ms), así que no se pierden
paquetes; el timeout de reinicio del flow (300 ms) y el watchdog del barrido de
ROE se siguen evaluando cada vuelta.

## 2. El cambio (por archivo)

| archivo | cambio |
|---|---|
| `src/radio.c` | `radio_thread()`: `radio_tick();` + `usleep(15000);` incondicional (antes `if (radio_tick()) usleep(15000);`). Único archivo de producción. |
| `tests/native/run_native_tests.sh` | nuevo stage `#219`: stagea `src/radio.c` con sus headers reales + stubs y corre el test. |
| `tests/native/test_radio_thread.c` | test determinista del lazo (nuevo). |
| `tests/native/radio_stubs.c` / `.h` | stubs y hooks del test (reloj falso/real, `usleep`, `x6200_flow_read`, contadores de idle/poll). |
| `tests/native/radio_thread_test_main.c` | runner del test. |
| `tests/native/stubs_radio/**` | stubs mínimos + copias fieles del SDK + `README.md` con la procedencia. |
| `tests/native/radio_bench.c` + `run_radio_thread_bench.sh` | banco HOST antes/después (no es gate). |

No se tocaron `radio_set_ptt`, `radio_set_morse_key`, `radio_start_atu`, el timer
del SWRScan, `main.c`, el interlock de CB ni el DSP; tampoco ninguna transición
de TX ni la prioridad del PTT. No hay settings nuevos.

## 3. Cómo se observa el lazo (test)

`src/radio.c` se compila por inclusión (`-DRADIO_C_PATH=...`, el mismo patrón que
`test_db_persistence.c` con `src/params/db.c`), así que el `radio_thread()`
estático y su estado privado (`now_time`, `idle_time`, `state`, `pack`) quedan
visibles. **No** se stubea `radio_tick()`: se controla su retorno por su única
entrada observable, el stub de `x6200_flow_read()`.

El lazo se corta con `longjmp` desde el stub de `usleep`; hay además un corte por
lectura como red de seguridad para que **una versión con el defecto falle en el
`CHECK` en vez de colgarse**. El reloj es falso (el stub avanza 15 ms por sleep,
sin `nanosleep` real), así que el test es instantáneo y determinista.

Casos:

1. `radio_tick()` **false** 1000 veces (`x6200_flow_read()` true): 1000 sleeps de
   exactamente 15000 us. **Es el caso RED**: la base no dormía ninguna vez.
2. `radio_tick()` **true** 1000 veces (`x6200_flow_read()` false): 1000 sleeps,
   la misma secuencia que el caso 1 (acá la base ya dormía).
3. Mezcla 1 false / 1 true / 100 false: 102 sleeps, todos de 15000 us.
4. `x6200_control_idle()` respeta su período: con 15 ms por vuelta dispara cada 67
   iteraciones (1000 ms), ni antes ni después.
5. En `RADIO_SWRSCAN`, el watchdog (`dialog_swrscan_poll`) corre en **cada** vuelta.
6. Al volver el paquete, el lazo lo procesa en la vuelta inmediata siguiente
   (≤ 1 sleep de 15 ms) y sin perder el estado (`state` sigue en `RADIO_RX`).

## 4. El banco HOST antes/después

`tests/native/run_radio_thread_bench.sh` compila el **mismo** banco dos veces,
contra el `radio.c` de la base (`BASE_REF`) y contra el del árbol, y mide con
`x6200_flow_read()` = true (o sea `radio_tick()` = false: la rama que giraba):
iteraciones (lecturas), sleeps, tiempo de pared y CPU del proceso (`getrusage`,
utime+stime), con `CPU%` sobre **un** núcleo.

El banco **no** es un gate: depende de la carga del host.

## 5. Resultados (salida real)

Entorno: AMD Ryzen 7 PRO 8840HS (16 vCPU), gcc 16.2.1. El host tenía carga de
otras sesiones en vuelo (load 1-min 7,57 -> 11,06 durante el banco; #221 corriendo
su propia suite): afecta la magnitud del banco, no los gates deterministas (el
contraste del banco es de 5-6 órdenes de magnitud).

### 5.1 Test del lazo (RED en la base, verde con el fix)

Gate: `sh tests/native/run_native_tests.sh` -> stage `#219`:

```
== #219: radio_thread duerme en cada vuelta del sondeo ==
36 comprobaciones, 0 fallos
RC=0
```

Mismo test contra el `radio.c` de la base (`b798f13b`), mismo stage (crudo en
[`red-base.txt`](red-base.txt)):

```
FALLO tests/native/test_radio_thread.c:85: radio_stub_flow_reads == 1000
FALLO tests/native/test_radio_thread.c:86: radio_stub_sleep_calls == 1000
FALLO tests/native/test_radio_thread.c:87: radio_stub_sleep_us_total == 1000ull * 15000ull
FALLO tests/native/test_radio_thread.c:124: radio_stub_flow_reads == 102
FALLO tests/native/test_radio_thread.c:125: radio_stub_sleep_calls == 102
FALLO tests/native/test_radio_thread.c:126: radio_stub_sleep_us_total == 102ull * 15000ull
FALLO tests/native/test_radio_thread.c:141: radio_stub_idle_marks_n == 14
FALLO tests/native/test_radio_thread.c:187: radio_stub_flow_reads == (uint64_t) reads_before + 1
FALLO tests/native/test_radio_thread.c:188: radio_stub_sleep_calls == 301
23 comprobaciones, 9 fallos
exit=1
```

Los fallos son los casos `radio_tick() == false` (0 sleeps de 1000), la mezcla, el
período de idle y la recuperación: exactamente el busy-spin. Los casos
`radio_tick() == true` pasan en ambos (la base ya dormía ahí).

### 5.2 Banco HOST antes/después

`SECONDS_WIN=10 WINDOWS=3 sh tests/native/run_radio_thread_bench.sh` (3 ventanas
de 10 s; `x6200_flow_read()` = true):

| variante | ventana | iteraciones/10 s | sleeps | cpu_ms | CPU% de un core |
|---|---|---|---|---|---|
| base | 1 | 421.697.209 | 0 | 9962,9 | 99,63 |
| base | 2 | 294.663.166 | 0 | 9873,3 | 98,73 |
| base | 3 | 353.295.332 | 0 | 9897,6 | 98,98 |
| head | 1 | 663 | 663 | 8,0 | 0,08 |
| head | 2 | 663 | 663 | 8,1 | 0,08 |
| head | 3 | 660 | 660 | 9,7 | 0,10 |

Lectura: la base gira a ~30-42 M lecturas/s y ~99% de un core; con el fix el lazo
corre a 66,0-66,3 iteraciones/s (≈67/s = 1/15000 us) y ≤0,10% de un core, con
sleeps == iteraciones (una por vuelta). El objetivo del issue (`radio_thread`
<10% de un core y <100 llamadas `radio_tick`/s) se cumple con margen.

### 5.3 Gates de la épica

| gate | comando | resultado real |
|---|---|---|
| nativos | `sh tests/native/run_native_tests.sh` | RC=0; stage #219 `36 comprobaciones, 0 fallos` |
| paridad CI | `sh tests/native/run_native_tests.sh --ci-parity` | RC=0; `ci-parity: OK (build host sin lvgl/)` |
| determinismo | `bash tests/check_harness_determinism.sh` | RC=0; `PASS: deterministic (identical PNGs for every fixture)` |
| natural | `bash tests/check_harness_natural.sh` | RC=0; `PASS: natural bench deterministic, profiled and delimited (main-natural)` |
| wakeups | `bash tests/check_harness_wakeups.sh` | RC=0; `PASS: wakeup profile measured, sourced and delimited (main-natural)` |
| waterfall incr. | `bash tests/check_waterfall_incremental.sh` | RC=0; `PASS: 195 checks, 0 fallas` (+ ASan/UBSan) |
| registro de pruebas | `python3 tests/audita-pruebas.py` | RC=0; `OK: estructura y evidencia en orden` |
| suite completa | `sh tests/run_all_tests.sh` | **19 OK, 0 fallos**, 5 SKIP (abajo) |
| matriz | `/home/pablo/desa/asf/scripts/apps/x6200/check-matrix.sh origin/lu2dsx/x6200-rom HEAD <wt>` | RC=0; native + 9 checks estáticos + `x6200-ab themes` + `x6200-ab contracts` `ok`; determinismo `SKIP` (no toca `tools/ui_harness/`); banco QEMU `BLOCKED` |

SKIP de `run_all_tests.sh` (razón real, nunca PASS):

- `wspr`, `sstv`, `wefax`, `js8`: la imagen `x6200-uiharness` de este host no trae
  `fftw3.h`/`sndfile.h` (haría falta el build full:
  `docker build -t x6200-uiharness tools/ui_harness`).
- `webserver`: no hay checkout `../.x6200_webserver` en este host.

La matriz corre con los recursos de banco PRIVADOS
(`VOLUMEN=x6200-next219-bench CHROOT=/nonexistent-next219`) para no compartir el
volumen/contenedor de banco de otras sesiones en vuelo (regla de laboratorio de
#216). Por eso el banco QEMU queda BLOCKED con su razón:

```
==> [check] banco QEMU: preflight (tools/captura-banco.sh --check)
    BLOCKED: falta entorno (corre en el host del owner, no en el runner):
      FALTA: falta el volumen x6200-next219-bench (build del GUI)
      FALTA: falta el chroot (/nonexistent-next219)
      FALTA: falta tools/remote/x6200-remote
```

No es un fallo del cambio: es el banco QEMU que no existe en este host/entorno
privado. El determinismo SÍ se corrió aparte (fila de arriba, `PASS`).

### 5.4 Warnings

El stage `#219` imprime un `-Wreturn-type` (`no return statement in function
returning non-void`) sobre `radio_thread()`. Es **preexistente**: sale igual con el
`radio.c` de la base, y la misma clase ya sale en la suite para
`src/params/params.c:617` y `clock.c:228`. No se suprimió con flags (sería tapar el
síntoma): el contrato del #219 acota el cambio a las dos líneas del sleep y
`while (true)` es la forma que ya usaba la función.

## 6. Constancia de recuperación y pendientes

- **Recuperación**: cubierta por el caso 6 del test (sin paquete durante una
  ventana y, al volver, el paquete se procesa en la vuelta inmediata siguiente, con
  el estado intacto) y por el caso 5 (el watchdog del barrido de ROE sigue
  evaluándose por vuelta). No hay retraso arbitrario: a lo sumo un sleep de 15 ms.
- **PENDING_HARDWARE**: radio física (BASE real, energía, térmica, RX real) no se
  toca en esta corrida; este fix no cambia la semántica de radio/TX/PTT ni los
  interlocks.
- **QEMU_USER pendiente**: no se corrió en esta sesión. El banco QEMU de la épica
  usa el contenedor `banco-apps` y el volumen `x6200-build-output` compartidos, y
  las reglas de laboratorio de #216 prohíben compartir esos recursos entre sesiones
  en vuelo. El número histórico de F-08 (~99,8% de un core en QEMU_USER) es
  referencia, **no** una medición de esta corrida; reproducirlo pide un banco QEMU
  privado y una ventana sin carga.
