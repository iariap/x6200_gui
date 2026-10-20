# Presets de filtro del DFL (issue #183)

Evidencia de la corrida del **#183** — «Presets de filtro en acceso rápido desde DFL,
ajuste manual y memorias persistentes» — sobre `lu2dsx/x6200-rom`.

| | |
|---|---|
| Rama / worktree | `factory/dfl-presets-183` (aislado) |
| Commit | **`a0fa0250`** «DFL #183: presets de filtro al primer toque, memorias y favoritos» |
| Base | **`26c8e8e7`** (trunk tip al arrancar) |
| Binario ARM (banco) | **`fd864a791cb5`** (13.899.984 B), sello de fuentes **`34bea401acbf`** |
| Validación física | **pendiente del owner** (no bloquea el PR, contrato §7) |

> Todo lo de acá es **software**: arnés de host (196 fixtures, 54 nuevos), tests nativos
> contra `sqlite3` real, y el **banco QEMU** con el binario ARM del port. La radio real no
> se tocó: la migración y las pruebas corren sobre bases temporales o el `params.db` del
> chroot del banco, **nunca** la del owner.

## Resultado mínimo innegociable (§8), punto por punto

| Pedido | Cómo se demuestra |
|---|---|
| Desde HOME, DFL corto → **página de tres presets** del modo | `capturas/dfl-presets-fav-tema-*.png` (6 temas) y `banco/banco-A-favoritos.png` (ARM) |
| **Un toque aplica** y el filtro cambia **de inmediato** | fixture `dfl-presets-apply` (asserta LOW/HIGH/BW en los Subjects) y `dfl-presets-touch` (toque LVGL real); en el banco, `{"cmd":"page","text":"dfl"}` + `{"cmd":"btn","val":0}` → `flo:0 fhi:6000` (Wide AM) |
| **Waterfall y VFO intactos** | `dfl-presets-apply` asserta que `cfg_cur.fg_freq` no cambió; `dfl-presets-rotary0` que rotary0 sigue sintonizando y no toca el filtro |
| `Ajustar` da acceso a LOW/HIGH/BW/Eq/**Gráfico** | `capturas/dfl-presets-adj-*` (B) y `dfl-presets-graph-*` (C, el editor del #179/#182 sin reimplementar dibujo) |
| **VOL** como único encoder | `dfl-presets-vol` (en C, VOL edita LOW y no toca frecuencia ni volumen); la página B son los botones VOL de la ROM |
| Guardar/recuperar **Mem1/2** y **reasignarlas** sin salir de DFL | `dfl-presets-save`, `-save-cancel`, `-save-fail`, `-assign-ok`, `-assign-cancel` |
| **Todo sobrevive reinicios** | test nativo (recarga de la cache desde la base) + `banco-params.db.txt`: la base real del chroot quedó en `version=5` con las dos tablas y `mode_params` del AM con el par aplicado |

## El modelo, el catálogo y la persistencia

`src/dfl_presets.h`/`.c` (modelo, sin LVGL y **sin SQL**) y `src/dfl_presets_store.h`/`.c`
(SQLite). El catálogo es la tabla FIJA del contrato §3 (SSB/DIGI/CW/AM × 3 presets, `BW =
HIGH − LOW`), verificado por `check_catalog()` en `tests/native/test_dfl_presets.c`
(**175 comprobaciones, 0 fallos**), incluido el `Focus` de DIGI y los `LOW` negativos de CW.

Persistencia (§4):

* **Migración nueva y versionada**: `_5_dfl_filter_tables` en `src/params/migrations.c`
  (el siguiente número libre era el 5) + las mismas dos tablas en `sql/params.sql` para
  instalaciones nuevas.
* Tablas **separadas** (`dfl_filter_memories`, `dfl_filter_favorites`) con los CHECKs
  exactos del contrato. **No** se reutiliza `memory` ni `mode_params` y **no** se renumeró
  `press_action_t`.
* **Idempotente**: `tests/native/test_migrations_cb.c` (62 comprobaciones) reejecuta la
  migración y verifica que no borra nada, que rechaza filas inválidas y que `mode_params`
  queda intacto.
* **Default sin filas** = `source = quick_slot` (el favorito n es el preset n), resuelto en
  memoria: no se escriben filas en el primer arranque.
* **Guardar un favorito guarda el ID de la fuente, no copias**: sobrescribir Mem 1 hace que
  el favorito enlazado muestre el contenido nuevo (test nativo).
* **Favorito huérfano** (fila que apunta a una memoria vacía): fallback al preset del mismo
  slot + aviso por stdout, sin crash ni reset global.
* Errores de SQLite (`db == NULL`, tabla ausente): `dfl_save_memory()` devuelve `false` y
  **nunca** se informa «guardada»; la UI muestra el error real (`dfl-presets-save-fail`).

## La aplicación segura (§5)

Sólo `cfg_mode_set_low_filter()` / `cfg_mode_set_high_filter()`, en el orden que las guardas
admiten (`dfl_apply_order_low_first()`), validando por familia y **releyendo los Subjects**:
sólo con el par exacto se informa OK. Si el segundo límite no entra, se **restaura el par
anterior** (sin half-apply). El BW lo derivan los observers de `src/cfg/mode.c`; el test
nativo **cuenta las escrituras** a `cfg_cur.filter.bw` y a `cfg_cur.filter.real.from/to` y
exige 0 (`dfl_pt_bw_writes()`/`dfl_pt_real_writes()`).

Caso del contrato, medido: desde LOW=3000/HIGH=4000, DX 300/2400 queda **exacto** (LOW
primero) con BW=2100 y 0 escrituras prohibidas. **Nada se aplica solo** (encender, cambiar
VFO/banda/modo o abrir el DFL no tocan el filtro): `check_no_auto_apply()`.

En **TX** los presets y aplicar Mem quedan deshabilitados (el estado sale de
`radio_get_state()`, así que entrar al DFL transmitiendo ya los deshabilita) y la navegación
sigue: fixture `dfl-presets-tx` (+ `banco`, que no captura TX).

## Navegación A–F y callbacks (§1/§2/§6)

| Página | Teclas | Quién la carga |
|---|---|---|
| **A** Favoritos | preset 1 \| preset 2 \| preset 3 \| Ajustar \| Más | `buttons_dfl_open()` (DFL corto desde fuera; `buttons_group_dfl` = A) |
| **B** Ajustar | Filter low \| Filter high \| Filter BW \| Eq \| Grafico | celda 4 de A (`dfl_adj_cb`) y `dfl_inline_collapse()` (Waterfall/ESC en C) |
| **C** Gráfico | LOW \| HIGH \| BW \| Eq \| Waterfall | celda 5 de B → `ACTION_APP_DFL_EDITOR` → `dfl_inline_expand()` (sin cambios) |
| **D** Más | Mem 1 \| Mem 2 \| Guardar 1 \| Guardar 2 \| Volver | celda 5 de A |
| **E** Confirmar | Confirmar \| Cancelar \| `--` \| `--` \| Volver | Guardar 1/2 de D |
| **F** Elegir origen | Wide \| Normal \| DX \| Mem 1 \| Mem 2 | **hold** sobre un favorito (sin aplicar) |

* **DFL corto** con el DFL abierto cancela lo pendiente y vuelve siempre a A (contrae C).
  Desde fuera abre A; con **NFM/WFM** (sin catálogo) abre B directo (`dfl-presets-nfm`).
* **ESC**: en A restaura la barra anterior; en B/D/F va a A; en C contrae a B; en E vuelve a
  D. Todo en `buttons_dfl_escape()`; fixtures `dfl-presets-nav`.
* **DFL largo** queda intacto: sigue despachando `params.long_dfl` (por defecto el editor
  inline) y `ACTION_APP_DFL_EDITOR` se conserva para el control remoto.
* La página actual **no se duplica**: se deduce de `buttons_get_cur_page()`.
* La banda contextual (`src/dfl_context.c`) es un chip discreto en el área superior del
  waterfall: `Personalizado` en A cuando ningún favorito coincide, el contexto de E
  (`Guardar Mem n - LOW … - HIGH …`, con `Se reemplazara Mem n` si ya existía) y
  `Asignar tecla n` + el **Cancelar TÁCTIL** de F. Sin popup, sin ventana, sin sexta tecla.
* **Ruta táctil**: los favoritos 1–3 suman `LV_EVENT_CLICKED`/`LV_EVENT_LONG_PRESSED` en su
  celda (LVGL suprime el click cuando ya mandó el long press). El resto de la barra sigue
  siendo una leyenda de teclas físicas. Fixtures `dfl-presets-touch` y `-touch-hold` (eventos
  LVGL reales, no la función de negocio).
* **VOL / rotary0**: en B/C el encoder VOL edita el parámetro (comportamiento del #179); la
  rueda principal sigue sintonizando y **nunca** toca el filtro (`dfl-presets-vol`,
  `dfl-presets-rotary0`).

## Pruebas y gates

Comandos y salidas crudas en `logs/`.

| Gate | Comando | Resultado |
|---|---|---|
| Nativos | `sh tests/native/run_native_tests.sh` | rc 0; **47.585 comprobaciones, 0 fallos**; `test_dfl_presets` 175 y la migración 62 (ver `logs/native.log`) |
| `-fanalyzer` | incluido en el runner | sin hallazgos en `dfl_presets.c` y `dfl_presets_store.c` |
| Estáticos | `check_{cfg_fills,actions,settings_focus,settings_grid,apps_menu,app_handlers,manual_apps,wcag,cw_tune_feed,build_identity}.py` + `audita-pruebas.py` | **11 OK** (`logs/estaticos.txt`) |
| Arnés (todos los fixtures) | `ui_harness --scenario …` | **196 fixtures, 0 fallos** (54 nuevos) |
| Clipping/glyphs | `ui_harness --inspect` | 54 fixtures, **2.994 labels: 0 truncados, 0 glyphs faltantes, 0 en fallback** (`logs/inspect-clipping-glyphs.txt`) |
| Determinismo | `tests/check_harness_determinism.sh` | **PASS**: 196 fixtures × 2 corridas, PNG idénticos (`logs/determinismo.log`) |
| Framework A/B | `ruff` / `ruff format --check` / `ty` / `pytest -m "not e2e"` | OK / OK / OK / **227 passed** |
| A/B e2e | `TMPDIR=<disco> pytest -m e2e` | **13 passed** |
| A/B de UI | `uv run x6200-ab compare --a 26c8e8e7 --b a0fa0250 --experiment-id AB-DFL183` | **PASS** (exit 0): Functional, Visual Regression, Quality Contract (1.480 checks) y Performance PASS; Hardware/Energy BLOCKED (advisory, sin radio). **140 de 142 escenarios idénticos**; los 6 `dfl-page-tema-*` cambian **explicados** dentro de las regiones declaradas (`botones`, `dfl_inline`) por el rótulo nuevo de la quinta celda (`Grafico`) y por llegar a esa página vía `Ajustar`. `ab-DFL183/` |
| Banco QEMU (#86) | `tools/captura-banco.sh` | preflight OK, `dirclean`+rebuild OK, **gate #86 3/3 marcas**, capturas A–E |

### Banco QEMU (binario ARM real, sin radio)

```
version: a0fa0250 DFL #183: presets de filtro al primer toque, memori
fuente: md5 34bea401acbf (src/ + lvgl/src)
binario: md5 fd864a791cb5   qemu-cpu: cortex-a7
OK: /…/usr/sbin/x6200_gui es el firmware del port (ELF32 ARM, 13899984 B, 3/3 marcas)
```

* `banco/banco-home.png` — home; `banco/banco-A-favoritos.png` — **A** con el catálogo del
  modo activo (AM: `Wide 6000 Hz / Normal 4500 Hz / Narrow 2500 Hz / Ajustar / Mas`) y el
  favorito **Wide resaltado**; `banco/banco-B-ajustar.png` — **B** (`Filter low: 0 Hz`,
  `Filter high: 6000 Hz`, `Filter BW: 6000 Hz`, Eq, Grafico); `banco/banco-C-grafico.png` —
  **C** (editor inline, `LOW/HIGH/BW/Eq/Waterfall`); `banco/banco-D-mas.png` — **D** (`Mem 1
  vacia`, `Mem 2 vacia` deshabilitadas, `Guardar 1/2`, `Volver`); `banco/banco-E-confirmar.png`
  — **E** (`Confirmar / Cancelar / -- / -- / Volver`) con la banda `Guardar Mem 1 - LOW 0 -
  HIGH 6000`.
* **Aplicación medida en el banco**: `{"cmd":"page","text":"dfl"}` + `{"cmd":"btn","val":0}`
  → el `get` siguiente devuelve `flo:0, fhi:6000, filter:6000` (antes 4500): el preset se
  aplicó por los setters. `logs/banco-params.db.txt` muestra la base del chroot ya migrada
  (`version=5`, las dos tablas, `mode_params` del AM con `filter_low=0/filter_high=6000`) y
  **`freq_step`/`spectrum_factor` intactos**.
* **Dos límites honestos del banco** (no del cambio): (1) el control remoto no tiene un
  comando de **hold**, así que la página **F** no se captura en ARM (sí en el arnés, con
  `LV_EVENT_LONG_PRESSED` real); (2) el chroot no tenía los **assets en `/dev/shm/images`**
  (en la radio el daemon los engancha), así que las primeras capturas salían sin la imagen de
  fondo de las teclas y el **resaltado** del favorito no se veía; se copiaron los 16 assets
  del rootfs (4,5 MB) al chroot y las capturas de arriba son las buenas.

## Rollback

El cambio es aditivo: `git revert a0fa0250` deja el firmware sin las páginas nuevas. La
migración `_5` sólo **crea** dos tablas vacías; una ROM vieja con la base ya migrada las
ignora (no hay downgrade destructivo). Los favoritos y memorias quedan en la base por si se
vuelve a subir la versión.

## Pendientes

* **Validación física** de la UX y de los presets en la radio (la hará el owner; el contrato
  dice explícitamente que no bloquea el PR/cierre). No se desplegó ni se transmitió.
* **Página F en el banco ARM**: falta un comando de hold en el control remoto (fuera del
  alcance de este issue); queda cubierta por el arnés de host.
* El **primer** intento del A/B abortó por **disco lleno** (`git worktree add` falló con
  8,2 GB libres: el checkout de una revisión pesa 2,3 GB). Se reintentó con el workspace en
  `/var/tmp` y pasó. El host quedó al 100% de uso; se limpiaron los artefactos de esta
  corrida (salidas intermedias del arnés y workspaces), **no** los del owner.
