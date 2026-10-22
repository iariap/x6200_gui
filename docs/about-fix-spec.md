# Spec: about-fix — PR follow-up del defecto que el run `bands-settings` dejó declarado

**Fecha**: 2026-10-15 · **Corrida**: `factory/about-fix` · **Base**: tip post-`0684861` (fix Bands mergeado)
**Origen**: run `bands-settings` (PR #41): hallazgo declarado "About queda muerto por la misma causa".

## Outcome (tres fixes menores, un solo PR)
1. **About abre desde el menú de apps** (`main_screen.c`): `case ACTION_APP_ABOUT` (3 líneas idénticas al fix del Bands, `dialog_about` ya existe y está renderizado como CREW por el fixture `dialog-about`).
2. **`check_apps_menu.py` endurecido**: debe contar los `case ACTION_APP_*` **solo dentro de `main_screen_start_app`** (la función que usa el botón del menú), no de todo el archivo — hoy acepta un botón muerto (About) porque el case vive solo en `main_screen_action`. Con el fix 1 el test vuelve a 24/24 y **queda blindado para la clase**.
3. **`tests/run_all_tests.sh`**: el contador `fallos` usado en los `||` de los chequeos (`fallos=$((fallos+1))`) **no existe** → los fallos de los chequeos no hacen fallar la suite. Inicializar/propagar de forma real (los chequeos del arnés y estáticos deben poder fallar la corrida).

## Criterios
```bash
# fixture nueva (o extendida) del arnés para About desde el menú de apps:
tools/ui_harness/fixtures/app-about.scn   # page app → btn de About
# determinismo + compare + contracts (fixture nueva, A suite sale de la revisión A)
```
- **banco**: el binario ARM real con `--bench-natural`/page app: `app About` abre el dialog (captura PNG antes/después).
- TODO el set de gates del repo (pytest/nativo/estáticos/ruff/ty) + `check_apps_menu` 24/24 + `run_all_tests.sh` succès con un chequeo deliberadamente roto (anti-falso-verde unit).

## Fuera de alcance
No tocar otros botones ni el route del waterfall/volume; no tocar `dialog_about.c` core.
