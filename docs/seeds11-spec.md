# Spec: seeds-11-fixtures — cierres de las 5 semillas del issue #11

**Fecha**: 2026-10-15 · **Corrida**: `factory/seeds-11-fixtures` · **Base**: trunk `c4ff0c5`
(24→25 fixtures ya, con `settings-audio`) · **Issue**: #11 (semillas UI-002/004/005/007/008).

## El objetivo
Que las 5 semillas dejen de ser `unverified` por falta de fixture, con la misma honestidad
de la cadena: fundar con métrica lo que se reproduce y declarar bien lo que no.
Al final del run el issue #11 queda listo para cerrar (lo cierra el orquestador).

## Mapa de las semillas (leer `docs/UI_KNOWN_ISSUES.md` tabla `Re-evaluación` y
`docs/HALLAZGOS.md` 17-21/28-29; estos defectos son HISTÓRICOS: lo que hay que decidir hoy,
con evidencia, es si reproducen o no)

### UI-002 — fixture del menú de apps
- Fixture `apps-menu.scn` (o una por página: juntas via los índices): abrir el **grupo APP**
  de la barra inferior (el índice de páginas del botón-página: en el arnés es
  `buttons_press`/`buttons_load_page` — mirá cómo lo hace `settings-perfil.scn`, que hace
  "Mas >" + "Apariencia"). **6 páginas APP** hoy (el menú quedó 22 apps en 6 páginas
  después de #46).
- Cobertura: inspección real de las 22 celdas de app (con texto y caja); si salen
  recortes/encabezados, se fundan ahí; si no, la semilla pasa a `verified` (sin defecto).
- **Exactitud anti-falso-verde**: el settle codifica una apertura real por página
  (vuelta al home entre páginas) y el nombre de al menos una app por página.

### UI-005 — diálogo viejo: texto tapado por los botones (BT/WeFax/SSTV)
- Fixture del diálogo **BT** (`bt` + `wefax`+`sstv` si la mecánica es la misma): el defecto
  histórico es texto acoplado a la fila de botones. Medilo con `--inspect` (calle del box
  del texto vs los botones). Si reproduce → fix (espacio/margen en ese diálogo) + waiver
  retiro; si NO reproduce (ya arreglado por el flujo del time), pasa a `verified`
  sin defecto, con la métrica en la postura.

### UI-004 — foco en Ajustes (inspector)
- **Arduino del inspector**: que el inspector emita **pertenencia al grupo de foco**
  (`keyboard_group`) por nodo, en la salida `--inspect` (numerada/named). Eso desbloquea
  el detector "widget inalcanzable con el foco" renderizado.
- La grilla `settings-home`/`settings-wfbat`/`settings-audio` ya existen: re-declarar
  con la métrica nueva (sin waiver nuevo).
- Si algún widget realmente inalcanzable aparece: fundarlo ahí y fix (o waiver si es
  estructural, con razón declarada).

### UI-007 — variante Ajustes + tema RS
- Fixture `settings-home` con **tema RS** (`settings-rs-theme.scn`): muestra el riel del
  switch con `control_border` del RS y deja que el par quede bajo
  `typography_contrast`/no-texto **REAL** (hoy lo cubre sólo el estático; según la tabla
  del issue: "sólo falta la evidencia renderizada").
- Ojo temas aplicados via `.scn` (mirá cómo los `main-tema-*` fijan el tema en la fixture).

### UI-008 — presupuesto de recursos (CMA 128 MB)
- NO es defecto de UI: es un presupuesto del firmware (`docs/RESOURCE_USAGE.md`). El
  arnés no lo mide. Tratamiento: **documento de decisión** de 5 líneas (por qué no es
  un defecto de UI y dónde queda monitoreado) en `docs/UI_KNOWN_ISSUES.md` — sin
  fixture ni fake-trick: queda `out-of-scope` declarado (no es un waiver de defecto,
  es la limitación del arnés con la razón por escrito).

## Gates (salida real)
```bash
tests/check_harness_determinism.sh          # fixtures x2 (los que crecen: declará el total)
cd tools/ab_testing
uv run x6200-ab compare --a c4ff0c5 --b HEAD --experiment-id AB-SEEDS11  # PASS
uv run x6200-ab contracts --revision HEAD && uv run x6200-ab audit --revision HEAD --suite visual
uv run pytest -m "not e2e" -q && uv run pytest -m e2e -q
tests/native/run_native_tests.sh
estáticos 6/6
```
- Los waivers de defecto que surjan de un defecto REAL reproducido van por el mismo
  cauce de la casa.
- Los veredictos finales los carga el orquestador en el issue y en
  `docs/UI_KNOWN_ISSUES.md` con fecha; tu commit ya deja los estados y la evidencia.

## Fuera de alcance
- El archivo de temas (#44), de audio ya resuelta, etc.; `lvgl/` pinned; DSP/RF/TX.

## Invariantes
- Commits `Pablo Iaria <iariap@gmail.com>`. **Sin push/PR** (publica el orquestador).
- Evidencia antes que output: si un defecto se dice "no reproduce", el métrico (px/contrast)
  queda en el reporte, no en una frase.

## Reporte final
1. Las 5 semillas: estado nuevo de cada una con su evidencia (fixture / métrica / decisión).
2. Los defectos SI los recrea: cada uno con sus 3 preguntas (repro, fix, test/waiver).
3. Gates completos con salidas reales.
4. PENDIENTES honestos (todas las piezas del issue quedan listas para cerrarse).
