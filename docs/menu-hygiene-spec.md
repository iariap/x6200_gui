# Spec: menu-hygiene — `app:off` cierra RTTY + volver al home tras cerrar diálogo + borrar freq_strip

**Fecha**: 2026-10-15 · **Corrida**: `factory/menu-hygiene` · **Base**: trunk `baf6164`
**Orden del owner**: decisiones 5, 6 y 7 del cuadro de producto (2026-10-15).

## Tarea 1 — `{"cmd":"app","text":"off"}` cierra RTTY (5: "implementar")
- **Contexto**: el `current_app()` del #50 ya nombra a RTTY, pero `off` no la toca: RTTY no es
  un diálogo (`main_screen_close_app()` → `dialog_destruct()` no aplica). Hoy abrir la
  siguiente app la mata vía `apps_disable()` (efecto colateral, no camino del "off").
- **El fix**: extender el cierre para las apps **no-diálogo** que creamos en pantalla.
  Guardar la página de botones que estaba **antes** de abrir la app
  (`buttons_load_page` guarda su estado en algúibles — mirá cómo #50 documentó
  `prev_page`) para volverla al cerrar.

## Tarea 2 — cerrar un diálogo vuelve al home (6: "ir al home")
- **Repro documentado** (#50): abrir RTTY y luego un diálogo → al cerrar, la barra queda
  en la página de RTTY (`buttons_load_page(prev_page)`), no en el home.
- **El fix**: el `close_app` restaura la página GPS/home (`page 0` del grupo `gen`? — el
  home distinguido por `manual` — decidí: el estado pre-apertura, no una página fijada),
  con el mismo caso de "cierre desde el hilo" que ya seguriste.
- **Cobertura**: fixture/sen the settle assertive (por `{"cmd":"get"}` + captura) del
  estado del home tras los cierres.

## Tarea 3 — eliminar `freq_strip.c` (7: "eliminar")
- Declarado latente desde UI-05: `freq_strip_init()` sin callers, sin fixture.
- Borrar `src/freq_strip.c/.h` (o el incorporado del build), limpiar referencias en
  build/`CMakeLists` y cualquier uso de arnés/ab_testing (grep `freq_strip` tras el
  borrado debe dar 0 hits de código).

## Banco y evidencia
- El banco está LIBRE — usalo para las validaciones del RTTY (`remote off` → `app=''` +
  captura del home; la imagen `x6200-uiharness-full` a mano si hace falta).
- `docs/*`históricas no reescribir: agrega nota fechada si el manual nomina el camino.

## Gates (salida real)
```bash
tests/check_harness_determinism.sh           # 33 x2 (hashes intactos, o diff declaredó por freq_strip)
cd tools/ab_testing
uv run x6200-ab compare --a baf6164 --b HEAD --experiment-id AB-HYGIENE  # PASS
uv run x6200-ab contracts --revision HEAD && uv run x6200-ab audit --revision HEAD --suite visual
uv run pytest -m "not e2e" -q && uv run pytest -m e2e -q
tests/native/run_native_tests.sh
estáticos 6/6
uv run x6200-ab themes                        # wcag no-transist (no debería cambiar)
```

## Fuera de alcance
- audio (consolidado), visual_policy/ECO (va en otra rama), fuentes libres, spectrum.
- `lvgl/` pinned `74d0a816` — NO TOCAR. DSP/RF/TX.

## Invariantes
- Commits `Pablo Iaria <iariap@gmail.com>`. **Sin push/PR** (publica el orquestador).
- PENDIENTE fundado > supuesto. If el banco libera el contenedor al cierre.

## Reporte final
1. Tareas 1-3, con repro/fix/test por cada una y el banco del RTTY (antes/después).
2. El home tras cerrar (fixture o banco — con imágenes).
3. Grep final de `freq_strip` (0 hits de código).
4. Gates con salidas reales.
5. PENDIENTES honestos.
