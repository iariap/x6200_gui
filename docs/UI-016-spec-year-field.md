# Spec: ui016-year-field — fix del recorte del año en Ajustes (4 px)

**Fecha**: 2026-10-15 · **Corrida**: `factory/ui016-year-field` · **Repo app**: `iariap/x6200_gui`
**Base**: tip `ccfe1f5` (con la fixture de Ajustes `settings-home.scn` mergeada) · **Origen**: los 2 defectos reales del run `ajustes-fixture` (PR #40; el UI-016 es el del Año recortado)

## Outcome
**UI-016 (real, medido por el arnés)**: el campo Año recorta 4 px del "2026". Fix en `src/dialog_settings.c` (el campo de Año sale cortado).

Nota honesta de la corrida: el owner decidió esta vez que **la pantalla NO es táctil** (se maneja con perilla/teclas) ⇒ **UI-015 (target táctil <44 px) queda NO APLICADO** — waiver permanente, sin fix.
**Pero el año recortado es un defecto visual real en cualquier caso de uso** (el "2026" se ve cortado con perilla o con touch) — el fix en código va sí o sí.
En la corrida de Ajustes quedó `exclusion acotada` con fecha 2027-03-31; **esta corrida lo corrige y retira el waiver** (patrón de la caja).

## Alcance
1. Abrir `src/dialog_settings.c`: buscar la creación del campo de Año (la casilla de año con "2026") y el redibujado del ancho con el dígito que corta 4 px.
2. El fix: cambiar la geometría del campo de Año (el `lv_textarea`/label del año) para que el dígito quede visible en [2026], sin tocar los otros campos (Día/Mes/Hora).
3. Si el ancho actual se calcula dinámico por fuente/tema: mirar primero el código que lo computa y ajustar el cálculo — más honesto que hardcodear.
4. **A/B del fix**: la fixture de Ajustes `settings-home.scn` define el diff de píxeles SOLO en la región del campo de Año (lo demás byte-idéntico). El natural no corre (no es ud. fuente del tema).
5. UI-016 → waiver retirado en el mismo PR (patrón de la casa).

## Fuera de alcance
- Los switches/slider (UI-015): waiver permanente.
- Otros recortes en el resto de los widgets de Ajustes (si aparecen, informe para otra corrida).

## Criterios (finish condition)
```bash
cd tools/ab_testing
uv run x6200-ab compare --a 89851e6 --b HEAD --experiment-id AB-UI016-T1   # diffs SOLO en la región del campo de Año (settings-home)
uv run x6200-ab contracts --revision HEAD                                  # PASS aprox 230
uv run x6200-ab audit --revision HEAD --suite visual                        # PASS (0 structural findings, y sin el waiver de UI-016)
uv run pytest -m "not e2e" -q && uv run pytest -m e2e -q && native + estáticos + ruff/ty
```
Capturas antes/después en el PR (con el "2026" completo).
