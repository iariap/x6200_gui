# Spec: about-remove — eliminar la pantalla About y todo acceso

**Fecha**: 2026-10-14/15 · **Corrida**: `factory/about-remove` · **Base**: tip `ccfe1f5`
**Orden del owner (literal)**: *"elimina la pantalla de about y el boton para accederla"*.

## Outcome
About deja de existir: ni la pantalla, ni el botón del menú, ni la fila de Ajustes, ni el
case del dispatcher, ni el enum, ni el servidor de control remoto, ni fixture/golden/checks.
Es una decisión de producto del owner, no un defecto. Queda el menú con **22 apps**
(20 `make_app_btn` + Bands como única `make_action_btn` que resta).

## Mapa de referencias (recon hecho por el orquestador)
- `src/buttons.cpp:344` — `btn_about` (menu app, `make_app_btn("About", ACTION_APP_ABOUT)`): BORRAR.
- `src/dialog_settings.c:1022` — fila `{ " About ", ACTION_APP_ABOUT }` de la lista de acciones
  del diálogo Settings: BORRAR. OJO al comentario del archivo (~línea 1010): lo vigila
  `check_actions.py` (no dejar botones sin acción) — la fila se quita porque el destino desparece.
- `src/params/params.h:63` — entrada `ACTION_APP_ABOUT` del enum: BORRAR **completa**
  (los demás valores del enum no van a ningún storage persistido por número de todos
  modos; verificá antes que nadie persista el valor crudo del enum en DB/JSON).
- `src/remote_server.cpp:297` — la lista `actions[]` de `open_app()` y la tabla
  `app_names[]` son PARALELAS (mismo índice): quitá `ACTION_APP_ABOUT` de la lista
  y `"About"` de `app_names[]` **a la vez** (mismo índice, sin desalinear el mapeo).
- `src/main_screen.c:252` y `:406` — los `case ACTION_APP_ABOUT:` de
  `main_screen_start_app()` y `main_screen_action()`: BORRAR ambos.
- `src/dialog_about.c` / `src/dialog_about.h` — BORRAR los archivos, y con ellos todo lo
  que sólo los referencie (inclusión en build, CMake/make/lib computation si la hay,
  `dialog_about` en `extern` lists de `main_screen.h`/`dialog.h`).
- Assets si sólo servían al About (fondo/bin del About, imágenes propias del diálogo):
  BORRAR los huérfanos; lo que comparte con otros widgets, no.

## Siguiente onda: arnés, golden y checkers
- `tools/ui_harness/fixtures/app-about.scn` — BORRAR la fixture (ya no hay pantalla).
- Golden ligado a `app-about` (`tools/ab_testing/golden/app-about.png` y todo lo que
  `golden/README.md`, `regions.yaml` y `contracts.yaml` referencien de ese fixture):
  BORRAR las entradas (las de OTRO fixture, no tocarlas).
- Conteos que fije el universo de fixtures (25): si `check_harness_determinism.sh`/
  pytest e2e o README de fixtures declaran "25", pasar a 24 (o mejor: que sea dinámico
  por glob y no literal — si tocarlo es trivial).
- `check_apps_menu.py` — el universo era 23 apps (21 `make_app_btn` + 2 `make_action_btn`);
  pasa a **22 (20+1)**: actualizá la lista/contadores del checker y/o su self-test tal
  como salieron del PR #43 (verificá que el checker FALLA de verdad si hay un botón
  cuyo `case` ya no existe — con el About removido tenés el caso de revocación natural
  en la mano: hácete un self-test breve si el archivo ya tiene estructura para eso;
  si no, dejalo documentado para /factory-correct).
- `check_actions.py` / `check_wcag.py` / `check_app_handlers.py` — buscá referencias izquierdas
  a `about`/`dialog_about` (exempt roles del sweep WCAG mencionaban `dialog_about`
  tras el fondo .bin opaco): quitá exemptions de código que ya no existe.
- `docs/UI_KNOWN_ISSUES.md` y `docs/AB_TESTING.md`: **no reescribir historia**; si
  alguna nota "estado actual" menciona About como disponible, actualizá sólo eso
  (con fecha y orden del owner en una línea).

## Gates (salida real, misma cadena que siempre)
```bash
cd <worktree>
tests/check_harness_determinism.sh          # 24 fixtures x2 (app-about desaparece del set)
cd tools/ab_testing
uv run x6200-ab compare --a ccfe1f5 --b HEAD --experiment-id AB-ABOUT-RM  # PASS; diffs esperables solo por fixture count / apps menu si hubiere captura de menú
uv run x6200-ab contracts --revision HEAD   # PASS (checas novas, sin waivers nuevos)
uv run x6200-ab audit --revision HEAD --suite visual  # PASS, 0 structural findings
uv run pytest -m "not e2e" -q && uv run pytest -m e2e -q
tests/native/run_native_tests.sh            # exit 0
# estáticos: check_cfg_fills / check_actions / check_settings_focus / check_apps_menu /
#            check_app_handlers / check_wcag — todos OK sin About
build host del arnés (make / compila si corresponde) sin warnings NUEVOS
```
- Si alguna fixture captura el menú de apps (¿existe `*-apps-menu*`?): refrescalá/declará
  con expected_changes scoped (píxeles del botón About fuera).
- `grep -ri 'about'` **sobre src/ + tools/** del resultado: limpiar los hits de código
  (los hits históricos de docs corren por cuenta propia).

## Fuera de alcance
- No toques la UI de Bands ni el resto del menú; el orden/quen del menú queda igual.
- No toques `visual_policy`, `backlight`, `lvgl/`, tokens/flex, workflows CI.
- Toda otra referencia a About en comentarios históricos de docs/ queda (bitácora),
  sin cambiar hechos pasados.

## Invariantes
- Commits con identidad `Pablo Iaria <iariap@gmail.com>`. **Sin push / PR** (el orquestador
  publica con el pipeline).
- Si algo no puede fundarse con check/gate, queda como PENDIENTE explícito — nada se infla.

## Reporte final (en el texto)
1. Qué archivos/tokens tocaste (diff resumido), incluidas assets borrados.
2. El grep final de `about` sobre `src/` y `tools/ui_harness`+`tools/ab_testing`.
3. Gates completos con salidas reales y los conteos nuevos (fixtures, apps del menú, contratos).
4. El estado del self-test de `check_apps_menu.py` (con el caso de revocación).
5. PENDIENTES honestos (ex. voz/face del menú visual, si hubieran).
