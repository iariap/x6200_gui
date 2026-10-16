# Spec: settings-en-order-109 — todos los Settings en inglés de ham radio, ordenados por utilidad para hacer contactos (issue #109)

**Fecha**: 2026-10-16 · Corrida: `factory/settings-en-order-109` · Base trunk `5944ff5`
**Issue**: #109 — orden del owner: "traduce todos los settings a inglés de ham radio y ordenalos
por utilidad en el uso del equipo para hacer contactos".

## Alcance

### 1. Inventario completo (primero, antes de cambiar nada)

TODAS las strings visibles del diálogo de Ajustes:

- **Tabs (hoy en español)** — `src/dialog_settings.c:120-129`: `Reloj`, `Pantalla`, `Info`, `Audio`,
  `Espectro`, `Operacion`, `Apariencia`, `Acciones`, `Mas >`, `< Mas` (paginación).
- Títulos del diálogo, headers de sección (varios ya están en inglés: "Day, Month, Year",
  "Hour, Min, Sec", "Band plan", "S-meter needle", ...), labels de filas, botones, textos de
  dialogs que se abren desde Settings, y cualquier string en español que aparezca en el recorrido
  (grep de acentos/español en todo lo visible: `grep -n 'ñ\|á\|é\|í\|ó\|ú' src/dialog_settings.c`
  + los dialogs que Settings lanza).
- Incluí las strings de `check_actions.py` (la lista de acciones que ofrece Ajustes) si están en
  español.
- **NO traducir**: logs (`LV_LOG_*`), comentarios, identifiers de código, strings de voz TTS que ya
  están en inglés con terminología correcta.

### 2. Traducción a inglés de ham radio (terminología estándar)

- Tabs propuestos (ajustá si el inventario real sugiere mejor estándar, con razón):
  `Reloj→Clock`, `Pantalla→Display`, `Espectro→Spectrum`, `Operacion→Operation`,
  `Apariencia→Appearance`, `Acciones→Actions`, `Mas >→More >`, `< Mas→< More`,
  `Info→Info`, `Audio→Audio`.
- Labels en inglés ya técnicas: **conservar** si son el término ham correcto
  ("S-meter", "ALC", "Band plan", "PTT"...). Sólo cambiá lo que sea no-idiomático o español.
- Terminología ham a preferir donde haya que elegir: PTT, RF Gain, Squelch, AGC, Mode, Band,
  Filter/BW, Preamp/Att, Mic gain, VOX... (lo que aplique al inventario real).

### 3. Orden por utilidad para hacer contactos (QSOs)

**Principio**: ordenar los tabs por lo que se usa mientras se hace un contacto vs lo eventual.

Esqueleto propuesto (confirmá/ajustá con el inventario real de contenidos de cada tab; cada
desvío de este esqueleto va explicado en el reporte):
1. **Operation** (modo/banda/potencia/PTT — lo QSO esencial)
2. **Audio** (mic/AF/ganancias para transmitir y oír)
3. **Spectrum** (hallar señales)
4. **Clock** (fecha/hora para el log — se toca al operar)
5. **Display** (legibilidad en operación)
6. **Appearance** (temas/fuentes — ergonomía)
7. **Actions** (asignación de teclas/MFK)
8. **Info** (sistema/about/update)
9. **More >** (lo eventual: voz, GPS, red, BT, transverter, ...)

La paginación `Mas >` / `< Mas` se recalcula según el nuevo orden (page 1 = primeros N tabs,
page 2 = resto — mirá cómo se arma hoy y mantené la misma mecánica).
- Documentá el orden final + la razón en el repo (dónde vive hoy la doc de settings: buscá en
  `docs/` qué describe los tabs/ajustes y actualizá eso; si no hay doc, creá
  `docs/settings-order.md` con la tabla orden↔utilidad).

### 4. FIXTURES — el punto más delicado

Las fixtures de settings navegan por **ÍNDICE de botón** (`buttons_press(N)`) y sus comentarios
nombran tabs en español:
- `settings-home.scn` (grupo 1 = hoy "Reloj")
- `settings-wfbat.scn` (`buttons_press(4)` = "Mas >", `buttons_press(0)` = tab "Espectro" de pág 2)
- `settings-perfil.scn`, `settings-tema-redshift.scn`, `settings-tema-sunlit.scn`,
  `settings-tema-aurora.scn` (`buttons_press(2)` = "Apariencia" de pág 2)
- Cualquier otra fixture que toque Ajustes (grep en `tools/ui_harness/fixtures/*.scn`).

**Requisito**: cada fixture debe seguir abriendo **la MISMA pantalla** que antes:
- Remapear los índices de `buttons_press` al nuevo orden/paginación.
- Actualizar los comentarios de cada fixture (hoy nombran tabs viejos).
- Verificar por la captura de la fixture (mismo contenido que antes salvo labels): el A/B lo confirma.
- OJO: con el cambio de labels, los goldens de estas fixtures cambian pixel → **re-baseline
  intencional** con el patrón de la casa (el reporte A/B explica cada diff como intencional;
  0 regresiones "unexplained").

### 5. Tests/checks que pueden morder

- `tests/check_settings_focus.py`: parsea `dialog_settings.c` (creación de widgets + que cada
  slider mencione su param). El orden no importa, pero si renombrás funciones/callbacks que el
  regex sigue, respetá las convenciones. Correrlo siempre.
- `tests/check_actions.py`: lista de acciones del diálogo — si cambian labels, actualizá el check.
- Contratos WCAG / `x6200-ab themes` / audit visual: re-baseline donde el texto cambie.
- Docs con referencias históricas a tabs viejos (HALLAZGOS, PRUEBAS, AB_TESTING): **registro
  histórico se queda como está**; sólo actualizá docs que describan el estado ACTUAL (manual/UX).

## Gates (pegá la salida real)

```bash
cd /home/pablo/desa/asf/state/worktrees/x6200/settings-en-order-109
tests/check_harness_determinism.sh
cd tools/ab_testing
uv run pytest -m "not e2e" -q && uv run pytest -m e2e -q
uv run x6200-ab compare --a origin/lu2dsx/x6200-rom --b HEAD --experiment-id AB-SETEN109
uv run x6200-ab contracts --revision HEAD && uv run x6200-ab audit --revision HEAD --suite visual
uv run x6200-ab themes
uv run ruff check . && uv run ruff format --check .
cd ../.. && tests/native/run_native_tests.sh
python3 tests/check_settings_focus.py && python3 tests/check_actions.py
estáticos: check_cfg_fills, check_apps_menu, check_app_handlers, check_wcag,
           audita-pruebas si tocás docs
```

## Banco QEMU (REGLA DEL OWNER: cierre con prueba QEMU salvo requiere-radio)

La pantalla de Ajustes es visible en QEMU → **captura obligatoria** en el banco con el nuevo orden
de tabs en inglés. Precedente del método (cómo llegar a Settings en el banco):
`docs/evidencia/2026-10-15-fallback91/banco.txt` — leelo y reusá el camino. Evidencia commiteada
en `docs/evidencia/2026-10-16-seten109-banco/` (README método + md5 binario/fuente/captura).
Banco ocupado por otra sesión → reportá BLOCKED (no toques infra compartida).

## Invariantes

- Escribís SOLO en el worktree; rama `factory/settings-en-order-109`.
- NO tocar: RF/TX/DSP, `lvgl/`, `theme.c`/fixtures de temas (el otro run #110 corre en paralelo
  en OTRO worktree y toca `make_date`: **evitá `make_date()`/la fila de fecha** para no chocar —
  tu diff va en tabs/labels/orden/docs/fixtures de navegación).
- Commits `Pablo Iaria <iariap@gmail.com>`; **NO push ni PR** (publica el orquestador).
- `.asf-active` no se commitea.

## Reporte final

1. Inventario (tabla: string vieja → nueva, con la razón de la terminología).
2. Orden final de tabs + razón por utilidad QSO (y los desvíos del esqueleto, si los hay).
3. Mapeo fixtures viejo→nuevo (índices) y verificación de que abren la misma pantalla.
4. Gates con salida real. 5. Banco + números. 6. PENDIENTES honestos.
