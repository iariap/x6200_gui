# Stubs del test de `radio_thread` (issue #219)

Este directorio es el arbol de headers contra el que se compila `src/radio.c`
para el test `tests/native/test_radio_thread.c` y el banco
`tests/native/radio_bench.c` (ver el bloque `#219` de
`tests/native/run_native_tests.sh`). `src/radio.c` se incluye por texto desde un
directorio propio (sin los headers reales al lado), asi que sus `#include "..."`
caen aca: el patron de staging que ya usan clock, backlight, hkey, SWR-Scan y el
resto de la suite nativa.

## Sin submodulo, sin SDK

El host de CI no tiene el submodulo `lvgl/` ni el SDK de la BASE. Por eso:

- `lvgl/lvgl.h`, `scheduler.h`, `events.h`, `dsp.h`, `msg.h`, `info.h`,
  `tx_info.h`, `hkey.h`, `sql_policy.h`, `util.h`, `main.h`, `btaudio.h`,
  `gpio_safe.h`, `cw.h`, `dialog_swrscan.h`, `clock.h`, `voice.h` y
  `sqlite3.h` son **stubs minimos**: declaran solo lo que `src/radio.c` usa, con
  las firmas de los headers reales.
- `aether_radio/x6200_control/**` son **copias fieles** del SDK publico
  (`include/aether_radio/x6200_control/{api,control,low/control,low/flow,low/gpio}.h`,
  LGPL-2.1-or-later, (c) Belousov Oleg aka R1CBU / Rui Oliveira aka CT7ALW): los
  tipos, enums y la struct `x6200_flow_t` tienen que ser los del equipo, no una
  aproximacion. El SDK no esta en este repo (lo vende Buildroot), asi que estas
  copias se versionan; un cambio del SDK se ve como un cambio de estos headers.
- `radio.h`, `visual_policy.h`, `cb.h`, `pubsub_ids.h` y
  `cfg/{subjects,common,band,atu,transverter,cfg}.h` **no** viven aca: sus tipos
  (`radio_state_t`, `visual_policy_t`, `cfg_cur_t`, `Subject`, ...) son el
  contrato entre `radio.c` y el resto del firmware, asi que el stage los COPIA
  del arbol real en cada corrida (`run_native_tests.sh`), como clock/restore con
  `dialog.h`/`params.h`. Asi no hay espejos que driftear: si `radio.c` empieza a
  usar un campo nuevo de `cfg_cur`/`params`, el compile del stage lo dice.

## El valor de retorno de `radio_tick()`

En `src/radio.c` (igual en la base y en el fix), `radio_tick()` devuelve:

- `false` cuando `x6200_flow_read()` entrego un paquete (corre el cuerpo grande);
- `true` cuando no hay paquete (corre el watchdog del barrido de ROE).

El test NO stubea `radio_tick()`: controla su valor de retorno por su unica
entrada observable, el stub de `x6200_flow_read()` (`tests/native/radio_stubs.c`).
Asi los dos casos del contrato del #219 quedan cubiertos:

| escenario | `x6200_flow_read()` | `radio_tick()` | sleeps por iteracion |
|---|---|---|---|
| 1 | `true`  | `false` | 1 (antes: 0 -> busy-spin) |
| 2 | `false` | `true`  | 1 (antes: 1, sin cambios) |

## Limitaciones

- La semantica del SDK (`flow_check`, CRC, `read()` del puerto) no se reproduce:
  el stub responde `true`/`false` a voluntad. El banco mide el lazo, no la
  recepcion.
- La medicion de CPU es del proceso host, no del A33/Cortex-A7 real (QEMU_USER
  queda PENDING por recursos compartidos; ver el README de la evidencia).
