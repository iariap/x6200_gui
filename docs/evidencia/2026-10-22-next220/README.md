# Evidencia — issue #220: feedback del operador con controles bloqueados

Hijo 4/6 de la épica [#216](https://github.com/iariap/x6200_gui/issues/216).
Rama `factory/next-220`, worktree
`state/worktrees/x6200/next-220` (harness ASF), base `b798f13b` = tip de
`origin/lu2dsx/x6200-rom` al arrancar la corrida.

> Nota: durante la corrida el remoto avanzó a `9e571d89` (#217 CW y #218 WiFi
> mergeados, PRs #233/#234 — los hijos 1 y 2 de la épica). Mi rama queda sobre
> `b798f13b`; el diff del PR contra `lu2dsx/x6200-rom` es solo el de este hijo
> (merge-base `b798f13b`). El rebase/orden lo maneja el orquestador (épica #216:
> CW → WiFi → radio_thread → feedback). El gate de la matriz de este reporte
> corrió con base `b798f13b`.

## Alcance y cambio

Solo producción: `src/main_screen.c`. Nada de `radio.c`, `cb.c`, tablas
`ACTION_*`, settings, TX/PTT ni el diseño visual.

- **BAND trabado** (`KEYPAD_BAND_UP/DOWN`, release): `Band locked`.
- **MODE trabado** (`KEYPAD_MODE_AM/CW/SSB`, release y long press): `Mode locked`.
- **VFO A/B trabado** (`KEYPAD_AB`, release y long press): `VFO locked`.
- **Dial principal trabado** (`main_screen_rotary_enhanced_cb` con `freq_lock`,
  incluido el dial CB): `Frequency locked`, con throttle de 2000 ms de reloj
  monotono (`get_time()`).
- **PRE/ATT**: al activar PRE se apaga ATT y el aviso lo dice
  (`Preamplifier On / Attenuator Off`); al activar ATT se apaga PRE
  (`Attenuator On / Preamplifier Off`); si la otra función ya estaba off se
  conserva el mensaje actual (`Pre: On` / `Att: On`).

Decisiones de implementación (contrato cerrado del issue):

- Los avisos van a la **barra de mensajes transitoria ya existente**
  (`msg_update_text_fmt`, `src/msg.c`), sin panel permanente, popover, toast
  nuevo, voz adicional ni settings nuevos. Es el único aviso transitorio que
  `move_foreground()` pone por encima de los diálogos abiertos (que es cuando
  el firmware arma los locks: `dialog_ft8.c`, `dialog_cw_trainer.c`); `msg_tiny`
  queda debajo del diálogo.
- **En TX el aviso no se emite** (`radio_get_state() == RADIO_TX`): los
  mensajes de error/interlock de la radio son más críticos y un control trabado
  no los debe pisar. No se agrega ninguna política que bloquee controles en TX.
- El throttle usa `get_time()` (CLOCK_MONOTONIC en la radio) y se reinicia al
  cambiar el lock (`freq_lock_notice_reset()` en las cuatro vías de toggle:
  `KEYPAD_FST` long, `HKEY_SPCH`, `KEYBOARD_SCRL_LOCK` y
  `main_screen_lock_freq()`), así el primer intento de cada sesión bloqueada
  avisa enseguida.
- Se conservan `dialog_rotary(enhanced->raw_diff)` y
  `main_screen_tuning_reset()` en los caminos trabados. No hay QSY colateral.

## Prueba dedicada (eventos reales)

`tools/ui_harness/fixtures/locked-feedback.scn` +
`settle_locked_feedback()` en `tools/ui_harness/src/screens.c`. Empuja los
**mismos eventos que la radio** (`EVENT_KEYPAD` sobre la pantalla,
`EVENT_ROTARY_ENHANCED` para el dial) con los locks que el firmware pone al
abrir una app, y corta el harness si una aserción falla.

Se agregaron dos piezas de infraestructura de test (solo test, inertes para el
resto del corpus):

- `mocks/mock_ui_deps.c`: `mock_event_bus_set_live(true)` entrega **solo** el
  `EVENT_MSG_UPDATE` al label real (`src/msg.c` / `src/msg_tiny.c`). Apagado por
  defecto: ningún otro fixture cambia de píxeles.
- `mocks/mock_cfg.c`: contadores de `cfg_band_load_next()` / `cfg_band_vfo_copy()`
  (hoy no-ops), para que "la tecla bloqueada no ejecutó la acción" no pase por
  vacuidad.

### Matriz de controles bloqueados (resultado real del fixture)

| control | evento | aviso esperado | estado | gate |
|---|---|---|---|---|
| BAND | `KEYPAD_BAND_UP` release | `Band locked` | `cfg_band_load_next` = 0 llamadas; banda sin cambio | PASS |
| BAND (destrabado) | `KEYPAD_BAND_UP` release | (sin mensaje) | `cfg_band_load_next` = 1 llamada | PASS |
| MODE | `KEYPAD_MODE_SSB` press | (sin mensaje) | modo sin cambio | PASS |
| MODE | `KEYPAD_MODE_SSB` release | `Mode locked` | modo sin cambio | PASS |
| MODE | `KEYPAD_MODE_SSB` long | `Mode locked` | modo sin cambio | PASS |
| MODE (destrabado) | `KEYPAD_MODE_SSB` release | (sin mensaje) | modo cambia (USB→LSB) | PASS |
| VFO A/B | `KEYPAD_AB` release | `VFO locked` | `radio_toggle_vfo` = 0 llamadas | PASS |
| VFO A/B (destrabado) | `KEYPAD_AB` release | (sin mensaje) | `radio_toggle_vfo` = 1 llamada | PASS |
| frecuencia | dial `+3` con `freq_lock` | `Frequency locked` (primer intento inmediato) | `fg_freq` sin cambio | PASS |
| frecuencia | dial `+3` a 1999 ms del aviso | (sin mensaje) | `fg_freq` sin cambio | PASS |
| frecuencia | re-trabar y primer intento | `Frequency locked` (throttle reiniciado) | `fg_freq` sin cambio | PASS |
| frecuencia | dial `+3` a 2000 ms exactos | `Frequency locked` | `fg_freq` sin cambio | PASS |
| frecuencia (destrabado) | dial `+3` | (sin mensaje) | `fg_freq` cambia | PASS |
| BAND en TX | `KEYPAD_BAND_UP` release | (sin mensaje: guard de TX) | — | PASS |
| PRE (ATT on) | `KEYPAD_PRE` release | `Preamplifier On / Attenuator Off` | PRE on / ATT off | PASS |
| ATT (PRE on) | `KEYPAD_PRE` long | `Attenuator On / Preamplifier Off` | ATT on / PRE off | PASS |

Captura real del fixture (`Band locked` en la barra transitoria):
`after-band-locked.png` (regenerable con el comando de abajo).
Referencia "antes": `before-home.png` — la misma home sin aviso, que es lo que
hacia el lock antes del cambio (la pulsación bloqueada se descartaba en
silencio: filas `UX01-01`/`UX01-18` de
`docs/evidencia/assessment-rom-194/03-ui-ux/operator-tasks.csv`). Las capturas
del host no reemplazan la prueba en la radio con teclas físicas
(PENDING_HARDWARE).

## Gates

Ver `gates.md` para la salida literal de cada uno. Resumen:

| gate | resultado |
|---|---|
| `sh tests/native/run_native_tests.sh` | PASS (0 fallos) |
| `sh tests/native/run_native_tests.sh --ci-parity` | PASS (build host sin `lvgl/`) |
| `sh tests/check_harness_determinism.sh` | PASS (205 fixtures × 2, PNG idénticos) |
| `python3 tests/audita-pruebas.py` | PASS |
| `x6200-ab themes` | PASS |
| `x6200-ab contracts` | PASS (2116 checks, 0 fallos) |
| `sh tests/check_harness_natural.sh` | PASS |
| `sh tests/check_harness_wakeups.sh` | PASS |
| `sh tests/check_waterfall_incremental.sh` | PASS |
| `sh tests/run_all_tests.sh` | PASS (19 OK, 0 fallos; 5 SKIP herméticos de la imagen `ci`) |
| `scripts/apps/x6200/check-matrix.sh` (matriz) | PASS/EXIT=0; banco QEMU BLOCKED |
| banco QEMU / teclas físicas | PENDING_HARDWARE (recursos privados del banco: no usados) |

## Cómo reproducir

```sh
# build del arnés de UI (target ci) y corrida del fixture
docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" \
  -v "$PWD:/src" -w /src/tools/ui_harness x6200-uiharness \
  -lc "mkdir -p build && cd build && cmake .. -DHARNESS_REVISION=$(git rev-parse HEAD) >/dev/null && make -j8"
docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" \
  -v "$PWD:/src" -w /src/tools/ui_harness x6200-uiharness \
  -lc "cd build && ./ui_harness --scenario ../fixtures/locked-feedback.scn \
        --png out/locked-feedback.png --json out/locked-feedback.json"

# gates nativos
sh tests/native/run_native_tests.sh
sh tests/native/run_native_tests.sh --ci-parity
SKIP_BUILD=1 sh tests/check_harness_determinism.sh
python3 tests/audita-pruebas.py
```

## Limitaciones de laboratorio

- Controles físicos (teclas BAND/MODE/A-B/FST y detents reales del encoder),
  energía y RF: **PENDING_HARDWARE** (pipeline remoto de QA, protocolo
  trazable). Este hijo no usa SSH/flash/deploy/TX.
- Sin recursos privados de otras sesiones: el build del arnés corre en
  `tools/ui_harness/build` de este worktree y usa la imagen
  `x6200-uiharness` del repo (la misma de los gates del CI).
