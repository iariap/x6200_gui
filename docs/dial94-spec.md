# Spec: swr-dial-restore-94 — el barrido de apps deja el modo y el dial cambiados (issue #94)

**Fecha**: 2026-10-15 · Corrida: `factory/swr-dial-restore-94` · Base: trunk actual
**Issue**: #94 (QA) — tras un barrido completo de apps la radio quedó en `7.167971 MHz USB-D`
(el owner tenía 7.162660 y el barrido de ROE la había dejado en LSB #84). El flood del log
(8.167/8.388 líneas = cambios de freq históricos de las apps, ~100 ms entre una y otra) es
**historico, no activo** (verificado por el QA: 0 cambios en 3-5 s → el log no crece — la
raíz del flood es el mismo set de cambios de las apps, no la prueba).

## La causa y el fix (patrón exacto del #84, expandido al DIAL)
- Al abrir apps digitales (FT8/WSPR/JS8/…), el equipo cambia el modo y el dial; al cerrar se
  queda el estado del último app — las apps **no restauran** (ni el dial ni en varios casos
  el modo).
- **El diseño del fix no es tocar app por app**: replicar el patrón de `radio_start_swrscan()`
  (#96: guarda y restaura fuera del radio_lock — el save/restore del estado del EQUIPO) en
  la apertura/cierre GENÉRICO de apps del dispatcher (el punto en que cada app muta):
  el **snapshot del estado de radio (dial + modo + potencia)** al abrir cualquier app
  digital (o al abrir apps que MICION el radio: dependiendo del flujo actual — investiga
  dónde mueren los cambios: `dialog_ft8.c`/`dialog_wspr.c`/`dialog_js8.c` setean freq y modo?)
  y restaura al cerrar. Puntual: el barrido de QA vuelve al estado del owner
  (7.162660 se restaura en el test).
- **Los dos niveles del diseño**: (a) en el dtor de cada app digital si es pública, o (b) en
  un SNAPSHOT centralizado de estado si las apps comparten el flujo为主题的 (mirar en media
  #96: el principal payload del fix del barrido; además el snapshot general es el costo del
  guard (quiân dead code/branches). ELEGÍ la pedal seria y DECLARALA con la evidencia del
  código (qué apps cambian el dial junto con el flujo关节炎, cómo la apertura del app muta
  el equipo).
- **El log flood (8.4k líneas)**: la segunda parte del issue — las mutaciones de freq quedan
  en el log y son **históricas** (no crece). No es defecto no-activo pero el issue lo Informa
  (el triage del QA). FIX opcional: si la freq-change es por SOURCE (app) cuando el json
  referido es de un script de app-periodico? Declarar qué se hizo (si se tira una
  sanidad: el log de las mutaciones es útil para traz成立于, el saneamiento NO es de producción),
  si el issue lo pide (leelo).

## Tests/banco
- **Nativo**: si el fix centralizara en radio.c, un test como el del #96 (save/restore con
  observadores reales); si cada app va por algo asberg, suficiente test del centralizado.
- **Banco**: barrido corto? el flujo real del QA: `tools/barrido-apps-banco.sh` 3 app-digitales
  → al final del barrido, el estado del equipo = estado inicial (freq+modo), 0 caídas, GUI viva.
- La confirmación de radio (el owner tiene 7.162660 y modo propio) queda en QA.

## Gates
```bash
cd /home/pablo/desa/asf/state/worktrees/x6200/swr-dial-restore-94
tests/check_harness_determinism.sh
cd tools/ab_testing
uv run x6200-ab compare --a origin/lu2dsx/x6200-rom --b HEAD --experiment-id AB-DIAL94
uv run x6200-ab contracts --revision HEAD && uv run x6200-ab audit --revision HEAD --suite visual
uv run x6200-ab themes
uv run pytest -m "not e2e" -q && uv run pytest -m e2e -q
tests/native/run_native_tests.sh
estáticos 7/7
+ banco flujo del barrido (3 apps → estado inicial restaurado)
```

## Invariantes
- NO tocar RF/TX de señal; `lvgl/` pinned. NO commitear `.asf-active`. NO toques el guard del
  build (#86 en otro worktree) ni el #73 del mismo git.
- Commits `Pablo Iaria <iariap@gmail.com>` — **NO push/PR**.

## Reporte final
1. El flujo resultante (qué apps mutan el equipo y dónde).
2. El fix (snapshot/restore) + el punto exacto del diseño y el por qué.
3. El test (RED/GREEN) + banco con la restauración final.
4. El flood del log: qué se hizo/ifd declarado.
5. Gates completos. PENDIENTES honestos (confirmación física de radio/QA).