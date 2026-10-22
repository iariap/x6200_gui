# Falso rojo del gate `boot anim gate (deterministic, no docker)` — cesión del framebuffer

- **App**: `iariap/x6200_gui` (port LU2DSX).
- **Rama**: `factory/bootanim-flake`.
- **Base**: `35bad790` (tip de `origin/lu2dsx/x6200-rom`, ya con el boot anim de
  #192/#204).
- **HEAD de esta corrida**: `a9c2cf97` (el fix; la evidencia va en el commit
  siguiente).
- **Issue de referencia**: #192 (§5, instrumentación bajo macro) / #204.

## 1. Síntoma (medido)

El job **`boot anim gate (deterministic, no docker)`** falló en el run
`39742568157` sobre el HEAD de **otro PR** (que no tocaba `tests/bootanim/**`):

```
FAIL .../tests/bootanim/handoff_test.c:174: now_ms() - t0 < 50 (corrupto "1": espero 51 ms)
FAIL .../tests/bootanim/handoff_test.c:174: now_ms() - t0 < 50 (corrupto "0": espero 60 ms)
FAIL .../tests/bootanim/handoff_test.c:174: now_ms() - t0 < 50 (corrupto "12abc": espero 80 ms)
##[error]Process completed with exit code 1
```

Al **re-ejecutar el job, pasó**: es un *flake*. Log crudo:
[`ci-fallo-run-39742568157.log`](ci-fallo-run-39742568157.log).

## 2. Causa

`tests/bootanim/handoff_test.c` medía **tiempo de pared** con `now_ms()`
alrededor de `boot_splash_stop()` y exigía `< 50 ms` en **cuatro** lugares
(PIDFILE ausente, PIDFILE corrupto ×9, PID muerto y zombi). La intención —*"no
tiene que esperar"*— es correcta, pero un runner cargado puede tardar >50 ms en
cualquier trabajo **sin que el código espere nada**. Un gate que se vende como
"deterministic" no puede depender de la velocidad de la máquina: eso es el falso
rojo.

El mismo defecto estaba en los **tres** asserts de las ventanas de
`SIGUSR1`/`SIGTERM`/`SIGKILL` (`dt < USR1+60`, etc.): también eran de pared.

## 3. Criterio nuevo (determinista)

El contrato de #192 §5 ya preveía el mecanismo: un camino **solo para el target
host del gate**, bajo `-DBOOTANIM_TEST`.

- `src/boot_splash.c` **cuenta los sondeos de la espera**
  (`boot_splash_test_waits()` = llamadas a `bs_sleep_ms`;
  `boot_splash_test_waited_ms()` = ms pedidos a esos sondeos;
  `boot_splash_test_reset_waits()`). Todo envuelto en `#ifdef BOOTANIM_TEST`.
- El test afirma **cuántas veces esperó**:
  - los cuatro casos "no tiene que esperar" (ausente / corrupto ×9 / ajeno /
    muerto / zombi) ahora exigen **`sondeos == 0`** (y `waited_ms == 0`), no
    "tardó <50 ms";
  - los casos de fallback afirman **presupuesto en ms pedidos**, nunca reloj:
    `waited_ms >= USR1` (se consume la ventana de USR1 antes de escalar) y
    `waited_ms <= USR1+TERM+KILL` (nunca pasa del presupuesto del contrato);
  - `stop()` solo puede devolver `false` **después de agotar todo el
    presupuesto** (`check_stop_result`), que es la única salida `false` del
    contrato; bajo un runner muy cargado el SO puede tardar en dejar morir al
    hijo y ese `false` es legítimo.
- El **oráculo** de cada camino es la señal de salida del hijo (`SIGUSR1` /
  `SIGTERM` / `SIGKILL`), que no depende del reloj.

Además se endureció el **setup** de los casos que sí usan procesos reales
(eran fuentes de falso rojo *independientes* del reloj, preexistentes):

- `wait_zombie()`: espera (con techo de 5 s) a que el `SIGKILL` se materialice
  en estado `Z` en `/proc/<pid>/stat` antes de escribir el PIDFILE. Antes se
  asumía que el kill era inmediato: bajo carga se medía un proceso **vivo** y el
  caso dejaba de probar un zombi. (De paso, el `waitpid(WNOHANG)` viejo podía
  **recolectar** al zombi: a veces el caso probaba "PID inexistente", no
  "zombi".)
- `wait_handler()`: espera (con techo) a que `fake_animator` tenga instalado el
  handler (bit `signo-1` de `SigCgt` en `/proc/<pid>/status`). Antes se asumía
  que el hijo ya ignoraba la señal: si no, moría con `SIGUSR1` y los casos de
  fallback no probaban nada.

### En producción no queda nada

La instrumentación es `#ifdef BOOTANIM_TEST` (macro que **sólo** define el gate
del host; ni CMake ni la ROM la definen). El gate lo verifica sobre el
**preprocesado** (paso 7):

```
ok: el limite de cuadros vive solo en -DBOOTANIM_TEST (el preprocesado de produccion no lo ve) y CMake no lo define
ok: la instrumentacion de sondeos vive solo en -DBOOTANIM_TEST (el preprocesado de produccion no la ve)
```

La restricción está **codificada** en el gate (paso 3): si `handoff_test.c`
vuelve a mirar el reloj, el gate falla.

```
grep -qE 'clock_gettime|now_ms' handoff_test.c  # 0 en HEAD; 18 en 35bad790
```

Los 7 asserts (4 de "no espera" + 3 de ventanas) quedaron deterministas.

## 4. Prueba de sensibilidad (el assert tiene dientes)

Script reproducible: [`gen-sensibilidad.sh`](gen-sensibilidad.sh).

**A — espera espuria**: se inyecta `bs_sleep_ms(BOOT_SPLASH_POLL_MS)` en el
camino "sin PIDFILE / PID corrupto" (el bug de fondo del falso rojo). El assert
**falla** (10 fallos): [`sensibilidad-A-espera-espuria.log`](sensibilidad-A-espera-espuria.log)

```
FAIL tests/bootanim/handoff_test.c:67: boot_splash_test_waits() == 0 && boot_splash_test_waited_ms() == 0 (sin PIDFILE: no tiene que esperar (sondeos=1, 10 ms))
FAIL tests/bootanim/handoff_test.c:67: ... (12abc: no tiene que esperar (sondeos=1, 10 ms))
...
handoff_test: 45 comprobaciones, 10 fallos
```

**B — ventana salteada**: se inyecta `bs_wait_gone(pid, 0)` (no respeta la
ventana de 100 ms de `SIGUSR1`, escala a `SIGTERM` enseguida). Los asserts de
presupuesto **fallan** (2 fallos):
[`sensibilidad-B-ventana-salteada.log`](sensibilidad-B-ventana-salteada.log)

```
FAIL tests/bootanim/handoff_test.c:429: boot_splash_test_waited_ms() >= BOOT_SPLASH_USR1_WAIT_MS (no espero los 100 ms de SIGUSR1 (10 ms))
FAIL tests/bootanim/handoff_test.c:459: boot_splash_test_waited_ms() >= BOOT_SPLASH_USR1_WAIT_MS + BOOT_SPLASH_TERM_WAIT_MS (no espero las dos ventanas (30 ms))
handoff_test: 45 comprobaciones, 2 fallos
```

Ambas inyecciones se **revirtieron**; el árbol queda idéntico al commit del fix.

## 5. Gates

- `bash tests/check_bootanim.sh` → **exit 0**. Log real:
  [`gate-bootanim-host.log`](gate-bootanim-host.log). `handoff_test: 45
  comprobaciones, 0 fallos` (antes 39; se agregaron los asserts de sondeos).
- ASan/UBSan: **SKIP local** — este host no tiene `libasan`/`libubsan`
  (`cannot find /usr/lib64/libasan.so.8.0.0`). Se verificó que los dos archivos
  **compilan** con `-fsanitize=address,undefined -DBOOTANIM_TEST` sin avisos; el
  job de CI (imagen con sanitizers) corre el mismo comando con el macro agregado.
- `python3 tests/audita-pruebas.py` → OK.

## 6. Bajo carga (ya no es flaky)

Comparación ORIGINAL (`35bad790`, tiempo de pared) vs NUEVO (sondeos), mismo
runner, mismo momento. Script: [`gen-carga.sh`](gen-carga.sh). Log:
[`bajo-carga-comparacion.log`](bajo-carga-comparacion.log).

| Escenario | OLD | NEW |
|---|---|---|
| A · 16 hogs (runner ocupado) | 10/30 fallos | **0/30** |
| B · 32 hogs, test en 2 cores | 10/30 fallos | **0/30** |
| C · `nice -n 19` + 24 hogs | 29/30 fallos | **0/30** |
| D · 1 core + `nice -n 19` + 24 hogs (starvation extrema) | 30/30 fallos | **0/30** |

Total: **NEW 120/120 verde**. Los fallos del OLD incluyen el assert de pared
`now_ms() - t0 < 50` del caso zombi (el mismo del CI) y las carreras de setup
(handler no instalado, `SIGKILL` no materializado): ver
[`old-bajo-carga-fallos.log`](old-bajo-carga-fallos.log).

Detalle de la verificación final del NUEVO (4 escenarios × 40 corridas):
[`bajo-carga-verificacion.log`](bajo-carga-verificacion.log).

### Iteración intermedia (documentada)

La primera iteración (sólo contadores de sondeos + `wait_zombie`/`wait_handler`)
ya bajó el OLD 30/30 → NEW 11/30 en starvation extrema, pero dejó fallos
**residuales** en los casos de fallback (`stop() == false` y `waited` hasta
140 ms): bajo starvation extrema el SO no deja morir al hijo dentro de las
ventanas y el código escala/sigue sin poder confirmar (comportamiento correcto).
Eso es lo que resolvió el criterio de "presupuesto del contrato" (§3). Evidencia
cruda: [`iteracion1-starvation-extrema.log`](iteracion1-starvation-extrema.log),
[`iteracion1-nice19-detalle.log`](iteracion1-nice19-detalle.log).

## 7. Auditoría de otros asserts del mismo gate

- **`tests/bootanim/handoff_test.c`**: era el único lugar con tiempo de pared
  sobre la *lógica* de la cesión. Los 7 asserts (4 de "no espera" + 3 de
  ventanas) quedaron deterministas. **No quedan `now_ms()` ni umbrales de
  pared.** ✓
- **`tests/check_bootanim.sh`**: mide latencia real de **procesos** (p. ej.
  `DT_MS < 150` de la salida del binario de producción ante `SIGUSR1`/`SIGTERM`,
  `START_MS < 100` del init). Es otra categoría: prueba una propiedad de latencia
  del binario real, con márgenes amplios, y no es el defecto observado (el fallo
  del CI fue el assert de 50 ms). No se toca: no hay forma de volverlo
  independiente del reloj sin instrumentar el binario de producción (lo que el
  encargo prohíbe). El resto de sus chequeos son de estado/framebuffer (byte a
  byte) o relativos (CPU vs pared), no umbrales absolutos.

## 8. Diff por archivo

| Archivo | Cambio |
|---|---|
| `src/boot_splash.h` | +16: declaraciones de la instrumentación de sondeos bajo `#ifdef BOOTANIM_TEST` |
| `src/boot_splash.c` | +27: contadores de sondeos (`bs_test_waits`/`bs_test_waited_ms`) y sus accesores, también bajo `#ifdef BOOTANIM_TEST`; incremento en `bs_sleep_ms` |
| `tests/bootanim/handoff_test.c` | +268/−69: fuera `now_ms()`; asserts por sondeos; `wait_zombie`/`wait_handler`; `check_no_waits`/`check_stop_result`; `#include <string.h>` |
| `tests/check_bootanim.sh` | +32: `handoff_test` (y su variante ASan) se compila con `-DBOOTANIM_TEST`; paso 3 codifica que el test no mida reloj; paso 7 verifica sobre el preprocesado que la instrumentación no llega a producción |
| `docs/boot-animation.md` | +11: 39 → 45 comprobaciones y criterio determinista documentado |

## 9. Pendientes / límites

- **ASan/UBSan**: no se pudo correr en este host (falta `libasan`); sí compila.
  Lo corre el job de CI.
- Los asserts de latencia de `check_bootanim.sh` (§7) siguen midiendo pared: es
  deliberado y explicado; convertirlos requeriría instrumentar el binario real.
- La verificación en la **radio** (arranque frío, handoff real) sigue pendiente
  como en #192/#204; este cambio es de banco de pruebas y no altera el binario
  de producción (verificado por preprocesado).
