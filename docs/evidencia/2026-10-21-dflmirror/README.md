# Los fixtures del espejo del DFL (#182) en rojo en el trunk — y el CI en verde

Corrida de **fix**: el gate de determinismo del arnés de UI estaba rojo en el trunk
(`dfl-mirror-lsb`, `dfl-mirror-usb`) y `x6200-ab contracts` no podía evaluar (BLOCKED),
sin que el CI lo notara.

| | |
|---|---|
| Worktree | `/home/pablo/desa/asf/state/worktrees/x6200/dfl-mirror-gate` |
| Rama | `factory/dfl-mirror-gate` (sin push: el PR lo abre el orquestador) |
| Base | `e6925db6` (`origin/lu2dsx/x6200-rom`, el tip al arrancar) |
| HEAD | `db6bc31b` (fix del firmware) · `20c9fc3c` (CI + evidencia) · `45bcb383` (la aserción del arnés, **último commit de código**) |
| Imagen del arnés | `x6200-uiharness` (`9eedee0e5605`, 2026-10-14) y una construida acá con `--target ci` |

Los gates que dependen del SHA corrieron sobre el **último commit de código** (`45bcb383`);
lo que vino después de esa corrida es sólo documentación/evidencia (esta página incluida).

## 1. El síntoma

```
$ SKIP_BUILD=1 tests/check_harness_determinism.sh      # base e6925db6, sin tocar nada
dfl-mirror-lsb: el gesto colgado tras cambiar de modo toco el filtro (low=200 high=3525)
dfl-mirror-usb: el handle LOW no esta dibujado en x=37 (mejor columna x=-1, run=0)
```

y por eso el gate `contracts` quedaba BLOCKED (exit 2). No era *timing*: las aserciones
internas del propio escenario (`scenario_settle` / `scenario_settle_pixels`) cortan el
arnés antes de exportar la captura.

## 2. Por qué local fallaba y el CI pasaba (las dos mitades)

### 2.a El rojo es real y está en el trunk desde **#183** (bisección, medido)

El arnés compilado en cada merge, con los dos fixtures (`biseccion.txt`, script en
`biseccion.sh`):

```
d32b59e8  DFL #182 (merge de #184):  dfl-mirror-lsb exit=0   dfl-mirror-usb exit=0
6666806e  DFL #183 (merge de #185):  dfl-mirror-lsb exit=1   dfl-mirror-usb exit=1   <- acá se rompe
db6bc31b  el fix:                    dfl-mirror-lsb exit=0   dfl-mirror-usb exit=0
```

**La imagen no es la diferencia** (`ci-hueco/dos-configuraciones-base.txt`): sobre el árbol
base, con la imagen local del owner y con una imagen construida ahora con
`docker build -t x6200-uiharness-dflmirror-ci --target ci tools/ui_harness`, los dos
fixtures dan **exactamente el mismo exit 1 y el mismo mensaje**; con el fix, las dos
imágenes dan exit 0. (La imagen sólo aporta dependencias: el arnés se compila del árbol
montado en `/src`.)

### 2.b El CI nunca lo vio, por dos motivos distintos (medidos con sus artefactos)

1. **El paso de reporte dejaba pasar un `BLOCKED`.** En el run `39644351915` (PR #191,
   head `01c5008a`) el job *UI Quality Contracts* quedó `success`
   (`ci-hueco/gh-runs.txt`) y su artefacto `ui-contracts` dice:

   ```json
   {"status": "BLOCKED", "exit_code": 2,
    "error": "harness failed for scenario dfl-mirror-lsb on 01c5008a43bf (exit 1): ...
              dfl-mirror-lsb: el gesto colgado tras cambiar de modo toco el filtro (low=200 high=3525)"}
   ```

   (`ci-hueco/run-39644351915-191-BLOCKED.json`). El paso sólo fallaba con
   `status == "FAIL"` (exit 0 siempre para lo demás).

2. **El run que “pasó” no evaluaba estos fixtures.** El `contracts PASS — 2033 checks,
   0 failure(s)` del reporte de #183 es el run `39511316773` (head `6542d48f`), que
   evaluó **196 escenarios sin `dfl-mirror-*`**: #183 salió de un punto de rama anterior
   al merge de #184, que es quien trajo esos dos fixtures. El run de #182
   (`39490433674`, head `e86a46e2`) sí los evaluó: **144 escenarios, los dos del espejo
   incluidos, PASS 1500 checks** (`ci-hueco/run-*-{182,183}-PASS.json`). Como el workflow
   sólo dispara en `pull_request`, el trunk no se revalida después del merge: el rojo vivió
   en main sin que ningún run lo mirara.

## 3. La causa (código) y el fix

El editor inline es la página **C** (`buttons_page_dfl_expanded`) de las páginas DFL de
#183, y se abre **directo** por el long press físico de DFL (`params.long_dfl`), por la
celda `Grafico` y por el control remoto, sin pasar por A/B. `buttons_dfl_open()` fija la
familia vigente (`dfl_last_family`) pero `dfl_inline_expand()` no: el **primer** cambio de
modo con el editor abierto se comparaba contra “sin familia”, se leía como familia NUEVA y
`buttons_dfl_on_mode_change()` contraía el editor y saltaba a Favoritos. Un cambio
USB-D ↔ LSB-D —que #183 documenta como la MISMA familia, “no se toca la página”— cerraba
el editor. Se ve en la captura: la barra queda en Favoritos con el chip `Personalizado`, sin
el panel del editor (`png/dfl-mirror-usb-trunk-e6925db6.png`, contra
`png/dfl-mirror-usb-fix-db6bc31b.png`).

**Fix del firmware** (`db6bc31b`): `buttons_dfl_graph_open()` (src/buttons.cpp, declarado en
src/buttons.h) centraliza la carga de la página C **y** la familia vigente;
`dfl_inline_expand()` la usa en lugar de `buttons_load_page(&buttons_page_dfl_expanded)`.

**Restricción codificada en el arnés** (`45bcb383`): el paso 6 de `dfl-editor-mirror`
(la aserción del “gesto colgado”) ahora exige, antes de mirar píxeles, que tras el cambio
de modo de la MISMA familia el editor siga abierto y la barra siga en C
(`tools/ui_harness/src/screens.c`). El mensaje viejo (`el handle LOW no esta dibujado en
x=37`) era el síntoma aguas abajo y costó una investigación; el nuevo corta en la causa.
La misma aserción, aplicada sobre el árbol de la revisión que rompió, lo reproduce
(`harness/aserion-nueva-en-183.txt`):

```
dfl-mirror-lsb: el cambio de modo de la misma familia cerro el editor (expanded=0, pagina=1, esperaba C)
```

**Los fixtures NO estaban obsoletos**: con el fix, el PNG de los dos fixtures es
**byte-idéntico** al de `d32b59e8` (pre-#183) — `07c37a57…` y `f2565125…`, que además son
los sha del gate de determinismo. El render volvió exactamente al de #182, así que no se
tocó ni un paso ni una aserción de los dos fixtures (`biseccion.txt`).

## 4. El agujero del CI, cerrado

El paso *Report the contracts and fail on a violation* (`.github/workflows/ab-testing.yml`)
ahora sólo deja pasar `PASS` y los estados que el **header del workflow** ya declara
legítimos (`REVIEW REQUIRED`, `INCONCLUSIVE`); `FAIL` y todo lo que no sea un PASS limpio
—`BLOCKED`, o un status ausente— fallan el job. La semántica documentada no cambia.

Verificación, corriendo el **cuerpo exacto del paso** (extraído del YAML con la misma
desindentación que hace YAML) sobre payloads reales (`ci-hueco/paso-reporte.txt`, script en
`ci-hueco/repro-paso-reporte.sh`):

| payload | paso de la base `e6925db6` | paso del árbol (con el fix) |
|---|---|---|
| `BLOCKED` real (artefacto de #191) | **exit 0 (verde: el agujero)** | **exit 1 (rojo)** |
| `PASS` real (artefacto de #183) | exit 0 | exit 0 |
| `FAIL` sintético | — | exit 1 |
| `REVIEW REQUIRED` | — | exit 0 (legítimo, sin cambios) |
| `INCONCLUSIVE` | — | exit 0 (legítimo, sin cambios) |
| `contracts.json` ausente | — | exit 2 |

## 5. Gates (salida real)

| gate | resultado | evidencia |
|---|---|---|
| `SKIP_BUILD=1 tests/check_harness_determinism.sh` | **PASS** — 199 fixtures, PNG idéntico entre las 2 corridas; `dfl-mirror-lsb` / `-usb` con los mismos sha que en `d32b59e8` | `determinismo-fix.txt` |
| `x6200-ab contracts --revision 45bcb383 --repo <worktree>` | **PASS** (exit 0) — 199 escenarios (los 2 del espejo incluidos), **2056 checks, 0 failure(s)** | `ab/contracts-fix.json` |
| `x6200-ab themes` | exit 0 | `ab/ruff-format-ty-themes.txt` |
| `ruff check` / `ruff format --check` / `ty check src tests` | exit 0 los tres | `ab/ruff-format-ty-themes.txt` |
| `pytest -m 'not e2e'` | exit 0 — 227 passed, 13 deselected | `ab/pytest-not-e2e.txt` |
| `pytest -m e2e` (TMPDIR en disco) | exit 0 — 13 passed (incluye `test_all_fixtures_can_be_captured`) | `ab/pytest-e2e.txt` |
| `tests/native/run_native_tests.sh` | exit 0 — 138 comprobaciones, 0 fallos | `nativos/run_native_tests.txt` |
| `tests/run_all_tests.sh --quick` | exit 0 — 17 OK, 0 fallos (3 codecs SKIP por deps ausentes en la imagen `ci`) | `run_all_tests-quick.txt` |
| `IMAGE=x6200-uiharness-full tests/run_all_tests.sh --quick` | exit 0 — 20 OK, 0 fallos (wspr 158 comprobaciones; sólo js8/webserver omitidos) | `run_all_tests-quick-full.txt` |
| estáticos del repo (8) | exit 0 los ocho | `estaticos/checks-del-repo.txt` |
| `check-matrix.sh origin/lu2dsx/x6200-rom HEAD <worktree>` | exit 0 — todo `ok`; el banco QEMU **BLOCKED** con su preflight real (sin chroot `.vfix3` en este worktree, y sin `tools/remote/x6200-remote`); NO se tocó ningún volumen/chroot del owner | `check-matrix.txt` |
| `.agents/hallazgos-fundados/scripts/audita-hallazgos.sh docs/HALLAZGOS.md` | 0 problemas de estructura | §62 de `docs/HALLAZGOS.md` |

Nota sobre la matriz: en la corrida de `20c9fc3c` (antes de la aserción del arnés) el gate
de determinismo salió `[SKIP]` por path (`sin cambios en tools/ui_harness/`); en la corrida
de `45bcb383` ese path sí cambia y el gate corre dentro de la matriz.

## 6. Hallazgo

`docs/HALLAZGOS.md` §62 (con la bisección, el artefacto del CI y la verificación).

## 7. Pendientes / SKIP razonados

- El **banco QEMU** de la matriz corre en el host del owner (volumen + chroot). El chroot
  que espera `tools/captura-banco.sh` es `$REPO/../.vfix3` = `state/worktrees/x6200/.vfix3`,
  que no existe en este worktree, y el `.vfix3` del owner ya no está en `work/`: la matriz
  lo reporta **BLOCKED con el preflight real** (`check-matrix.txt`); no se creó ni se borró
  ningún volumen/chroot ajenos. Lo que el banco habría aportado (el binario ARM corriendo)
  no se midió acá: este fix es de estado de UI y el render quedó byte-idéntico.
- No se corrió un **A/B visual** (`x6200-ab compare`) ni el **benchmark**: no son gates de
  este fix y el render es byte-idéntico al de `d32b59e8` (medido con `sha256sum`), así que
  no hay cambio estético ni de costo de render que comparar.
- **Sin fundar**: que el workflow de main nunca haya corrido `contracts` con estos fixtures
  después del merge de #183 (no hay runs de `push` para mirar). Lo medido es el trigger
  `pull_request` y los 196 escenarios del run de #183.
