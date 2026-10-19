# #131 reabierto — LVGL desde hilos ajenos al de GUI (auditoria + fix + TDD)

**Fecha**: 2026-10-19
**Issue**: [#131 — SIGSEGV (fallo=0xf363) al cambiar filtro/banda: lista de eventos de LVGL corrupta](https://github.com/iariap/x6200_gui/issues/131)
**Base**: `b50303edf0f0f750d58efc1e727cdd9075d54d90` (trunk tip)
**Commit del fix**: `260602c5f28859d957c1a3d31ccbdbabad2b43d2` (rama `factory/lvgl-threads-131b`, sin push)

> El fix del #144 (`hkey_put()` diferido) elimino **ese** acceso fuera de hilo, pero el SIGSEGV
> reaparecio el 19/10 con el mismo `pc` (`_lv_event_mark_deleted` / `lv_obj_destructor`) en el
> build `02090454`, que ya incluia el fix. Esta corrida audita **todos** los caminos que tocan
> LVGL desde hilos que no son el de GUI y difiere los que faltaban.

---

## 0. Veredicto

Tres caminos tocaban LVGL fuera del hilo de GUI. Los tres se difieren al hilo de LVGL con el
`scheduler` (patron de la casa), sin cambio visible:

| Camino | Funcion LVGL | Hilo | Fix |
|---|---|---|---|
| `src/cw_tune_ui.c` | `lv_event_send` (refresh) | audio (PulseAudio mainloop) | `cw_tune_set_freq()` -> `scheduler_put_noargs(cw_tune_apply)` |
| `src/cw_tune_ui.c` | `lv_obj_has_flag` | audio | `cw_tune_ui_visible()` lee un `atomic_bool` |
| `src/buttons.cpp` | `lv_label_set_text`, `lv_obj_*` | keyer CW / mensajes de voz | `buttons_load_page()`/`buttons_unload_page()` se auto-difieren |
| `src/buttons.cpp` | `lv_label_set_text` (por `param_changed_cb`) | el que manda `MSG_PARAM_CHANGED` | `param_changed_cb()` difiere si no es el hilo de GUI |

Los candidatos que el issue listaba (`tx_info_update`, `msg_update_text_fmt`/`voice_say_*`,
`dialog_*_remote`, `clock_update_power`, `meter_update`, `band_info`, `spot_marks`,
`cluster_column`) se auditaron uno por uno: ya estaban diferidos o no tocan LVGL. Detalle en
`docs/HALLAZGOS.md` §59.

---

## 1. TDD (RED -> GREEN)

`tests/native/test_cw_tune.c` corre el `src/cw_tune_ui.c` real contra stubs que registran el hilo
de cada primitiva LVGL (mismo patron que `test_hkey.c` / `test_spectrum.c`).

```text
$ cat tdd-red.log          # src/cw_tune_ui.c de la base b50303ed
FALLO tests/native/test_cw_tune.c:93: cw_tune_stub_lvgl_calls() == 0
FALLO tests/native/test_cw_tune.c:94: cw_tune_stub_lvgl_foreign_calls() == 0
...
35 comprobaciones, 13 fallos
exit=1

$ cat tdd-green.log        # fix 260602c5
35 comprobaciones, 0 fallos
exit=0
```

Cubre: pedido de frecuencia desde el hilo de audio (diferido, refresco en el hilo de GUI), consulta
de visibilidad sin LVGL, modo fuera de CW y coalescencia de pedidos.

---

## 2. Gates (host)

```text
$ tests/native/run_native_tests.sh          # native-suite.log
... exit 0 (incluye el bloque nuevo "CW tune (#131): el hilo de audio no toca LVGL")

$ gcc -fanalyzer src/cw_tune_ui.c           # native-suite.log
sin observaciones

$ tests/check_*.py + audita-pruebas.py      # static-checks.log
10/10 OK (check_cw_tune_feed.py actualizado a la invariante nueva)

$ sh .agents/hallazgos-fundados/scripts/audita-hallazgos.sh docs/HALLAZGOS.md
exit 0 (0 problemas de estructura)

$ tests/check_harness_determinism.sh        # determinism.log
PASS: deterministic (identical PNGs for every fixture)

$ x6200-ab themes --repo .                  # themes.log
exit 0 (pares WCAG de los temas + non-text 19 objetos, 0 fallos)

$ x6200-ab compare --a b50303ed --b 260602c5 --experiment-id AB-LVGL131B --profile host-ui
PASS (exit 0): 89 escenarios pixel-identicos, 913 contratos, Hardware/Energy BLOCKED (advisory)

$ cd tools/ab_testing && uv run ruff check . && uv run ruff format --check . && uv run ty check
All checks passed! / 185 files already formatted / All checks passed!

$ uv run pytest -q                          # pytest-ab.log
240 passed (incluye los e2e; con TMPDIR en /home porque el checkout en tmpfs falla por espacio)

$ cd tools/ab_testing && uv run x6200-ab compare ...
```

El reporte completo quedo en `tools/ab_testing/reports/AB-LVGL131B/` (`report.md`, `results.json`).

---

## 3. ARM cross-compile y banco QEMU

**ARM** (`arm-cross.log`): `make x6200-gui-dirclean && make x6200-gui-rebuild` en el volumen
privado `x6200-build-output-lvgl131b` (la imagen trae el buildroot; el host no monta `$BUILDROOT`).
Binario `ELF32 ARM`, **13.854.112 B**, **`md5 2ddef0bebd7312bafe44b243dc401054`**; pasa
`verifica-firmware-port.py` (3/3 marcas).

**Banco QEMU** (`banco-smoke.log`, `banco-secuencia.log`, `banco-home.png`), binario
`2ddef0bebd73`, `qemu-cpu cortex-a7`, chroot propio `lvgl131b-chroot` (rootfs del build + DATA con
`params.default.db`):

- **smoke**: captura `banco-home.png` 20.891 B / 210 colores, GUI viva.
- **secuencia** (`secuencia-banco.sh`): **120 ordenes** de `freq`/`mode`/`band`/`filter` (6 rondas
  de 20), **0 errores**, GUI viva (`{"cmd":"get"}` responde `state`), framebuffer 8.388.608 B y
  **sin `crash.txt`**.

El banco **no tiene flow** ni placa de sonido: no corre `radio_tick()` (teclas del panel) ni
`cw_put_audio_samples()` (hilo de audio), que es donde vive la concurrencia. Ver §4.

---

## 4. requires-radio (la receta de la radio)

Con el binario del commit `260602c5` desplegado (`tools/prueba-gui-en-radio.sh`, que pasa por
`verifica-firmware-port.py`) y confirmado el md5:

1. **Reproducir el escenario del #159/#131**: abrir la app **CW**
   (`{"cmd":"app","text":"CW"}`) y, en CB (27.185), transmitir con el teclado CW
   (`{"cmd":"cw_send","text":"PARIS PARIS"}` / `{"cmd":"cw_stop"}`) mientras se cambia de
   frecuencia y modo por el control remoto (`{"cmd":"freq","val":...}`, `{"cmd":"mode","val":...}`)
   y con el panel (teclas FIL / MODE / BAND / F1 / F2).
2. **200 ciclos** de `FREQ -> MODE -> BAND` (los tres que dispararon los cuatro crashes previos),
   con una tecla del panel entre paso y paso.
3. Cada 20 ciclos: `CRASHLOG` 0 en el log vivo (`grep -c CRASHLOG /mnt/app_logs/boot.log` sin
   crecer) y `pidof x6200_gui` estable (si cambia, la GUI se relanzo).
4. Cerrar con `tools/verifica-radio.sh` (sin senales de crash) y leer `/mnt/lastbreath`: la ultima
   linea tiene que ser limpia tras el apagado ordenado.

Si el crash vuelve, el `CRASHLOG-MOD` (del #137) graba el md5 de cada modulo y el `LASTBREATH`
(del #138) deja el punto exacto: con eso se cierra el ultimo camino que quede.
