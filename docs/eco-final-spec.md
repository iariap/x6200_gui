# Spec: eco-final — decisiones del owner 2/3/4 (tick sin hilo + spectrum ECO + cascada offscreen)

**Fecha**: 2026-10-15 · **Corrida**: `factory/eco-final` · **Base**: trunk `baf6164`
**Orden del owner**: "2: aplicarlo. 3: aplicarlo. 4: arreglarlo" (sobre el cuadro de producto).

## Pieza 1 — LV_TICK_CUSTOM: el hilo del tick desaparece (decisión 2: "aplicarlo")
- **La medición de #56**: tick-thread 200 wakeups/s (5 ms, `src/main.c`) = **93.5%** de los
  wakeups. Recomendación fundada ya publicada: `LV_TICK_CUSTOM 1` con `CLOCK_MONOTONIC`.
- **La macro**: `lv_conf.h` del PROYECTO (no de `lvgl/`) — `LV_TICK_CUSTOM` con
  `LV_TICK_CUSTOM_INCLUDE` + expresión de milisegundos monotónicos (`clock_gettime`),
  y el **hilo que hoy hace `usleep(N)*1000 + lv_tick_inc(N)` sale de `src/main.c`**.
- **Cobertura del bench**: `check_harness_wakeups.sh` tiene que seguir leyendo de fuentes:
  con `LV_TICK_CUSTOM=1` el gate reporta **0 wakeups del thread** (no existe) y exige el
  `_tick_custom` coherente; sin la macro el gate sigue validando el hilo. Añadí':
  el modo `--bench-wakeups` del arnés debe reflejar el modo (0 wakeups de tick al host,
  que ya era Clock-time).
- **Nativos**: el stub del tiempo (si lo toca) queda en la misma armonización.
- **Invariantes de la decisión**: la resolución del tick queda igual en LVGL (1 ms con
  monotonic) — sin cambios semánticos en la API del usuario (el volumen/latency).

## Pieza 2 — spectrum ECO: cadencia con borde seguro (decisión 3: "aplicarlo")
- **El diseño de 5 líneas** publicado en `docs/AB_TESTING.md` (remanente del incr. #56):
  - `spectrum_set_refresh_period(frames)` + un contador de frames en la ruta del
    re-dibujo (los frames no son "reales": mandá los eventos reales del draw-graph);
  - skip del re-dibujo COMPLETO del espectro cuando el frame no es due (perfil ECO/CRITICAL);
    QUALITY/BALANCED siempre dibujan (parametrizado por wiring de `visual_policy`:
    `visual_policy_spectrum_apply()` igual que el waterfall);
  - **Motherboard del borde de bajada**: el último skip NINGUUN frame en deuda — la
    pasada `frames_due` del perfil ya garantiza que el último de la sesión dibuja completo.
- **Medición**: los valores px y delta del natural bench forzando ECO (o el modo del arnés
  si el driver al perfil del arnés tiene que crecer); las regiones son las de la casa
  (`spectrum`/`espectro`) si el diff pinta algo con expected_changes scoped y razon.

## Pieza 3 — redibujo offscreen de la cascada con display apagado (decisión 4: "arreglarlo")
- **El hallazgo de #56**: con display off, `refresh_waterfall()` hace `redraw_cb(NULL)`
  (los ~196000 px de paleta) **antes** de invalidar; la invalidación disable no evita
  el redibujo off-screen. El espectro no lo hace.
- **El fix**: contador del redibujo offscreen (instrumento nativo) para el test, y el
  corte del redibujo del waterfall (saltarse el cómputo de la paleta) mientras el display
  no está invalidando (chequeo del estado disponible en el módulo, sin tocar `lvgl/`).
- **La medición**: `--display-off` del arnés: los px de redibujo offscreen en 0 (o la
  reducción) y el test nativo que fija el mismo comportamiento con la misma API
  que el switch del backlight.

## Gates (salida real)
```bash
tests/check_harness_determinism.sh           # 33 x2 — o region/expected si el natural mueve
tests/check_harness_natural.sh && tests/check_harness_wakeups.sh   # actualizados
cd tools/ab_testing
uv run x6200-ab compare --a baf6164 --b HEAD --experiment-id AB-ECOFINAL  # PASS o diff scoped
uv run x6200-ab contracts --revision HEAD && uv run x6200-ab audit --revision HEAD --suite visual
uv run pytest -m "not e2e" -q && uv run pytest -m e2e -q    # 3x si tocás performance.py
tests/native/run_native_tests.sh   # + los sweeps nuevos (tick/ECO/contador)
estáticos 6/6
ruff/format/ty = limpios
```

## Fuera de alcance
- `lvgl/` Pinned (adentro); audio, fuentes libres (otro run), radio.
- No cambiar los umbrales/semántica del perfil manual/auto (lo haga ya con lo entregado).

## Invariantes
- Commits `Pablo Iaria <iariap@gmail.com>`. **Sin push/PR** (publica el orquestador).
- El bloque de render y px se declara con el bench: si el delta no se reproduce x3, no se consume.
  Todo cambio con expected_changes scoped + razón + autor (mecanismo de la casa).

## Reporte final
1. Pieza 1: la macro del tick + el gate con 0 wakeups; el delta medido en el arnés.
2. Pieza 2: el contador/setter + el wiring del perfil + Δ px del bench ECO (o natural) x3.
3. Pieza 3: el contador del redibujo offscreen + el corte con los px o test config.
4. Gates completos con salida real.
5. PENDIENTES honestos.