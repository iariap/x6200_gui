# Cobertura consolidada — método y resultados

## 1. Qué se unió y qué no

La fuente del inventario es [`01-baseline/coverage.csv`](../01-baseline/coverage.csv): **334
ítems únicos** (`cov_id`) que cubren apps, acciones, grupos/ítems de Settings, páginas de botones,
entradas físicas, pantallas, subsistemas, servicios, paquetes, configuración/CI y las 114 pruebas
existentes. Cada ítem tiene un `owner_unit` asignado una sola vez.

- **#197** materializa **142 de esos 334** en [`03-ui-ux/ui-coverage.csv`](../03-ui-ux/ui-coverage.csv).
  No agrega ítems: es una vista de los mismos `cov_id`, con captura/evidencia o motivo de ausencia.
  El generador verifica que las 142 filas de #197 estén todas en el inventario y que los 334
  `cov_id` no se repitan.
- **#196, #198 y #199** producen `results.csv` con **métricas/escenarios**, no ítems nuevos de
  inventario. Por eso no se suman al denominador.
- **Filas de resultados** (01: 84, 02: 86, 03: 256, 04: 599) = **1.025**, se reportan aparte como
  evidencia, nunca como "tests pasados" acumulados.

Por lo tanto: **cobertura final = 334 ítems**, sin doble conteo. El script
[`scripts/build_coverage_final.py`](scripts/build_coverage_final.py) genera el CSV de forma
determinista y falla si aparece un `cov_id` duplicado.

## 2. Regla de adjudicación

`coverage-final.csv` conserva `coverage_195` y `status_195` y agrega `status_final`,
`evidence_ref`, `status_basis` y `limitation`. La adjudicación es **documental**: toma el veredicto
que la unidad dueña dejó en su README/results y lo aplica al ítem; **no es una nueva medición**.

- **#195**: PASS del inventario/baseline; los pocos PENDING se adjudican por su resultado
  (`ENV-03` REVIEW, `TST-020` REVIEW, build/identidad PASS).
- **#196**: mapeo explícito a los escenarios ROM-01..ROM-08 (DATA/update/DB/watchdog/init).
- **#197**: PASS de render/contrato HOST se conserva como PASS HOST (no hardware); los PENDING se
  separan en PENDING_HARDWARE (entradas físicas/periféricos) o REVIEW_REQUIRED (sin fixture,
  efecto runtime, diálogos inertes, mensajes).
- **#198**: mapeo a PERF-01..PERF-06; lo que requiere radio/FFT/medidor queda
  PENDING_HARDWARE/BLOCKED.
- **#199**: los 21 ítems de hardware/webserver quedan PENDING_HARDWARE/BLOCKED según
  `hardware-plan.csv`.
- **SKIP** de #195 (webserver, suites) se pliega a **BLOCKED** en el estado final; el CSV conserva
  `status_195=SKIP` y la base dice "SKIP (#195)". Un SKIP nunca es PASS.

**Limitación:** un PASS HOST no implica PASS QEMU/USER ni HARDWARE; `status_final` siempre se lee
junto con `layer` y `coverage_195`. Las filas con `coverage_195=UNSUPPORTED` no se convierten en
PASS por tener un test estático.

## 3. Distribuciones

### Por cobertura del inventario

| Cobertura | Ítems |
|---|---:|
| REAL | 151 |
| PARTIAL | 138 |
| SYNTHETIC | 4 |
| UNSUPPORTED | 41 |
| **Total** | **334** |

### Por dueño

| Unidad | Ítems |
|---|---:|
| #197 UI/UX | 142 |
| #195 baseline | 102 |
| #196 ROM | 51 |
| #199 hardware | 21 |
| #198 performance | 18 |

### Estado final consolidado

| Estado | Ítems |
|---|---:|
| PASS | 174 |
| REVIEW_REQUIRED | 76 |
| PENDING_HARDWARE | 62 |
| BLOCKED | 13 |
| FAIL | 6 |
| UNSUPPORTED | 3 |
| **Total** | **334** |

Los 6 FAIL son: `SYS-25` (SQLite/DB), `SYS-29` (update), `SYS-30` (DATA), `SRV-06`
(S01create_data), `SRV-07` (S02update) y `BLD-04` (fstab vs script). `BLD-02` (imagen sin p3)
queda REVIEW_REQUIRED por dependencia de procedimiento externo (F-18).
Los 13 BLOCKED incluyen banco ocupado, webserver, CTest, diálogos y barridos.

### Por tipo

| Tipo | Total | PASS | REVIEW | PENDING_HW | BLOCKED | FAIL | UNSUP |
|---|---:|---:|---:|---:|---:|---:|---:|
| action | 42 | 40 | 0 | 0 | 0 | 0 | 2 |
| app | 26 | 10 | 8 | 5 | 3 | 0 | 0 |
| build_config | 4 | 2 | 0 | 0 | 0 | 2 | 0 |
| build_package | 7 | 1 | 0 | 3 | 3 | 0 | 0 |
| buttons_page | 16 | 0 | 0 | 16 | 0 | 0 | 0 |
| ci_environment | 6 | 5 | 1 | 0 | 0 | 0 | 0 |
| physical_input | 10 | 0 | 0 | 10 | 0 | 0 | 0 |
| screen | 3 | 0 | 3 | 0 | 0 | 0 | 0 |
| service | 18 | 0 | 1 | 14 | 1 | 2 | 0 |
| settings_group | 8 | 8 | 0 | 0 | 0 | 0 | 0 |
| settings_item | 31 | 0 | 31 | 0 | 0 | 0 | 0 |
| subsystem | 49 | 14 | 14 | 14 | 3 | 3 | 1 |
| test | 114 | 94 | 17 | 0 | 3 | 0 | 0 |

### Pruebas

114 pruebas inventariadas: **94 PASS, 17 REVIEW_REQUIRED, 3 BLOCKED** (CTest ft8/qth y el e2e de
apps). **PASS de inventario/asignación**, no de ejecución individual: la ejecución real es por
suite (17 OK, 0 fallos, 5 SKIP; códecs en imagen full; 138 checks nativos). No se ejecutaron
pruebas nuevas para este dossier; los resultados vienen de #195/#196/#198.

## 4. Resultados por unidad (métricas)

| Unidad | Filas | PASS | FAIL | INCONC | REVIEW | BLOCKED | PENDING_HW | SKIP | UNSUP |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| #195 | 84 | 64 | 3 | 2 | 7 | 3 | 0 | 5 | 0 |
| #196 | 86 | 59 | 12 | 1 | 7 | 6 | 1 | 0 | 0 |
| #197 | 256 | 236 | 0 | 10 | 2 | 2 | 5 | 0 | 1 |
| #198 | 599 | 574 | 0 | 11 | 5 | 2 | 6 | 0 | 1 |
| **Total** | **1.025** | **933** | **15** | **24** | **21** | **13** | **12** | **5** | **2** |

Nota: #197 declara `UNSUPPORTED 1` y #198 `UNSUPPORTED 1`; los `SKIP` son 5 de #195. Los 15 FAIL
corresponden a 3 de #195 (build/defconfig) + 12 de #196 (DATA/update/DB); los de #195 quedaron
**resueltos en el repo** por Buildroot PR #2 o son del proceso de build, no del firmware.

## 5. Huecos que no se tapan

- **Hardware**: 62 PENDING_HARDWARE + 6 HR BLOCKED + 8 HW PENDING; todo con causa y plan.
- **Apps**: 3 barridos de 25 apps BLOCKED por `banco-apps` ocupado; no se usó ni detuvo.
- **UI**: 22 diálogos inertes en el harness; 5 filas `PH-*` PENDING_HARDWARE; 2 REVIEW de
  mensajes/matriz.
- **Performance**: FFT/DSP y mixer BLOCKED; audio solo SYNTHETIC (null-sink); energía/LCD/input
  PENDING_HARDWARE; render/A-A INCONCLUSIVE.
- **Sistema**: webserver SKIP; CTest ft8/qth BLOCKED; no hay CI del SHA candidato.
- **Persistencia**: DB llena BLOCKED; corrupción/RO/faltante medida en QEMU (no hardware).

Estos huecos están en `coverage-final.csv` y en `findings.csv`/`hardware-plan.csv`; **no** se
descuentan del denominador ni se convierten en PASS.
