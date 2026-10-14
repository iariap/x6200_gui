# Spec: ui07-bench-natural — issue #13, incremento 2: modo bench de invalidación natural + datos por frame

**Fecha**: 2026-10-14 · **Corrida**: `factory/ui07-bench-natural` · **Repo app**: `iariap/x6200_gui`
**Base**: tip post-#34 (perfil UI-07 incr. 1 mergeado) · **Issue**: #13 ([UI-07])

## Por qué (outcome medido en incr. 1)
El bench actual **fabrica** el trabajo (`lv_obj_invalidate(lv_scr_act())`+`lv_refr_now()` por pase — `harness_main.c:312`) y los fixtures alimentan los datos **una vez** (`screens.c:54`): imposible medir el trabajo de render natural de espectro/cascada (el 85% del total — perfil H1+H2). El perfil lo declaró como el camino viable.

## Alcance (arnés host; `src/` de la ROM NO toca)
1. **Modo bench natural** (`RECONSTRUIR`-style flag del CLI, p.ej. `--bench-natural N`): corre el **loop natural de LVGL** (timers reales del main_screen: espectro cada `spectrum_fps_ms`, cascada por fila, reloj 500ms) durante N frames de tiempo virtual, y reporta por frame: área invalidada natural, buffer intermedio del flush (1:1), `flushe_i`. **La métrica ya existente (`metrics`) se mantiene intacta** para el modo forzado: el natural es un modo separado y adicional (compatibilidad total del formato).
2. **Datos por frame deterministas**: la señal drive del espectro/cascada usa el generador determinista que ya existe (`src/signal_gen.c` en el arnés) — cada frame/columna nueva con seed fijo ⇒ el flujo NATURAL del arnés es reproducible (misma sumas binarias en dos corridas), y el agua avanza filas naturalmente.
3. **Un fixture dedicado** (o los que existan SIN tocar los 18 del comparador): los fixtures actuales del `compare` **no cambian** (cero impacto en la cadena A/B gates). El natural-mode corre en fixtures nuevas o standalone con los datos por frame activos.
4. **Protocolo de medición y reporte**: por-frame flush cycles/arm/area en JSON + protocolo en el README; el gate `check_harness_determinism.sh` (18×2 existente) **sin cambios**.

## Exclusiones (que no rompan el +cero-diff garantizado de la cadena A/B)
- Todo modo natural debe ser **extra**, no reemplazo de los gates existentes (A/B compare contracts/determinism sobre las 18 fixtures intactas: PNG byte-by-byte igual en A=B).
- Determinismo 18×2 con cero cambios de hash.

## Gates (finish condition)
```bash
cd tools/ab_testing
uv run pytest -m "not e2e" -q                   # includes self-tests de natural (uso los nuevos tests de comparación)
tests/check_harness_determinism.sh               # 18×2 PASS: hashes EXACTOS a tip base
uv run x6200-ab compare --a <base> --b HEAD --experiment-id AB-UI07-T2   # 16-18/18 pixel-identical (fixtures intactas)
```
**Sensibilidad del NATURAL MODE** (protagonista, es el produto): el modo natural debe reportar **las áreas naturales** ( checkpoints esperados del perfil: espectro ≈132k/frame sin faltar cascada ~196k/fila, reloj 15.8/k/500ms; los números deben caer cerca de los del perfil (±measurables). Self-test 1: dos corridas = same JSON (determinismo del NATURAL). Self-test 2: calibration — natural mode area total < fullscreen sum (matching perfil de trace).

## Out of scope
* apps del IRNO en main_screen (radio D/S code), lv_conf.h, cambios de src/, DSP logic, TF7/tick cadence (el tema del chip #15)
* Reglas del A/B gate pipeline (solo bench CLI extension)

## Risks
* Toca harness C (tools/ui_harness/), model risk medium: 2º nivel de display driver (flush counting dispone full-screen buffer 1:1) — natural flush model reuse the instrumentation design temporal ya probada en incr. 1 (display.c contadores temporales que se revertieron).
* e2e/defense: si un dataa fixture renderiza diferente (cada frame los datos cambian) — solo nuevo natural-mode fixture. Los fixture existentes no cambian.
