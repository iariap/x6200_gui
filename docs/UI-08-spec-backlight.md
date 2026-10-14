# Spec: ui08-backlight — issue #14, incremento 3: límites de brillo por perfil (solo STATE caps + native)

**Fecha**: 2026-10-14 · **Corrida**: `factory/ui08-backlight` · **Repo app**: `iariap/x6200_gui`
**Base**: tip post-`e5f1010` (incr. 2 mergeado) · **Issue**: #14 ([UI-08]; parent #3)

## Outcome
El perfil de batería declara y aplica los **límites de brillo** por perfil (QUALITY/BALANCED/ECO/CRITICAL) en la capa de estado del backlight (`backlight.c` del ROM). En host y banco todo queda **pixel-identical**: el brillo del backlight NO cambia el framebuffer de LVGL (está bajo el render). La percepción física del brillo queda para la pasada física final del owner.

## Alcance
1. **Investigar** la forma en que `backlight.c` maneja su nivel actual (camp editor, params de brillo, presencia de PWM) — y declararlo con líneas/archivos antes de tocar nada.
2. **`src/visual_policy.c/h`**: gancho `visual_policy_brightness_apply(profile, user_setting)` — misme funcion que `visual_policy_waterfall_apply` en exp. 2:
   - `VP_ECO/VP_CRITICAL` limitan el brillo máximo por perfil (cap) — solo clamped a ≤ el setting del usuario;
   - `VP_QUALITY/BALANCED` → restauran el setting del usuario;
   - **el setting del usuario nunca se pisa en disco** (patrón waterfall: el perfil solo cambia runtime).
3. **Test nativo** de la tabla perfil→cap (como la ya usada para waterfall_mode en incr. 2).
4. **Fixture/banco**: capturas A/B serán **pixel-identical** — declararlo explícitamente en el PR ("el brillo no cambia el render: la percepción la prueba la física final"); el natural bench corre igual (sin cambios). La única evidencia funcional del wire en ARM: banco rebuild + smoke (md5 nuevo del binario).

## Fuera de alcance
- Parámetros de disco, settings UI, persistence (incr. 4, bloqueado por el fixture de Ajustes).
- Cualquier cambio a `params.c`, `main_screen.c`, `clock.c`, waterfall mode (ya readonly el toggle de Ajustes; se respeta), tokens, flex, `lvgl/`, workflows.
- Anímicamente: el nivel real de brillo percCada vez en el LCD/ PWM. Es hardware — queda para la física.

## Criterios
```bash
cd tools/ab_testing
uv run x6200-ab compare --a e5f1010 --b HEAD --experiment-id AB-UI08-T3    # 20/20 pixel-identical (esperable)
uv run x6200-ab contracts --revision HEAD                                  # PASS (200/0/0)
uv run pytest -m "not e2e" -q && uv run pytest -m e2e -q
tests/native/run_native_tests.sh                                           # aumenta el race de checks (tests del caps)
tests/check_harness_determinism.sh                                         # PASS 20×2
```
El banco QEMU post-merge corre esa drive (último test host, el bromo en ARM queda para tu pass física).

## Riesgos
- `backlight.c` es un caminante fácil caer en estado display; la wiring debe ser idempotente y ligeramente declarada (la report/policy es el dueño). Verificar que el brillo no cambia la respuesta del fichero para el modo ESCO (solo alcanzable EN cero/Escenario batería).
- Brillo físico:审批 en pasada física — prohibido declaración de energy/luminescence.
