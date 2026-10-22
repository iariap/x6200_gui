# Evidencia #160 — el teclado CW (`cw_send`) respeta el interlock de CB

**Fecha**: 2026-10-19. **Rama**: `factory/interlock-keyer-160` (base `ec5e2730`, trunk tip).
**HEAD de código**: `59871960` — *CW (#160): todos los caminos de TX de software pasan por el interlock de CB*.
**Binario ARM medido**: `md5 40d94ab49aff27ea0cda8ea06eb16745` (13.853.612 B, ELF32 ARM, gate #86 OK).
**Fuente (sello del banco)**: `md5 c92e8b8e4f22` (`src/` + `lvgl/src`).

**Alcance**: el banco QEMU **no tiene radio** (ni *flow* ni MCU), así que el campo `tx` del
estado de radio queda `false` por diseño. Lo que sí se mide por socket es el **keyer CW**:
`ax.busy` / `ax.sent` del estado de la app CW (ON AIR / keyer armado). El TX real es
`requires-radio` (receta al final). La lógica del interlock se prueba entera en la suite nativa.

## El defecto (medido en radio, issue #160)

Con `cb_tx_scope = 0` (default, sólo el SBC argentino 26,960–27,410 MHz), en **26.515 MHz**
(grid B, fuera del SBC) el `cw_send` **transmitió igual** (`tx:true`, sin `TX blocked`),
mientras que el control en 27.185 sí transmite y 28.074 no se ve afectado.

**Causa**: el interlock vivía sólo en `radio_set_ptt()` (`src/radio.c:1068`), y el keyer CW
cierra la llave por `radio_set_morse_key()` (`src/cw_encoder.c` → `src/radio.c`, un GPIO), que
**nunca pasaba por ahí**.

## 1. Inventario de caminos de TX y quién consulta el interlock

| Camino de TX | Entrada | `archivo:línea` | ¿Consultaba `cb_tx_allowed()`? |
|---|---|---|---|
| PTT de software (apps / CAT / PTT físico) | `radio_set_ptt()` | `src/radio.c:1094` | **Sí** (desde #146) |
| FT8 | `dialog_ft8.c:1241` → `radio_set_ptt()` | — | **Sí** (vía PTT) |
| Mensajes de voz | `dialog_msg_voice.c:357,388` → `radio_set_ptt()` | — | **Sí** (vía PTT) |
| CAT (`C_CTL_PTT`) | `cat.cpp:790` → `radio_set_ptt()` | — | **Sí** (vía PTT) |
| **Keyer CW** | `radio_set_morse_key()` (GPIO) | `src/radio.c:1183` (antes 1161) | **No → corregido acá** |
| PTT físico en CW (tecla PTT) | `main_screen.c:914` `radio_set_ptt` + `:919` `radio_set_morse_key` | — | El PTT sí; el morse **no → corregido** (choke point) |
| **ATU tune** (portadora a potencia plena) | `radio_start_atu()` → `x6200_control_atu_tune(true)` | `src/radio.c:707` (tune en `:276`) | **No → corregido acá** |
| **Barrido de ROE** (0,1 W, diagnóstico) | `radio_start_swrscan()` → `x6200_control_swrscan_set(true)` | `src/radio.c:742` | **No, a propósito** (ver §2) |
| `radio_set_modem()` (tx_player: JS8/SSTV/WeFax) | `tx_player.c:98` | `src/radio.c:1115` | **No → corregido acá** |
| PTT del micrófono (MCU) | `pack->flag.tx` del hardware | `src/radio.c:241` | **No controlable por software** |

## 2. Fix

`src/radio.c` — nuevo helper `radio_tx_allowed()` (`:711`) que centraliza
`cb_tx_allowed(subject_get_int(cfg_cur.fg_freq), params.cb_tx_scope.x)`:

- `radio_set_morse_key()`: rechaza **cerrar** la llave fuera de la ventana (soltarla siempre
  pasa: nunca se deja el keyer pegado). Es el *choke point* del GPIO: cubre el keyer y el PTT
  físico en CW (que llamaba a `radio_set_morse_key()` aunque `radio_set_ptt()` hubiera bloqueado).
- `radio_start_atu()`: el tune del ATU transmite a **potencia plena**, así que pasa por el mismo
  gate (mensaje + voz). **Decisión**: se gatea. Dejarlo afuera mantendría abierto el hueco de
  cumplimiento a potencia de operador, peor que el del keyer.
- `radio_set_modem()`: el **modo modem** es lo que keyea el TX de **SSTV/WeFax/JS8** (`tx_player`),
  otro camino de software que no pasa por `radio_set_ptt()`. Mismo gate (mensaje + voz).
- `radio_set_ptt()`: sin cambio de comportamiento (usa el helper).
- **Barrido de ROE**: **queda afuera a propósito**. Es un diagnóstico a **0,1 W** (piso de
  potencia del equipo) y es el único camino de TX que el owner necesita en CB (la F0 del #146 lo
  usó). Gatearlo quitaría la herramienta de verificación sin reducir de forma significativa el
  riesgo (0,1 W). Se registra como riesgo residual en PENDIENTES.

`src/cw_encoder.c` — el keyer consulta el interlock **directamente** (seam de test):

- `cw_encoder_send()`: antes de armar, si `cb_tx_allowed()` da false → mensaje
  `TX blocked: not a CB channel` + voz, y **no arma** (el encoder queda IDLE, la app no muestra
  ON AIR). Idéntico a `radio_set_ptt()`.
- `key_down()`: guarda por elemento (si el dial sale de la ventana a mitad del mensaje o del
  beacon, la llave deja de cerrarse; el aviso ya se dio al armar, así que acá es silencioso).
- Fuera de 26–28 MHz `cb_tx_allowed()` devuelve true: **las bandas de aficionado quedan igual**.

## 3. TDD nativo (determinista, sin radio)

Seam existente `tests/native/stubs_cwenc/` + `src/cb.c` real staged. El stub mueve
`cfg_cur.fg_freq` y `params.cb_tx_scope`, así que el test recorre la matriz de `cb_tx_allowed()`
**a través del keyer** (no un gate falso).

- `tdd-red.log`: el test contra el `src/cw_encoder.c` de la base `ec5e2730`: **5 fallos / 33
  comprobaciones** (26.515 keyeaba igual, sin mensaje).
- `tdd-green.log`: el mismo test con el fix: **33 comprobaciones, 0 fallos**, exit 0.

Casos: `scope 0` → 26.515 **bloqueado sin GPIO** (0 toggles, 1 mensaje + 1 voz), 27.185 y 28.074
permitidos; `scope 1` → grids A/B/D permitidos y el hueco A–B bloqueado; `scope 2` → todo
permitido. `cb_tx_allowed()` en sí está cubierto exhaustivamente por `tests/native/test_cb.c`.

## 4. Gates (`gates.log`)

| Gate | Resultado |
|---|---|
| Suite nativa completa + `-fanalyzer` | exit 0 (`native-host.log`) |
| Estáticos de la app (10) + `bash -n` | 10/10 OK (`check-matrix.log`) |
| Determinismo del arnés de UI | **PASS** (PNG idénticos por fixture; `determinism.log`) |
| A/B `AB-INT160` (`ec5e2730` vs `59871960`) | **PASS**, 89 escenarios **pixel-idénticos**, 913 contratos (`ab-int160.log`) |
| `contracts` | PASS — 913 checks, 0 fallos (`contracts.log`) |
| `audit` | 11 issues conocidos, **0 reproducidos**; 9810 objetos, 0 hallazgos estructurales (`audit.log`) |
| `themes` | PASS (WCAG 2.1 AA; `themes.log`) |
| `pytest` (unit) | **227 passed** (`pytest.log`) |
| `pytest -m e2e` | **13 passed** (`pytest-e2e.log`) |
| `ruff check` / `ruff format --check` / `ty check` | todo OK (`ruff-format-ty.log`) |
| Cross-compile ARM (dirclean + rebuild) | `100% Built target x6200_gui`, `cw_encoder.c.o` + `radio.c.o` incluidos (`arm-cross-build.log`); binario `md5 40d94ab49aff` |

**Pixel-idéntico**: el A/B es PASS con capturas idénticas porque el arnés de UI compila
`main_screen.c`/`cb.c` reales pero **mockea `radio.c`** (`mocks/mock_radio.c`) y **no compila
`cw_encoder.c`**: el fix del keyer/interlock no cambia píxeles.

**Nota del rebuild (#157)**: el `rsync -au` de buildroot **no** repone fuentes más nuevas; el
build válido se hizo con `make x6200-gui-dirclean && make x6200-gui-rebuild` y se verificó que
la copia staged del build tiene el fix (`tx_allowed` en `cw_encoder.c`, `radio_tx_allowed` en
`radio.c`).

## 5. Banco QEMU (gate #86), sin RF

`tools/captura-banco.sh` con **volumen privado** `x6200-build-output-int160` y **chroot privado**
`/home/pablo/desa/x6200/.vint160` (el volumen `x6200-build-output` y los chroots compartidos
estaban en uso por otras corridas). Gate #86: `OK: ... es el firmware del port (ELF32 ARM,
13853612 B, 3/3 marcas)`; `crash.txt` ausente.

Secuencia por socket (cliente `tools/remote/x6200-remote`), app **CW** abierta, **modo CW**
tras cada cambio de frecuencia (la banda recarga su modo: CB→AM, 10m→USB). Transcripción cruda
en `banco-transcript.txt`:

| Frecuencia | `cb_tx_scope` | `cw_send` → `ax.busy` / `ax.sent` | Veredicto |
|---|---|---|---|
| **26.515** (CB B, CH 1, fuera del SBC) | 0 | `busy:0`, `sent:0` en 6 muestras | **bloqueado** ✓ (`banco-blocked-26515.png`: `TX blocked: not a CB channel`, estado **READY**) |
| **27.185** (CB C, CH 19, dentro del SBC) | 0 | `busy:1`, `sent` 0→2→3→6→8 | **keyea** ✓ (`banco-onair-27185.png`: **ON AIR**) |
| **28.074** (10 m, fuera de la región) | 0 | `busy:1`, `sent` 0→2→3→6→8 | **keyea** ✓ (sin interlock, como debe) |

`tx` (estado de radio) queda `false` en las tres: el banco no tiene MCU que reporte el `flag.tx`.
El observable del keyer es `ax.busy`/`ax.sent`; el `tx:true` real se mide en la radio (abajo).

## 6. requires-radio — receta para el owner

Con el binario `40d94ab49aff` (o el HEAD desplegado) en la radio `192.168.0.236`:

1. Dejar `cb_tx_scope = 0` (default). Abrir la app **CW** (`{"cmd":"app","text":"CW"}`) y poner
   **modo CW** (`{"cmd":"mode","val":"CW"}`). **Recordar**: cambiar de frecuencia recarga el modo
   por banda → volver a poner CW después de cada cambio de frecuencia.
2. En **26.515 MHz** (`{"cmd":"freq","val":26515000}` + `mode CW`): `{"cmd":"cw_send","text":"PARIS PARIS"}`
   → la radio **NO debe transmitir**: `{"cmd":"get"}` debe dar `"tx":false`, la app debe mostrar
   **READY** y el mensaje **`TX blocked: not a CB channel`** (y la voz "TX blocked, not a C B
   channel"). Verificar en el medidor que no hay RF.
3. Control positivo en **27.185 MHz**: el mismo `cw_send` → `"tx":true`, **ON AIR**, y el medidor
   en PWR/SWR sigue el ritmo del CW.
4. Control en **28.074 MHz** (10 m): el `cw_send` → `"tx":true` (fuera de 26–28 MHz no hay
   interlock).
5. Opcional: `cb_tx_scope = 1` (por SQLite) → en 26.515/26.065/27.415 el `cw_send` **sí** debe
   transmitir; `cb_tx_scope = 2` → sin interlock.
6. Verificar el **PTT físico en CW**: con la tecla PTT en 26.515 (scope 0) **no** debe keyear
   (bloqueo en `radio_set_morse_key()`), y en 27.185 sí.
7. Restaurar frecuencia/modo/app al terminar.

## 7. PENDIENTES

- **requires-radio**: pasos de §6 (no ejecutados; el banco no tiene radio).
- **ATU tune**: gateado por código pero **no medido** en radio (no hay comando de socket para el
  ATU); confirmar en el equipo que en 26.515 con scope 0 el ATU no transmite.
- **Riesgo residual declarado**: el **barrido de ROE** sigue transmitiendo a 0,1 W fuera del SBC
  (se dejó afuera por decisión fundada, §2). Si el owner quiere cerrarlo, es un cambio de una
  línea en `radio_start_swrscan()`, con el costo de perder la herramienta de diagnóstico en CB.
- **PTT del micrófono (MCU)**: fuera del alcance del software (no pasa por Linux).

## 8. Índice de la evidencia

| Archivo | Qué es |
|---|---|
| `tdd-red.log` / `tdd-green.log` | RED (5 fallos) → GREEN (33/0) del keyer con `cb.c` real |
| `native-host.log` | suite nativa completa + `-fanalyzer` |
| `check-matrix.log` | estáticos de la app (10) + `bash -n` |
| `determinism.log` | gate de determinismo del arnés |
| `ab-int160.log` | A/B `AB-INT160` (reporte en `tools/ab_testing/reports/AB-INT160`, no commiteado) |
| `contracts.log`, `audit.log`, `themes.log` | contratos, auditoría y contraste de temas |
| `pytest.log`, `pytest-e2e.log` | pytest unit (227) + e2e (13) |
| `ruff-format-ty.log` | ruff check/format + ty |
| `arm-cross-build.log` | cross-compile ARM (dirclean + rebuild) |
| `banco-shot.log` | arranque del banco (gate #86, md5, fuente) |
| `banco-transcript.txt` | secuencia por socket: 26.515 / 27.185 / 28.074 |
| `banco-blocked-26515.png`, `banco-onair-27185.png` | 26.515 bloqueado (READY + `TX blocked`) vs 27.185 ON AIR |
| `banco-cw.png` | app CW abierta en el banco (binario final) |
