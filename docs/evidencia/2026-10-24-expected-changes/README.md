# Evidencia — issue #296: `expected_changes` scoped por fixture + procedencia en el reporte

Corrida del **2026-10-24** del issue
[#296](https://github.com/iariap/x6200_gui/issues/296) (epic de pruebas de la GUI).

| | |
|---|---|
| Worktree | `state/worktrees/x6200/expchanges-296` |
| Rama | `factory/expchanges-296` (base `origin/main` = `0d3f9e68`) |
| Alcance | `tools/ab_testing/**` + esta carpeta. **Cero** cambios de firmware, de `src/`, de `.factory/**` |
| Radio / hardware | **no** (sin SSH, flash, deploy ni TX); todo es arnés de host + Docker |
| Identidad de los commits | `Pablo Iaria <iariap@gmail.com>`; el commit efímero del control negativo usa la identidad de scaffolding de `tests/test_e2e.py` y **no** se pushea |

## 1. El problema, medido (#288)

El gate visual consideraba **explicado** cualquier cambio que cayera dentro de una
**región habilitada** (`quality/regions.yaml`: un rectángulo de la pantalla),
**para todas las fixtures**. Las 45 entradas de `expected_changes` declaraban
sólo `region`, así que la unión de sus rectángulos habilitaba **toda la pantalla
en toda fixture**: en el A/B de #288 (`a=e1807671 b=884841c5`) el cambio del
diálogo de SWRScan pasaba como "explicado" por entradas de **Ajustes**
(`ajustes_dialogo` = `[2,66,796,348]`, la caja compartida de las apps).

Antes de este cambio ese A/B decía:

```
swrscan-3pass-stop: 238 changed pixels, all inside ['ajustes_anio',
'ajustes_audio_ganancia', 'ajustes_dialogo', 'ajustes_perfil', 'ajustes_scrollbar',
'ajustes_tema', 'barra_superior', 'botones', 'dfl_inline', 'espectro', 'frecuencia',
'reloj_potencia', 's_metro', 'waterfall', 'waterfall_ticks']
```

Sin autor, sin motivo y sin decir qué entrada lo explicaba.

Después (mismo A/B, misma config de firmware):

```
swrscan-3pass-stop: 238 changed pixels, all inside ['ajustes_dialogo', 'espectro']
— `espectro` declared by `dfl-visual-173` until 2027-10-20: AB-DFL173-1 (issue
#173, PR 1): …; `ajustes_dialogo` declared by `ui-polish-app-copy-284` until
2027-04-30: AB-APCOPY288 (issue #288, epica #284): el copy de operador de las
apps pasa a ingles. …
```

## 2. Qué cambia

1. **`ExpectedChange.fixtures`** (`quality_config.py`): lista de **globs** de
   fixture (`fnmatchcase`, para que el match no dependa del host). Es
   **requerida**: si falta es `ConfigError` (nunca un default silencioso a "todas"),
   y cada glob tiene que matchear **al menos una fixture de la suite** (si no, es
   un typo, no un scope). `load_contracts` recibe el set de fixtures conocidas (el
   CLI lo resuelve de `tools/ui_harness/fixtures/*.scn`).
2. **Gate por fixture** (`quality_gates.py` + `evaluator.py`): `allowed` deja de
   ser la unión global y pasa a ser `QualityContracts.allowed_for(fixture)` — sólo
   las entradas activas **cuyo glob matchea esa fixture**. `unexplained_changed_pixels`
   recibe ese set. Los issues que B arregla (`FIXED`) siguen habilitando su región,
   pero sólo en la fixture del issue.
3. **Procedencia en el reporte**: el gate emite `explained_by` (por fixture: píxeles,
   regiones y las entradas con `region`/`fixtures`/`reason`/`author`/`until`) y la
   línea humana dice qué entrada explicó el cambio. `report.md` y `report.html`
   ganan la sección **"Visual changes explained by a dated entry"** con la tabla
   corta y, debajo, el `reason` completo en una línea por entrada. Sólo se nombran
   las entradas cuya región **tuvo píxeles cambiados**: una declaración de la
   fixture que no se movió no explica nada.
4. **Migración de las 45 entradas**: cada scope sale del **A/B propio de la
   entrada** (columna *Fuente* de [`migracion.md`](migracion.md)), restringido a
   las fixtures que la suite todavía publica. **Ninguna entrada usa `*`** (lo fija
   un test).

## 3. La migración (45 filas)

La tabla completa, con la fuente de cada scope y los conteos medido/vivas, está en
[`migracion.md`](migracion.md). Resumen:

* **40 entradas migradas** con globs medidos. Ejemplos: el A/B de #288 mueve
  `swrscan-3pass-stop` y nada más ⇒ esa entrada (`ui-polish-app-copy-284`) lleva
  `fixtures: [swrscan-3pass-stop]`; el de #287 mueve 19 fixtures de Ajustes ⇒
  `[settings-audio, settings-grupo-*, settings-home, settings-perfil, settings-tema-*]`;
  la migración de fuentes (#9) movió las 34 fixtures de su suite en la barra
  superior ⇒ los 24 nombres que siguen existiendo; el cambio de tema por defecto
  (#74) movió las 25 fixtures que no fijan tema ⇒ esos 25 nombres.
* **5 entradas retiradas**: UI-010, UI-011, UI-012, UI-014 y el arco RS del
  `wcag-full` estaban ancladas a fixtures que **THEMES v2 #74 retiró** con los
  temas VISION/RS (`main-tema-vision*`, `main-otro-tema`, `main-tema-material-3`,
  `main-tema-rs`, `settings-rs-theme`). Un A/B contra el árbol actual **no puede
  comparar esas fixtures** (A define la suite: una fixture que B no publica queda
  fuera del diff), así que sus globs no podían matchear nada. Es el mismo criterio
  que `tests/test_known_issues.py::test_repo_ui010_ui013_are_retired_with_their_fixtures`
  ya aplica al registro de known issues. Retirarlas **endurece** el gate y ningún
  veredicto cambia.

## 4. A/B antes / después

Mismo par de revisiones, mismos píxeles; lo único que cambia es la config y el
código del gate. "Antes" = tool y `contracts.yaml` de `origin/main`; "después" =
esta rama. Los JSON completos están en `logs/resumen-ab.json` y la sección nueva
del reporte, tal cual la escribe el tool, en `logs/report-seccion-copy288.md`.

| A/B | Antes | Después | Fixtures explicadas | Entradas que las explican (después) |
|---|---|---|---|---|
| `a=e1807671 b=884841c5` — #288, el caso del issue | **PASS** | **PASS** | 1 → 1 (las mismas) | `dfl-visual-173` (espectro); `ui-polish-app-copy-284` (ajustes_dialogo) |
| `a=db31e24e b=2f646084` — #287, tinta de los items del diálogo | **PASS** | **PASS** | 19 → 19 (las mismas) | `dfl-visual-173` (espectro, waterfall_ticks); `fonts-libre` (ajustes_anio, ajustes_audio_ganancia, ajustes_perfil, espectro, frecuencia, waterfall); `home-clock-battery-250` (ajustes_dialogo); `seten109` (ajustes_dialogo); `settings-year-110` (ajustes_anio); `themes-removal-74` (ajustes_anio, ajustes_audio_ganancia, ajustes_perfil, ajustes_tema, espectro, frecuencia, waterfall); `ui-polish-contrast-284` (ajustes_dialogo); `ui016-year-field` (ajustes_anio) |
| `a=6ec345aa b=0d3f9e68` — 10 días de UI (8 fixtures comparadas) | **PASS** | **PASS** | 8 → 8 (las mismas) | `dfl-visual-173` (espectro, waterfall_ticks); `fonts-libre` (ajustes_anio, ajustes_audio_ganancia, ajustes_perfil, barra_superior, botones, espectro, frecuencia, reloj_potencia, s_metro, waterfall); `home-clock-battery-250` (reloj_potencia); `home-status-legibility-251` (barra_superior); `themes-removal-74` (ajustes_anio, ajustes_audio_ganancia, ajustes_perfil, ajustes_scrollbar, ajustes_tema, barra_superior, botones, espectro, frecuencia, reloj_potencia, s_metro, waterfall); `waterfall-center-line` (waterfall) |
| `a=eea3ed20 b=0d3f9e68` — 10 PRs agregados (205 fixtures) | **PASS** | **FAIL** | 205 → 0 | — |
| | | | **180 fixtures fallan** | ver la observación de la sección 8 |

## 5. Control negativo (obligatorio)

Se inyectó un cambio de píxeles en una fixture que **ninguna entrada nombra**:
`tools/ui_harness/fixtures/autotune-am.scn`, `smeter_db -73` → `-70` (mueve la
aguja y la etiqueta del S-meter), en un commit efímero sobre `0d3f9e68`
(`9efb3133be5c`, identidad de scaffolding, **no** se pushea). El commit se
descartó después: la rama no lo contiene.

| Corrida | Veredicto | Qué dice el gate |
|---|---|---|
| antes (config global) | **PASS** | `autotune-am: 331 changed pixels, all inside ['ajustes_anio', 'ajustes_audio_ganancia', 'ajustes_dialogo', 'ajustes_perfil', 'ajustes_scrollbar', 'ajustes_tema', 'barra_superior', 'botones', 'dfl_inline', 'espectro', 'frecuencia', 'reloj_potencia', 's_metro', 'waterfall', 'waterfall_ticks']` |
| después (scope por fixture) | **FAIL** | `autotune-am: 331 unexplained changed pixels (0.09% of the screen)` |

Mismo cambio, mismas dos revisiones: lo único distinto es la config. Antes pasaba porque la unión global habilitaba el rectángulo en toda fixture; después falla porque **ninguna entrada nombra `autotune-am`**.

## 6. Gates

| Gate | Comando | Resultado |
|---|---|---|
| Unitarios del framework | `uv run pytest -m "not e2e"` | **255 passed**, 13 deselected (237 antes de este cambio: +18 tests nuevos) |
| Lint | `uv run ruff check .` | All checks passed |
| Formato | `uv run ruff format --check .` | 201 files already formatted |
| Tipos | `uv run ty check src tests` | All checks passed (nota: dentro de un worktree linkeado `ty` no descubre los directorios; se verificó también en un checkout plano, ver §8) |
| Contratos sobre el candidato | `uv run x6200-ab contracts --revision 0d3f9e68` | **PASS — 2118 checks, 0 failures, 0 unchanged defects** (205 escenarios) |
| Suite del repo | `sh tests/run_all_tests.sh --quick` | **23 OK, 0 fallos** (omitidos: qemu_apps, wspr, sstv, wefax, js8, webserver) |
| Contraste renderizado | `tests/check_wcag_rendered.py` (dentro de la suite) | PASS — **0 entradas** de baseline, sin violaciones nuevas |
| Texto de widgets | `tests/check_widget_text.py` (dentro de la suite) | PASS — **0 hallazgos** |
| Proxy de perf | gate `Performance` de cada A/B | PASS en los cuatro pares, sin excepciones aplicadas |

## 7. Cómo reproducir

```sh
# framework
cd tools/ab_testing
uv sync --offline
uv run pytest -m "not e2e"
uv run ruff check . && uv run ruff format --check . && uv run ty check src tests

# contratos sobre el candidato (205 escenarios, 2118 checks)
uv run x6200-ab contracts --revision 0d3f9e68 --repo <repo> --workspace <ws>

# A/B del caso del issue: antes y después del cambio de gate
git stash            # tool + contracts de main
uv run x6200-ab compare --a e1807671 --b 884841c5 --repo <repo> --workspace <ws> \
    --experiment-id AB-296-before-copy288
git stash pop        # tool + contracts migrados
uv run x6200-ab compare --a e1807671 --b 884841c5 --repo <repo> --workspace <ws> \
    --experiment-id AB-296-after-copy288

# suite del repo
sh tests/run_all_tests.sh --quick
```

Los exit codes reales de cada corrida (`0` PASS, `1` FAIL) están en
`logs/comandos.txt`.

## 8. Observaciones y límites

* **La suite la define A.** Una fixture que A no tiene no se compara, así que un
  A/B no puede "ver" un cambio sobre una fixture que no existía en A. Es la razón
  por la que las cinco entradas retiradas no podían explicar nada.
* **Las regiones se solapan** (`espectro` contiene la franja superior de la caja
  del diálogo). Con el scope por fixture, el cambio de #288 queda explicado por
  `dfl-visual-173` (`espectro`: en su propio A/B esos píxeles se movieron) y por
  `ui-polish-app-copy-284` (`ajustes_dialogo`) — las entradas de **Ajustes** ya no
  lo explican, que es lo que pedía el issue. Un cambio en la mitad inferior del
  diálogo, en cambio, ya no cae en ninguna región habilitada de SWRScan y **falla**
  (control negativo de la sección 5).
* **El A/B que agrega 10 PRs** (`a=eea3ed20 b=0d3f9e68`) pasa de PASS a FAIL en 180
  fixtures: contiene cambios que **nunca tuvieron declaración propia** (la
  superficie opaca del diálogo de #286 en `ajustes_dialogo`/`waterfall`/`dfl_inline`,
  la sincronización de modo de #292 en el contenido del waterfall, y el slot de
  fuente de #285 en `botones`/`barra_superior`). Con el gate global pasaban porque
  la unión cubría la pantalla; con el gate por fixture fallan donde ninguna entrada
  declara la banda. Declararlos exigiría un `*` sobre `waterfall`/`espectro`/
  `ajustes_dialogo`, que es exactamente el agujero que #296 cierra: **no se aflojó
  el gate para taparlo**, queda como decisión del owner (declaración nueva, medida
  y acotada, o aceptar que ese agregado arqueológico ya no pasa).
* **`ty` en un worktree linkeado**: `uv run ty check src tests` avisa `No python
  files found` dentro de este worktree (limitación de la herramienta, no del
  código). En un checkout normal y en una copia plana del mismo árbol pasa limpio;
  se verificó en las dos formas.
