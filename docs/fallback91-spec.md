# Spec: theme-fallback-91 — tema guardado 'Outdoor' que ya no carga: ~1,2 núcleos + pantalla lavada (issue #91)

**Fecha**: 2026-10-15 · Corrida: `factory/theme-fallback-91` · Base: trunk actual
**Issue**: #91 (QA en radio, PRIORIDAD — reportado tras el reflash con el fix del #89).
**Dato clave**: el flicker del #89 DESAPARECIÓ (1 solo GUI) — pero al boot la radio queda
**lavada** y come ~1,2 núcleos con el tema guardado 'Outdoor', que "ya no existe".

> **Actualización (rebase 2026-10-16, trunk `7032d3bb`).** El trunk avanzó y el #103 (cierre del
> epic THEMES v2, #66/#74) **retiró los legados**: `Outdoor` ya no es un built-in ni un tema, el
> default de la colección es **Aurora** y un `theme_name=Outdoor` guardado **migra a Aurora** al
> arrancar. Las premisas de "Qué investigar" (puntos 1 y 2) y la invariante "NO remover legados"
> quedaron **superadas por el #103**: acá se conservan como el registro del pedido original. El
> estado final, medido, está en [`docs/fallback91.md`](fallback91.md).

## Qué investigar (la pregunta principal del issue)
1. **¿'Outdoor' existe en el trunk atual?** El epic v2 NO removió los legados aún (eso es #74),
   entonces `Outdoor` debería seguir cargado en ROM (`src/theme.c`: verificá SI está en el
   `builtin_names[]`/lista de temas servidos — `x6200-ab themes` del trunk). Si existe,
   ¿por qué la radio dice que no? Paths a explorar:
   - La imagen que QA flasheó (¿build sin los temas legacy en rootfs? los themes file no van
     al rootfs de una imagen vieja/parcheada — mirá el build de QA en `.agents/qa-radio/`).
   - El guard del CONSUMO (~1,2 núcleos): puede ser el dispositivo cayendo a un estado de
     tema a mitades (algunos roles por default, otros del archivo) o render loops.
2. **El fix robusto (la parte importante y determinista del issue)**: el **fallback del
   tema**. Si params guarda un nombre que NO se puede resolver (tema removido, DATA
   `/mnt/themes` sin el archivo, typo), el engine debe resolver el tema default del sistema
   de forma deterministic (sin estados mitad-y-mitad, sin loops de render):
   - Cor defecto determinista verificable (referencia: SOLVABLE el tema default).
   - Test nativo/TDD: params con tema inexistente (resolución de nombre → tema default) al
     boot, sin estado mitad-y-mitad (los 122/roles) y sin re-dibujo en loop.
   - Banco: boot con `params.db` de prueba que pida un tema inexistente (una vez.my para
     inyectar el params: el banco de params.db del deploy disponible? el skill del banco
     usa su params... mirá tools/captura-banco.sh cómo setea params y si hay un path para
     pre-carga) → captura del framebuffer UNIFORME (no washed) y CU con un solo GUI.
3. **El 1,2 núcleos**: si el repro reproduce el consumo (en banco no hay hardware real...
   la medición CPU del banco fuzzy QEMU): mipió en radio ievas que marca el. C membero
   declarado, no fundes si no se mide; el issue #91 ya da los datos.

## Determinista
- El fallback no depende de que el tema exista: params INVÁLIDO → default del esquema.
- Los 122 roles en el boot del repuesto (sin herenze media desde el default).

## Gates
```bash
cd /home/pablo/desa/asf/state/worktrees/x6200/theme-fallback-91
tests/check_harness_determinism.sh
cd tools/ab_testing
uv run x6200-ab compare --a origin/lu2dsx/x6200-rom --b HEAD --experiment-id AB-TFALL91
uv run x6200-ab contracts --revision HEAD && uv run x6200-ab audit --revision HEAD --suite visual
uv run x6200-ab themes
uv run pytest -m "not e2e" -q && uv run pytest -m e2e -q
tests/native/run_native_tests.sh
estáticos 7/7 (8/8 si audita-pruebas)
```
+ Banco: repro boot con tema inválido (captura conforme; si el banco no lo ejerce, declaralo).

## Invariantes
- NO remover legados (és #74 — PERO si la investigación muestra que la radio está en una
  imagen SIN los JSON legacy (o con otros), DOCUMENTAR el hallazgo y hacer el fix robusto
  independiente de eso).
- NO tocar `lvgl/`, DSP/RF. NO tocar la partición de datos ni la imagen del owner.
- Commits `Pablo Iaria <iariap@gmail.com>` — sin push/PR.

## Reporte final
1. El estado del trunk del tema 'Outdoor' (y la cadena de COMO la radio llega a 'no existe').
2. El fix del fallback (diff + test TDD inexistente→default).
3. Banco repro con params inválido (captura).
4. Las cabeceras de CPU/wash TV: medidas o declaradas.
5. Gates. PENDIENTES honestos (la confirmación física va en #91).
