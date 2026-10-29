# A′ (#367) — readback real, dueño único del reg 12 y guardián fail-safe

**Fecha:** 2026-10-29 (corrida host 23:00–23:15Z; radio sólo lectura 23:03:25Z).
**Issue:** [#367](https://github.com/iariap/x6200_gui/issues/367) · **Épico:** [#362](https://github.com/iariap/x6200_gui/issues/362) · **Antecedentes:** [#363](https://github.com/iariap/x6200_gui/issues/363) / [PR #366](https://github.com/iariap/x6200_gui/pull/366).
**Base `main` verificada:** `cb9ea287548534ac5c4438c786b57deb436c7c40` (`git fetch origin main` → `origin/main`).
**Worktree/rama:** `x6200-rx-mux-guard-aprime` / `feat/rx-mux-guard-aprime`.
**Alcance:** infraestructura de seguridad; **el MUX no se activa** y todo el camino activo queda **OFF**.

## Decisión de gates

| Gate | Estado | Resumen |
|---|---|---|
| **G0** estático/readback | **PARCIAL → `R_BLOCKED_NO_HARDWARE_TRUTH`** | Hashes/offsets del vendor y framing I2C documentados por REA, pero **no existe lectura real del registro 12** en ningún artefacto autorizado (ver `readback-re.md`). El coordinador no escribe sin readback. |
| **G1** mocks/invariantes | **PASS** | Máscara preserva PTT/ATU/modem/otros bits en **≥2009 valores** (en la práctica 9188 comprobaciones del stage); **1000 eventos interleaved** (idle/PTT/ATU/record/RX-TX); `dac=true` jamás con TX/ATU/modem; readback mismatch no habilita nada. Log: `logs/native-tests.log`. |
| **G2** worker+guardian procesos reales | **PASS (host)** | Fork + socketpair + bus mock en memoria compartida; **100 repeticiones**; restore total medido con `CLOCK_MONOTONIC` desde el último heartbeat válido: **p50 101,7 ms · p99 103,3 ms · max 103,3 ms** (objetivo ≤250 ms). Casos: GUI responsiva y GUI bloqueada (+SIGKILL y verificación de bus libre). **Host, no hardware.** |
| **G3** build/ARM/lifecycle | **PASS (parcial declarado)** | Helper+module cross-compilados con el toolchain de la ROM (`arm-buildroot-linux-gnueabihf-gcc 12.3.0`): ELF32 ARM EABI5, y `--self-check` **corre bajo `qemu-arm-static`** con el sysroot de la ROM (exit 0). `src/radio.c` (GUI modificada) cross-compila a objeto ARM (exit 0). Lifecycle del helper probado: arranca INACTIVO, rechaza ARM sin readback, SIGTERM limpia el socket, exit 0. **No se ejecutó el link completo de la imagen GUI**: el sysroot de `AetherX6200Buildroot` presente en el host no trae `crt1.o/Scrt1.o` (medido) y no se toca el checkout base `rom-build`; se declara la limitación. |
| **G4** radio read-only | **NOT_EXECUTED (activo)** | Snapshot sólo-lectura: `tx=false`, `radio=RX`, fuente RX RUNNING, sink RUNNING, GUI/lib sha256 iguales, único dueño `i2c-0` = `x6200_gui`. **No se activó readback ni MUX**: no hay vía real identificada y este issue no escribe. Log: `logs/g4-radio-readonly.txt`. |
| **G5** A4 físico | **Deferido a #363** | No se hizo prueba acústica ni TX/PTT/ATU. Sigue bloqueando `A_PASS`. |
| **PR** | **DRAFT** | Gate de readback bloqueado ⇒ PR en draft, sin merge, sin cerrar #362/#363/#364. |

## Qué se implementó (todo OFF en producción)

1. `src/rx_audio_mux_guard.h/.c` — módulo puro y testeable:
   - máscara selectiva del bit `voice_rec=0x8` preservando **todos** los demás bits;
   - interlock: nunca entra DAC con `iptt=0x40000`, ATU (`0x1000/0x2000`), modem (`0x4000`), barrido (`0x10`), apagado (`0x10000`);
   - backend de bus abstracto con **readback obligatorio** (`rxmux_bus_ops_t`): sin `read_reg12` no hay escritura (`RXMUX_BLOCKED_NO_READBACK`);
   - protocolo IPC (magic/version/tipo/token/PID/starttime/monotonic/lease/base readback);
   - FSM del guardián: lease 100 ms, heartbeat ≤50 ms, acciones `REQUEST_RESTORE → STOP_WRITER → RESTORE_NOW`, y `REFUSE_NO_READBACK` si no hay lectura real;
   - restore de emergencia que **preserva PTT/ATU** y verifica que sólo cambió el bit 0x8.
2. `src/radio.c/.h` — único punto de conmutación nominal `radio_audio_mux_request(bool dac)`, bajo `radio_lock`, con chequeos de estado `RADIO_RX`, prioridad de audio generado (detector de dueño extranjero sobre la sombra) y **fail-closed** sin readback. **No se modificó `audio_play_en` ni `src/audio.c`.**
3. `src/rx_audio_mux_guard_main.c` — helper `x6200_audio_guard` (socket UNIX `/run/x6200_audio_guard.sock`, mínimo 0600, INACTIVO por defecto). Se construye e instala con el build real (target CMake + `install(TARGETS … DESTINATION sbin)`), pero **`x6200_daemon` no lo lanza todavía**: sin readback real no puede restaurar, y habilitarlo sería un falso fail-safe.
4. Tests: `tests/native/test_rx_audio_mux_guard.c` + `rxmux_guard_test_main.c`, integrados en `tests/native/run_native_tests.sh` (stage propio, host y `--ci-parity`).
5. `tests/native/radio_stubs.[ch]`: stub de `x6200_control_get` para el stage de `radio_thread` (la sombra sólo se usa como detector de dueño, nunca como readback).

## Lo que NO se hizo (por diseño)

- No se escribió el registro 12 ni se activó el MUX en la radio.
- No se llamó `audio_play_en()`, no se tocaron HMic/IMic, TX/PTT/ATU, calibración, flash ni `src/audio_enhancement.*`.
- No se usó I2C crudo desde un segundo proceso; no se agregó UI ni opciones.
- No se instaló nada en la radio; no se tocó el checkout base `rom-build` ni el harness ASF.

## Limitaciones declaradas

1. **Readback bloqueado**: el único estado físico real disponible es el canal serial del BASE (flow), que reporta muestras/flags/meters/teclas **pero no el valor del registro 12** (ver `readback-re.md`). Sin lectura real, por contrato, el coordinador y el guardián **no escriben**; por eso el PR queda DRAFT y #364 sigue bloqueado.
2. **G2 es host** con bus mock; no es PASS de hardware ni reemplaza la prueba física de #363.
3. **G3**: link completo de imagen GUI no ejecutado (sysroot de `AetherX6200Buildroot` incompleto en el host, medido); sí se verificó ELF ARM real del helper y objeto ARM de `radio.c`.
4. La radio cambió de estado entre corridas (frecuencia/volumen) por **acción del owner**, no del agente; esta corrida sólo hizo lecturas/GET.

## Handoff

- `readback-re.md` — REA del binario vendor y por qué no hay readback del reg 12.
- `ownership.md` — inventario de escritores, diseño del coordinador y contrato fail-closed.
- `guardian-contract.md` — protocolo/lease/restore y mediciones G2.
- `hardware-handoff.md` — qué debe probar #363/#365 y qué falta para `A_PASS`.
- `gates.csv`, `commands.log`, `logs/`.
