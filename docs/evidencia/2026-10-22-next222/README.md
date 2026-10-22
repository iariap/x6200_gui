# Evidencia — #222 · Cobertura QEMU de diálogos reales, CI por SHA y baseline reproducible

- **Issue**: [`iariap/x6200_gui#222`](https://github.com/iariap/x6200_gui/issues/222) — hijo **6/6** de la épica
  [`#216`](https://github.com/iariap/x6200_gui/issues/216).
- **Rama**: `factory/next-222` (worktree `state/worktrees/x6200/next-222`, harness ASF).
- **Base**: `f825fe4b` = tip de `origin/lu2dsx/x6200-rom`, con los cinco hijos integrados
  (#217 `a01598b0`, #218 `9e571d89`, #219 `85bf06fb`, #220 `fea0960b`, #221 `f825fe4b`).
- **Capa de esta corrida**: **HOST + CI** (y el runner de QEMU_USER, con su preflight real).
  **HARDWARE** sigue `PENDING_HARDWARE` (pipeline físico separado).
- **Alcance ejecutado**: tests, datos de cobertura y documentación. **No se tocó `src/**`**, ni
  presupuestos, ni golden, ni excepciones, ni el dossier histórico de #194.
- **Sin push**: el PR lo abre el orquestador.

---

## 0. Estado de un vistazo

| Frente del contrato (§ del issue) | Resultado real | Evidencia |
|---|---|---|
| §2 barrido QEMU de las 25 apps × 3 rondas | **BLOCKED** (falta el volumen privado de build y el chroot privado) — runner implementado, preflight real | `logs/gate-qemu-preflight.txt`, `tests/qemu/` |
| §3 tabla de los 22 diálogos inertes | **OK** — 22 filas validadas contra el código, con resultado QEMU esperado y razón exacta de bloqueo | `tests/qemu/dialog-coverage.csv`, `logs/gate-dialog-coverage.txt` |
| §4 CI sobre el commit integrado exacto (F-13) | **OK** (arreglado) — `push` a `lu2dsx/x6200-rom` dispara el arnés; hoy **no existe** corrida sobre `f825fe4b` | `logs/gate-ci-sha.txt`, `.github/workflows/ab-testing.yml` |
| §5 benchmark A/A en el mismo SHA | **PASS** — 205 escenarios, dispersión CV mediana 0,87 %, proxies deterministas Δ 0,0 % | `logs/gate-bench-aa.txt`, `reports/BENCH-AA-next222/` |
| §6 docs reconciliadas | **OK** — About retirada, acceso DFL documentado, rótulo del reloj, conteos fechados | `docs/TESTING.md`, `docs/INTEGRATION_AUDIT.md`, `docs/manual/MANUAL.md`+`.html`, `docs/settings-order.md` |
| §7 manifiesto de identidad y baseline reproducible | **OK** — `--verify` reproducible; Buildroot/artefactos `PENDING` con su razón | `baseline.json`, `logs/gate-baseline.txt` |
| Gates actuales | **20 OK / 0 fallos** en `run_all_tests.sh --quick` + determinismo/natural/wakeups/waterfall + framework A/B | `logs/gate-*.txt` |

---

## 1. Método

1. **Reconocimiento del contrato y del estado**: issue #222 completo, épica #216, y el estado real
   de los cinco hijos ya integrados en `f825fe4b` (`git log`, `gh run list`).
2. **Cobertura**: se leyó `tools/ui_harness/mocks/mock_main_screen.c` (los 22 `= &mock_dialog;`),
   las fuentes de producción de cada diálogo y `remote_server.cpp::app_names[]` (las 25 apps), y se
   armó `tests/qemu/dialog-coverage.csv` como dato **verificable**, no como prosa.
3. **Medición**: se reusaron las primitivas del banco del owner (binario ARM real, `libfakefb`,
   `x6200-remote`, socket, captura y rotación del framebuffer) en un runner **privado** nuevo, y se
   corrió su **preflight real**.
4. **Gates**: se corrieron todos los del contrato y se guardó la salida literal de cada uno.
5. **Baseline**: se escribió el manifiesto de identidad y se comprobó que es **reproducible**.

**Regla de laboratorio respetada**: sólo HOST/QEMU_USER/CI. No se usó ni compartió
`banco-apps`, `banco-audio`, `x6200-build-output`, ni rootfs/volúmenes/build dirs de otras
corridas (había sesiones en vuelo: `agitated_pascal` montando el lab de #211). No hubo SSH a la
radio, flash, deploy, TX/RF ni `sudo`. **Nunca PASS sin salida real**: lo que no se pudo medir
quedó `SKIP`/`BLOCKED`/`PENDING` con su razón.

---

## 2. Qué se agregó (archivos)

| Archivo | Qué es |
|---|---|
| `tests/qemu/run_apps_qemu.sh` | Barrido QEMU de las 25 apps × 3 rondas con recursos privados, guard #86 y preflight fail-closed. |
| `tests/qemu/barrido_qemu.py` | Clasificación `PASS`/`FAIL`/`PARTIAL`/`BLOCKED`, `barrido.json` + `results.csv` + `index.txt`, y `selftest` del reporteador. |
| `tests/qemu/dialog-coverage.csv` | La tabla HOST/QEMU de los 22 diálogos inertes. |
| `tests/qemu/README.md` | Cómo se corre, aislamiento y garantías. |
| `tests/check_dialog_coverage.py` | Gate determinista: la tabla contra los mocks, las fuentes reales y `app_names[]`. |
| `tests/release/baseline_manifest.py` | Manifiesto de identidad del par GUI+Buildroot, artefactos y capa QEMU; `--verify` reproducible. |
| `tests/run_all_tests.sh` | Suma `dialog_coverage` y `qemu_apps` (este último `SKIP` sin banco privado). |
| `.github/workflows/ab-testing.yml` | `push` a la rama de integración (CI del SHA integrado) + el nuevo chequeo estático. |
| `docs/TESTING.md` | Secciones nuevas del barrido QEMU, la tabla de cobertura y el baseline; conteos fechados. |
| `docs/INTEGRATION_AUDIT.md` | About retirada, acceso DFL, conteo nativo actual, estado de los 22 diálogos. |
| `docs/manual/MANUAL.md` + `MANUAL.html` | About → **editor DFL**; rótulo del reloj; lista de acciones completa. |
| `docs/settings-order.md` | Rótulo del reloj. |

---

## 3. Matriz de cobertura de los 22 diálogos inertes — antes / después

**Antes** (dossier #194/#197): el arnés de host declara 22 constructores de diálogo como no-op
(`mock_main_screen.c`), así que esas ventanas no se renderizan. El dossier lo reporta como hueco
(“22 diálogos inertes”) **sin tabla por diálogo**: `03-ui-ux/ui-coverage.csv` no tiene ni una fila
con capa QEMU (141 HOST + 1 HARDWARE) y el baseline de #195 tiene 3 ítems `QEMU_USER` sobre 334.

**Después**: cada uno de los 22 tiene su fila con la fuente de producción, el fixture de host que
llega a su camino, la app del barrido que lo abre y el resultado QEMU **esperado**; lo **medido**
en esta corrida es `BLOCKED` con la razón exacta (no había banco privado), y un gate impide que el
inventario se desactualice.

| Diálogo | Fuente de producción | Arnés de host | App del barrido | QEMU esperado | QEMU medido |
|---|---|---|---|---|---|
| Bands | `src/dialog_bands.c:64` | fixture `app-bands` (llega, no renderiza) | `Bands` | REAL | BLOCKED |
| Callsign | `src/dialog_callsign.c:27` | — | `Callsign` | REAL | BLOCKED |
| Cluster | `src/dialog_cluster.c:117` | — | `Cluster` | PARTIAL (spots de red) | BLOCKED |
| CW Keyboard | `src/dialog_cw_kbd.c:104` | — | `CW` | REAL | BLOCKED |
| CW Trainer | `src/dialog_cw_trainer.c:224` | — | `CwTrainer` | REAL | BLOCKED |
| EQ | `src/dialog_eq.cpp:200` | — | `Eq` | REAL | BLOCKED |
| Field Ops | `src/dialog_field.c:114` | — | `Field` | PARTIAL (red QRZ/POTA) | BLOCKED |
| Frequency entry | `src/dialog_freq.c:40` | — | — | UNSUPPORTED (HKEY_FINP) | UNSUPPORTED |
| FT8 | `src/dialog_ft8.c:233` | — | `FT8` (FT4 alias) | REAL | BLOCKED |
| GPS | `src/dialog_gps.c:188` | — | `GPS` | PARTIAL (sin GNSS) | BLOCKED |
| JS8 | `src/dialog_js8.c:521` | — | `JS8` | REAL | BLOCKED |
| CW messages | `src/dialog_msg_cw.c:134` | — | — | UNSUPPORTED (pulsación larga) | UNSUPPORTED |
| Voice messages | `src/dialog_msg_voice.c:188` | — | — | UNSUPPORTED (pulsación larga) | UNSUPPORTED |
| NavTex | `src/dialog_navtex.c:130` | — | `NavTex` | REAL | BLOCKED |
| Propagation | `src/dialog_propagation.c:204` | — | `Propagation` | PARTIAL (cluster/NTP) | BLOCKED |
| QTH | `src/dialog_qth.c:29` | — | `QTH` | REAL | BLOCKED |
| Recorder | `src/dialog_recorder.c:117` | — | `Recorder` | REAL | BLOCKED |
| SSTV | `src/dialog_sstv.c:771` | — | `SSTV` | REAL | BLOCKED |
| Status | `src/dialog_status.c:258` | — | `Status` | REAL | BLOCKED |
| WeFax | `src/dialog_wefax.c:512` | — | `WeFax` | REAL | BLOCKED |
| WiFi | `src/dialog_wifi.c:131` | — | `WiFi` | PARTIAL (sin NM) | BLOCKED |
| WSPR | `src/dialog_wspr.c:134` | — | `WSPR` | PARTIAL (subida de red) | BLOCKED |

Totales: **13 REAL · 6 PARTIAL · 3 UNSUPPORTED** esperados; **0 REAL/PARTIAL medidos** en esta
corrida (banco `BLOCKED`), **0 falsos PASS**. La tabla completa, con la razón exacta por fila, es
`tests/qemu/dialog-coverage.csv`.

**Qué pasó de inerte a cubierto, y qué no:**

- **De inerte a “tabla + gate”**: los 22. Antes el hueco era una frase en un README; ahora es un
  inventario que el CI valida contra el código (mock ↔ fuentes ↔ `app_names[]`) y que falla si una
  fila declara cobertura real sin captura.
- **De inerte a cubierto en QEMU**: **ninguno**, porque el barrido no pudo correr. El contrato
  pide medir componentes reales en QEMU_USER y **no** fabricar mocks que los pinten: por eso no se
  promovió nada a REAL y el host sigue declarado sintético para esos 22.
- **Sigue UNSUPPORTED y por qué**: `dialog_freq` (se abre con `HKEY_FINP`, una tecla larga),
  `dialog_msg_cw` y `dialog_msg_voice` (pulsación larga del botón de modo en CW y en SSB/AM/NFM).
  El protocolo de control **no inyecta teclas** (`remote_server.cpp` no expone ningún comando de
  evento), así que el barrido de las 25 apps no los alcanza; necesitan un probe de eventos
  distinto, fuera de este contrato.
- **Cobertura QEMU del fixture de #220** (`locked-feedback`): el barrido abre y cierra apps, pero
  el aviso de control trabado se dispara con **eventos de tecla/dial** que el socket tampoco puede
  inyectar → queda **BLOCKED** junto con el resto (mismo motivo de fondo: sin banco y sin inyección
  de eventos). La evidencia de #220 sigue siendo la del arnés de host, y las teclas físicas
  `PENDING_HARDWARE`.

---

## 4. Pin de CI por SHA (F-13)

**Estado antes**: el workflow `UI A/B testing harness` sólo corría en `pull_request` (y con
`workflow_dispatch`). Una corrida de `pull_request` evalúa el **head del PR**, no el commit que el
merge deja en la rama: el squash commit es otro SHA. Resultado medido:

```
$ gh run list --repo iariap/x6200_gui --commit f825fe4bc1ec6477a83bf509d6524d91353a4313 --limit 20
(sin filas: no hay corrida sobre ese commit)

$ gh run list --repo iariap/x6200_gui --branch lu2dsx/x6200-rom --limit 10
... boot-rootfs  lu2dsx/x6200-rom  push  (x8)
... UI A/B testing harness  lu2dsx/x6200-rom  workflow_dispatch  2026-10-14
```

O sea: el tip integrado tenía corridas de `boot-rootfs` (que dispara por `push`) y **ninguna** del
arnés de UI. Eso es exactamente F-13 del dossier #194.

**Arreglo** (`.github/workflows/ab-testing.yml`): se agregó

```yaml
  push:
    branches: ["lu2dsx/x6200-rom"]
    paths: [ "tools/**", "src/**", "tests/**", "rootfs/usr/share/x6200/themes/**",
             ".github/workflows/ab-testing.yml" ]
```

con el **mismo filtro de paths** que el `pull_request`. Con eso cada merge a la rama de integración
deja una corrida cuyo `github.sha` **es** el commit integrado; en un `push` no hay
`pull_request.head.sha`, así que los pasos que lo usan caen a `github.sha` (el propio commit) y el
job visual/benchmark compara la revisión **contra sí misma** (A/A) en vez de inventar un baseline.
Eso está escrito en el encabezado del workflow para que no se lea como una mejora.

Además, el nuevo chequeo estático entra al job `functional` (y al resumen):

```yaml
          python3 tests/check_dialog_coverage.py
```

`logs/gate-ci-sha.txt` guarda la salida cruda de los cuatro comandos de arriba.

> **No hecho a propósito**: no se “pinneó” cada `uses:` a un SHA de commit. El contrato de #222
> pide la corrida **sobre el commit exacto integrado**, no el pinneado de las acciones de
> terceros; cambiar 20 `uses:` es una decisión de supply chain que no está en el alcance cerrado.
> Queda como recomendación (nota al orquestador).

---

## 5. Baseline reproducible

`tests/release/baseline_manifest.py` (sólo lectura) emite el manifiesto del contrato §7. Salida
real de esta corrida (`baseline.json`, y `logs/gate-baseline.txt` en texto):

```
GUI_SHA          f825fe4bc1ec6477a83bf509d6524d91353a4313  (factory/next-222)
fuente-md5       da8ea582a4cacea79089b00bf6fd1941  (0 archivos sucios en src/)
LVGL_gitlink     74d0a816a440eea53e030c4f1af842a94f7ce3d3  (v8.3.11)
Buildroot_SHA    PENDING: sin clon de Buildroot (pasar BR=<clon>)
GUI binario      PENDING: sin binario de la GUI (pasar OUT=<salida Buildroot> o BIN=<ruta>)
imagenes         PENDING: sin directorio de artefactos (pasar ARTEFACTOS=<dir>)
QEMU_CPU         cortex-a7   imagen x6200-buildroot   contenedor x6200-next222-qemu
seed             PENDING: sin seed de la corrida (las fixtures del arnes lo fijan por escenario)
inventario       205 fixtures de host, 22 dialogos inertes, 3 rondas x 25 apps
```

**Reproducibilidad**: `tests/release/baseline_manifest.py --verify` corre el manifiesto dos veces
sobre el mismo árbol y compara todos los campos deterministas (sólo `fecha_utc` es volátil):

```
  OK: el manifiesto es reproducible (dos corridas sobre el mismo arbol coinciden)
```

Los `PENDING` son honestos: en esta corrida no había clon de Buildroot ni artefactos, y un campo
ausente se declara con su razón en vez de leerse como cero. Con `BR=`/`OUT=`/`ARTEFACTOS=` se
completan `Buildroot_SHA`, el md5/sha256 del binario (y su sello embebido) y los hashes de
`sdcard.img`/`rootfs.tar`/`rootfs.ext4`/`boot.vfat`.

**Ancla del baseline en el árbol real**: 205 fixtures de host (el gate de determinismo los corre
2× cada uno) y 22 diálogos inertes. Los conteos de suites se reportan fechados en `docs/TESTING.md`
y `docs/INTEGRATION_AUDIT.md`, **sin** reemplazar los números históricos.

---

## 6. Gates — salida real

| Gate | Comando | Resultado | Log |
|---|---|---|---|
| Nativos (host) | `sh tests/native/run_native_tests.sh` | **OK** — 52.946 comprobaciones, 0 fallos (24 etapas) | `logs/gate-native.txt` |
| Nativos (paridad CI) | `sh tests/native/run_native_tests.sh --ci-parity` | **OK** — 52.946, 0 fallos (`lvgl/` oculto) | `logs/gate-native-ci-parity.txt` |
| Suite completa | `sh tests/run_all_tests.sh --quick` | **OK** — 20 OK, 0 fallos; 6 omitidos (`qemu_apps`, `wspr`, `sstv`, `wefax`, `js8`, `webserver`) | `logs/gate-run-all-tests.txt` |
| Determinismo | `bash tests/check_harness_determinism.sh` | **PASS** — 205 fixtures × 2, PNG idénticos | `logs/gate-determinism.txt` |
| Natural | `bash tests/check_harness_natural.sh` | **PASS** — perfil natural determinista y acotado | `logs/gate-natural.txt` |
| Wakeups | `bash tests/check_harness_wakeups.sh` | **PASS** — perfil medido y delimitado | `logs/gate-wakeups.txt` |
| Waterfall incremental | `bash tests/check_waterfall_incremental.sh` | **PASS** — 195 checks, 0 fallas (+ ASan/UBSan) | `logs/gate-waterfall.txt` |
| Auditoría de pruebas | `python3 tests/audita-pruebas.py` | **OK** — estructura y evidencia en orden | `logs/gate-audita-pruebas.txt` |
| Cobertura de diálogos (nuevo) | `python3 tests/check_dialog_coverage.py` | **OK** — 22 diálogos, 19 apps con fila, 6 excepciones verificadas sobre 25 | `logs/gate-dialog-coverage.txt` |
| Reporteador QEMU (nuevo) | `python3 tests/qemu/barrido_qemu.py selftest` | **OK** — 19 casos (capa SELFTEST, no cobertura) | `logs/gate-qemu-selftest.txt` |
| Preflight QEMU (nuevo) | `bash tests/qemu/run_apps_qemu.sh --check` | **SKIP (77)** — 2 requisitos faltantes, con nombre exacto | `logs/gate-qemu-preflight.txt` |
| Baseline reproducible (nuevo) | `python3 tests/release/baseline_manifest.py --verify` | **OK** — reproducible | `logs/gate-baseline-verify.txt` |
| Guard #86 | `python3 tools/verifica-firmware-port.py --config` | **OK** — señales calibradas | `logs/gate-verifica-firmware-port.txt` |
| Framework A/B | `uv sync` + `ruff check` + `ruff format --check` + `ty check` + `pytest -m "not e2e"` | **OK** — 233 passed, 13 deselected | `logs/gate-ab-*.txt` |
| Temas (contraste) | `uv run x6200-ab themes` | **PASS** — 10 paletas, 90 pares, 0 fallas | `logs/gate-ab-themes.txt` |
| Benchmark A/A | `uv run x6200-ab benchmark --a f825fe4b --b f825fe4b` | **PASS** — 205 escenarios | `logs/gate-bench-aa.txt` |
| Matriz del harness | `scripts/apps/x6200/check-matrix.sh origin/lu2dsx/x6200-rom HEAD <worktree>` | **rc=0** — 4 checks `ok` (themes, contracts sobre `6d793160`, `bash -n` de los dos scripts); 0 FAIL/BLOCKED; SKIP por paths no tocados | `logs/gate-check-matrix.txt` |

> La matriz **no** corre `check_dialog_coverage` todavía: su lista de checks está en el repo `asf`
> (`scripts/apps/x6200/check-matrix.sh`) y agregarla ahí es riesgo alto / fuera del alcance del
> ejecutor (§8.2). Localmente lo corre `tests/run_all_tests.sh` y en el CI el job `functional`.

### 6.1 Benchmark A/A (mismo SHA, host)

`--a` y `--b` con el **mismo** commit `f825fe4b`; presupuesto del repo (`warmup 3`, `repeats 5`,
`rounds 4` → n ≥ 5 repeticiones × 205 fixtures); host **sin carga competidora** medida
(`load average 0.86/0.79/0.90` sobre 16 CPUs al arrancar; `logs/gate-bench-aa-entorno.txt`).

| Métrica (A/A, 205 escenarios) | min | mediana | max |
|---|---:|---:|---:|
| `p95_delta_pct` | −19,88 % | **+1,21 %** | +24,15 % |
| `flushes_delta_pct` | 0,0 % | **0,0 %** | 0,0 % |
| `redrawn_area_delta_pct` | 0,0 % | **0,0 %** | 0,0 % |
| `dispersion_cv_pct` (A) | 0,231 % | **0,867 %** | 3,653 % |

Veredicto **PASS** (205/205 `PASS`; sin `INCONCLUSIVE` y sin `FAIL`). Los dos proxies
deterministas quedan en **Δ 0,0 % exacto** en los 205: el render es determinista y el ruido del
host sólo mueve el p95. **No** se declara FPS del LCD: el `fps` del reporte (mediana 294,7) es una
derivada del host x86, no la cadencia del panel (nota del propio reporte).

`logs/gate-bench-aa.txt` guarda el resumen; el JSON completo de las 205 comparaciones (la salida
cruda del CLI) queda en `reports/BENCH-AA-next222/results.json` junto con `report.md` (la tabla
por escenario) y `metadata.json`.

---

## 7. SKIP / BLOCKED / PENDING razonados

| Qué | Estado | Razón exacta |
|---|---|---|
| Barrido QEMU 25 apps × 3 rondas | **BLOCKED** | `tests/qemu/run_apps_qemu.sh --check`: falta el **volumen privado de build** de la GUI y el **chroot privado**. El banco del owner no está disponible (`x6200-build-output` no existe) y sus otros recursos están en vuelo; la regla de la épica prohíbe usarlos. Construir el volumen propio exige un build de Buildroot completo (horas) → fuera de esta corrida. |
| Cobertura QEMU de los 22 diálogos | **BLOCKED** | Consecuencia del anterior: sin binario ARM construido ni chroot no hay captura. Las filas quedan `BLOCKED` con la razón, nunca `REAL`. |
| `dialog_freq`, `dialog_msg_cw`, `dialog_msg_voice` | **UNSUPPORTED** | Se abren con teclas largas; el protocolo de control no inyecta eventos. No es un defecto del firmware. |
| Fixture `locked-feedback` (#220) en QEMU | **BLOCKED** | Mismo banco + el aviso necesita eventos de tecla/dial que el socket no puede inyectar. |
| `qemu_apps` en `run_all_tests.sh` | **SKIP (77)** | Sin banco privado; el runner lo declara con los requisitos exactos. |
| `wspr`, `sstv`, `wefax`, `js8` en la suite | **SKIP (77)** | La imagen `x6200-uiharness` (`--target ci`) no trae las deps de códec; es el SKIP preexistente, no un pase. |
| `webserver` en la suite | **SKIP** | Sin checkout `.x6200_webserver` y sin el volumen de build del owner. |
| Artefactos del release (imagen/rootfs/binario) | **PENDING** | No hay salida de Buildroot en esta corrida; el manifiesto los declara con su ruta esperada. |
| `MANUAL.pdf` | **PENDING** | El `.md` y el `.html` quedaron consistentes; el PDF es un artefacto generado y no hay generador en el repo (necesita decisión/toolchain). |
| Legibilidad al sol, teclas físicas, energía, RF | **PENDING_HARDWARE** | Pipeline físico separado; esta corrida no toca la radio. |

### 7.1 Transitorio observado (no es un defecto del cambio)

Una corrida de `run_all_tests.sh --quick` (16:35 UTC) **se colgó ~30 min en el paso `daemon`**
(`tests/daemon/run_daemon_tests.sh`), que usa `unshare -Ur -m -p` y binds; el log quedó con
`case-*.out: No such file or directory` y `6 fallos` por el corte, no por el test. El mismo paso
**pasó solo** (`15 casos, 0 fallos`, en segundos) y la corrida completa se repitió **entera y
verde** (`20 OK, 0 fallos`, `logs/gate-run-all-tests.txt`). Se deja anotado porque el paso es
sensible al entorno (namespaces/`/tmp` compartidos): si vuelve a colgarse, hay que mirar el host,
no el cambio. Nada de este hijo toca ese camino.

---

## 8. Pendientes para el orquestador (fuera del alcance del ejecutor)

1. **Correr el barrido QEMU** con un volumen privado (build de Buildroot) o con `ROOTFS=<rootfs.tar>`:
   `CONSTRUIR=si BUILDROOT=<clon> RONDAS=3 tests/qemu/run_apps_qemu.sh`. Después, actualizar
   `tests/qemu/dialog-coverage.csv` con las capturas reales (el gate exige evidencia para
   `REAL`/`PARTIAL`).
2. **Matriz del harness**: `scripts/apps/x6200/check-matrix.sh` (repo `asf`, riesgo alto) no lista
   todavía `check_dialog_coverage`; agregarlo para que la matriz local y el CI no diverjan.
3. **Strings en español pendientes** (no se tocaron: son producción y el contrato lo prohíbe):
   `src/dialog_propagation.c` conserva `Paso 1/4 | Preparando`, `Paso 2/4 | Sincronizando`,
   `Paso 3/4 | Midiendo`, `SIN BEACONS IBP`, `la banda actual`, `No se pudo crear el hilo del barrido.`
   y las notas multilínea; `src/gps_format.c:50` sigue devolviendo `"sin datos"` (fijado por
   `tests/native/test_gps_format.c`). Decisión de producto pendiente.
4. **`MANUAL.pdf`**: regenerarlo cuando haya toolchain, o dejar de versionarlo.
5. **Pinning de acciones a SHA** (`actions/checkout@v4` → SHA, y sobre todo
   `jlumbroso/free-disk-space@main`): recomendado, no hecho (§4).

---

## 9. Cómo reproducir

```sh
# gates
sh tests/native/run_native_tests.sh
sh tests/native/run_native_tests.sh --ci-parity
sh tests/run_all_tests.sh --quick
bash tests/check_harness_determinism.sh
bash tests/check_harness_natural.sh
bash tests/check_harness_wakeups.sh
bash tests/check_waterfall_incremental.sh
python3 tests/audita-pruebas.py
python3 tests/check_dialog_coverage.py
python3 tests/qemu/barrido_qemu.py selftest

# cobertura QEMU (necesita el entorno privado)
bash tests/qemu/run_apps_qemu.sh --check
CONSTRUIR=si BUILDROOT=<clon Buildroot> RONDAS=3 tests/qemu/run_apps_qemu.sh

# baseline y benchmark
python3 tests/release/baseline_manifest.py --txt
python3 tests/release/baseline_manifest.py --verify
cd tools/ab_testing && uv run x6200-ab benchmark --a <sha> --b <sha> --repo <worktree> --json

# framework A/B
cd tools/ab_testing && uv sync && uv run ruff check . && uv run ruff format --check . \
    && uv run ty check $(git ls-files 'src/**/*.py' 'tests/**/*.py') && uv run pytest -m "not e2e"
```

> **Nota de medición (worktree del harness)**: dentro de `state/worktrees/x6200/<slug>` el
> `uv run ty check src tests` del CI **no chequea nada** (ty resuelve el project root al repo del
> harness, `/home/pablo/desa/asf`, que tiene su propio `pyproject.toml`): imprime
> `WARN No python files found under the given path(s)` y sale 0. En un checkout **no anidado**
> (como `$GITHUB_WORKSPACE`) sí chequea: se verificó con un archivo con un error de tipo plantado,
> que el CI atrapa y el worktree anidado no. Por eso acá el `ty` del framework se corrió con la
> lista explícita de archivos (`git ls-files`), que sí mide.
