# Unidad #197 — Evidencia integral de UI y UX orientada al operador

- **Campaña:** assessment pre-release ROM #194 (`iariap/x6200_gui`).
- **Unidad:** #197 (3/5), ejecutada el 2026-10-21 (UTC) por un agente ejecutor de evidencia.
- **Carpeta:** `docs/evidencia/assessment-rom-194/03-ui-ux/`.
- **Rama del informe:** `assessment/197-evidence` (base `assessment/rom-194`).
- **Rol:** recolección de evidencia. **No** se rediseñó, arregló, optimizó ni se propuso una UX mejor;
  no se abrieron subtareas; no se tocó firmware, tests, contratos, CI, budgets, golden ni excepciones.
- **Sin radio:** #195–#198 no tocan el equipo. No hubo SSH/socket a la radio, ni RF, ni flash/deploy/tags/release.

## 0. Estado de un vistazo

| Corrida | Resultado real | Evidencia |
|---|---|---|
| Determinismo `tests/check_harness_determinism.sh` | **PASS** — 199 fixtures, PNG byte-idénticas A/A | [`logs/step2/determinism.log`](logs/step2/determinism.log) |
| `x6200-ab capture` | **exit 0** — 199 capturas PNG+JSON | [`captures/`](captures/), [`logs/step3/capture.log`](logs/step3/capture.log) |
| `x6200-ab audit` | **PASS** — 0 hallazgos estructurales sobre 26 455 objetos / 199 escenarios | [`logs/step3/audit-contracts.log`](logs/step3/audit-contracts.log) |
| `x6200-ab contracts` | **PASS** — 2056 checks, 0 fallos, 0 defectos UNCHANGED | [`logs/step3/audit-contracts.log`](logs/step3/audit-contracts.log) |
| `x6200-ab themes` | **PASS** — 10 paletas, 90 pares WCAG, 0 fallos, 0 errores | [`logs/step3/themes.json`](logs/step3/themes.json) |

`results.csv`: **256 filas** (una por métrica). Estados: **PASS 236, INCONCLUSIVE 10, PENDING_HARDWARE 5,
BLOCKED 2, REVIEW_REQUIRED 2, UNSUPPORTED 1**. **Ningún `FAIL`**: no se demostró ningún fallo de contrato
(un PASS de gate no significa ausencia de defectos; los UNCHANGED/waived quedan a la vista).

> Nota de honestidad: esta unidad **depende del #196** (ver §2), que **no está integrado**. Por orden
> explícita del owner se ejecutó igual: se produjo toda la evidencia que depende sólo del candidato y de
> herramientas host, y lo que exigía artefactos del #196 quedó **BLOCKED con causa**, no tapado.

## 1. Base congelada, identidad y hashes

| Elemento | Valor |
|---|---|
| GUI candidato (medido) | `c888308eede71205605c06383432ee7e165f202e` |
| `fuente-md5` del checkout | `964193ae9306b5127845cec44a57d52c` (coincide con #195) |
| Gitlink LVGL | `74d0a816a440eea53e030c4f1af842a94f7ce3d3` (v8.3.11) |
| Buildroot | `d084e2870a00b64d9c57474e7eceeef80f43f25b` (`iariap/AetherX6200Buildroot`, revalidado) |
| Rama de integración | `assessment/rom-194` = `230b20634fd240fe5836cd7d10e39646c83cd93a` |
| Base del reporte | `230b2063` (tip de `origin/assessment/rom-194`, ya integra el baseline #195) |
| Checkout de medición (privado, limpio) | clon del candidato en `c888308e`, `git status --porcelain` vacío, submódulos inicializados |
| Imagen host | `x6200-uiharness` = `x6200-uiharness-ci` `sha256:9eedee0e5605…` (tag preexistente; **no se reconstruyó ni retagueó**) |

Verificación de hashes en [`logs/step1/identity.log`](logs/step1/identity.log) y
[`artifact-manifest.json`](artifact-manifest.json). No se comparó con `main`; no se editó
`known_issues`/excepciones ni golden; no se actualizó ningún baseline.

## 2. Dependencia no satisfecha — #196 (BLOCKED explícito)

El issue declara que esta unidad **depende de #196 aceptado e integrado** y que debe **consumir** su
inventario y sus capturas/estado de apps. La base `origin/assessment/rom-194` **sólo tiene `01-baseline/`**;
#196 no está integrado (ni su PR abierto). Prueba:

```
$ git ls-tree -r --name-only origin/assessment/rom-194 docs/evidencia/assessment-rom-194
docs/evidencia/assessment-rom-194/01-baseline/…   (sólo #195; no hay 02-rom/)
```

Consecuencia (fila `DEP-196` de `results.csv`, estado `BLOCKED`): lo que exigía capturas del #196
**no se pudo producir** y se marca BLOCKED con la causa exacta en vez de fabricarse. En particular, los
**diálogos que el host no compila** (fila `UX03-inert-dialogs`): `tools/ui_harness/mocks/mock_main_screen.c`
reemplaza por una instancia inerte (no-op) 22 constructores de diálogo
(`dialog_bands, dialog_callsign, dialog_cluster, dialog_cw_kbd, dialog_cw_trainer, dialog_eq, dialog_field,
dialog_freq, dialog_ft8, dialog_gps, dialog_js8, dialog_msg_cw, dialog_msg_voice, dialog_navtex,
dialog_propagation, dialog_qth, dialog_recorder, dialog_sstv, dialog_status, dialog_wefax, dialog_wifi,
dialog_wspr`). Sólo Ajustes, BT y SWR Scan se compilan reales. No se inventó una captura ni se sustituyó por
un mock que probara menos.

## 3. Aislamiento y preflight real

- **`banco-apps` estaba ocupado** (`x6200-buildroot`, `Up 4 hours`) al iniciar y durante toda la corrida:
  **no se usó, no se detuvo, no se borró** → los escenarios que lo requieran quedan BLOCKED. No existe
  `banco-audio`.
- **Tag `x6200-uiharness`**: se usó **tal cual** (id `9eedee0e5605…`, creado 2026-10-14, preexistente a
  esta campaña y registrado por #195); no se retagueó ni se reconstruyó.
- **No se tocó** `/home/pablo/desa/x6200/assessment-194/**`, ni el checkout del owner, ni rootfs/volúmenes/
  contenedores/radio ajenos. Toda la medición vive en un **scratch privado**: clon del candidato +
  `uv-venv` + workspaces de `x6200-ab`.
- **Desviación declarada (espacio)**: el scratch se movió de `/tmp/opencode/assessment197/` a
  `state/worktrees/x6200/assessment-197-cand/` porque el `tmpfs /tmp` tenía ~9.6 GB libres (consumidos por
  otra sesión) y el build del harness murió con `No space left on device` / `Disk quota exceeded`
  (primer intento fallido, registrado). En `/home` (107 GB libres) el build pasó.
- `qemu-system-x86_64` presente; `qemu-user-static` ausente. No hay QEMU de sistema en esta unidad
  (cobertura QEMU_USER = UNSUPPORTED acá).

## 4. Metodología y comandos (§3 del issue)

Todos con `CANDIDATE=c888308eede71205605c06383432ee7e165f202e` y `OUT` = esta carpeta; detalle completo
en [`commands.jsonl`](commands.jsonl). Los JSON/PNG/inspection/logs originales se preservaron antes de
cualquier limpieza.

```
# 1) identidad y materialización del inventario #195  -> §1 y ui-coverage.csv
# 2) determinismo (una vez, cada fixture dos veces)
bash tests/check_harness_determinism.sh
# 3) herramientas del arnés (cwd tools/ab_testing)
uv run x6200-ab capture   --revision "$CANDIDATE" --label candidate --out "$OUT/captures" --json
uv run x6200-ab audit     --revision "$CANDIDATE" --json --keep-workspace
uv run x6200-ab contracts --revision "$CANDIDATE" --json --keep-workspace
uv run x6200-ab themes    --json
```

- **Determinismo**: las 199 fixtures renderizadas dos veces dieron PNG idénticas
  ([log](logs/step2/determinism.log), 199 líneas `fixture: sha256`). Es **A/A**: no valida regresión
  contra un firmware anterior (por eso no se declara "sin regresión").
- **`audit`**: los detectores de `known_issues` más la inspección estructural (`--inspect`) de 26 455
  objetos. `UI-001` (`multiplicador` con contenido 0.0 ≤ 0.05) y `UI-009` (reloj arranca con U+F240) **no se
  reprodujeron** (fixed). `UI-002/003/004/005/006/015/016/017/018` quedan `unverified` (sin detector);
  `UI-008` es `out-of-scope` (presupuesto CMA, no UI). 0 hallazgos estructurales no exceptuados.
- **`contracts`**: 2056 checks sobre el candidato solo, 0 fallos y **0 defectos UNCHANGED**.
- **`themes`**: chequeo estático WCAG sobre las **10 paletas** (4 base de `src/theme.h`: IU1WXY, Legacy,
  Outdoor, Classic; + los 6 JSON que sirve la ROM: Aurora, Cathode, Redshift, Sprint, Sunlit, Trail), 90
  pares, 0 fallos, 0 errores; superficies no-texto 19/19. Los temas runtime no resueltos se declaran, no se
  omiten. Contraste calculado **no** prueba visibilidad al sol ni LCD real.

## 5. UI-01 — evidencia visual (capa HOST, 800×480)

Cubierto por capturas reales (ver [`atlas.md`](atlas.md) y `results.csv`):

| Dimensión | Cobertura | Nota |
|---|---|---|
| Seis temas de menú | **Aurora, Cathode, Redshift, Sprint, Sunlit, Trail** | 199 capturas reparten 101/20/23/18/19/18 por tema; `theme.requested == theme.active` en todas (UI-013) |
| Ajustes (grupos) | `settings-home/-wfbat/-audio/-bands/-perfil` + `settings-grupo-*` + 6 variantes de tema | Operation/Audio/Spectrum/Clock/Display/Appearance/Actions/Info según fixtures |
| Páginas APP | `apps-menu-1..6` (6 páginas, 22 apps) + `app-bands`, `app-close-home` | la app About ya no existe |
| DFL expandido / presets | `dfl-*` (editor, presets, mirror, espejo LSB, temas) | ver §7 |
| Frecuencias extremas | `home-frecuencia-extrema`, `dfl-usb-d-span50k/100k` | |
| Modos | USB/LSB/USB-D/LSB-D/CW/CWR/AM/NFM presentes en editor DFL y `cw-tune-*` | ver hueco abajo |
| VFO A/B | `main-tema-*`; toggle por tecla A/B (code) | sin fixture de toggle puro |
| RX/TX simulado | `main-tx-simulado`, `main-tema-*-tx`, `dfl-presets-tx`, `dfl-editor-tx` | `swrscan-*` con backend fake |
| split/RIT/XIT/lock | por código (split=AGC long, lock=FST long, RIT/XIT=botones MFK) | RIT/XIT sin fixture dedicado |
| batería / fuente | `main-bateria-baja`, `home-bateria-externa` | |

**Matriz de combinaciones no cubiertas** (en vez de multiplicar fixtures a mano): el cruce
*tema × pantalla × modo* sólo existe para `main-*`, `dfl-*`, `settings-*`, `bt-dialog` y `swrscan-3pass`.
**No hay** variante por tema de las 6 páginas APP, de `autotune`/`cw-tune`, ni de `home-*`/`main-tx`; y
los modos sólo tienen fixture en el editor DFL y en `cw-tune`, no en la pantalla principal (fila
`UI01-matrix-gaps`, `REVIEW_REQUIRED`).

## 6. UX-01 — matriz de tareas frecuentes ([`operator-tasks.csv`](operator-tasks.csv))

25 tareas, con estado inicial fijo (home 14.074 MHz USB, VFO A, RX, DFL cerrado). Columnas por tarea:
ruta preferente física, secuencia literal de teclas/encoder/touch, short/long, cantidad de pulsaciones/
toques/cambios de modo, **detents contados aparte de acciones**, unidades y feedback, cancel/restore y
riesgo de error. **Toda la matriz es CODE walkthrough** (cita `src/…`), no prueba de mano humana: el
harness renderiza estados, no simula secuencias de teclado/encoder. Conteos destacados: cambiar banda 1
pulsación; cambiar modo 1 pulsación + 1 long (sentido inverso); VFO 1 + 1 long (clone A→B); sintonizar
1 detent (despacio) / n detents (rápido, multiplicador 10x LITE–24x STRONG); DFL abrir/cerrar 1 pulsación;
editar LOW/HIGH/BW por encoder VOL (rotary0 **no** edita); memoria recall 1 / guardar 1 long.

Riesgos de error registrados (código, no opinión): `band_lock`/`ab_lock` descartan la pulsación en silencio;
`pre`/`att` se auto-excluyen sin avisar; el encoder VOL edita el filtro (no el volumen) con el DFL
expandido; guardar memoria y clonar VFO pisan estado sin confirmación.

## 7. UX-02 — negativos DFL / dial / Auto Tune

Se consumieron las fixtures negativas del candidato; los contratos ya decididos están **codificados como
assertions en `tools/ui_harness/src/screens.c` (`scenario_settle`)**: una captura que completa significa que
la assertion pasó (capa HOST, cobertura REAL para el contrato codificado). Filas `UX02-*` de `results.csv`:

- `dfl-editor-rotary0` — rotary0 conserva la sintonía y **no** toca los filtros; con `freq_lock` no mueve;
  en CB cuenta canales (screens.c:1099-1138).
- `dfl-presets-rotary0` — la rueda principal mueve la frecuencia y no toca el filtro aplicado (screens.c:1879).
- `dfl-presets-vol` — el encoder VOL edita el DFL estando abierto.
- `dfl-presets-assign-cancel` no guarda; `dfl-presets-save-fail` conserva el estado; `dfl-presets-mode-change`
  no altera IDs/config previas.
- `dfl-presets-touch` (toque fuera >22 px no cambia nada) y `dfl-presets-touch-hold`.
- `autotune-notcw` — Auto Tune fuera de CW/CWR **no** arranca sesión ni mueve el dial (screens.c:2227-2259).
- `dfl-mirror-lsb`/`dfl-mirror-usb` — espejo LOW/HIGH coherente con el modo.

**Hueco explícito** (fila `UX02-gap-fft`, `UNSUPPORTED`): no hay FFT en el harness y no se emite RF, así que
el caso "Auto Tune sin medida confiable" no se puede forzar; se documenta la secuencia pendiente en vez de
inventar FFT.

## 8. UX-03 — apps / errores / alcance real

- **Diálogos que el host no compila → BLOCKED** (`UX03-inert-dialogs`): los 22 constructores inertes (§2).
  Es el hueco que dependía del #196. No se tradujo ni cambió ninguna UI.
- **Mensajes literales actuales** (`UX03-messages`, `REVIEW_REQUIRED`; CODE walkthrough): GPS
  `"None: this radio has no GPS"` (EN) + `"sin datos"` (ES); WiFi `"Unknown"`/`"Scanning..."`/
  `"Can't update new connection"` (EN); Status `"not loaded"`/`"no adapter"` (EN); Propagation
  `"Sin QTH configurado…"`/`"Reloj sin NTP…"` (ES). Hay mezcla ES/EN: se reporta el hecho, **no** se
  clasifica como bug (decisión editorial = REVIEW_REQUIRED).
- **Alcance** (`UX03-app-scope`, PASS): `src/remote_server.cpp::app_names` tiene **25 nombres sin `off`**
  (FT4 alias de FT8, DFLEditor inline), con `static_assert` contra `actions[]`; el menú APP son 22 apps.
  El remoto no es toda la UI.

## 9. Atlas indexado

[`atlas.md`](atlas.md): **199** entradas, cada una con miniatura (enlace al PNG original en
[`captures/`](captures/)) y fixture / pantalla / tema solicitado–efectivo / seed / capa / revisión /
cobertura dominante. Se incluye también la inspección estructural por fixture (árbol de objetos LVGL de
`--inspect`), comprimida sin pérdida en [`inspection/`](inspection/) (audit y contracts).

## 10. Cobertura y clasificación

- [`ui-coverage.csv`](ui-coverage.csv): las **142 filas del #195 asignadas a #197**, cada una con su captura
  o `UNSUPPORTED`/`sin fixture propio` + motivo (sin filtrar pantallas ni declarar "100% UI").
- Clasificación por evidencia: **sólo fallo de contrato demostrado = FAIL** (hubo **0**); **opinión
  estética = REVIEW_REQUIRED** (`UI01-matrix-gaps`, `UX03-messages`); lo derivado de leer el código se
  etiqueta **CODE walkthrough** (toda la matriz UX-01, los mensajes); **touch/encoders/legibilidad al
  sol/audio/RF = PENDING_HARDWARE** (5 filas `PH-*`).
- Capa y cobertura **por componente** (HOST/QEMU_USER/HARDWARE; REAL/PARTIAL/SYNTHETIC/UNSUPPORTED): no se
  generaliza desde el nombre del fixture.

## 11. Limitaciones y pendientes físicos

1. **#196 no integrado**: lo que exigía sus capturas/estado de apps queda BLOCKED (§2). Unidad 4 no
   debería empezar hasta aceptar este informe.
2. **Diálogos inertes en host** (22): Status/WiFi/BT/GPS/Propagation/Recorder/RTTY/CW-kbd/EQ/Field/… no
   se compilan; su contenido real no se mide acá.
3. **Sin FFT / sin RF**: el caso "Auto Tune sin medida confiable" queda pendiente.
4. **Determinismo A/A**: no prueba ausencia de regresión contra firmware anterior.
5. **`banco-apps` ocupado**: escenarios que lo requieran, BLOCKED.
6. **PENDING_HARDWARE** (fase #199, no la aprueba este informe): pantalla táctil, encoders/detents,
   legibilidad al sol y LCD, audio y RF/PTX/ATU/SWR, consumo.
7. **Espacio**: scratch movido de `/tmp` a `/home` por falta de espacio (desviación declarada, §3).
8. Este informe **no** aprueba el candidato: describe la experiencia vigente, fallos y límites.

## 12. Entregables y reproducibilidad

- [`README.md`](README.md) (este archivo), [`results.csv`](results.csv), [`ui-coverage.csv`](ui-coverage.csv),
  [`operator-tasks.csv`](operator-tasks.csv), [`atlas.md`](atlas.md),
  [`artifact-manifest.json`](artifact-manifest.json), [`commands.jsonl`](commands.jsonl),
  [`SHA256SUMS`](SHA256SUMS), [`logs/`](logs/), [`captures/`](captures/), [`inspection/`](inspection/),
  [`scripts/`](scripts/).
- Los `scripts/` son pegamento de análisis sobre herramientas existentes (generan las tablas); **no**
  sustituyen el harness, el parser, un mock, un test ni un contrato.
- No ROM/binarios/WAV grandes en Git: las capturas son PNG 800×480 y las inspecciones van comprimidas sin
  pérdida. Sin links sólo a `/tmp`.
- Revisores: `git diff --name-only origin/assessment/rom-194...HEAD` debe listar **sólo** esta carpeta.
