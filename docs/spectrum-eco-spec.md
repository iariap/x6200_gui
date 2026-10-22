# Spec: spectrum-eco-13 — UI-07 remainder: cadencia del espectro + wakeups del tick

**Fecha**: 2026-10-15 · **Corrida**: `factory/spectrum-eco-13` · **Base**: trunk `ec89fc3`
**Issue**: #13 (`[UI-07] Optimize software renderer, waterfall and wakeups`) — lo entregado
vive en los PRs de la cadena (bench modo natural c65d204, waterfall ECO −48.5% px e5f1010,
2 timers de 2 s descubiertos). Este run cubre el **remanente declarado**:

1. **Cegar el wake-up del tick si es abordable** (o PENDIENTE con medición de cuánto
   despierta en el banco y cuánto costaría).
2. **Pantalla apagada no trabaja de más** (comprobable en el banco: la ruta de
   backlight apagado ya puede saltarse renders; medí y si falta, arreglá).
3. Si (1) y/o (2) resultan fáciles y medibles, también es bienvenida la
   **GDP del espectro** (el header `visual_policy.h`; sin diseño nuevo: NO lo conecto).

## Nota honesta del-origin
El header de visual_policy.h declara ("El del spectrum sigue pendiente") y el incr.5 de
UI-08 lo marcó PENDIENTE con análisis (dibujo del espectro es por EVENTO, sin contador de
frames ni setter de cadencia; conectarlo pide diseño nuevo). Queda pendiente y si el
análisis mueve el marco (fuera del trabajo pequeño), declarar PENDIENTE e issue propio.

## Contexto de los specs previos (leer)
- `docs/AB_TESTING.md` (secciones UI-07/UI-08: el modo natural y su anti-falso-verde).
- `src/visual_policy.h/.c` (perfil ECO: waterfall cadencia ya salvada).
- `src/backlight.c/.h` (`backlight_set_brightness`/switch/offline gates).
- `tests/native/run_native_tests.sh` (stubs del backlight).

## Gates (salida real, los mismos de siempre)
```bash
tests/check_harness_determinism.sh          # fixtures al día (33)
cd tools/ab_testing
uv run x6200-ab compare --a ec89fc3 --b HEAD --experiment-id AB-ECO13   # PASS / diff por regiones si mueve
uv run x6200-ab contracts --revision HEAD && uv run x6200-ab audit --revision HEAD --suite visual
uv run pytest -m "not e2e" -q && uv run pytest -m "e2e" -q                # 3× si moviste performance.py
tests/native/run_native_tests.sh
estáticos 6/6
```

## Fuera de alcance
- DSP/RF/TX/audio; `lvgl/` pinned `74d0a816` (el tick interno de LVGL NO se toca; si
  `lv_tick_inc` lo ajeno, medir y recomendar, no reescribir); tokens/lexicos ya cortadas.

## Invariantes
- Commits `Pablo Iaria <iariap@gmail.com>`. **Sin push/PR** (publica el orquestador).
- Cada pieza con medición o PENDIENTE fundado — nada teórico.
- El bench natural es la vara: una pieza que no aparezca en el bench queda "sin evidencia
  de reducción de trabajo de render" (el banco mide px exactos por corrida).

## Reporte final
1. Por pieza (wakeups/blanked/spectrum): qué se midió (números), qué se arregló, qué queda.
2. El efecto en el bench natural (si aplica) y si el delta ES reproducible (campaña x3).
3. Gates con salidas reales.
4. PENDIENTES honestos (diseños que quedarian para el siguiente run).
