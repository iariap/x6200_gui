# Spec: bands-from-settings — corrida: repro banco + fix de "Bands no abre desde Settings"

**Fecha**: 2026-10-15 · **Corrida**: `factory/bands-settings` · **Repo app**: `iariap/x6200_gui`
**Base**: tip `89851e6` · **Origen**: reporte del owner (radio real, 2026-10-15)
**Ruta declarada por el owner**: entrar por el **botón APP** → Apps → **Settings** → desde ahí ejecutar **Bands** → no hace nada.

## Por qué (lo que muestra el código)
`ACTION_APP_BANDS` se despacha **solo** en `main_screen.c:300-302` (`dialog_construct(dialog_bands, obj)`). El escalado del dispatch depende de dónde vive `obj` — la acción puede alcanzarse desde Ajustes (las asignaciones de acción `app_items_t` en `dialog_settings.c:1011` y el botón de la barra `buttons.cpp:340`) pero el dialog_bands debería construirse/right sobre el contexto acertado.
El banco (`botón APP` → que ya lo reproduce el socket `{"cmd":"app","text":"Bands"}`) y el arnés QEMU pueden medir el estado INVITADO.

## Alcance
1. **Repro banco (obligatorio primero)**: con el binario ARM real (`banco`):
   - encender via `{"cmd":"app","text":"Settings"}` → capturar PNG `bands-settings-a.png` (el estado INTERMEDIO declarado diría: Bands no abre);
   - driver del app desde la App grid: **aprieta los BTN desde Settings, bands activ Attempts** (el **commands are the same** de `app_cmds[]` — mirar `remote_server.cpp` os apps-long). Documentar la secuencia exacta en el reporte.
   - Banco snapshot: PNG después de la activación, y la estación "no hace nada" donde quede visible (swap estado / sin abierto).
2. **Fix (si reproduce)**: la acción `ACTION_APP_BANDS` debe abrir el dialog_bands **sobre el estado actual** igual que el resto de apps (patrón `ACTION_APP_*`), o el "app action desde settings" debe enrutarse por el mismo handler global — el coder debe elegir el ajuste mínimo** y hannegarlo a A/B.
   - A/B arnés: fixture `settings-bands.scn` (Ajustes + la acción hacia Bands activa) — el compare esperado "política del fix: los px del dialog_bands APARECEN" con `expected_changes` scoped, o pixel-identical del RUNNING screen si el dialog cambia de objeto: declararlo en la spec del fix antes de correr.
3. **Tests** nativos si procede (acciones de settings) y TODO el set del harness + determinismo + compare + contracts.

## Guard: sin repro = no fix
Si el banco muestra que **Bands abre bien** desde la ruta descrita, reportar INCONCLUSIVE con las capturasExtendidas y pedir confirmación al owner (si era otra ruta referida).

## Criterios
```bash
cd tools/ab_testing
uv run x6200-ab compare + contracts + audit + determinism (22x2 con las fixtures nuevas)
tests/native/run_native_tests.sh + estáticos + pytest ×2 + ruff/ty
```
Banco: capturas A/B + md5 del binario. Sin Signs (los Steps documentados en el PR del fix (datos reales de la secuencia en la radio).
