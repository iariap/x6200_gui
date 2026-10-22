# Spec: ui08-enforce — issue #14, incremento 4: enforcement persistente del cap (backlight_tick/timer por la política)

**Fecha**: 2026-10-14 · **Corrida**: `factory/ui08-enforce` · **Base**: tip post-`4c711f6` (incr. 3 mergeado)
**Issue**: #14 ([UI-08]; parent #3) · **Última corrida de código del manager**

## Outcome
Cerrar el gap que el incr. 3 dejó declarado: en la radio real, `backlight_tick()` (disparado por input: `encoder.c`, `hkey.c`, `keypad.c`, `rotary.c`, `dialog_settings.c`) y `backlight_timer()` (timeout idle) reescriben el nivel **desde params** y **saltan el cap del perfil** → un ECO real recorta hasta el próximo input/idle. Que el cap sea **persistente** en las mismas cadencias: los call-sites de backlight pasan por el cap en el mismo módulo.

## Alcance (mínimo, reversible)
1. `src/backlight.c`: los rewrites en `backlight_tick()` y `backlight_timer()` pasan por el cap — la clave es `visual_policy_cap(profile, setting)` (query, sin bajar el locking al sector): `min(user_setting, cap)` estable.
2. **El setting del usuario sigue intacto en disco** y la política solo clampa el **runtime level** — mismo patrón del waterfall (el param de disco queda del user).
3. **Test nativo** de la ruta (stub del backlight registra la salida en paths tick/timer, con el mismo corpus público del incr. 3): el cap **recorta al tick/timer**, y con QUALITY/BALANCED y/o external el setting del usuario queda sin tocar (same apply).
4. Cero render: el A/B quedará pixel-identical (igual que incr. 3); el banco reporta el mismo `check_harness_natural.sh` + el rebuild/smoke de siempre.

## Exclusiones
- El brillo físico (PWM real, percepción) → pasada física del owner.
- Correlato de settings UI + persistencia → pendiente del fixture faltante de Ajustes (publicado en la discusión de #14).
- Otros consumos del runtime (`visual_policy_waterfall_apply`, etc.) ya a TOGO.

## Criterios (finish condition)
```bash
tests/native/run_native_tests.sh               # suma checks (test de tick-through-policy)
tests/check_harness_determinism.sh             # 20x2 PASS (PNG idénticos)
cd tools/ab_testing
uv run x6200-ab compare --a <base> --b HEAD --experiment-id AB-UI08-T4    # 20/20 pixel-identical (declarado)
uv run x6200-ab contracts + audit + benchmark (delta 0)                    # PASS
uv run pytest -m "not e2e" -q && uv run pytest -m e2e -q && estáticos/ruff/ty
```
Sensibilidad: cap de debug en el test del tick que dispara el clamp y revert exacto (documentado en el PR).
