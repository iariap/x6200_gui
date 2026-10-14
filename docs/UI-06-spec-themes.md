# Spec: ui06-themes — issue #12, incremento 1: chequeo estático de contraste para TODOS los temas servidos

**Fecha**: 2026-10-14 · **Corrida**: `factory/ui06-themes` (placeholder; el nombre puede ser `ui06-t1`) · **Repo app**: `iariap/x6200_gui`
**Base**: tip post-#32 (con UI-014 mergeado) · **Issue**: #12 ([UI-06]; parent #3; depende de #10 y cobertura #7 — cumplidas)

## Por qué este incremento primero
El chequeo estático de pares texto/fondo (`x6200-ab themes`, `theme_pairs.yaml`, 27 pares de UI-03) hoy corre contra **3 files de tema** (los que tenían el schema esperado). Los demás temas env built (Classic/IU1WXY/Outdoor/Outdoor-Negative/VISION/…) o no están cubiertos o usan esquemas distintos. El issue #12 pide "consistent semantic colors, contrast, focus states, button/popup styles, iconography y waterfall palettes" — todo eso necesita primero el **inventario honesto**: qué tema tiene qué cobertura de chequeo y qué pares realmente miden.

## Alcance (incremento 1)
1. **Inventario real**: para cada JSON servido por `rootfs/usr/share/x6200/themes/` (y los built-in), mapear qué contiene (roles de color, nombre de claves) y qué coverages ofrece el chequeo actual.
2. **Extender el chequeo estático** (`theme_files.py`/`theme_pairs.yaml`) para abarcar todos los temas cuyo schema sea parseable hoy — **color roles con nuevas reglas si一直没有统**; rechazo/falta declarada para los que no sea (ej. binary/.bin-estés).
3. **Hallazgos reales del extendido**: los pares que fallen en cualquier tema → REPORTAR en issues (sin fundar sin fixture, según la regla del plan: fallo del chequeo estático que no esté reproducible en pantalla = hallazgo unverified con evidencia numérica Document). Prioridad para UI-010 (waterfall) si aparece en el extendido. No fixes en este incremento.
4. `docs/AB_TESTING.md`: tabla "pares estáticos × tema" y "UNCOVERED con motivo".

## Fuera de alcance (incrementos typi 2+ de #12)
Cambios visuales de temas (fixes de pares fallidos), focus states, waterfall palettes (requiere hardware/physical), JSON schema compatibility & /mnt/themes (requiere radio o banco con montaje dedicado).

## Criterios (finish condition)
```bash
cd tools/ab_testing && uv run x6200-ab themes        # N files / M pairs / k hallazgos, con tabla por tema
uv run x6200-ab contracts --revision HEAD            # PASS (pares solo informativos esta corrida: sin regla que bloquee de nada defecto nuevo sin fixture — declarar explícitamente cuáles quedan cómo)
uv run pytest -m "not e2e" -q
```
Nada del render cambia (no toca src/, fixtures, harness). Risks: LOW (framework Python docs).

## PENDIENTES previstas
Si el inventario muestra temas no-parseables, declararlos UNSUPPORTED con motivo (no forzar parser). El siguiente incremento de #12 decide de founded de los hallazgos y el waterfall/Focus states.
