# Spec: ui08-policy — issue #14, incremento 1: capa de política de batería (pura, sin render)

**Fecha**: 2026-10-14 · **Corrida**: `factory/ui08-policy` · **Repo app**: `iariap/x6200_gui`
**Base**: tip post-#35 (con el modo bench natural mergeado — la métrica host que usará el wiring del incr. 2) · **Issue**: #14 ([UI-08]; depende de UI-07/UI-01 — cumplidas)

## Outcome
Primera pieza del manager: la **capa de política** como módulo propio, is puntos isolat y testeable en nativo, **sin tocar aún el render** (el wiring waterfall/backlight son los incrementos 2/3). El issue pide UI-only + hysteresis + stale-data fallback + user override + **never alter radio/DSP/RF power logic** — exactamente el borde de este incremento.

## Alcance (incremento 1)
1. **`src/visual_policy.c/h`** (nuevo):
   - enum: `VP_QUALITY, VP_BALANCED, VP_ECO, VP_CRITICAL`, y `VP_AUTO` (modo de usuario vs estado real), roles tipados (no ints mágicos).
   - API pura: `visual_policy_input()` (leibniz: `vbat_mv`, `vext`, `batcap`, `charging`, `now_ms`), y una función `visual_policy_eval()` → (perfil, transición con **hysteresis** en los umbrales, `stale_ms` LEFT recordar-safe).
   - **hysteresis real**: cada perfil tiene banda de entrada/salida (salida ECO es más exigente que entrada; misma matemática invertida para CRITICAL), sin flaps.
   - **stale-data fallback**: si `now_ms - last_bat_ms > STALE_MS` → perfil conservador declarado (sin moss la señal; acción solo para el wiring futuro) + flag de estado visible: (para que incr. 2 lo muestre).
   - **fallback de lectura inválida** (vbat fuera de rango/charging en imposible): se descarta la muestra, no la policy.
   - **user override**: `VP_MANUAL(<profile>)` fuerza el perfil y congela el AUTO (hysteresis segura, configurable por usuario).
   - Tabla de umbrales NOMBRADA y por defecto (mV sueltas, separadas para load/chargé — no literal).
2. **Test nativo**: `tests/native/test_visual_policy.c` — un archivo:
   - matriz de AUTO (entrada/salida de ECO y CRITICAL con hysteresis exacta);
   - changé el perfil inmediato (`vext`/`charging` gana);
   - stale → estado conderset safe + resume al volver datos válidos;
   - override manual que congela AUTO;
   - lecturas inválidas descartadas.
   Estas pruebas van integradas en `run_native_tests.sh` (patrón `test_clock.c`).
3. **NO tocar** render/renderizado, `spectrum`, `waterfall`, `backlight.c`, `params.c` (ningún param nuevo en este incr.), ni timer. El módulo no se llama desde el flujo aún (declara la API con el patrón, NO wiring). Cero render-efecto: los 19 fixtures quedan BYTE-IDÉNTICOS (el módulo no está wired) — determinismo + `compare`igual a base lo prueban.

## Exclusiones (para los siguientes incrementos)
- Incremento 2: wiring de waterfall (`waterfall_set_battery_mode()` y `params.waterfall_battery` ya existen) + spectrum cadencia; clase vista con el **natural bench** (evidencia de lo ahorrado por perfil).
- Incremento 3: backlight/brillo seguro; increment 4: settings UI + persistencia (requiere fixture de Ajustes= UNSUPPORTED) — depende del dial/Settings.
- Si tocar `src/` para este módulo rompe müllu (`_inject_structural_defects`), sige el pattern.

## Risk/finish condition
- `src/` cero render cambio garantizado por electrónicas: `git diff base..HEAD --^src` y los 19 fixtures hash idénticos.
- Gym: `run_native_tests.sh` incluye el test nuevo **y todo el set existente**, `pytest -m "not e2e"`/e2e/native/estáticas/ruff/ty limpios.
- Cero `expected_change`, cero contratos nuevos.
