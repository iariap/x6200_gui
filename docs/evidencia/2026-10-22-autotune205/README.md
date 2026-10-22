# Auto Tune multimodo — implementación del issue #205 — evidencia de la corrida

Corrida de implementación de **`iariap/x6200_gui#205`** ("[RX][Auto Tune]
Extender Auto Tune v2 a USB/LSB y AM/SAM sin degradar CW").

| | |
|---|---|
| Worktree | `/home/pablo/desa/asf/state/worktrees/x6200/autotune-multimode-205` |
| Rama | `factory/autotune-multimode-205` (el issue proponía `feat/autotune-multimode`; el harness usa una rama por corrida) |
| Base | `00a653e7` = tip de `origin/lu2dsx/x6200-rom` al arrancar (incluye Auto Tune v2 de #190/#191, el supervisor, SQLite y el fix del gate del boot anim) |
| HEAD al medir | `7fa314f9eee27790c7430e83d991134f25377795` (5 commits; ver *Commits* abajo) |
| Push | **ninguno**: el PR lo abre el orquestador contra `lu2dsx/x6200-rom` con `Closes #205` |

> ## **Hardware RF validation pending**
> Todo lo de abajo es host/banco (políticas puras con espectros sintéticos, banco
> de UI sin RF y sin DSP). La precisión con señales reales **no** se ejecutó en un
> X6200: la lista de lo que falta medir en la radio está en la sección *Pendientes*
> y en `docs/autotune.md`. Ningún resultado de esta corrida se atribuye a RF real.

## 1. Qué se implementó (una acción, dos políticas nuevas)

| modo | estrategia | escritura | verificación | confirmación |
|---|---|---|---|---|
| CW/CWR | la de #190, **sin tocar** | 1 corrección ≤ ±300 Hz | 3 tramas nuevas | no |
| AM/SAM | `AM_CARRIER`: portadora estrecha, única y dominante | 1 corrección ≤ ±1 kHz | guarda 300 ms + 3 descartes + 4 tramas nuevas, rollback condicionado | no (Undo 5000 ms) |
| USB/LSB | `SSB_SUGGEST`: energía ocupada en la banda lateral | **1 sola escritura, sólo tras la 2.ª pulsación** | no aplica (no hay inteligibilidad que verificar) | **sí, obligatoria** (Undo 5000 ms) |
| USB-D/LSB-D, FM/NFM/WFM | — | 0 | — | `Auto Tune: mode unsupported` |

Archivos (diff por archivo, ver la sección 7):
`src/dsp.h`, `src/dsp.cpp`, `src/autotune_spectrum.c/h` (nuevos), `src/autotune_ui.c`,
`src/CMakeLists.txt`, `tests/native/test_autotune_spectrum.c` (nuevo),
`tests/native/test_main.c`, `tests/native/run_native_tests.sh`,
`tests/check_autotune_v2.py`, `tools/ui_harness/{CMakeLists.txt,mocks/mock_dsp.c,mocks/mock_spectrum.h,src/screens.c,src/coverage.c,fixtures/*}`,
`tools/ab_testing/tests/test_scenarios.py`, `docs/autotune.md`.

## 2. Los 169 checks de CW/CWR: intactos (hecho probado, no afirmación)

El núcleo CW de #190 **no se tocó**: sus tres archivos son byte a byte los de la
base, y eso está **pineado** por md5 en `tests/check_autotune_v2.py`
(sección 7 del chequeo), así que cualquier cambio al núcleo CW rompe el gate
estático del PR.

```
$ for f in src/autotune.c src/autotune.h tests/native/test_autotune.c; do
    echo -n "$f "; git show 00a653e7:"$f" | md5sum | cut -d' ' -f1; done
src/autotune.c                 30814c17da9761b73bcce6423343ffcf
src/autotune.h                 e5e62ebee115502599e15cd6a7372c7a
tests/native/test_autotune.c   3c223be5ebef84012f4061c585ea2794
# (idénticos en HEAD: el pin del chequeo estático los compara y pasa)
```

Su suite, corrida aislada (mismo binario, mismos 169 checks de siempre):

```
$ gcc -std=gnu11 -O1 -g -Wall -Wextra -Wno-unused-parameter -Itests/native -Isrc \
    only_at.c src/autotune.c tests/native/test_autotune.c tests/native/test_framework.c -lm
$ ./only_at
169 comprobaciones, 0 fallos
```

La ruta CW dentro del coordinador tampoco cambió: el cuerpo de `tick_cb()` para CW
se movió **tal cual** a `cw_tick()` (mismos umbrales, mismos mensajes, misma
verificación con 3 tramas nuevas) y el único agregado es el despacho por
estrategia alrededor. Los fixtures CW del banco (`autotune`,
`autotune-progress`, `autotune-found`) siguen pasando sin cambios.

## 3. Tests puros nuevos: `tests/native/test_autotune_spectrum.c` (225 checks)

```
$ gcc -std=gnu11 -O1 -g -Wall -Wextra -Wno-unused-parameter -Itests/native -Isrc \
    only_sp.c src/autotune_spectrum.c src/autotune.c tests/native/test_autotune_spectrum.c \
    tests/native/test_framework.c -lm     # only_sp.c: main() que llama solo a ese test
$ ./only_sp
225 comprobaciones, 0 fallos
```

Cobertura de la lista obligatoria de la sección 10 del issue (todos con salida real
de la corrida; el detalle de cada caso está en el fuente):

| caso pedido | dónde | resultado |
|---|---|---|
| PSD plano / silencio / QRM (3 dB y empate) | `test_am_flat_and_silence`, `test_am_thresholds` | rechazo en los tres ✓ |
| NaN / Inf (pico y piso de ruido) | `test_am_non_finite_rejects`, `test_ssb_measurement_conditions` | rechazo ✓ |
| empate de portadoras / picos superpuestos | `test_am_thresholds` | empate rechaza; superpuestos (Δ < resolución) son **una** portadora ✓ |
| AM ±390 / ±780 Hz | `test_am_single_carrier` | candidatos −390/390/780 (y 376/752 a 96 kHz) ✓ |
| AM ya centrado y a ±1000 (borde) | `test_am_already_centered_and_at_the_edge` | 0 ⇒ `CENTERED`; borde ⇒ ±975 ✓ |
| AM con pico < 12 dB | `test_am_thresholds` | 11 dB rechaza, 12 dB acepta ✓ |
| borde de banda / portadora fuera de la ventana | `test_am_outside_the_window_is_not_a_carrier` | rechazo ✓ |
| espectro TX / epoch viejo / zoom (384 kHz) | `test_measurable_limits`, `test_gate_context_and_tx` | rechazo / `BROKEN` / no medible ✓ |
| resolución 512 vs 800 | `test_coordinates_and_resolution` | res 195,3125 Hz y step 195 ≠ 125 ✓ |
| 4 tramas nuevas post-300 ms con verificación y rollback | `test_am_session_converges`, `..._rolls_back_when_the_carrier_does_not_move` | 1 escritura y verificada / 2 escrituras (rollback) y 3 publicaciones descartadas ✓ |
| SSB USB dominante y espejo LSB con banda real | `test_ssb_usb_voice_outside_the_passband`, `test_ssb_lsb_is_mirrored` | +752 Hz / −752 Hz ✓ |
| dos estaciones, voz no estacionaria, filtros fuera de rango, candidato 0 o > 800, score cerca del umbral | `test_ssb_abstains`, `test_ssb_score_threshold`, `test_ssb_collect_five_frames`, `test_ssb_measurement_conditions` | abstención en todos ✓ |
| 5 tramas válidas | `test_ssb_collect_five_frames` | `READY` + `SUGGEST` ✓ |
| PREVIEW timeout, 2.º trigger, Undo dentro/fuera de 5000 ms | `test_phase_windows`, `test_press_semantics` | plazos exactos (3000/2500/5000/5000), 4999 ms vivo / 5000 ms vencido, PREVIEW⇒APPLY, UNDO⇒UNDO ✓ |
| QSY externo durante PREVIEW y Undo | `test_qsy_wins_during_preview_and_undo` | `autotune_owns()` false ⇒ jamás se escribe encima ✓ |

Dos hallazgos del contrato, codificados en los tests (no supuestos):

1. **El tope de ancho hace redundante al de resolución**: con `width ≤ 100 kHz` la
   resolución máxima es 195,3 Hz, así que el guardia de 250 Hz no puede aflojarse
   nunca (`test_measurable_limits` lo afirma con los números).
2. **En USB/LSB la tolerancia de la ventana (max(200, resolución)) es más ancha que
   un paso de cuantización**, así que el jitter de una voz estacionaria nunca la
   rompe: lo que la rompe es cambiar de lado (el estado no mezcla signos).
   `test_ssb_collect_five_frames` lo declara explícitamente.

## 4. Gates, con su salida real

### 4.1 Suite nativa completa — `sh tests/native/run_native_tests.sh`

```
$ sh tests/native/run_native_tests.sh
== compilador: gcc (GCC) 16.2.1 20260819 (Red Hat 16.2.1-2) ==
== sanitizers: NO disponibles en este toolchain (se omite) ==
...
44738 comprobaciones, 0 fallos      <- binario principal (incluye las 225 nuevas y las 169 CW)
...
```
`nativos-native-tests.log` (log completo). **0 fallos**, exit 0.

### 4.2 Contrato estático — `python3 tests/check_autotune_v2.py`

```
$ python3 tests/check_autotune_v2.py
  textos: 8 CW + 8 multimodo | long_action_items: 37 entradas | ACTION_ATUNE=129 |
  pins CW: 3 | fixtures: 9
  contrato estatico de Auto Tune (CW v2 + multimodo) OK
```
`estaticos/check_autotune_v2.log`. Qué cubre: los 16 textos exactos (los 8 de CW
que sobreviven + los 8 nuevos), la **ausencia** del texto viejo
`Auto Tune: CW/CWR only`, la entrada de Settings, el id 129, los defaults
intactos, la pureza de los dos núcleos, las prohibiciones de la UI, los pines md5
del núcleo CW, el snapshot (800/512), el publicador en `dsp_samples()`, la única
vía de escritura del dial, la invalidación del epoch, el rechazo de digital/FM y la
existencia + armado de los 9 fixtures de la familia.

**Prueba negativa del propio chequeo** (`estaticos/check_autotune_v2-mutaciones.log`):
4 mutaciones deliberadas y cada una tiene que ser un FALLO.
1. quitar `Auto Tune: restored` → `FALLO: falta el texto exacto ...`;
2. tocar `src/autotune.c` → `FALLO: src/autotune.c cambio (md5 ... != ...)`;
3. reintroducir `Auto Tune: CW/CWR only` → dos FALLOS (falta el nuevo y sobra el viejo);
4. devolver `autotune-notcw.scn` → `FALLO: autotune-notcw.scn sigue existiendo ...`.
Los cuatro dieron exit 1; el estado final sin mutaciones, exit 0.

### 4.3 Banco de UI: los 9 escenarios de la familia

```
$ docker run ... x6200-uiharness -lc 'for n in autotune...; do ./ui_harness --scenario ../fixtures/$n.scn ...; done'
OK   autotune             -> out/at/autotune.png (249304 bytes), flushes=6
OK   autotune-progress    -> out/at/autotune-progress.png (249850 bytes), flushes=9
OK   autotune-found       -> out/at/autotune-found.png (249850 bytes), flushes=10
OK   autotune-digital     -> out/at/autotune-digital.png (249130 bytes), flushes=6
OK   autotune-am          -> out/at/autotune-am.png (248376 bytes), flushes=10
OK   autotune-usb         -> out/at/autotune-usb.png (245794 bytes), flushes=16
OK   autotune-lsb         -> out/at/autotune-lsb.png (245780 bytes), flushes=16
OK   autotune-usb-applied -> out/at/autotune-usb-applied.png (247159 bytes), flushes=19
OK   autotune-usb-timeout -> out/at/autotune-usb-timeout.png (245794 bytes), flushes=16
```
Capturas y JSON en `banco-ui/` (PNG + JSON + log por escenario). Qué afirma cada
uno (las comprobaciones viven en `scenario_settle`, con el estado real del
firmware, no en el pixel):

- `autotune-digital`: en USB-D la acción **no** abre sesión, **no** pide ninguna
  publicación espectral y **no** toca el dial de datos.
- `autotune-am`: una **sola** escritura, el dial queda en `start + 376` y **sigue
  ahí** después de la ventana de verificación, con la sesión viva (Undo abierta).
  Si la verificación fallara, el rollback lo devolvería al arranque ⇒ el pixel no
  podría estar en `start + 376`.
- `autotune-usb` / `autotune-lsb`: avanzando **más allá** del plazo de COLLECT
  (3,5 s), el dial está **exactamente** donde estaba y la sesión sigue viva ⇒ sólo
  puede estar en PREVIEW. La voz **no** se retunea sola.
- `autotune-usb-applied`: 1.ª pulsación sin mover; 2.ª aplica **una** escritura a
  `start + 752` y deja viva la Undo; 3.ª restaura `start` y cierra la sesión.
- `autotune-usb-timeout`: plantea la propuesta y vence los 5 s **sin** escritura
  y **sin** sesión viva (no hay auto-apply ni auto-undo).

Nota honesta: `autotune-usb` y `autotune-usb-timeout` terminan con el mismo PNG
(mismo dial, misma pantalla; la barra de mensajes no se dibuja en el host), así
que lo que los distingue es la comprobación de estado, no el pixel.
`autotune-usb`/`autotune-lsb` sí difieren entre sí (la banda lateral cambia el
espectro dibujado).

### 4.4 Determinismo del banco — `tests/check_harness_determinism.sh`

```
$ SKIP_BUILD=1 bash tests/check_harness_determinism.sh
autotune-am: 7accf2351f9d9fd663b949486381eee19ab401adddc6b35d9309075cf5a666d9
autotune-usb: 83fb1417221552a83865e1d81901bfa95bd3aa0f1e0851481315b31dae98c0bf
...
PASS: deterministic (identical PNGs for every fixture)
```
`banco-ui/determinism-completo.log` (los 204 fixtures, 2 corridas cada uno, PNG
idénticos). El gate también lo corre la matriz (4.7).

### 4.5 Framework del arnés A/B (ruff, pytest, themes, contracts)

```
$ (cd tools/ab_testing && uv run ruff check .)          -> All checks passed!
$ (cd tools/ab_testing && uv run ruff format --check .) -> 199 files already formatted
$ (cd tools/ab_testing && uv run pytest -m 'not e2e' -q) -> 227 passed, 13 deselected
$ (cd tools/ab_testing && uv run x6200-ab themes)       -> OK (contrastes WCAG)
$ (cd tools/ab_testing && uv run x6200-ab contracts --revision 7fa314f9... --repo ...)
contracts 7fa314f9eee2: PASS — 2106 checks, 0 failure(s), 0 unchanged baseline defect(s)
```
`gates/ab-testing-pytest.log`, `gates/x6200-ab-contracts.log`. El gate `contracts`
materializa el commit, compila el arnés en `x6200-uiharness` y captura **todos**
los escenarios: es la prueba de que los fixtures nuevos no rompen ningún contrato
de calidad ni de tema.

### 4.6 `sh tests/run_all_tests.sh --quick`

```
$ sh tests/run_all_tests.sh --quick
  config: OK ... autotune_v2: OK ... host: OK ... host_ci_parity: OK ...
  wspr: SKIP SKIP: la imagen x6200-uiharness no trae /usr/include/fftw3.h (build full: ...)
  sstv: SKIP ... wefax: SKIP ... js8: SKIP ... webserver: SKIP (omitidos)
  19 OK, 0 fallos
```
`gates/run_all_tests-quick.log`. Los 4 SKIP son de entorno y preexistentes: la
imagen instalada es la `ci` (sin fftw/sndfile) y no hay checkout del webserver; se
corren con la imagen `full`. `autotune_v2: OK` y `host_ci_parity: OK` incluyen el
código de esta corrida.

### 4.7 Matriz del harness (la corrida completa)

```
$ scripts/apps/x6200/check-matrix.sh origin/lu2dsx/x6200-rom HEAD <worktree>
==> [check] native tests (tests/native/run_native_tests.sh)      ok
==> [check] check_cfg_fills / check_actions / check_settings_focus / check_apps_menu
            / check_app_handlers / check_manual_apps / check_wcag / check_cw_tune_feed
            / check_build_identity                              ok (9/9)
==> [SKIP] check_wcag (temas de la imagen)   sin cambios en rootfs/**/themes/*.json
==> [check] ruff check / ruff format --check / pytest -m 'not e2e' (ab_testing)  ok
==> [check] x6200-ab themes / x6200-ab contracts (7fa314f9...)                   ok
==> [check] bash -n tests/native/run_native_tests.sh                              ok
==> [check] harness determinism gate                                              ok
==> [check] banco QEMU: preflight (tools/captura-banco.sh --check)   BLOCKED
rc=0
```
`gates/check-matrix.log`. El BLOCKED del banco QEMU es de entorno y **no** marca
FAILED (la matriz lo declara así por diseño): faltan el chroot del rootfs
(`../.vfix3`), el cliente ARM `tools/remote/x6200-remote` y dos libs más — son
artefactos del host del owner, no del runner ni de este worktree. Ver 4.8.

### 4.8 BLOCKED / SKIP declarados (nunca PASS)

| gate | estado | razón exacta |
|---|---|---|
| Banco QEMU (`captura-banco.sh`) | **BLOCKED** | `tools/captura-banco.sh --check` falta el chroot (`state/worktrees/x6200/.vfix3`), el cliente `tools/remote/x6200-remote` y libs del banco. Salida literal en `gates/check-matrix.log`. |
| Compilación ARM del cambio | **BLOCKED** | No hay toolchain ARM usable en este entorno: el árbol `AetherX6200Buildroot` no está en los checkouts del harness (vive en el host del owner) y la imagen `x6200-buildroot` no trae el cross-gcc (lo invoca desde `/workspace`, que es ese árbol). El backup es que las fuentes nuevas son C11 sin dependencias (mismo código que corre en el host) y que `src/CMakeLists.txt` las lista para el build de la ROM. |
| `wspr` / `sstv` / `wefax` / `js8` / `webserver` en `run_all_tests --quick` | **SKIP** | imagen `ci` sin `fftw3.h`/`sndfile.h` (los corre la imagen `full`) y sin checkout del webserver. Preexistente. |
| `check_wcag (temas de la imagen)` | **SKIP** | no cambia `rootfs/**/themes/*.json`. |
| Sanitizers en la suite nativa | **SKIP (del runner)** | el toolchain de este host no trae la runtime de ASan (`libasan.so.8.0.0`); el runner lo declara y sigue. En el contenedor `x6200-uiharness` sí corren (el job functional de CI). |
| liquid-dsp (signo de CW con FFT real) | **SKIP en el host** | no hay `liquid/liquid.h` en el host; lo corre el contenedor/CI. No se tocó nada del camino CW, así que no cambia. |

## 5. Contrato del issue, punto por punto (sección 11)

- [x] **Una `ACTION_ATUNE` para todos**; `ACTION_ATUNE == 129` pineado; CW/CWR v2
      original sin regresiones (núcleo byte a byte + 169 checks + fixtures CW).
- [x] **AM/SAM sólo portadora fuerte, única y estable**: 12 dB sobre el piso, 6 dB
      de dominancia sobre el segundo pico, cuatro tramas del mismo signo con
      `max-min ≤ resolución`; una sola escritura ≤ 1 kHz, verificada con cuatro
      tramas nuevas y rollback condicionado; nunca RSSI ni voz.
- [x] **USB/LSB propone sólo pasando todos los guards y no mueve el VFO hasta la
      confirmación**; Undo condicionado de 5000 ms. Ninguna afirmación de "voz
      perfectamente afinada": el score es energía ocupada, y el banco lo fija con
      el dial quieto en PREVIEW.
- [x] **Digital/FM sin cambios de frecuencia**, feedback en inglés
      (`mode unsupported`), Settings/APP 6/defaults/IDs preservados.
- [x] **Snapshot DSP coherente e independiente de la UI**; 512 bins efectivos
      distinguidos de los 800 visuales; sin FFT extra ni configuración nueva.
- [x] **Usuario, CAT, VFO, TX/PTT/ATU y lock prevalecen**: una sola vía de
      escritura que pregunta `autotune_owns()`; ninguna escritura en TX y ningún
      rollback sobre un dial externo (tests de QSY + fixtures de cancelación).
- [x] **Tests puros, banco de UI, estáticos y regresión ejecutados y reportados**;
      el reporte distingue lo medido de lo pendiente.
- [x] **Docs actualizadas** (tabla por modo, resolución, límites, 2.ª pulsación y
      Undo; frase CW-only corregida) y **sin cambios fuera de alcance** (no hay
      issues hijos, no hay refactor de DSP, no hay segunda FFT ni hilo nuevo).

## 6. Pendientes (honestos)

**Hardware RF validation pending** — nada de esto se ejecutó en un X6200 real:

1. **AM/SAM**: portadora estable a **±390 Hz** y **±780 Hz** del dial (centrado
   aproximado + verificación) y **portadora débil o con QRM** (rechazo, sin QSY
   dudoso).
2. **USB/LSB**: **voz real** con desplazamiento de ~±500 Hz → propuesta **no
   automática**, confirmación y Undo; **dos QSO cercanos** → abstención.
3. **DFL y anchos de FFT**: medir en 96 kHz y en una vista más angosta; confirmar
   el rechazo a 384 kHz (750 Hz de resolución).
4. **PTT, freq lock, CAT y rueda** durante la medición y durante el Undo.
5. **Modo digital fijo**: FT8/JS8/WSPR no mueven el dial.
6. **CW/CWR original** a ±35 Hz con el AUTO del decodificador #139 ON y OFF.
7. **Encoder y waterfall** fluidos durante la sesión.
8. **Compilación ARM y banco QEMU** del cambio (BLOCKED acá por entorno: ver 4.8).

## 7. Commits (identidad `Pablo Iaria <iariap@gmail.com>`, sin push)

| sha | asunto |
|---|---|
| `7c5b7262` | Politica espectral pura y snapshot del DSP para Auto Tune multimodo (ref #205) |
| `5609a32c` | Coordinador multimodo: AM/SAM con portadora y USB/LSB con confirmacion (ref #205) |
| `47d6efa8` | Tests puros multimodo y contrato estatico (ref #205) |
| `8015b597` | Fixtures y banco de UI multimodo (ref #205) |
| `7fa314f9` | docs(autotune): tabla por modo, politicas espectrales, ventanas y pendientes (ref #205) |

`git diff --stat 00a653e7..7fa314f9` (salida real):

```
 docs/autotune.md                                   |  452 +++++----
 src/CMakeLists.txt                                 |    2 +-
 src/autotune_spectrum.c                            |  680 +++++++++++++
 src/autotune_spectrum.h                            |  302 ++++++
 src/autotune_ui.c                                  |  544 +++++++++-
 src/dsp.cpp                                        |   97 ++
 src/dsp.h                                          |   25 +
 tests/check_autotune_v2.py                         |  223 +++-
 tests/native/run_native_tests.sh                   |    6 +-
 tests/native/test_autotune_spectrum.c              | 1064 ++++++++++++++++++++
 tests/native/test_main.c                           |    2 +
 tools/ab_testing/tests/test_scenarios.py           |   14 +-
 tools/ui_harness/CMakeLists.txt                    |    4 +
 tools/ui_harness/fixtures/autotune-am.scn          |   51 +
 tools/ui_harness/fixtures/autotune-digital.scn     |   47 +
 tools/ui_harness/fixtures/autotune-lsb.scn         |   44 +
 tools/ui_harness/fixtures/autotune-notcw.scn       |   41 -
 tools/ui_harness/fixtures/autotune-usb-applied.scn |   48 +
 tools/ui_harness/fixtures/autotune-usb-timeout.scn |   43 +
 tools/ui_harness/fixtures/autotune-usb.scn         |   52 +
 tools/ui_harness/fixtures/autotune.scn             |   10 +-
 tools/ui_harness/mocks/mock_dsp.c                  |  148 ++-
 tools/ui_harness/mocks/mock_spectrum.h             |   45 +
 tools/ui_harness/src/coverage.c                    |   38 +-
 tools/ui_harness/src/screens.c                     |  207 +++-
 25 files changed, 3857 insertions(+), 332 deletions(-)
```

## 8. Reproducir

```sh
W=/home/pablo/desa/asf/state/worktrees/x6200/autotune-multimode-205
cd $W
sh tests/native/run_native_tests.sh                 # 44738 comprobaciones, 0 fallos
python3 tests/check_autotune_v2.py                  # contrato estático multimodo
bash tests/check_harness_determinism.sh             # PNG idénticos (usa docker)
sh tests/run_all_tests.sh --quick                   # 19 OK, 0 fallos
(cd tools/ab_testing && uv run pytest -m 'not e2e' -q && uv run x6200-ab themes)
/home/pablo/desa/asf/scripts/apps/x6200/check-matrix.sh origin/lu2dsx/x6200-rom HEAD $W
```
