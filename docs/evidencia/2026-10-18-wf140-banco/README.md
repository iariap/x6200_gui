# Evidencia — #140: la línea central del waterfall no mide 0

**Fecha**: 2026-10-18 · Rama `factory/wf-line-140-coder` · Base `94c2c6ab` (trunk)
**HEAD de código**: `dc45d924` — *waterfall: la linea central se crea una vez, sigue el alto y
respeta el parametro (#140)*
**Banco**: QEMU `cortex-a7` con el binario ARM real de la imagen, framebuffer falso y el cliente de
control de `tools/remote`; **sin radio** (la radio no estuvo accesible en esta corrida).

| binario | commit | md5 | tamaño |
|---|---|---|---|
| trunk (RED) | `94c2c6ab` | `2a0be37fdb293383b605316a9881c932` | 13.773.024 B |
| fix (GREEN) | `dc45d924` | `d8c059178386b109d5438ac5d521497f` | 13.773.080 B |

El md5 del RED salió del binario que quedó en el chroot. El del GREEN se reprodujo con un build
independiente del mismo commit (volumen `sigsegv131-build-output`, matriz de gates): mismo hash de
fuentes (`0b8eb9…`), mismo tamaño y mismos 12 hex que el log de la captura (ver “Contingencias”).

**Gate fail-closed #86** (lo corre el banco sobre el binario copiado antes de arrancar la GUI), en
las dos corridas:

```
OK: /tmp/opencode/vfix3-wf140/usr/sbin/x6200_gui es el firmware del port (ELF32 ARM, 13773080 B, 3/3 marcas)
```

## Qué dice el issue y qué se midió

El issue #140 dice que la línea central del waterfall *no se ve* porque se crea con **alto 0**
(los dos puntos de la `lv_line` en `(0,0)` y nadie los vuelve a fijar).

**Eso no es lo que hace el trunk.** El código lo dice y el banco lo mide:

- `waterfall_set_height()` (`src/waterfall.c:187`) llama a `draw_middle_line()` **al final**, después
  de `height = lv_obj_get_height(obj)` (`:169`). Ahí no hay alto 0: `height` ya es el alto real.
- `draw_middle_line()` ya hacía `middle_line_points[1].y = height` + `lv_line_set_points()` (código:
  `src/waterfall.c:201-208` en `94c2c6ab`).
- La línea y las filas del waterfall comparten `height`: si la línea midiera 0, el waterfall también.
  El waterfall se ve (lo reporta el propio owner), así que la premisa se contradice sola.
- **Medido en el banco con el trunk** (`red-trunk.png`): la línea se dibuja **3 px de ancho
  (x=398..400), RGB(11,41,66)**, de punta a punta del waterfall, en **222/227** filas con color de
  línea y en **207/208** filas medibles (99,5 %). Números crudos en
  [`medicion-red-trunk.txt`](medicion-red-trunk.txt).
- **Medido en la radio** (evidencia de otra corrida, `docs/evidencia/2026-10-17-qa-radio-swr125/07-restaurado.png`,
  tema Redshift): la línea está, 1 px de ancho (el ancho del tema, no el largo), **217/217** filas
  medibles. El #92 ya había medido 3 px en Sunlit el 2026-10-15.
- **Inspección en el host** (`inspeccion-host.txt`, `ui_harness --inspect`): el objeto `lv_line` vivo
  mide `w=3 h=243`, en `x=399 y=239`, con clip de 240 px y `hidden:false`; es decir, el alto es el del
  waterfall (240) más el trazo (3), no 0.

Conclusión: **la causa declarada en el issue es falsa**. Lo que se cambió es el *invariante* (que la
línea se cree una sola vez, siga el alto y respete el parámetro desde el primer frame) y el *gate*
que lo fija en el arnés; el render no cambia (ver A/B más abajo).

## Qué se cambió

`src/waterfall.c` / `src/waterfall.h`:

- `draw_middle_line()` idempotente: crea la línea **una sola vez por `obj`** (antes, una llamada de
  más dejaba la línea vieja con el alto viejo y el callback duplicado), recalcula
  `middle_line_points[1].y = height` en **cada** llamada (el largo sigue al waterfall) y resuelve la
  visibilidad con `params.waterfall_center_line` **en el acto**, sin esperar al próximo
  `DRAW_POST_END`.
- `middle_line_cb()` NULL-safe (antes desreferenciaba `middle_line` antes del `if (middle_line)`).
- `waterfall_init()` resetea `middle_line`: la línea pertenece al `obj` de esa pantalla; sin el reset,
  una pantalla nueva reusaría el puntero de la anterior.
- `waterfall_middle_line()` (`src/waterfall.h`): accesor del objeto vivo, para el arnés (mismo
  espíritu que `waterfall_offscreen_redraw_count`).

`tools/ui_harness/src/screens.c`: `scenario_settle()` corta la corrida si, con el parámetro
encendido, la línea no existe, está oculta, no mide el alto del waterfall **más su trazo**, no está
centrada (±1) o no es el último hijo del waterfall (se dibujaría debajo de la paleta). Con el
parámetro apagado exige que no se vea. Es geometría de los objetos vivos: el arnés no puede mirar
píxeles.

## Método del banco

```sh
# chroot privado (tema Sunlit, /mnt/themes con los 6 temas v2 del trunk) y volumen privado
export CHROOT=/tmp/opencode/vfix3-wf140
export VOLUMEN=x6200-build-output-wf140        # (ver Contingencias)
export BUILDROOT=/home/pablo/desa/x6200/buildroot_new/AetherX6200Buildroot

# RED: el trunk, en un checkout de 94c2c6ab (git checkout 94c2c6ab)
RECONSTRUIR=si ESPERA=12 tools/captura-banco.sh red-trunk.png '{"cmd":"get"}'
# GREEN: la rama con el fix (dc45d924)
RECONSTRUIR=si ESPERA=12 tools/captura-banco.sh green-home.png '{"cmd":"get"}'
```

Garantías que da el script (skill `.agents/pruebas-sin-radio`): `x6200-gui-dirclean` + rebuild desde
el worktree en cada corrida con `RECONSTRUIR=si`, gate #86 sobre el binario copiado, handshake
`{"cmd":"get"}` con reintento, y md5 de la fuente y del binario en el log
([`red-trunk.log`](red-trunk.log), [`green-home.log`](green-home.log)).

**Receta de captura determinista**: `ESPERA=12` y una orden `{"cmd":"get"}`. Sin la espera, la
captura sale a los ~2 s del arranque y agarra —o no— el aviso `X6200 de R1CBU v0.1.2-lu2dsx`
(`msg.c`, `DURATION=2000 ms`, encolado detrás de los mensajes del arranque): dos capturas de la
**misma** corrida pueden diferir en ese texto (medido: 5227 px, todos en la banda del aviso). Con la
espera el aviso ya se apagó y el resto es estable (dos capturas del mismo binario: idénticas salvo
el reloj).

## Mediciones (PIL, [`medir-linea.py`](medir-linea.py))

El método no usa coordenadas escritas a mano: detecta la banda del waterfall por el fondo oscuro a
los costados de la columna central, y en cada fila medible compara el píxel de `x=400` contra
`x-4` y `x+4`. También informa la corrida vertical constante sobre `x=400` y su ancho.

| captura | commit | filas con color de línea | ancho | filas medibles con diferencia | corrida `x=400` |
|---|---|---|---|---|---|
| [`red-trunk.png`](red-trunk.png) | `94c2c6ab` | 227/240 | **3 px (x=398..400) en 222/227** | **207/208 = 99,5 %** | 189/240, RGB(11,41,66) |
| [`green-home.png`](green-home.png) | `dc45d924` | 227/240 | **3 px (x=398..400) en 222/227** | **207/208 = 99,5 %** | 189/240, RGB(11,41,66) |
| [`green-app-abierta.png`](green-app-abierta.png) (Status) | `dc45d924` | 239/240 | 3 px en 239/239 | 239/240 = 99,6 % | 239/240, RGB(11,41,66) |
| [`green-app-cerrada.png`](green-app-cerrada.png) | `dc45d924` | = `green-home.png` (byte-idéntica) | 3 px en 222/227 | 207/208 = 99,5 % | 189/240, RGB(11,41,66) |
| [`param-off.png`](param-off.png) (`waterfall_center_line=0`) | `dc45d924` | **0/240** | — | **0/208 = 0,0 %** | 189/240, RGB(0,0,0) |

`RGB(11,41,66)` = `0x0b2942` = `panel_border` de Sunlit (`#14456F`) × 153/256 sobre negro
(`LV_OPA_60` y `LV_BLEND_MODE_ADDITIVE`), el mismo color que midió el #92.

Los números completos de cada captura están en `medicion-<captura>.txt`.

## A/B del render: el fix no cambia un píxel

[`comparaciones.txt`](comparaciones.txt) (diff pixel a pixel, PIL):

- **trunk vs fix**: 1636 px distintos, **0 en la banda del waterfall (y≥240)** y **0 en las columnas
  de la línea (x=396..404)**. Los 1636 están en `y=7..50, x=637..751`: es el **reloj** de arriba a
  la derecha (las capturas son de distinto minuto). La geometría y el color de la línea son
  idénticos.
- **arnés host, 85 fixtures**: los 85 PNG renderizados en `dc45d924` son **byte-idénticos** a los de
  antes del fix (0 diferencias de md5; md5 de cada uno en [`fixtures-md5.txt`](fixtures-md5.txt)).
- **goldens comprometidas**: las 11 PNG de `tools/ab_testing/golden/` son **idénticas** a las
  fixtures recién renderizadas ([`goldens.txt`](goldens.txt)) ⇒ **no hay re-baseline**.

## Regresión del parámetro y reconstrucción

- Con `waterfall_center_line=0` en `params.db` (y contenedor nuevo, para que la GUI lo lea al
  arrancar) la línea **no se dibuja**: 0/208 filas, `x=400` negro (`param-off.png`). Con `=1` vuelve
  (3 px). El parámetro se lee en el arranque del waterfall y el callback lo mantiene al día en
  Ajustes.
- **Abrir y cerrar una app**: `{"cmd":"app","text":"Status"}` abre el diálogo (la línea sigue ahí:
  239/239 filas de 3 px, `green-app-abierta.png`) y `{"cmd":"app","text":"off"}` vuelve a la home.
  La captura posterior es **byte-idéntica** a la home prístina (`md5 c39744054f74…`), es decir el
  waterfall y su línea sobreviven al ciclo de la app (cierre por `main_screen_close_app()`), que es
  el camino que toca el `static middle_line` y su callback.

## Mutación del gate (el assert no es decorativo)

Con el fix aplicado se cambió a mano `middle_line_points[1].y = height;` por `= 0;` (el defecto que
describe el issue) y se reconstruyó el arnés:

```
main-tema-sunlit-agua: la linea central mide 3 px y el waterfall 240 (+3 de trazo): largo cero o alto viejo (issue #140, src/waterfall.c draw_middle_line)
```

exit 1 (el arnés **no captura** una pantalla sin línea: corta). Restaurado el fuente, la misma
fixture sale exit 0.

## Provenance (md5)

| Qué | md5 |
|---|---|
| fuente RED (`src/` + `lvgl/src`, `94c2c6ab`) | `37f55d16fed458744ef6824645c305c5` |
| fuente GREEN (`src/` + `lvgl/src`, `dc45d924`) | `0b8eb919c6d8d933b504489039adbc6f` |
| `src/waterfall.c` (GREEN) | `4da891c56abb7595f57971a03a30c2e9` |
| `src/waterfall.h` (GREEN) | `f0238ca2d8f3435a0fc74d20b433e2f2` |
| `tools/ui_harness/src/screens.c` (GREEN) | `dde9db9db15408739c334473ba07070f` |
| binario RED | `2a0be37fdb293383b605316a9881c932` |
| binario GREEN | `d8c059178386b109d5438ac5d521497f` (reproducido: mismo commit y hash de fuentes) |
| tema `Sunlit.json` (chroot y `rootfs/`) | `71dd165b824483ddb75b74cb8d011eb9` (los dos iguales ⇒ DATA fresca) |
| `red-trunk.png` | `94a34040d733e9a6aefd333ce17219e6` |
| `green-home.png` = `green-app-cerrada.png` | `c39744054f74087adcdfd457db265ccc` |
| `green-app-abierta.png` | `14f67bc92b13ad755c9ca809d6d10708` |
| `param-off.png` | `6d8efe01bdc1329af6a5da606e25d814` |

## Gates

| Gate | Resultado |
|---|---|
| `tests/native/run_native_tests.sh` | todas las suites, 0 fallos |
| 8 chequeos estáticos del repo | `check_cfg_fills`, `check_actions`, `check_settings_focus`, `check_apps_menu`, `check_app_handlers`, `check_manual_apps`, `check_wcag`, `check_cw_tune_feed`: **8/8 ok** |
| `tests/check_harness_determinism.sh` | `PASS: deterministic (identical PNGs for every fixture)` |
| `scripts/apps/x6200/check-matrix.sh 94c2c6ab dc45d924 <worktree>` | **exit 0**: nativos + 8/8 estáticos + determinismo + preflight del banco + captura de humo (`99,5 %`, mismo binario `d8c059178386`); log en [`matrix.log`](matrix.log) |
| ARM cross-compile (buildroot, `arm-buildroot-linux-gnueabihf`, `cortex-a7`) | `make x6200-gui-dirclean` + `x6200-gui-rebuild` **exit 0** en las dos corridas (RED y GREEN) |
| `tools/ab_testing`: `ruff check` / `ruff format --check` / `pytest -m "not e2e"` | `All checks passed!` · `176 files already formatted` · **227 passed, 13 deselected** (los e2e no se corren porque no cambió ningún `.py`: política de la matriz) |
| `audita-hallazgos.sh` | exit 0 (hallazgo #54 agregado; ver `docs/HALLAZGOS.md`) |

El fix vive en `src/waterfall.c` + `src/waterfall.h` + `tools/ui_harness/src/screens.c` (tests del
arnés): no toca RF, TX, DSP ni `lvgl/`, y con `waterfall_center_line=false` no se dibuja nada.

## Contingencias (lo que hay que saber para leer estos números)

1. **Volumen compartido**: el banco de esta corrida usó un volumen privado porque otra corrida
   (`cw-autotune-139`) estaba reconstruyendo la GUI en `x6200-build-output` al mismo tiempo. Ese
   volumen privado (`x6200-build-output-wf140`) **lo borró y recreó otra corrida a las 19:56**
   (`docker events`: `volume destroy` / `volume create`, ver [`docker-eventos.txt`](docker-eventos.txt)).
   Las capturas y los logs ya estaban hechos; el binario GREEN se reconstruyó después en otro volumen
   (`sigsegv131-build-output`, la matriz de gates) desde el mismo commit y el mismo hash de fuentes,
   y dio el mismo tamaño y los mismos 12 hex que el de la captura (`d8c059178386`), así que su md5 es
   `d8c059178386b109d5438ac5d521497f`.
2. **No se reprodujo el síntoma del owner.** En el trunk la línea está, de punta a punta, con el
   ancho del tema. Lo que no se puede ver desde acá es su `params.db` ni su binario: las hipótesis
   que quedan para lo que vio son (a) `waterfall_center_line` apagado, (b) un tema con
   `waterfall_signal_width=1` sobre el fondo (el caso del #92: se ve, pero finito) o (c) una imagen
   anterior al #92. **Sin fundar** hasta ver su `params.db`/su build.
3. El aviso de arranque (`X6200 de R1CBU v0.1.2-lu2dsx`) hace que dos capturas sin `ESPERA` puedan
   diferir; se documentó arriba y todas las capturas de esta carpeta usan la receta con `ESPERA=12`.
4. **Contenedor del banco**: la captura de humo de la matriz deja el contenedor `banco-apps` corriendo
   (el nombre es global y otras corridas lo reusan). El de esta corrida quedó bindeado a
   `/tmp/opencode/vfix3-wf140` (se comprobó en `docker inspect`) y se limpió con `docker rm -f
   banco-apps` al cerrar; no queda ningún contenedor `banco-apps` de esta corrida.
