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

## Actualización — fix del rojo de CI (head `7dc6aea7`)

El job `UI Functional Tests` (run 39821158267, head `1e3860b5`) falló en
`tests/test_scenarios.py::test_repo_fixtures_are_all_parsable` porque
`locked-feedback` no estaba en el inventario versionado. Fix en
`tools/ab_testing/tests/test_scenarios.py` (lista ordenada índice 135 + set de
`screens` conocidas). Gates re-corridos:

```
cd tools/ab_testing && uv run pytest -m "not e2e" -q
233 passed, 13 deselected in 2.28s
EXIT=0

uv run ruff check .            -> All checks passed! (exit 0)
uv run ruff format --check .   -> 200 files already formatted (exit 0)
python3 tests/check_autotune_v2.py -> contrato estatico de Auto Tune (CW v2 + multimodo) OK (exit 0)

SKIP_BUILD=1 sh tests/check_harness_determinism.sh
PASS: deterministic (identical PNGs for every fixture)
determinism EXIT=0

x6200-ab contracts --revision 7dc6aea7
contracts 7dc6aea7f42c: PASS — 2116 checks, 0 failure(s), 0 unchanged baseline defect(s)

sh tests/run_all_tests.sh --quick
19 OK, 0 fallos
run_all_quick EXIT=0

check-matrix.sh b798f13b HEAD <worktree>
==> [check] pytest -m 'not e2e' (ab_testing)
    ok
==> [check] x6200-ab contracts (7dc6aea7f42c64d0814400f573d70511c1b09554)
    ok
==> [check] harness determinism gate
    ok
matrix EXIT=0 (todos los checks `ok`; banco QEMU BLOCKED por entorno, ver arriba)
```

Nota: la matriz de esta actualización se corrió con base `b798f13b` (el
merge-base real de la rama). El ref `origin/lu2dsx/x6200-rom` avanzó a
`9e571d89` (#233/#234) durante la corrida; usar ese ref como base mostraría como
"borrados" los archivos de #217/#218 y ensuciaría el gate (los cambios de la rama
siguen siendo sólo los de este hijo).
