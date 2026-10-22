# Fix de infraestructura de tests: la suite nativa y los contratos del trunk

Fecha: 2026-10-22
Base: `fea0960b` (tip de `origin/lu2dsx/x6200-rom`)
Rama: `factory/fix-native-sdk-stub`
HEAD con el fix: `151e9a9e`

El merge del PR #232 (`fix(audio): use the CW enhancement profile…`, merge
`c02487a3`) dejó el trunk en rojo: `UI Functional Tests` y `UI Quality
Contracts` fallan desde entonces. Este documento deja el antes medido, la
causa y cada gate con su salida real.

## Antes (control)

En un worktree limpio de `fea0960b`, sin cambios:

```
$ sh tests/native/run_native_tests.sh
== compilador: gcc (GCC) 16.2.1 20260819 (Red Hat 16.2.1-2) ==
== sanitizers: NO disponibles en este toolchain (se omite) ==
In file included from .../src/cfg/cfg.h:5,
                 from .../src/audio_enhancement.c:11:
.../src/cfg/band.h:5:10: fatal error: aether_radio/x6200_control/control.h: No such file or directory
    5 | #include <aether_radio/x6200_control/control.h>
      |          ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
compilation terminated.
rc=1
```

```
$ cd tools/ab_testing && uv run x6200-ab contracts --revision HEAD --repo <worktree>
...
/usr/bin/ld: CMakeFiles/ui_harness.dir/src/src/dialog_settings.c.o: in function `audio_enh_update_cb':
dialog_settings.c:(.text+0xaf6): undefined reference to `audio_enh_apply_mode_preset'
collect2: error: ld returned 1 exit status
rc=2
```

## Causa: la cadena de includes y símbolos que introdujo el #232

El #232 tocó `src/audio_enhancement.c` (no sólo agregó la función, también
includes nuevos) y `src/dialog_settings.c`/`src/dsp.cpp`/`src/main.c`:

- `src/audio_enhancement.c` ahora incluye `cfg/cfg.h` → `cfg/common.h` →
  `cfg/band.h`, y `params/params.h`.
  - `src/cfg/band.h:5` incluye `<aether_radio/x6200_control/control.h>` (SDK de
    la radio, que **no está en este repo**: lo vende Buildroot). Ese include es
    con ángulos, así que sólo lo resuelve un `-I`.
  - `src/params/params.h:15` incluye `<ft8lib/constants.h>` (ft8_lib), también
    con ángulos.
- `audio_enh_apply_mode_preset()` (nueva) usa `params.audio_enh_preset`,
  `subject_get_int(cfg_cur.mode)` y `cfg_cur.mode`: tres símbolos que el stage
  principal del arnés nativo no definía (el test nunca la llamaba, pero el
  linker la necesita porque el objeto está en el binario).
- `src/dialog_settings.c` pasó de llamar `audio_enh_preset_load()` a
  `audio_enh_apply_mode_preset()`; el arnés de UI (`tools/ui_harness`) compila
  `dialog_settings.c` pero **no** `audio_enhancement.c`, así que su capa de
  mocks tenía que aportar el símbolo nuevo.

Dependencias faltantes encontradas **más allá de la primera** (se destaparon
una tras otra al compilar): `ft8lib/constants.h`, y los símbolos
`params`/`cfg_cur`/`subject_get_int` (nativo) y `audio_enh_apply_mode_preset`
(arnés de UI).

## Fix elegido y por qué

1. **`tests/native/run_native_tests.sh`**: el stage principal resuelve el SDK y
   ft8_lib con un único `-I` a los headers ya vendorizados del buildroot,
   `tools/ui_harness/third_party/` (copia verbatim, el mismo `-I` que ya usa el
   stage del #186). Se eligió reusar ese árbol en vez de:
   - copiar los headers a `tests/native/stubs/` (duplicaría lo que ya está
     versionado y podría driftear), o
   - agregar `-I tests/native/stubs_radio` (arrastraría al stage principal
     `lvgl/lvgl.h`, `sqlite3.h`, `params/params.h` y demás stubs ajenos a este
     build, con riesgo de sombrear headers reales).
   El `-I` se centralizó en la variable `MAIN_INCLUDES` y se aplica igual al
   compile y al `-fanalyzer` del stage.
2. **`tests/native/audio_enh_stubs.c`** (nuevo): definiciones de link de
   `params`, `cfg_cur` y `subject_get_int`, con los tipos reales
   (`params_t`/`cfg_cur_t`). El test de este stage nunca llama a
   `audio_enh_apply_mode_preset()`, así que son sólo para el linker; van en un
   archivo propio para que el motivo quede explícito.
3. **`tools/ui_harness/mocks/mock_ui_deps.c`**: mock no-op de
   `audio_enh_apply_mode_preset()`, hermano del de `audio_enh_preset_load()`.
   En el host el preset sigue siendo no-op (no corre speex/liquid), así que no
   cambia ninguna cobertura del arnés.

No se tocó código de producción (`src/**`), no se debilitó ningún assert, no se
quitó `audio_enhancement.c` de `SOURCES` ni se usó `|| true`.

## Gates (salida real, después del fix)

### `sh tests/native/run_native_tests.sh` → rc=0

```
== compilador: gcc (GCC) 16.2.1 20260819 (Red Hat 16.2.1-2) ==
== sanitizers: NO disponibles en este toolchain (se omite) ==
...
44738 comprobaciones, 0 fallos
...
36 comprobaciones, 0 fallos
rc=0
```

(el stage principal pasa de no compilar a 44738 comprobaciones, 0 fallos; la
última línea es el test `#219 radio_thread`)

### `sh tests/native/run_native_tests.sh --ci-parity` → rc=0

```
== ci-parity: lvgl/ oculto (el arbol que ve el job functional) ==
...
== ci-parity: OK (build host sin lvgl/) ==
rc=0
```

### `uv run x6200-ab contracts --revision HEAD --repo <worktree>` → PASS

```
contracts 151e9a9ef156: PASS — 2116 checks, 0 failure(s), 0 unchanged baseline defect(s)
rc=0
```

### `sh tests/run_all_tests.sh --quick` → rc=0

```
================ resumen ================
  config: OK ...
  ...
  host: OK   36 comprobaciones, 0 fallos
  host_ci_parity: OK   36 comprobaciones, 0 fallos
  ...
  19 OK, 0 fallos
```

### `python3 tests/audita-pruebas.py` → rc=0

```
  builds en la tabla: 237 | secciones: 14 | enlaces: 59
  OK: estructura y evidencia en orden
audita rc=0
```

### `bash -n tests/native/run_native_tests.sh` → ok (también `sh -n`)

### Matriz del harness `check-matrix.sh origin/lu2dsx/x6200-rom HEAD <worktree>` → rc=0

```
==> [SKIP] firmware / nativo: sin cambios en src/**, lv_conf.h, lv_drv_conf.h ni CMakeLists.txt
==> [SKIP] check_build_identity: sin cambios en la identidad del build
==> [SKIP] check_wcag (temas de la imagen): sin cambios en rootfs/**/themes/*.json
==> [SKIP] ruff del framework A/B: sin cambios en tools/ab_testing/**/*.py
==> [SKIP] pytest del framework A/B: sin cambios en tools/ab_testing/**
==> [check] x6200-ab themes
    ok
==> [check] x6200-ab contracts (151e9a9ef1560ff6ab98b077eacf697dd119a861)
    ok
==> [check] bash -n tests/native/run_native_tests.sh
    ok
==> [check] harness determinism gate
    ok
==> [check] banco QEMU: preflight (tools/captura-banco.sh --check)
    BLOCKED: falta entorno (corre en el host del owner, no en el runner)
```

El banco QEMU queda BLOCKED por entorno del runner (falta el volumen
`x6200-build-output`, el chroot y `tools/remote/x6200-remote`), como declara el
propio script: no es un fallo del cambio ni cambia el exit de la matriz.

## Archivos cambiados

| Archivo | Cambio |
|---|---|
| `tests/native/run_native_tests.sh` | `MAIN_INCLUDES` con `-I…/tools/ui_harness/third_party` en el compile y el `-fanalyzer` del stage principal; `audio_enh_stubs.c` en `SOURCES` |
| `tests/native/audio_enh_stubs.c` | nuevo: definiciones de link de `params`, `cfg_cur`, `subject_get_int` |
| `tools/ui_harness/mocks/mock_ui_deps.c` | mock no-op de `audio_enh_apply_mode_preset()` |
