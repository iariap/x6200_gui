# Spec: settings-year-110 — el campo de año de la fila de fecha con el mismo ancho que Day/Month (issue #110)

**Fecha**: 2026-10-16 · Corrida: `factory/settings-year-110` · Base trunk `5944ff5`
**Issue**: #110 (reporte del owner: en Settings, la primera fecha que aparece — Day, Month, Year —
el input field de año es más chico que el resto de los widgets de la columna).

## Diagnóstico (verificado por el orquestador leyendo src/dialog_settings.c)

En `make_date()`:

- **Day**: `lv_obj_set_size(obj, SMALL_2, 56)` → 93 px (2 columnas de la grilla, SMALL_1=45 + pad 3).
- **Month**: `lv_obj_set_size(obj, SMALL_2, 56)` → 93 px, idéntico a Day.
- **Year**: `lv_obj_set_size(obj, ancho_campo_cifras(obj, 4), 56)` con `lv_obj_set_style_pad_hor(obj, SMALL_PAD, LV_PART_MAIN)`.
  `ancho_campo_cifras` mide en runtime `lv_txt_get_size("0000")` (= 84 px con la fuente del tema,
  Fira Sans default) + pad_left + pad_right (SMALL_3 = 3 px por lado) + 2·border → **≈ 90 px**.

Resultado: la caja del año queda **~3 px más chica** que Day/Month → es el síntoma que reporta el owner.
El sizing por contenido nació para arreglar UI-016 (con SMALL_2=93 y padding de textarea 11 px/lado
el "2026" quedaba recortado 4 px), pero dejó la fila inconsistente.

## El fix (consistente y sin recorte)

1. **Year pasa a `lv_obj_set_size(obj, SMALL_2, 56)`** — igual que Day y Month: los tres campos de la
   fila con la MISMA caja (93 × 56), alineados en la grilla (sigue span de 2 columnas, así que la caja
   llena exacto su span: sin huecos ni desbordes).
2. **Conservar `pad_hor = SMALL_PAD` (3 px)** en Year (el que ya está): el padding de textarea de 11 px
   era el que comía el contenido; con 3 px por lado la caja útil es `93 − 6 − 2·border`. **Medir** con
   la fuente vigente: texto natural de 4 cifras = 84 px → margen ≥ 3 px. Si border_width > 0, contarlo
   en la medida y reportar el margen real.
3. **Si `ancho_campo_cifras()` queda sin uso: eliminarla** (un `static` sin uso es warning con -Wall y
   el repo trata warnings como fallas en los builds del port). Su valor documental (medir, no clavar)
   vive en el comentario de la caja: actualizar ese comentario para que explique por qué el ancho es
   SMALL_2 y cómo se verificó que 4 cifras entran (número medido).
4. **Verificación obligatoria con la fixture `settings-home`**: la evidencia de UI-016 midió
   `text_natural.w 84 vs caja util 67` en la captura. Repetir la medición tras el fix y reportar:
   ancho de caja de Day/Month/Year (los tres iguales) y que `text_natural.w` del año ≤ caja útil
   (con el margen). Si el harness expone las métricas en la salida de la fixture, pegalas; si no,
   medí la captura con PIL (columnas de la caja + píxeles del texto).

## TDD (lo que el repo permite)

- No hay test nativo de `dialog_settings.c` (es UI LVGL con mocks); el apoyo determinista es la
  fixture `settings-home` + `check_settings_focus.py` (parsea el fuente) + el A/B visual.
- **Agregar la aserción donde viva la medición**: si `check_settings_focus.py` o un check similar es
  el lugar natural para "los 3 spinboxes de make_date tienen el mismo ancho declarado", agregá una
  línea de check ahí (regex sobre `lv_obj_set_size` en make_date: los tres iguales). Si no encaja,
  dejá la medición en la evidencia del A/B y reportá por qué el check no se agregó (honesto, sin
  forzar).

## Gates (pegá la salida real)

```bash
cd /home/pablo/desa/asf/state/worktrees/x6200/settings-year-110
tests/check_harness_determinism.sh
cd tools/ab_testing
uv run pytest -m "not e2e" -q && uv run pytest -m e2e -q
uv run x6200-ab compare --a origin/lu2dsx/x6200-rom --b HEAD --experiment-id AB-YEAR110
uv run x6200-ab contracts --revision HEAD && uv run x6200-ab audit --revision HEAD --suite visual
uv run x6200-ab themes
uv run ruff check . && uv run ruff format --check .
cd ../.. && tests/native/run_native_tests.sh
estáticos: check_cfg_fills, check_actions, check_settings_focus, check_apps_menu,
           check_app_handlers, check_wcag, (audita-pruebas si hay docs tocados)
```

- El A/B va a mostrar diff visual en `settings-home` (la caja del año cambia de ancho): es el cambio
  intencional → re-baseline del golden correspondiente con el patrón de la casa (el reporte
  explica el diff como intencional, sin dejar regresiones "unexplained").

## Banco QEMU (REGLA DEL OWNER: cierre con prueba QEMU salvo requiere-radio)

Este fix es 100% visible en QEMU (no requiere RF): **hay que capturarlo en el banco**.
Método con precedente: `docs/evidencia/2026-10-15-fallback91/banco.txt` (capturas de pantallas de
Settings en el banco — leelo y reusá el mismo camino de navegación). Con eso:
- Captura de la fila de fecha con los tres campos (Day/Month/Year) en la imagen ARM.
- Medición con PIL en la captura: ancho de las tres cajas (iguales) + el "2026" completo (sin
  recorte).
- La evidencia va commiteada en `docs/evidencia/2026-10-16-year110-banco/` (README con método, md5
  del binario/fuente/captura, números crudos de la medición) — igual que el patrón del #92/#108.
- El banco puede estar ocupado por otra sesión: `docker ps` antes; si está en uso, reportá BLOCKED
  explícito (no toques infra compartida).

## Invariantes

- Escribís SOLO en el worktree; rama `factory/settings-year-110`.
- NO tocar: RF/TX/DSP, `lvgl/`, temas (theme.c), otros runs en vuelo (#109 settings-en-order-109
  corre en paralelo en OTRO worktree: no los mezcles).
- Commits `Pablo Iaria <iariap@gmail.com>`; **NO push ni PR** (publica el orquestador).
- `.asf-active` no se commitea.
- No romper `check_settings_focus.py` (el regex mira make_date).

## Reporte final

1. Diff + por qué el ancho queda igual (fórmula del contenido útil con los números medidos).
2. Medición antes/después (caja Day/Month/Year y text_natural del año).
3. Gates con salida real. 4. Banco + números de la captura (o BLOCKED con razón).
5. PENDIENTES honestos.

## Apéndice — corrección del diagnóstico (medido, no estimado)

El diagnóstico de arriba es una **estimación hecha leyendo el fuente** y sus números **no
cierran**: decía "≈ 90 px" para el ancho por contenido y "~3 px más chica" para la diferencia
contra Día/Mes. La medición real (inspección estructural del harness sobre la fixture
`settings-home`, camino del A/B, y píxeles sobre las capturas) da otra cosa:

| Campo | Caja declarada | Medido (harness) |
|---|---|---|
| Día | `SMALL_2, 56` | `499,100 93x56` |
| Mes | `SMALL_2, 56` | `595,100 93x56` |
| Año (antes) | `ancho_campo_cifras(obj, 4), 56` | `691,100 **70**x56` |
| Año (después) | `SMALL_2, 56` | `691,100 **93**x56` |

La diferencia **no** era ~3 px: el campo del Año quedaba **23 px más angosto** que Día y Mes
(70 vs 93). La fórmula que cierra exacto con la fuente vigente (`sony_36`, la del tema por
defecto del harness) es:

```
ancho_campo_cifras("0000") = 60 (cifra más ancha: el '0' mide 15 px, x4)
                          +  6 (SMALL_PAD = 3 + 3)
                          +  4 (borde = 2 + 2)
                          = 70 px
```

`lv_txt_get_size("0000")` da **60**, no los ~80 que sugería la estimación con "Fira Sans". El
"texto natural" del spinbox de 4 cifras es 60 (la inspección estructural reporta el label
`2026` con `60x28` de texto natural). Con el fix, la caja útil es `93 − 6 (pad) − 4 (borde) =
83`, así que el `2026` entra con **23 px de margen** (no con los ≥3 px que estimaba el spec).

En el banco (ARM real) el ancho base medido es **72 px** (el `ancho_campo_cifras` corre en
runtime y la fuente real mide `0000` un poco más ancho que en el harness); ver
`docs/evidencia/2026-10-16-year110-banco/`.

La conclusión del fix no cambia (el Año pasa a la caja de la fila), pero la magnitud del defecto
era **7,7×** la estimada y el margen del fix es **holgado**, no al límite.
