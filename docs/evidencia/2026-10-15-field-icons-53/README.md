# Evidencia: los iconos de Field Ops (issue #53, la ruta del X6100)

`src/dialog_field.c` heredó del port el prefijo de los tiles del **X6100**
(`FIELD_ICONS "/usr/share/x6100/field"`): esa carpeta no existe en la imagen del
X6200, así que los cuatro PNG no cargaban y los tiles del HOME salían **sin
icono** (el issue #53 lo reporta así). La causa estaba ya acreditada por el owner
en el issue; acá queda el fix, la prueba de la ruta contra la imagen real y la
verificación en el **banco** (binario ARM de la imagen con `qemu-arm-static`,
framebuffer falso y control remoto por socket, el mismo camino que la radio).

Cada captura va atada al **md5 del binario que la produjo** (lo imprime
`tools/captura-banco.sh`), porque dos builds de la misma fuente dan md5 distinto
(ver §4: el rebuild del banco reusa un build dir persistente):

| lado | fuente | binario que corrió |
|---|---|---|
| antes | trunk `11f8d689` (sin el fix) | `md5 30bef0dbec2a` |
| después | con el fix (commit `b426806`), build limpio | `md5 163f1f5883ae` |

## 1. El fix y el estado del rootfs

Una línea, `src/dialog_field.c:71` (antes `:67`):

```c
-#define FIELD_ICONS "/usr/share/x6100/field"
+#define FIELD_ICONS "/usr/share/x6200/field"
```

**No hubo que copiar ningún asset**: la ROM ya instala los ocho PNGs.
`rootfs/` es lo que entra a la imagen — `CMakeLists.txt:27`,
`install(DIRECTORY rootfs/ DESTINATION /)` — y los tiles vienen con la app desde
`da183f3` ("Add Field Ops (SOTA/POTA)…"):

| | repo (`rootfs/usr/share/x6200/field`) | chroot del banco (`.vfix3/usr/share/x6200/field`) |
|---|---|---|
| `bands_64/96`, `log_64/96`, `pota_64/96`, `sota_64/96` | 8 archivos | los mismos 8, **md5 idéntico** |
| `/usr/share/x6100` (la carpeta que el código pedía) | no existe | **no existe** (`ls: cannot access …`) |

Los `find`/`ls`/`md5sum` crudos están en `rootfs-iconos.txt`. Que la carpeta del
X6100 no exista **en la imagen** es exactamente el defecto: el código pedía un
directorio que la ROM nunca tuvo.

## 2. El chequeo determinista nuevo (`tests/native`)

Hasta ahora **nada ataba el prefijo del código a los assets de la imagen**. De las
rutas absolutas del firmware que se resuelven contra `rootfs/`, dos tenían test
que las leía de ahí (`src/dxcc.c` → `cty.dat` vía `CTY_PATH`, y `src/theme.c` →
`themes/` vía `X6200_SHIPPED_THEMES`, los dos exports de
`tests/native/run_native_tests.sh`) y la de Field no: por eso #53 pudo pasar con
la suite nativa en verde.

`tests/native/test_field.c:test_icons_shipped()` cierra ese hueco con el mismo
patrón: `run_native_tests.sh` exporta `X6200_DIALOG_FIELD_SRC=$SRC/dialog_field.c`,
el test **lee el `#define FIELD_ICONS` del fuente** y abre, dentro de `rootfs/`,
los 8 PNG que el diálogo pide (`sota/pota/bands/log` × 64/96 — los nombres de
`home_icons[]`, `head_set()` y el panel de programación). Un prefijo equivocado
falla en el host, sin banco ni Docker.

Es red/green de verdad (`nativo-antes-despues.txt`, corrida completa):

| fuente | resultado |
|---|---|
| prefijo del issue (`/usr/share/x6100/field`) | `FALLO tests/native/test_field.c:244: icon != NULL` ×8 · **8 fallos** |
| con el fix | **819 comprobaciones, 0 fallos** |

## 3. La app abriendo con los iconos (banco)

Misma secuencia de órdenes (`{"cmd":"app","text":"Field"}` y los `field_screen`),
mismo driver de una sola corrida, mismo chroot: lo único que cambia entre A y B es
el binario. Capturas en `banco/`, transcripciones completas en
`banco/secuencia-*.txt`.

| Paso | Comando | Antes (`30bef0dbec2a`) | Después (`163f1f5883ae`) |
|---|---|---|---|
| HOME | `{"cmd":"app","text":"Field"}` | 3 tiles **vacíos**, 795 colores | **iconos SOTA (montaña), POTA (pinos), BANDS (barras)**, 1996 colores — `banco/{before,after}-home.png` |
| BANDS | `{"cmd":"field_screen","text":"BANDS"}` | sin icono de cabecera, 332 colores | icono azul de 64 px al lado de "BAND OVERVIEW", 456 colores — `banco/{before,after}-bands.png` |
| SOTA | `{"cmd":"field_screen","text":"SOTA"}` | sin icono, 310 colores | icono de cabecera de 64 px, 780 colores — `banco/{before,after}-sota.png` |
| errores `Theme image` | — | **8** | **0** |

Los 8 errores del banco son literalmente la ruta vieja, y son la contraprueba de
que la captura "antes" es de un binario con el código viejo
(`banco/secuencia-before.txt`, y la lista cruda en
`banco/before-errores-iconos.txt`):

```text
Theme image /usr/share/x6100/field/bands_64.png: No such file or directory
Theme image /usr/share/x6100/field/bands_96.png: No such file or directory
Theme image /usr/share/x6100/field/pota_96.png:  No such file or directory
Theme image /usr/share/x6100/field/sota_64.png:  No such file or directory  (×4)
Theme image /usr/share/x6100/field/sota_96.png:  No such file or directory
```

Con `HOME_TILES 3` el tile `log` no se dibuja en el HOME; su PNG entra por el
chequeo nativo de §2 (que abre los 8) y por `head_set("log", …)`, no por la
captura.

## 4. Hallazgo del entorno (importante): el rebuild del banco puede compilar un árbol viejo

La evidencia "después" de arriba tuvo que rehacerse por esto, así que va con
prueba. `make x6200-gui-rebuild` (el que corre `tools/captura-banco.sh` cuando
`RECONSTRUIR` no es `no`) compila en un **build dir persistente del volumen**
(`/b/build/x6200-gui-custom`), que tiene su **propia copia del repo**. En la
primera pasada esa copia tenía `src/dialog_field.c` **sin el fix** (con el `#define`
en la línea 67, la vieja) y con mtime **más viejo** que el `dialog_field.c.o` que
ya estaba del build anterior: `make` no recompiló nada, relinkó el `.o` viejo y el
binario "nuevo" siguió pidiendo `/usr/share/x6100/field`.

Prueba (`banco/secuencia-after-rebuild-stale.txt`, mismo driver, mismo chroot):

```text
### after2: md5=5a6e561e502f15f65c0c32906c5deaa0     <- binario del rebuild común, con el fuente ya arreglado afuera
  errores 'Theme image' en la secuencia: 8
      4 Theme image /usr/share/x6100/field/sota_64.png: No such file or directory
      ...
```

Forzando el build limpio (`make x6200-gui-dirclean && make x6200-gui-rebuild`) la
copia se refresca, todo recompila y el binario **sí** trae el fix
(`163f1f5883ae`, `banco/secuencia-after.txt`: 0 errores e iconos a la vista).

Y ojo con esto, porque **engaña a los gates**: la captura de humo del
`check-matrix.sh` con el binario viejo (`5a6e561e502f`) daba **ok** — su captura
es la pantalla principal, donde los tiles de Field no se ven. El `ok` de esa paso
no dice nada del fix; la verificación real es la de §3 más el chequeo nativo de §2.
También quedó claro que **dos builds de la misma fuente dan md5 distinto**
(cuando el `params.db` generado se borra antes, el build cambia): por eso cada
claim de acá va con el md5 del binario que lo produjo, y no como "rebuild
reproducible".

## 5. Nota honesta del banco: la `liblvgl.so` vieja del chroot

La primera corrida quedó con la **UI congelada**: el framebuffer salía byte a byte
idéntico tras 70 s y en el log estaba
`lv_timer_handler: It seems lv_tick_inc() is not called`. La causa no era el
cambio: el chroot (`.vfix3`, fuera del repo) tenía una `liblvgl.so` del 12-oct
(`md5 5ad04ccc`) compilada con `LV_TICK_CUSTOM 0`, mientras el `x6200_gui` de hoy
espera el tick de `lv_tick_get()` sobre `CLOCK_MONOTONIC` (`ca20c2b`, #58, que
sacó el thread de `lv_tick_inc`). Se copiaron al chroot la `liblvgl.so` y la
`liblv_drivers.so` frescas del build target y el tick volvió. `tools/captura-banco.sh`
copia a propósito sólo `/usr/sbin/x6200_gui`, nunca las shared libs del build:
por eso el chroot puede quedar desincronizado sin que el banco avise.

## 6. Por qué no hay fixture del arnés de host

`tools/ui_harness` no compila `src/dialog_field.c`: `dialog_field` es un mock
inerte (`tools/ui_harness/mocks/mock_main_screen.c:59`, `dialog_field =
&mock_dialog`), y el diálogo real arrastra ~12 módulos (cluster, `cw_encoder`,
`dialog_msg_voice`, `field/{bands,flog,rbn,refs}`, gps, knobs, qth, station,
textarea_window) que hoy no están mockeados (~30 símbolos nuevos). Y aunque se
mockeara, el arnés corre en el **host**, donde `/usr/share/x6200/field` tampoco
existe: necesitaría un override del tipo `THEMES_DIR_SHIPPED` para los temas, que
es justamente lo que taparía el defecto que #53 reporta (una ruta del runtime).
Por eso la cobertura va por donde el defecto es real: el chequeo nativo de §2
(ata el prefijo a `rootfs/`, lo que la imagen instala) y el banco de §3 (lo que
la app dibuja en la radio).

## 7. Hallazgos de la MISMA clase, fuera del alcance de #53 (no tocados)

Mientras se buscaba la ruta de los tiles aparecieron dos usos **idénticos** del
nombre viejo: el código invoca un ejecutable del X6100 que la imagen no tiene (la
ROM instala los suyos con el prefijo `x6200-`). No se tocaron: la spec acota este
cambio a la ruta/visión de los iconos de Field.

| archivo:línea | el código pide | la imagen tiene | efecto |
|---|---|---|---|
| `src/dialog_field.c:76` (`HELPER`) | `x6100-field` | `rootfs/usr/sbin/x6200-field` | los 4 jobs del diálogo (`update` de refs, `pota-spot`, `qrz-upload`) no arrancan |
| `src/cluster/client.c:47` (`POTA_SPOTS`) | `/usr/sbin/x6100-pota-spots` | `rootfs/usr/sbin/x6200-pota-spots` | el feed de spots de POTA no devuelve nada (`popen(… " 2>/dev/null")` se come el error) |

Evidencia del nombre que sí instala la ROM (misma lista para repo y chroot):

```text
$ ls rootfs/usr/sbin/        # y el chroot del banco trae los mismos
btaudio-ctl  btcat-ctl  btcat-setup  x6200_daemon  x6200-field  x6200-pota-spots
$ find .vfix3 -name '*6100*'
/usr/bin/x6100_webserver          # el webserver sí conservó el nombre viejo
/usr/lib/python3.11/site-packages/x6100_webserver
```

Los dos casos son un fix de una línea cada uno, pero **cambian comportamiento**
(el feed de spots y los jobs del diálogo), así que van como issue/hallazgo aparte.

## Gates

`gates.txt` tiene la salida real, corrida sobre el árbol de código de esta
corrida (la salida de `contracts` imprime el sha que evaluó, `b426806cafb8`; el
commit final sólo suma este directorio de evidencia, no toca código):
`check-matrix.sh` (los 7 estáticos + preflight/idempotencia/captura
de humo del banco, con el binario `163f1f5883ae`), el gate de determinismo del
arnés de UI, `AB-FIELD53` (PASS; Hardware/Energy BLOCKED por no haber radio),
`contracts`, `audit --suite visual`, `pytest -m "not e2e"`, `pytest -m e2e` y la
suite nativa. El reporte del A/B queda en `tools/ab_testing/reports/AB-FIELD53/`.

## PENDIENTES

- **El rebuild del banco (§4)**: `make x6200-gui-rebuild` solo puede reusar la
  copia del repo y los `.o` del build dir persistente y compilar un árbol viejo.
  Hoy la única forma de asegurarlo es `make x6200-gui-dirclean && make
  x6200-gui-rebuild` (y así se hizo la evidencia final). Falta que
  `tools/captura-banco.sh` lo haga (o que compare `src/` contra el `.o`) antes de
  decir "reconstruyendo".
- **La captura de humo del `check-matrix.sh` no cubre nada de la app**: pasa en
  verde con cualquier binario que dibuje la pantalla principal (por eso el `ok`
  con el binario viejo de §4).
- **Los dos hallazgos de §7**: mismo defecto de nombre heredado, con su propio
  test/verificación.
- **El chroot del banco puede quedar con libs viejas** (§5): copiar también
  `liblvgl.so`/`liblv_drivers.so` del target, o chequear el md5 en el preflight.
- **El handshake del banco reinicia la GUI**: `{"cmd":"get"}` falla contra una GUI
  ya arrancada y `captura-banco.sh` la reinicia entre llamadas, así que una ruta
  de varios pasos por el script mide un reinicio. Las capturas de acá salen de un
  driver de una sola corrida. Defecto del banco, no del cambio.
- **La suite nativa corrió sin sanitizers** (ASan/UBSan) en esta máquina: falta
  `/usr/lib64/libasan.so.8.0.0` (entorno, no el cambio); el script lo informa y
  sigue sin ellos.
- **El chequeo nativo es específico de los iconos de Field**: no es un gate
  genérico de "toda ruta absoluta del código existe en la imagen"
  (`FIELD_DIR`, `/mnt/*` y los `/usr/sbin/*` de §7 siguen sin cobertura).
