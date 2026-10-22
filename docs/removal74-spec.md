# Spec: themes-removal-74 — retirar los temas legados, default Aurora y migración (cierre THEMES v2)

**Fecha**: 2026-10-15 · Corrida: `factory/themes-removal-74` (a crear) · Base: trunk.
**Issues**: #74 (leelo con gh) + #66 (requisitos: "exactly six selectable", migración segura, temas DATA no destruidos).

## Qué debe cerrar #74 (del epic y la deuda del estado)
1. **Remoción de TODOS los legados** del catálogo ROM: `IU1WXY`, `Classic`, `Outdoor`,
   `Outdoor-Negative`, `VISION` (builtins en `src/theme.c`) y los archivos `Holo.json`,
   `Material 3.json`, `RS.json` (de `rootfs/usr/share/x6200/themes/`).
   **Preguntar fijación**: si el selector six-only del #73 (mergeado) ya no los muestra,
   la ROM puede dejar de contenerlos: eliminar los builtins de `builtin_names[]`/`enum` de
   `theme.c` (cuidado: las posiciones del enum SON índices en código: **cuidar a los usos**
   de B_IU1WXY/B_* — el fallback default reference B_IU1WXY: migrar TODO el default a aurora,
   ver 2), y borrar los JSON del rootfs (los files).
   **Preservar**: temas DATA de `/mnt/themes` del usuario (el code solo de ROM/discovery;
   #66 exige "temas DATA no destruidos").
2. **Default = Aurora** + **migración del valor guardado**: params con `theme_name` legado
   (o "Classic"/dictado) → **Aurora**; además los defaults cuando no hay theme_name →
   Aurora (era IU1WXY — mirar `src/params/params.c` defaults y `theme.c` falla/`fallback`).
   La migración es la de la spec del epic: quitar carrera en default + migración segura del
   valor guardado (idempotente y comprensible con `params.db` migro de la casa: mirá de
   `src/params/migrations.c` por el patrón de migrations ya existentes (init/ft8/freq ATU).
3. **Contratos/fixture/goldens del remanente**: los 8 legados quitados ROM suman test counts
   `themes` ( haunting: 15 → 7 temas servecats! (6 v2 + ??? legados fuera). Los inventarios /
   fixtures `main-tema-outdoor-negative`, `main-tema-vision`, etc, del arnés quitzarnos del
   sweep/`fixtures` o convertirse en *dead (todos los contages recalibrados con las suite
   reales como siempre.
4. **UI-008/validación**: la fila de regressiones no-texto para los 6 (los ratios no cambian).
5. **Banco**: UNA captura del binario final con la colección final (default Aurora visible al
   boot con params LIMPIO). Léase el banco con su guard nuevo del #90 (`dirclean + rebuild`) —
   el banco del owner (con los params.db del owner nt) y el chroot .vfix3 READ-ONLY como siempre.
6. Docs de la casa: docs/THEMES.md la colección final (6 entradas; la legados fuera), AB_TESTING,
   y los docstrings/etc al estado final. Marked de migration en elsuite de narrativos.

## Gates (la salida real)
```bash
cd /home/pablo/desa/asf/state/worktrees/x6200/themes-removal-74
tests/check_harness_determinism.sh
cd tools/ab_testing
uv run x6200-ab compare --a origin/lu2dsx/x6200-rom --b HEAD --experiment-id AB-REMOVE74
uv run x6200-ab contracts --revision HEAD && uv run x6200-ab audit --revision HEAD --suite visual
uv run x6200-ab themes
uv run pytest -m "not e2e" -q && uv run pytest -m e2e -q
tests/native/run_native_tests.sh
estáticos 7/7 (8/8 si audita-pruebas)
+ banco session (UNA)
```
(Asumí que los contadores por legados rezaron; el chequeo determinista con tests reales calibra.)

## Invariantes
- NO borrar DATA del OWNER (los fixtures legacy de imagen sí pueden ir: el tema de las expreciones).
- NO tocar `lvgl/`, DSP/RF; NO commitear `.asf-active`.
- El epic cierra #: al final, verificar con gh que #66/#73/#74 quedan en el estado deseado.

## Reporte final
1. Lo eliminado (lista de confirmación: ROM ya no tiene las entradas legadas y el borrado
   JSON en rootfs).
2. La migración (diffs + tests de TDD rompedor default / params Migration al aurora).
3. El contado final de temas: `themes` → sólo 6+DATA (o a cero si las fixture legacy también
   se confieren).
4. Captura del banco con el default Aurora al boot (md5 del binario).
5. Gates complejos. PENDIENTES honestos (la revisión estética en la radio es la del owner con
   el epic PRUEBAS en #66).
