# Spec: ui07-render — issue #13, incremento 1: perfil del renderer + primer candidato de invalidación parcial

**Fecha**: 2026-10-14 · **Corrida**: `factory/ui07-render` · **Repo app**: `iariap/x6200_gui`
**Base**: tip post-#33 (`d9c63b2`) · **Issue**: #13 ([UI-07]; depende de UI-01/02 — cumplidas)

## Outcome
Issue exige **profile primero** y optimizaciones **aisladas por experimento** con evidencia. Incremento 1: (a) perfil objetivo del costo actual con el bench existente, (b) el primer experimento si el perfil confirma un candidato de **invalidación parcial** en el camino espectro/cascada — la reducción de TRABAJO de render (área redibujada/flushes) que **no** cambia el PNG final es exactamente el tipo de mejora que este framework sí puede demostrar en host.

## Datos que ya tenemos (del bench BENCH-ci y sucesores)
`redrawn_area_mean = 384000` (= 800×480 entero) y `flushes_mean = 1.0` en TODOS los escenarios → cada frame se redibuja **full-screen**. Si viene de invalidaciones full-object en espectro/cascada (cada línea nueva invalida el widget completo), el área redibujada debería ser ~ancho×alturas parciales.

## Alcance
1. **Perfil** (con el harness, sin tocar `src/` de la ROM): tablas por escenario de `redrawn_area/unique_redrawn/flushes/frames` + lectura del código `spectrum.c`/`waterfall.c`/`main_screen.c` donde se invalida (`lv_obj_invalidate`, timers). Hipótesis por candidato con el Gancho que el bench probaría.
2. **Experimento (solo si el perfil lo confirma)**: invalidación **parcial** en la presentación de espectro/cascada (no DSP/no datos): mismo PNG final byte-idéntico, `redrawn_area_mean` ↓ (objetivo documentado en el PR con el número), flushes coherentes. **Gates**:
   - `compare` pixel-identical (16→18 fixtures, A/B byte-identical) — el render NO cambia ni un px.
   - `benchmark`: `flushes/área` delta distinto de 0 con **dirección de mejora declarada antes** (anti-cherry-pick: el objetivo se escribe en el PR antes de correr); la dispersión del p95 host no decide (las métricas de trabajo sí).
   - contracts/audit (1382+ objetos → +0; paths structures) · determinismo 18×2 · pytest/e2e/native/ estáticos · ruff/ty.
   - UI-001 detector `region_content_above`/UI-010 `region_min_content:*` intactos.
3. Si el perfil NO confirma un candidato parcial (ej. el full-frame viene de la naturaleza del tema/VISION de `lv_obj_set_style_*` global), **terminá el incremento con las tablas + la lista de hipótesis y las razones** por qué no hay ganancia host-medible, sin tocar `src/`.

## Fuera de alcance (incrementos siguientes de #13)
`lv_tick_inc` 5 ms / monotonic / wakeups (host no lo mide; el consumidor real es el banco o el radio) — espec del hardware-pass; `lv_conf.h` (color depth/buffers) — experimento aparte; el blank/off path — sin fixture; cualquier DSP/RF (prohibido).

## Riesgos
- `spectrum/waterfall` son las áreas PROHIBIDAS de #10 pero **para la presentación en sí** están incluidos por #13 ("optimize spectrum/waterfall **presentation** cost") — tocar la FORMA de dibujar sí; tocar datos/DSP/subóptimo no.
- Bench host es x86: un win del ARM (framebuffer parcial, color conversions) no se ve igual en host → separar claim host vs ARM explícito.
- `_inject_structural_defects` inmoviliza `src/info.c`; verificar intacto.

## Finish condition
Perfil commiteado (`docs/AB_TESTING.md` o report en `reports/") + SI HUBO candidato: A/B con PNG byte-identical y trabajo de render reducido con el número declarado. NOTA: Hardware/Energy BLOCKED siempre.
