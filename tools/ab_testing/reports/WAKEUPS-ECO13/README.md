# Perfil de wakeups del hilo de la GUI — WAKEUPS-ECO13

- Status: **PASS** (`tests/check_harness_wakeups.sh`, exit 0)
- Date: 2026-10-15
- Revision: `ec89fc3` (corrida `factory/spectrum-eco-13`, issue #13 / UI-07 remanente;
  el harness agrega el modo, el `src/` de la ROM queda intacto)
- Fixture: `tools/ui_harness/fixtures/main-natural.scn` (seed 13)
- Host: x86 (el costo de un wakeup —y la energía del Cortex-A7— quedan sin medir)

> Modo nuevo y adicional del harness (`--bench-wakeups`). La captura común, el
> bench forzado (`--bench`), el modo natural (`--bench-natural`), el `compare`,
> los `contracts` y el `audit` conservan su formato y sus números.

## Por qué

El issue #13 pide mirar también los **wakeups**. La ROM tiene dos fuentes:

1. el lazo de la GUI, `src/main.c`:
   ```c
   loop_start_time = get_time();
   observer_delayed_notify_all(); event_obj_check(); scheduler_work();
   next_loop_time = lv_timer_handler() + loop_start_time;
   sleep_time = next_loop_time - get_time();
   if (sleep_time > 0) usleep(sleep_time * 1000);
   ```
   que despierta sólo cuando el `lv_timer_handler()` dice que hay un timer listo
   (los posts del DSP **no** despiertan al lazo: `src/scheduler.cpp` es una cola
   común, se drena en la próxima vuelta);
2. el hilo del tick, `src/main.c`, que despierta **pase lo que pase**:
   ```c
   while (1) { usleep(5 * 1000); lv_tick_inc(5); }
   ```

`--bench-wakeups [MS]` reproduce el lazo (1) sobre la **misma** base de timers
que la ROM arma —los componentes reales son los que crean y arman sus timers— y
lo reporta al lado de la cadencia de (2), que el gate lee de `src/main.c`. La
ventana es virtual (sin reloj de pared: dos corridas escriben los mismos bytes).

El lazo se corre con el periodo de refresco **de la ROM** (`LV_DISP_DEF_REFR_PERIOD
50`, `lv_conf.h`): el `lv_conf.h` del harness no lo define, así que LVGL le queda en
su default de 30 ms y el lazo dormiría contra el periodo equivocado. El gate lee
ese número (y la cadencia del DSP, `src/dsp.cpp`) de las fuentes, así que ninguna
de las dos puede correrse de lugar sin que el gate lo diga.

## Medido (fixture `main-natural`, seed 13, ventana de 1000 ms)

| medición | valor |
|---|---|
| wakeups del lazo, con el camino de datos (espectro + cascada) | **14** |
| wakeups del lazo, sin el camino de datos (control) | **11** |
| sueños del lazo (min–max) | **10–200 ms** |
| px flusheados en el lazo, con / sin datos | **2492000 / 414080** |
| wakeups del hilo del tick (5 ms, `src/main.c`) | **200** |
| **share del tick sobre las dos fuentes** | **93.5%** |
| vueltas sin sueño (`lv_timer_handler()` en 0) o sin timer armado | **0** |

Con la ventana de 3000 ms (`--bench-wakeups 3000`): lazo **31**, tick **600**
(mismos órdenes; el conteo escala con la ventana).

Los timers del firmware que dejan al lazo durmiendo son los de la pantalla
principal (reloj 500 ms, `check_time_sync` 200 ms, band-info/info 2000 ms), más
las vueltas que agrega el camino de datos (espectro 1000/15 ms y cascada
1000/25 ms): el lazo nunca gira sin dormir (`immediate = 0`, `no_timer = 0`), así
que cada vuelta contada es un ciclo dormir→despertar.

## Qué se recomienda (no se tocó la ROM)

Con el tick como **93.5%** de los wakeups, la vía de reducción es sacar el hilo
del tick, no optimizar el lazo:

1. **`LV_TICK_CUSTOM 1` con una fuente monotónica** (`clock_gettime(CLOCK_MONOTONIC)`
   ya está en la ROM: `get_time()`): elimina el hilo entero (200 → 0 wakeups/s) y
   deja el tick en el mismo reloj que el resto del firmware. Toca `lv_conf.h` y el
   `main.c` (sacar el `pthread_create` del tick), **no** `lvgl/` (pineado
   `74d0a816`): es un cambio de configuración de la ROM, no del renderer.
   Riesgo: es un cambio transversal (todo timer/animación pasa a leer el reloj
   monotónico) y necesita pasada por hardware.
2. **Engrosar el tick sin tocar `lv_conf.h`** (`src/main.c`: `usleep(20*1000)` +
   `lv_tick_inc(20)`): 200 → 50 wakeups/s, a costa de la resolución del tick
   (animaciones y timers cuantizados a 20 ms). El gate ya lee el `usleep`/`lv_tick_inc`
   de `src/main.c` y exige que digan el mismo número, así que este camino también
   queda medido si se toma.

Ninguna de las dos se aplicó en esta corrida: el spec pide **medir y recomendar**
(la decisión de tocar la cadencia global de la ROM es del owner).

## Evidencia

| archivo | qué es |
|---|---|
| `wakeups.json` | `--bench-wakeups` (ventana default 1000 ms) |
| `run1.json` | la primera de las dos corridas idénticas del gate (idempotencia) |
| `window.json` | `--bench-wakeups 3000` |
| `plain.json` | captura común (sin modos): NO lleva bloque `wakeups` |
| `wakeups.png` / `window.png` / `plain.png` | los PNG de esas corridas (sha256 abajo) |

El gate (`tests/check_harness_wakeups.sh`) además comprueba que
`--bench-wakeups` junto con `--bench-natural`, y `--bench-wakeups 0`, se rechazan
(exit 2): un modo no se mide encima de otro y una ventana no positiva es un typo.

## Límites honestos

- x86: se cuenta **cuántas veces** despierta cada fuente, no cuánto cuesta cada
  wakeup. El tiempo en un wakeup, el estado de idle del Cortex-A7 y la batería son
  la pasada por hardware (BLOCKED, sin radio). La comparación 200 vs ~14 es de
  conteo, y el número no depende de la máquina.
- El lazo se reproduce sobre los timers reales, pero **el hilo del DSP no está**:
  el harness inyecta los datos en el reloj virtual con la cadencia de
  `src/dsp.cpp`. Un cambio de esa cadencia en la ROM lo ve el gate (lee el
  divisor del fuente), no la medición viva.
- El periodo de refresco con el que corre el modo es el de la ROM (50 ms), pero
  el modo natural del harness sigue corriendo a 30 ms (el default de LVGL en el
  `lv_conf.h` del host): son dos varas distintas y están declaradas en cada JSON
  (`refresh_period_ms`, `frame_ms`). Alinear el modo natural a 50 ms re-baselinea
  el corpus de UI-07 (40 frames = 2000 ms en vez de 1200 ms) y es otro run.
