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
