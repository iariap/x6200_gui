# Salida literal de los gates — issue #220

HEAD de la corrida: `fdf7a6bc0dac39ca06d43a7ab4823285b1e9da94` (base `b798f13b`).
Logs crudos en `logs/`. Se registran las últimas líneas de cada gate.

## `sh tests/native/run_native_tests.sh`

```
EXIT=0
21 stages con "N comprobaciones, 0 fallos"
(1018, 138, 141, 175, 215, 461, 4851, 44738, ... comprobaciones — todos 0 fallos)
```

## `sh tests/native/run_native_tests.sh --ci-parity`

```
== ci-parity: OK (build host sin lvgl/) ==
EXIT=0
```

## `python3 tests/audita-pruebas.py`

```
  builds en la tabla: 237 | secciones: 14 | enlaces: 59
  para revisar a mano (32): ...
  OK: estructura y evidencia en orden
EXIT=0
```

## `SKIP_BUILD=1 sh tests/check_harness_determinism.sh`

```
running every fixture 2 times...
<205 fixtures × 2, sha256 idénticos por fixture, incluido locked-feedback>
PASS: deterministic (identical PNGs for every fixture)
```

## Fixture dedicada `tools/ui_harness/fixtures/locked-feedback.scn`

```
ui_harness: locked-feedback -> out/locked-feedback.png (254030 bytes, flushes=132, flushed_pixels=7516060)
EXIT=0
```

Todas las aserciones de `settle_locked_feedback()` (ver la matriz en README.md)
pasaron; el harness corta con exit 1 y el mensaje de la aserción si alguna falla.

## Gates de la épica (resultado real)

- `x6200-ab contracts`: PASS — `contracts fdf7a6bc0dac: PASS — 2116 checks, 0 failure(s), 0 unchanged baseline defect(s)`
  (con el fixture dejando visible el indicador `xN` del dial, el gate lo
  reportaba como `layout_overlap` contra el reloj: el auto-ocultado de 2 s del
  `xN` se resolvio dejandolo vencer en el fixture, no con un waiver)
- `x6200-ab themes`: PASS (EXIT=0)
- `sh tests/check_harness_natural.sh`: PASS — `PASS: natural bench deterministic, profiled and delimited (main-natural)`
- `sh tests/check_harness_wakeups.sh`: PASS (EXIT=0; `png sha256: 7a83e632376a2f5c4ea5948549aed9a09fb43801b21e9b7f6b7dcc81e18e1f66`)
- `sh tests/check_waterfall_incremental.sh`: PASS — `PASS: waterfall incremental fixtures + ASan/UBSan (#148)`
- `sh tests/run_all_tests.sh`: PASS — `19 OK, 0 fallos` + SKIPs herméticos por la imagen `ci` (sin fftw3/sndfile): `wspr sstv wefax js8 webserver`
- `scripts/apps/x6200/check-matrix.sh origin/lu2dsx/x6200-rom HEAD <worktree>`: EXIT=0 — native tests + 9 checks estáticos + build_identity + `x6200-ab themes` + `x6200-ab contracts` + harness determinism gate: todos `ok`. Banco QEMU: **BLOCKED** por entorno (`falta el volumen x6200-build-output`, `falta el chroot`, `falta tools/remote/x6200-remote` — recursos privados del owner que este hijo no usa por la política de laboratorio).
