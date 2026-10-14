# Spec: wcag-full — corrida: cumplimiento WCAG verificable de TODOS los temas (y hallazgos reales)

**Fecha**: 2026-10-15 · **Corrida**: `factory/wcag-full` · **Repo app**: `iariap/x6200_gui`
**Base**: tip `0684861` (post-Bands) · **Origen**: orden del owner: **todos los temas deben ser WCAG compliant** (issue #12)
**Referencias**: WCAG 2.1: 1.4.3 contraste mínimo (texto ≥4.5:1, texto-grande ≥3:1), 1.4.11 contraste no-texto (≥3:1 para objetos gráficos requeridos para entender contenido); excepciones *disabled/incidental* justificadas in situ.

## Estado hoy (evidencia de la cadena)
- `x6200-ab themes` (framework): 7 temas × 9 pares = **63 PASS**; **VISION/Outdoor-Negative UNCOVERED** (runtime-derived, decisión D1 de la corrida previa); RS partial (`binary`). Sweep del ROM `check_wcag.py`: 990 OK / 0 fallas (9 paletas, con exentos declarados).
- Fixtures renderizando los 9 temas (22 en total, deterministas, commiteadas).

## Alcance (todo lo legal para decir "WCAG compliant" con evidencia)
1. **Cobertura 9/9 en el framework**: portar la derivación `use_builtin()` (`negative()` sobre Outdoor y `vision_colors[]` sobre IU1WXY, `src/theme.c`) al chequeo estático del framework (`theme_files.py`) — hoy UNCOVERED por decisión; el owner ordenó 9/9, y la fuente C única sigue siendo `theme.c` (el port TESTEA contra ella con un self-test de paridad). Retirar la sección UNSUPPORTED y declarar 9×9 = 81 pares.
2. **Contraste no-texto en el render (1.4.11)**: chequeo **pixel-based** sobre las fixtures commiteadas: para cada tema, validar ≥3:1 en los objetos gráficos informativos con regiones FIJAS declaradas (mismas que el `layout_checks`/contratos `region_*` ya usan): (a) waterfall información (columna activa vs fondo del pané), (b) S-meter peak vs fondo, (c) foco visible (+1 outline) en `main-foco`, (d) marcas CW/DIG del strip. Declarar el algoritmo (percentilesColor de la región vs fondo, no adivina宁夏) con self-tests.
3. **RS binary**: los PNGs de la barra/btn/strip **son fondo bajo texto**: medir contraste del TEXTO dibujado (color del rol) sobre los píxels reales del PNG en la render del fixture (`main-tema-rs`). Si el PNG no es parseable en claro (565 BMP), hacerlo desde el render (los píxeles del fixture son fuente).
4. **Hallazgos y fixes**:
   - Cualquier par/objeto que **falle** el 4.5:1/3:1 se registra con fixture/detector.
   - **Los fixes concretos se hacen EN EL MISMO CAMBIO** cuando el alcance sea del `src/theme.c` / JSON del tema (roles, claves) — si el fix choca con el waterfall (dato del DSP), queda como hallazgo con waiver-mock (no fix in data, cuantizado después).
   - Los exentos POR WCAG (disabled/incidental) quedan listados con su razón legal (1.4.3). Si owner quiere stricto coppia, se listan igual.
5. **Reporte**: `x6200-ab themes` extendido (o `x6200-ab wcag`) con la salida **per tema: [pares texto, no-texto, exentos, hallazgos, cobertura]** — tabla por tema en `docs/AB_TESTING.md`; summary en la PR. El framework queda midiendo 9/9 temas y los pares/no-texto en cada corrida (gate: los pares FAIL ROM no pasan, los de valor hallazgo not-infrequente no bloquean).

## Fuera de alcance / declarado
- El **waterfall del DSP en vivo** (paleta político del RADIO/Here: el color del bin de la señal NO ES EL TEMA, es la paleta del runtime que lo dibuja el firmware): medir el bench del subject/color de fondo no sería disability; si el audit del waterfy focus en `main-espectro-agua` FAILea, el fix queda como hallazgo (no tocar el DSP).
- Cuánto es strictly "disabled": caja por pair; no hay silencio.

## Criterios (finish condition)
```bash
cd tools/ab_testing
uv run x6200-ab themes          # "9/9 themes × 9/9 pairs texto + WCAG-no-texto per tema, hallazgos k"
uv run pytest -m "not e2e" -q   # tests nuevos del derivado port + no-texto + self-tests
tests/check_harness_determinism.sh   # PASS 22×2 (0 cambio de render SI no hubi fixes visuales; los fixes declaran expected_change)
uv run x6200-ab compare --a 0684861 --b HEAD  # 22/22 identical SI sin fixes; con fixes: diffs declar Scoped por caso
uv run x6200-ab contracts --revision HEAD, audit, benchmark, pytest -m e2e, native, estáticos
```

## Riesgos
- El port de `VISION colors` debe **correlacionar byte** con `check_wcag.py` (misma fuente C) — self-test.
- RS `binary`: si el PNG no es standalone parseable en el fixture, declarar el límite déclaré y hacer el fix del color del rol (walk a través del pixel base del render) con self-test.
- Los fixes del waterfall: si hay hallazgo real in el look, no tocar el DSP: el fix es del tema/ivol color roles o documentar como pending del基调 para la física.
- No inflar nada: hallazgo con fixture + detector; el resto informativo.
