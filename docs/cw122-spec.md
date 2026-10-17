# Spec DRAFT — #122: el widget de Tune no aparece en la banda CW

Fecha: 2026-10-16. Rama: `factory/cw-tune-122` (base `3553a476`, trunk tip).
Creado por: orquestador (spec stage). Implementación: delegada al coder.

## Contexto (del issue #122, medido en radio)

Reporte del owner: *"cuando está en la banda CW debería aparecer un widget para ver dónde está la
señal de radio y poder hacer el ajuste ultra fino"*. El issue deja tres hipótesis: (1) la página CW
sólo se alcanza con el decodificador CW encendido; (2) el widget no recibe el tono; (3) no se
descubre / el parámetro viene apagado.

**Estado medido en la radio** (sólo lectura, `sshpass -p 123 ssh root@192.168.0.236`, 2026-10-17
03:05 UTC; la radio corre el build del port `aaa60c1d3ab54d9682423379e688a57a`, 13.747.976 B,
mtime `Oct 17 02:40`):

```
$ md5sum /usr/sbin/x6200_gui /mnt/params.db
aaa60c1d3ab54d9682423379e688a57a  /usr/sbin/x6200_gui
c6e49987d11c35a8b98bb793f247d601  /mnt/params.db
$ sqlite3 /mnt/params.db "select name,val from params where name in ('cw_tune','cw_decoder','theme_name');"
cw_decoder|0
cw_tune|0
theme_name|Redshift
```

Es decir: **el parámetro `cw_tune` está en 0** (y el tema es oscuro, `Redshift`; ver más abajo).

**El widget existe y la página es alcanzable** (código):

- `cw_tune_init()` (`src/cw_tune_ui.c:57`) crea el objeto (`139x40` = `WIDTH`x`HEIGHT`,
  `src/cw_tune_ui.h:17-21`) y lo mete en la pantalla.
- La visibilidad la decide `update_visibility()` (`src/cw_tune_ui.c:158-171`):
  `on = subject_get_int(cfg.cw_tune.val) && (mode == cw || mode == cwr)` — con `cw_tune = 0` agrega
  `LV_OBJ_FLAG_HIDDEN`.
- `cfg.cw_tune` nace en `false` (`src/cfg/cfg.c:349`), así que **la radio lo trae apagado** y sólo se
  enciende si el usuario lo toca.
- El botón que lo enciende vive en la página `page_cw_decoder_1` (`(CW 1:2)`), ítem índice 2
  (`src/buttons.cpp:443-444`): `btn_cw_tuner` → `controls_toggle_cw_tuner` (`src/buttons.cpp:274-276`,
  `src/controls.cpp:35`).
- La página se alcanza por la tecla **KEY**: `KEYPAD_KEY` → `buttons_load_page_group(buttons_group_key)`
  (`src/main_screen.c:773`) y `buttons_group_key` contiene las cuatro páginas
  `page_key_1 / page_key_2 / page_cw_decoder_1 / page_cw_decoder_2` (`src/buttons.cpp:526-531`).

**Hipótesis 1 refutada** (código): ni `buttons_load_page_group` ni el grupo KEY consultan
`cw_decoder`; la página `(CW 1:2)` está siempre en el grupo. El decodificador sólo decide si la
página **hace** algo, no si **se llega** a ella.

**Hipótesis 2, ubicada** (código): el tono que mueve el bloque activo lo empuja el DSP por
`cw_put_audio_samples()`, y esa función corta con
`if ((!cw_decoder) && (!cw_tune) && (!trainer)) return;` (`src/cw.cpp:215-217`). Es decir: el feed del
tono también depende de `cw_tune`; con el parámetro en 0 no hay ni widget ni tono. No es una causa
independiente.

**Hipótesis 3 confirmada** (código + medido): el defecto es el default (`src/cfg/cfg.c:349`) y el
estado persistido en la radio (`cw_tune=0`).

**No es una regresión del port**: el upstream `gdyuldin/x6200_gui` (clonado en `e1476ec`) tiene el
mismo gate, el mismo mapeo de página y el mismo default.

**Hallazgo aparte** (no es la causa del issue, pero lo destapa la misma fixture): en el único tema
claro (**Sunlit**) las barras del widget quedan a **1.01:1** contra el panel del widget y no se
distinguen (HALLAZGOS §49). En `Redshift` —el tema de la radio— miden `2.00:1` y se ven. Es un
defecto de contraste independiente del #122.

## Objetivo (en orden — fundar antes de tocar)

1. **Fundar la causa** con evidencia reproducible: parámetro persistido (`params.db`), gate
   (`src/cw_tune_ui.c:158-171`) y default (`src/cfg/cfg.c:349`), sin depender de la radio viva.
2. **Refutar la hipótesis 1** de forma determinista (que la página `(CW 1:2)` se alcanza con
   `cw_decoder=0`), no con una afirmación.
3. **Medir el A/B** del widget (oculto con `cw_tune=0`, visible con `cw_tune=1`) en las dos
   superficies disponibles: el **arnés de UI** (`tools/ui_harness/`, determinista) y el **banco QEMU**
   (binario ARM real, el mismo que corre la radio).
4. **Decidir el fix, no implementarlo**: el arreglo es una decisión de arquitectura/UX (dónde vive el
   botón, si el widget depende del parámetro). Se documentan las opciones y **se frena** (espera
   decisión del owner).
5. **Registrar** lo medido: fixtures + capturas + `docs/HALLAZGOS.md` (§49).

## Invariantes (no romper)

- No tocar RF/TX/DSP ni `lvgl/`; una sola escritora por worktree; commits
  `Pablo Iaria <iariap@gmail.com>`; sin push; `.asf-active` sin commitear.
- La radio se usa **sólo lectura** (hay otra sesión activa); el A/B se hace en el banco/arnés.
- Gates completos con salida real (arnés determinista, `ab_testing` ruff/format/ty/pytest, nativos,
  estáticos, `check-matrix`, cross-compile ARM); todo en foreground.
- Sin supresiones de lint/tipos; la restricción que necesitaría un comentario se codifica
  (assert en el fixture) en vez de explicarse en prosa.

## Criterios de aceptación

- Causa fundada con línea + comando (parámetro persistido, gate, default) — **hecho**.
- Hipótesis 1 refutada de forma determinista (código + fixture que corre con `cw_decoder=0` y
  assertion que lo codifica) — **hecho**.
- A/B medido en arnés y banco con capturas en `docs/evidencia/2026-10-16-cw122-banco/` — **hecho**.
- Fix **no** implementado; decisión del owner documentada con las opciones y sus consecuencias —
  **hecho**.
- Hallazgo de contraste registrado (§49) y auditoría de hallazgos sin problemas nuevos — **hecho**.
- Spec commiteada `docs/cw122-spec.md`; reporte final con TDD/evidencia, gates, HEAD+base y
  PENDIENTES.

## Decisión pendiente (del owner)

El widget está sano: aparece y funciona en cuanto `cw_tune=1`. El arreglo cambia **cuándo/dónde** se
descubre, así que es decisión de arquitectura/UX:

- **(A) Mover `btn_cw_tuner` a una página que no sea la del decodificador** (p. ej. `page_key_1`,
  junto al keyer, en la banda CW). Toca `src/buttons.cpp` (reordenar páginas) y cambia el contrato de
  la barra; hay que actualizar el arnés (`buttons_press` posicional) y el `check-matrix`.
- **(B) Mostrar el widget en CW/CWR sin depender de `cfg.cw_tune`.** Diverge del upstream y del gate
  actual; el feed del DSP sigue gateado en `src/cw.cpp:215-217`, así que habría que decidir también si
  se enciende el procesamiento (costo de CPU permanente en modo CW).
- **(C) Dejarlo como está**: es descubrible (KEY ×3 + un toque en Tuner) y el default es una decisión
  de producto; sólo faltaría documentarlo.

Sin esa decisión, el cambio no se implementa (invariante de la spec: fundar y frenar).

---

## Implementación (2026-10-17): opción (B), elegida por el owner

El owner eligió **(B)**: el widget se muestra en CW/CWR sin depender de `cfg.cw_tune`. Esta sección
es lo implementado en la rama, con la medición de cada punto.

### 1. Visibilidad: el modo decide, no el parámetro

`update_visibility()` (`src/cw_tune_ui.c:182`) deja de mirar `cfg.cw_tune`:

```diff
-    bool on = subject_get_int(cfg.cw_tune.val) && ((mode == x6200_mode_cw) || (mode == x6200_mode_cwr));
+    bool on = (mode == x6200_mode_cw) || (mode == x6200_mode_cwr);
```

Se quitan también el observer de `cfg.cw_tune.val` y el `update_visibility(cfg.cw_tune.val, NULL)` de
`cw_tune_init()` (quedaban sin efecto); el arranque se resuelve con
`update_visibility(cfg_cur.mode, NULL)`. **El botón `Tuner` y el parámetro `cfg.cw_tune` NO se
quitan** (orden del owner): siguen existiendo, con la misma ruta (`src/buttons.cpp:274-276` →
`src/controls.cpp:35`), pero ya no tienen efecto funcional — se reporta abajo.

### 2. Feed del tono: sigue a la visibilidad del widget

`cw_put_audio_samples()` (`src/cw.cpp:213`) alimenta al decodificador CW, al trainer **y** al widget
(`update_peak_freq` → `cw_tune_set_freq`, que mueve el bloque que marca dónde está el tono). El gate
pasa de `cw_tune` a la visibilidad del widget:

```diff
-    if ((!cw_decoder) && (!cw_tune) && (!trainer)) {
+    if ((!cw_decoder) && (!trainer) && (!cw_tune_ui_visible())) {
```

`cw_tune_ui_visible()` (`src/cw_tune_ui.c:198`, declarado en `src/cw_tune_ui.h:19`) es `obj != NULL &&
!HIDDEN`, es decir exactamente lo que ve el usuario. La variable `static bool cw_tune` de `src/cw.cpp`
y su `subscribe` se quitan (quedaban muertos al salir del gate).

**Costo, medido sobre el código** (`src/audio.c:68` → `src/dsp.cpp:251`): el feed corre una vez por
bloque de captura (~100 ms, 44 100 Hz → ~4 410 muestras; `src/audio_enhancement.c:320` documenta el
mismo tamaño), es decir ~10 veces por segundo. Por bloque decima con `NUM_STAGES=6` (`DECIM_FACTOR=64`)
y por cada 128 muestras decimadas hace una FFT de 128 puntos (`FFT=128`, `src/cw.cpp:34-36`):

```
4410 muestras/bloque x 10 bloques/s = 44 100 muestras/s de entrada
44 100 / 64                        =    ~689 salidas del decimador por segundo
689 / 128                          =      ~5.4 FFT de 128 puntos por segundo
```

Es el mismo trabajo que ya se paga hoy con el decodificador CW encendido (`cw_decoder=1`), que es el
caso normal de la banda CW; el cambio sólo lo enciende además en el caso `cw_decoder=0` **dentro de
CW/CWR**, que es donde el widget se ve. `cw_put_audio_samples()` se llama **sólo** en CW/CWR
(`src/dsp.cpp:250-251`: el `else if` del modo), así que el gate nuevo es efectivamente siempre
verdadero ahí: el feed corre siempre que el widget pueda verse, que es lo que el widget necesita para
marcar el tono.

**Por qué no hay un A/B de CPU del feed**: la GUI bajo `qemu-arm` satura su core (medido con
`docker top banco-apps` durante la corrida: `qemu-arm-static /usr/sbin/x6200_gui` al 99 % de un core,
sin bajar), así que el costo del feed —miles de muestras por segundo contra un lazo de eventos que no
duerme— no es observable en el banco; y `src/cw.cpp` no se compila ni en el arnés de UI ni en la
suite nativa (necesita liquid-dsp), así que tampoco hay dónde medirlo aislado. Queda la cuenta de
arriba (del código: `src/cw.cpp:34-36`, `src/dsp.cpp:250-251`, `src/audio.c:68`) más la estructura: es
el trabajo que **ya** se hace con el decodificador encendido, y sólo dentro de CW/CWR. Lo que sí está
medido en el binario real es que el render no cambia: `13-banco-cw-decoder-off.png` (decodificador
apagado) es byte a byte `11-banco-cw-tune-sunlit.png` (encendido), mismo md5.

### 3. Contraste de las barras (HALLAZGOS §49)

Las barras inactivas llevan ahora **borde de 1 px** (`rect_dsc.border_width = 1`,
`rect_dsc.border_opa = LV_OPA_COVER`, `src/cw_tune_ui.c:64-69`) cuyo color se elige en cada dibujo
contra la superficie que las rodea —el **panel** del widget, no su propio relleno—, con el mismo
criterio y la misma cuenta que el bloque activo (§27) y que `band_plan.c`:

```c
rect_dsc.border_color = rel_lum(panel_color()) < LUM_CORTE ? lv_color_white() : lv_color_black();
```

`panel_color()` calcula el color real del panel (negro al 50 % sobre el fondo del tema,
`src/styles.c:290-296`) porque el panel no es un rol del tema que el chequeo estático pueda resolver.

Ancho **1** y no 2 como el bloque activo: la barra mide `BLOCK_W=5` px y con 2 px de borde el relleno
que marca el tono casi desaparece.

### 4. Verificación

| qué | cómo | resultado |
|---|---|---|
| RED | fixtures nuevos con el accessor ya declarado y `update_visibility` sin tocar | los 3 fixtures de CW fallan ("el widget de CW tune esta OCULTO en modo CW con `cw_tune=0`"); `cw-tune-ssb` pasa |
| GREEN | los 4 fixtures | pasan; `cw-tune-default`/`cw-tune-tuner` con `cw_tune=0` y el widget visible, `cw-tune-ssb` (USB) oculto, `cw-tune-tema-sunlit` con el tema claro |
| A/B de píxeles | los 64 renders pre-existentes contra `before.sha256` | **idénticos** (byte a byte); sólo cambian/aparecen los `cw-tune-*` |
| determinismo | `tests/check_harness_determinism.sh` | PASS (2 corridas, PNG idénticos por fixture) |
| contraste | borde vs panel en las 10 paletas (`tests/check_wcag.py`) | ≥ 3:1 en todas, mínimo **4.88:1** (Sunlit/Outdoor); antes el relleno medía 1.01:1 |
| contraste (render) | objeto `cw_tune_bars` de `quality/nontext.yaml` sobre los renders dorados | Sunlit **5.02:1**, Redshift **20.67:1**; antes (captura del banco, binario real) **1.01:1** |
| contraste (banco) | binario ARM real, capturas `11`-`14` de `docs/evidencia/2026-10-16-cw122-banco/` | Sunlit `1.01:1` → **5.02:1**, Redshift → **20.67:1**; `11` ↔ `03` (pre-arreglo, mismos params) pasa de **1** a **11** colores: el widget visible con `cw_tune=0` |
| compilación ARM | `arm-buildroot-linux-gnueabihf-g++ -fsyntax-only` con las banderas del build real + rebuild del banco | pasa (sonda negativa: sin la declaración de `cw_tune_ui_visible()` falla en `src/cw.cpp:223`); `make x6200-gui-rebuild` deja `OK: ... es el firmware del port (ELF32 ARM, 13748068 B, 3/3 marcas)` |
| estáticos | los 7 (`check_cfg_fills`, `check_actions`, `check_settings_focus`, `check_apps_menu`, `check_app_handlers`, `check_wcag`, `check_cw_tune_feed`) | 7/7 ok |
| nativos | `tests/native/run_native_tests.sh` | 0 fallos |
| framework A/B | `ruff check`, `ruff format --check`, `ty check`, `pytest -m "not e2e"` | limpio (169 archivos formateados) / 227 pasan (13 e2e deseleccionados) |
| anti-regresión | `tests/check_cw_tune_feed.py` (nuevo) | verde; sus dos sondas negativas (volver a gatear por `cw_tune`; sacar el accessor del gate) fallan |
| matriz por paths | `scripts/apps/x6200/check-matrix.sh 3553a476 HEAD <worktree>` | exit 0; sin `CHROOT` el paso del banco sale **BLOCKED** por entorno (diseñado), con `CHROOT`/`BUILDROOT` pasa el preflight, la idempotencia del rebuild y la captura de humo (binario `66b766562d75`) |
| e2e del framework A/B | `pytest -m e2e` (post-commit) | 13 pasan |

### 5. Reportes al owner (no se tocó nada por esto)

- **El botón `Tuner` quedó sin efecto funcional**: el parámetro `cfg.cw_tune` se sigue guardando y el
  rótulo sigue cambiando (`Tuner: On/Off`), pero ni la visibilidad ni el feed lo miran. No se quitó
  (orden del owner: reportar, no quitar). Si se decide quitarlo, toca `src/buttons.cpp`,
  `src/controls.cpp`, `src/cfg/cfg.c` y los fixtures del arnés.
- **`scripts/apps/x6200/check-matrix.sh` (harness ASF) enumera los estáticos del repo uno por uno** y
  no incluye el nuevo `tests/check_cw_tune_feed.py`: hay que sumarlo allí para que corra también en la
  matriz por paths. Queda PENDIENTE (fuera de este worktree, es el repo del harness).
