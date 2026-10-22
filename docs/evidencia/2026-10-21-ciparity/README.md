# Paridad del build host con CI: el host nunca depende del submodulo `lvgl`

Corrida `factory-correct` (lección *divergencia local↔CI*), rama `factory/ci-parity`,
base `1c900a98`. Lección de origen: el rojo del PR #187 (run `39516311133`, job
`functional` de `.github/workflows/ab-testing.yml`).

## 1. Que era la divergencia

El job `functional` hace `actions/checkout@v4` **sin** `submodules: true`
(`.github/workflows/ab-testing.yml:43`), asi que **en CI no existe `lvgl/`**. Todo
build host resuelve entonces `#include "lvgl/lvgl.h"` al stub
`tests/native/stubs*/lvgl/lvgl.h`, nunca al header real.

Antes del parche a mano, el stage de `params` tenía `-I$DIR/../..` (la raíz del
repo, el directorio que contiene `lvgl/`) **antes** de `-I$DIR/stubs` (así se
reconstruyó para medirlo: ver §3). Consecuencia, con el mismo código:

| dónde | `lvgl/lvgl.h` resuelto | resultado |
|---|---|---|
| máquina del dev (submódulo inicializado) | `lvgl/lvgl.h` **real** | verde |
| CI (sin submódulo) | `tests/native/stubs/lvgl/lvgl.h` | rojo |

Como el stub era el que compilaba **sólo** en CI, nada local delataba que al stub le
faltaran tipos que el código real usa. El error que apareció fue literalmente:

```text
src/dialog.h:37:5: error: unknown type name 'lv_event_cb_t'
tests/native/params_stubs.c:143:18: error: unknown type name 'lv_log_level_t'
```

(La instrucción que lo dispara: `params.c` → `dialog_msg_cw.h` → `dialog.h:37
lv_event_cb_t key_cb;`, y `params_stubs.c:143 _lv_log_add(lv_log_level_t ...)`.)

Un parche a mano (ya en la base) agregó esos tipos al stub y puso el stub primero.
Esta corrida convierte eso en **regla general y verificable**: ningún `-I` del build
host puede alcanzar el submódulo, y hay un modo que corre el runner con `lvgl/`
oculto y exige exit 0 (encadenado en `tests/run_all_tests.sh`).

## 2. El fix

- `tests/native/run_native_tests.sh`
  - **`--ci-parity [out-dir]`**: renombra `lvgl/` (el árbol que ve el job de CI),
    corre el runner completo y exige exit 0. Restaura el submódulo con `trap`
    (`EXIT`/`INT`/`TERM`/`HUP`); si no puede ocultarlo **falla** (nunca declara
    paridad sin medirla). Si `lvgl/` ya no está (CI), corre tal cual y lo dice.
  - Stage de `params`: se quita `-I$DIR/../..` (raíz del repo). El único header
    LVGL que un stage host puede alcanzar es `tests/native/stubs/lvgl/lvgl.h`, que
    queda primero en la lista. `-I$DIR/stubs -I$SRC` siguen (el stub también
    provee `liquid/liquid.h`).
- `tests/run_all_tests.sh`: `run host_ci_parity sh "$DIR/native/run_native_tests.sh" --ci-parity`
  justo después de `run host`, para que la paridad se mida en el flujo normal.

No se tocó el workflow de CI (en CI el modo es un no-op: no hay `lvgl/` que ocultar),
ni el submódulo más allá de ocultarlo/restaurarlo durante la medición.

Salvedad conocida del modo: mientras corre, `lvgl/` se llama `lvgl.ci-parity-off`; si
el proceso muere con `SIGKILL` el `trap` no corre y queda renombrado (se recupera con
`mv lvgl.ci-parity-off lvgl`). Con `INT`/`TERM`/`HUP`/`EXIT` el trap restaura.

## 3. Antes / después, con `lvgl/` oculto (`mv lvgl .lvgl.off`)

### Antes — reconstrucción del estado pre-fix

Los dos componentes del parche a mano se revirtieron temporalmente sobre la base
(`PARAMS_INCLUDES` con `-I$DIR/../..` primero y sin los tipos en el stub) para
volver a medir el defecto; los dos archivos se restauraron con `git checkout`
después de la prueba.

**(a) Con el submódulo presente (como un dev) → VERDE, y usa el header real.**
Esto es lo que hacía invisible el problema:

```text
$ sh tests/native/run_native_tests.sh
...
138 comprobaciones, 0 fallos          # ultimo stage; EXIT=0
                                      # (suma de los 20 stages: 51 773, 0 fallos)

$ gcc ... -I<repo root> ... -E -H src/params/params.c | grep lvgl/lvgl.h
... /home/pablo/desa/asf/state/worktrees/x6200/ci-parity/tests/native/../../lvgl/lvgl.h
```

**(b) Con `lvgl/` oculto (el árbol de CI) → ROJO**, con los mismos errores del PR:

```text
$ mv lvgl .lvgl.off && sh tests/native/run_native_tests.sh; echo EXIT=$?
...
tests/native/../../src/params/../dialog.h:37:5: error: unknown type name 'lv_event_cb_t'
   37 |     lv_event_cb_t           key_cb;
      |     ^~~~~~~~~~~~~
tests/native/../../src/params/../dialog.h:54:35: error: unknown type name 'lv_event_t'
   54 | bool dialog_key(dialog_t *dialog, lv_event_t * e);
      |                                   ^~~~~~~~~~
tests/native/../../src/params/../dialog.h:55:18: error: unknown type name 'lv_event_code_t'
   55 | void dialog_send(lv_event_code_t event_code, void *param);
      |                  ^~~~~~~~~~~~~~~
tests/native/../../src/dialog.h:37:5: error: unknown type name 'lv_event_cb_t'
   37 |     lv_event_cb_t           key_cb;
      |     ^~~~~~~~~~~~~
tests/native/../../src/dialog.h:54:35: error: unknown type name 'lv_event_t'
   54 | bool dialog_key(dialog_t *dialog, lv_event_t * e);
      |                                   ^~~~~~~~~~
tests/native/../../src/dialog.h:55:18: error: unknown type name 'lv_event_code_t'
   55 | void dialog_send(lv_event_code_t event_code, void *param);
      |                  ^~~~~~~~~~~~~~~
tests/native/params_stubs.c:143:18: error: unknown type name 'lv_log_level_t'
  143 | void _lv_log_add(lv_log_level_t level, const char *file, int line,
      |                  ^~~~~~~~~~~~~~
EXIT=1
```

### Después — código de esta rama

**(a) Con el submódulo presente: el stub es el que compila** (misma orden de flags,
sin la raíz del repo):

```text
$ gcc ... -I tests/native/stubs -I src ... -E -H src/params/params.c | grep lvgl/lvgl.h
... /home/pablo/desa/asf/state/worktrees/x6200/ci-parity/tests/native/stubs/lvgl/lvgl.h
```

**(b) `--ci-parity` (oculta `lvgl/`, corre todo, restaura) → EXIT 0:**

```text
$ sh tests/native/run_native_tests.sh --ci-parity
== ci-parity: lvgl/ oculto (el arbol que ve el job functional) ==
== compilador: gcc (GCC) 16.2.1 20260819 (Red Hat 16.2.1-2) ==
== sanitizers: NO disponibles en este toolchain (se omite) ==
...
138 comprobaciones, 0 fallos
== ci-parity: OK (build host sin lvgl/) ==
EXIT=0
$ ls -d lvgl
lvgl                                  # restaurado por el trap
```

**(c) Control negativo (el gate tiene dientes)**: se inyectó la regresión
(`-I$DIR/../..` primero + stub sin los tipos) y se corrieron los dos caminos:

```text
$ sh tests/native/run_native_tests.sh            # lvgl presente
138 comprobaciones, 0 fallos          # EXIT=0  -> verde en local, como un dev

$ sh tests/native/run_native_tests.sh --ci-parity
tests/native/../../src/params/../dialog.h:37:5: error: unknown type name 'lv_event_cb_t'
tests/native/../../src/params/../dialog.h:54:35: error: unknown type name 'lv_event_t'
tests/native/../../src/params/../dialog.h:55:18: error: unknown type name 'lv_event_code_t'
tests/native/../../src/dialog.h:37:5: error: unknown type name 'lv_event_cb_t'
tests/native/../../src/dialog.h:54:35: error: unknown type name 'lv_event_t'
tests/native/../../src/dialog.h:55:18: error: unknown type name 'lv_event_code_t'
FALLO ci-parity: el build host depende de lvgl/ (ver arriba)
EXIT=1
```

Es decir: la misma clase de error que llegó a CI ahora se ve en local, antes del PR.

## 4. Gates corridos en este host (salida real)

`tests/native/run_native_tests.sh` (completo, sin `--ci-parity`):

```text
$ sh tests/native/run_native_tests.sh
== compilador: gcc (GCC) 16.2.1 20260819 (Red Hat 16.2.1-2) ==
== sanitizers: NO disponibles en este toolchain (se omite) ==
...
138 comprobaciones, 0 fallos
EXIT=0
```

Los 20 stages del runner suman **51 773 comprobaciones, 0 fallos**; el mismo total
da el árbol de la base y el de esta rama, así que el cambio no altera ninguna
cuenta, sólo el camino de `#include` de un stage. `--ci-parity` imprime lo mismo
(51 773 / 0) con `lvgl/` oculto.

`tests/run_all_tests.sh --quick`:

```text
$ sh tests/run_all_tests.sh --quick
================ resumen ================
  config: OK     OK: los 70 campos se inicializan, en orden
  actions: OK     todas las acciones ofrecidas tienen manejador
  settings_focus: OK     todos los widgets de Ajustes estan en el grupo de foco
  settings_grid: OK
  apps_menu: OK     todas las apps del menu llegan al dispatcher que usa su boton
  app_handlers: OK     todos los botones de las apps tienen handler
  manual_apps: OK     todas las apps tienen su seccion (24 apps)
  pruebas: OK     OK: estructura y evidencia en orden
  wcag: OK     todos los pares cumplen WCAG 2.1 (AA)
  cw_tune_feed: OK
  build_identity: OK
  curve_evidence: OK   PASS: 8 pruebas de swrscan_curve_evidence
  host: OK   138 comprobaciones, 0 fallos
  host_ci_parity: OK   138 comprobaciones, 0 fallos
  drawbuf_partial: OK   PASS: partial draw buffer renders the identical physical framebuffer (sw-rotate 90)
  wspr: SKIP SKIP: wspr: la imagen x6200-uiharness no trae /usr/include/fftw3.h (build full: docker build -t x6200-uiharness tools/ui_harness)
  sstv: SKIP SKIP: sstv: la imagen x6200-uiharness no trae /usr/include/sndfile.h (build full: docker build -t x6200-uiharness tools/ui_harness)
  wefax: SKIP SKIP: wefax: la imagen x6200-uiharness no trae /usr/include/sndfile.h (build full: docker build -t x6200-uiharness tools/ui_harness)
  navtex: OK   all OK
  (omitidos: wspr sstv wefax js8 webserver)
  16 OK, 0 fallos
EXIT=0
```

SKIPs, con la razón exacta (no se declaran PASS):

- `wspr`/`sstv`/`wefax`: la imagen `x6200-uiharness` de este host es la mínima
  (`--target ci`), sin `fftw3.h`/`sndfile.h`. El runner sale 77 y `run_all` lo cuenta
  como omitido. Para correrlos: `docker build -t x6200-uiharness tools/ui_harness`
  (target full).
- `js8`: omitido por `--quick` (en la corrida completa corre el suite del X6100).
- `webserver`: no hay checkout en `state/worktrees/x6200/.x6200_webserver`
  (`WS=tests/../.x6200_webserver`), así que `run_all_tests.sh` lo omite.
- Sanitizers (ASan/UBSan/TSan): no disponibles en el toolchain de este host
  (gcc 16.2.1). El runner lo imprime y sigue con compilación normal + `-fanalyzer`;
  no es un PASS de sanitizers.
- `shellcheck`/`shfmt`: no instalados en este host y el repo no los corre en CI
  (los workflows sólo usan los chequeos python + el runner nativo). Sin veredicto.
