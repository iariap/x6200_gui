# Spec: fonts-libre — migración a fuentes libres (decisión 1) + natural a 50 ms (decisión 8)

**Fecha**: 2026-10-15 · **Corrida**: `factory/fonts-libre` · **Base**: trunk `02c1311`
**Orden del owner**: "migrar a fuentes libres" con **más opciones** (la matriz de candidatos)
y "alinear" el natural a 50 ms (8) en la misma re-basación. Trails: issue #9, epic #3.

## Qué
1. **Pipeline de conversión de fuentes**: tomar cada candidata y producirla en el formato
   que hoy usa el ROM (las `sony_*` del repo — mirá la herramienta que las generó/build del
   arnés; es probable que sea la pipeline de fuentes de LVGL o el tool del repo). Cada
   candidata, misma configuración (los 10 tamaños/estilos que hoy existen como familia).
2. **Evaluación medición**: por candidata:
   - Cobertura de glifos con el detector del inspector (`missing_glyph` — el defecto conocido
     era `U+F293` de BT; el símbolo ahora es `lv_line`, pero el detector da la métrica real).
   - Delta de caja de texto por fixture con `--inspect` (→ regiones que esperamos).
   - Legibilidad a tamaño real (la lista de la 480×800 con `sony_20` del corpus actual).
   - PNG de galería (cada candidato al mismo render) para el owner.
3. **Default por score** (re-optimo): la usada por defecto del ROM aparte de su score;
   la decisión del owner es un **flip de un valor** (param de tema / fuente por defecto). Elegí
   la candidata con métricas más cercanas a la “Sony” actual (uso distintas) — Menciona tu
   puntuación; la que ganó; qué pruebas darían los fotos.
4. **Align del natural bench a 50 ms** (decisión 8): el `--bench-natural` corre a 30 ms; la ROM
   usa 50. Alineado de la misma pieza que ya definimos (`LV_DISP_DEF_REFR_PERIOD` de ROM),
   re-basando el corpus del arnés/ab datos (los números de la corrida cambian a medida).
5. **THIRD_PARTY**: `docs/THIRD_PARTY.md` con licencia/fuente/URL de cada font incluida,
   y el commit sigue la convención del repositorio. Todas las fonts están con
   licencia eniringing (OFL/Apache). Guardada los vestigios de rastro legal.

## Candidatos (la matriz)
Inter, Roboto, Roboto Condensed, Noto Sans, IBM Plex Sans, Fira Sans, Open Sans,
Source Sans Pro, PT Sans, Lato, DejaVu Sans, Archivo, Archivo Narrow, Ubuntu,
JetBrains Mono (sólo el panel de frecuencia si da la métrica).
**PREFERÍANCE**: los que tienen métrica son tus candidatos; **evaluá TODOS los
posibles** (si una font se queda un análisis para arriba (extent of evidence
a lo "reemplazable en esto, el resto", NO: documentá la curva del arte). El default
stock slide elegido por el owner (decisión 1 ya dijo "migrar"), y el resto queda
**ropdown/seleccionable** (tema default? fonts del tema? decidí el mecanismo minimal).

## La re-basificación (el "corpus" completo)
- Los fixtures/contracts/goldens: cada fuente cambia las cajas → las regiones de la casa
  repetidas `typed` ranges se **declarar las regiones expected_changes scoped** con
  razón+autor para CADA cambio (mechanism de la casa; esto es el volumen real de este PR).
- El naturaleza-juggling del corpus: el bench/estado píxeles van a los números de la
  fuente nueva (con su región).
- Contratos: los `typography_*` se recompilan aquí por cada fuente del `main-tema-*`
  y de las fijas del corpus — si hay incumbimiento de contraste del pacheco,
  declaró con el by-hand de #44 (región + cambio de valor ANTES/después).

## Gates (salida real)
```bash
tests/check_harness_determinism.sh            # fixture hashes (regiones esperadas)
cd tools/ab_testing
uv run x6200-ab compare --a 02c1311 --b HEAD --experiment-id AB-FONTS
uv run x6200-ab contracts --revision HEAD && uv run x6200-ab audit --revision HEAD --suite visual
uv run x6200-ab themes                        # wcag 9 esquemas, pares recomprüngados por la fuente
uv run pytest -m "not e2e" -q && uv run pytest -m e2e -q
tests/native/run_native_tests.sh
estáticos 7/7 (check_manual_apps ajusta si cambian las secciones)
ruff/format/ty limpios
# + el delta x3 del bench (los #3 si cambian el timing del arnés)
# + la TXT de la galería con todas las candidatas en el PR
```

## Fuera de alcance
- El tema RF/DSP/audio; `lvgl/` pinned (la OPTIMIZATION de las fonts es del proyecto).
- El vuelco físico del ton/legibilidad real en la radio es #15 (la misma coisa que el arnés
  ya tiene de la casa... el owner decide la definitiva con la galería en el PR).
- La UI-015 (pantalla no táctil). El resto del menú.

## Invariantes
- Commits `Pablo Iaria <iariap@gmail.com>`. **Sin push/PR** (publica el orquestador).
- El corpus no se toca a-mano (toda re-basación documentada y mecánica de la casa).
- PENDIENTE fundado > supuesto; no fusiones workarounds no-determinísticos en CI.

## Reporte final
1. La pipeline de conversión (el script del repo) y su salida por candidata.
2. La matriz evaluada: cobertura/deltas/legibilidad/tabla y PNGs de galería.
3. El default propuesto por score con su justificación (y el flip del owner está listo).
4. La align 50 ms (su medición del bench después y los cambios arriba).
5. Gates completos con salida real (el re-basación de las regiones es documentada).
6. PENDIENTES honestos.
