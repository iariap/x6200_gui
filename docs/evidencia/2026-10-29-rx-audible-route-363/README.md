# #363 — Fase A de #362: gate REA + ruta RX audible (DAC/MUX) y restore seguro

**Fecha de la corrida:** 2026-10-29 (snapshot A0 `22:00:24Z`; cierre A5 `22:21:55Z`; host America/Argentina/Buenos_Aires, UTC−3).
**Issue:** [#363](https://github.com/iariap/x6200_gui/issues/363) · **Épico:** [#362](https://github.com/iariap/x6200_gui/issues/362) · **Predecesores citados:** [#347](https://github.com/iariap/x6200_gui/issues/347), [#359](https://github.com/iariap/x6200_gui/issues/359).
**Base `main` verificada al empezar:** `cb9ea287548534ac5c4438c786b57deb436c7c40` (`git fetch origin main` → `origin/main`).
**Worktree/rama:** `x6200-audio-route-363` / `audit/rx-audible-route-363` (PR documental; **sin cambios en `src/`**).
**PR documental:** [#366](https://github.com/iariap/x6200_gui/pull/366) (sin merge).
**Trabajo:** sólo lectura sobre la radio; cero escrituras de registros, cero TX/PTT/ATU, cero `audio_play_en`, cero flash, cero cambios de GUI/PulseAudio.

## Veredicto

| Gate | Estado | Resumen |
|---|---|---|
| **A0** preflight read-only | **PASS** | Snapshot completo; radio en RX (`tx=false`); BT apagado; sin crashlogs; audio/ALSA/PulseAudio medidos; canal remoto estable.
| **A1** mapa estático/REA | **PASS** (unknowns declarados) | reg 12/`0x30`, bit `0x8`, escritores, RMW sobre sombra, restore conocido, SoC/DAC/PA/clocks y captura. REA 6.7.0 + Ghidra 12.1.4 **ejecutados**; fw del MCU **encriptado** ⇒ MUX interno UNKNOWN.
| **A2** readback/arbiter real | **BLOCKED_SAFE_MUX** | No hay readback del reg 12 (`get` = sombra RAM) y no hay dueño serializado fuera de la GUI; `idle()` reescribe 218 B cada ≤1 s. |
| **A3** guardián/restore fail-safe | **A3_BLOCKED** | No existe guardián independiente ≤250 ms; el único restore es reiniciar la GUI (backoff 2 s + init ~1,8 s) reescribiendo defaults. No se puede probar sin ser dueño del registro.
| **A4** prueba RX física | **NOT_EXECUTED / PHYSICAL_OUTPUT_NOT_VERIFIED** | A0–A3 no PASS y no hubo instrumentación ni escucha autorizada; el MUX no se conmutó.
| **A5** restauración final | **PASS** | Cero escrituras del agente; snapshots inicial y final con `tx=false`, mismos sha256, sin sinks/módulos nuevos. (Hubo una caída transitoria de WiFi 22:16–22:21Z sin reinicio; se recuperó sola.)
| **GATE_A** | **`BLOCKED_SAFE_MUX`** | **#364 NO debe arrancar.** No hay `A_PASS`.

> **Resultado correcto del issue:** el gate no se puede acreditar con la evidencia disponible. No se simula PASS: se documenta el bloqueo concreto y qué haría falta para desbloquearlo.

## Qué se encontró (lo importante)

1. **El registro compartido no tiene readback real.** `x6200_control_get()` devuelve la sombra `all_cmd.arg[]` en RAM
   (código `src/low/control.c:466`, REA del binario desplegado `0x281c` con un solo load, sin I2C).
   `x6200_control_cmd()` actualiza la sombra **antes** del ioctl y no propaga el error
   (`src/low/control.c:452-464`; REA `0x275c`), así que la sombra puede quedar falsa.
2. **No hay dueño serializado fuera de la GUI.** Único proceso con `/dev/i2c-0`: `x6200_gui` (pid medido).
   `x6200_control_idle()` reescribe los 54 registros (218 B) desde la sombra cada ≤1000 ms
   (`src/radio.c:45,337-340`; REA `0x28f8`): cualquier escritor externo es clobbeado.
3. **El único restore es el reinicio de la GUI**, ≳4 s (backoff 2/5/15/60 s del supervisor +
   init 1,7–1,9 s medido en `boot.log`), y reescribe todo el register file con defaults, no un restore
   selectivo del bit.
4. **`audio_play_en()` no es una vía RX neutral:** prende `voice_rec` pero además pone HMic/IMic en 0
   (medido en el binario desplegado `0x3dca4/0x3dcac/0x3dcb4`; código `src/audio.c:458-478`).
5. **El firmware que implementa el MUX/PA** es el del MCU/FPGA del BASE y sólo existe **encriptado**
   (`.xgf`, 512 KB, sin strings legibles): no es analizable con REA/Ghidra ⇒ semántica interna **UNKNOWN**.
   La conmutación `voice_rec=1 → DAC del SoC al parlante` está documentada por el proyecto Mestre
   (externo, otro equipo), **no medida en el nuestro**.
6. **La captura Linux sí recibe señal real** del BASE (medido antes: RMS −22,44 dBFS), pero eso es
   RX→Linux, no Linux→parlante; no hay salida física nueva verificada en esta corrida.

## Entorno medido (A0, solo lectura)

| ítem | valor | fuente |
|---|---|---|
| Radio / kernel | X6200, `Linux 6.1.82 armv7l`, 4×Cortex-A7 @1008 MHz | `logs/a0-preflight.txt` |
| ROM/GUI | GUI sha256 `4ee87aef…66b0`, md5 `80bbe5bd70b3` (la del deploy del #351, base `fc18a831`); lib control sha256 `80c460e4…6df6` | idem |
| BASE | `BASE ver1 1.0.0`, fw **1.0.6** | `boot.log` (read-only) |
| Estado RX | LSB **7.143 MHz**, `vol=7`, `nb=1 nr=1` (nb_level 5/nr_level 40), DNF 0, `tx=false` | GET remoto ×2 + `/mnt/params.db` |
| Bluetooth | apagado (`Powered: no`), sin dispositivos | `bluetoothctl show` |
| Audio | sink/source `platform-sound.stereo-fallback` RUNNING; PCM capture/playback RUNNING (owner PulseAudio); mixer en tabla de `registers.md` §4 | `pactl`, `/proc/asound`, `amixer` |
| Proceso / bus | GUI pid 4716, 13 hilos, único dueño de `/dev/i2c-0` y `/dev/ttyS1` | `/proc/*/fd` |
| Crashlogs | 0 archivos de crash; última GUI estable | `/mnt/app_logs`, `/mnt/lastbreath` |

## Artefactos de esta evidencia

- [`reverse-engineering.md`](reverse-engineering.md) — sesión REA (capabilities/analyze/decompile), hashes, offsets, binario vendor, fw MCU encriptado.
- [`registers.md`](registers.md) — mapa del reg 12/bit `0x8`, escritores, sombra vs readback, dueño/clobber, restore, DAC/PA/clocks.
- [`gates.csv`](gates.csv) — A0–A5 con estados, clase de evidencia y razones.
- [`commands.log`](commands.log) — comandos y exit codes reales (IP/MAC anonimizados).
- [`physical-signal.md`](physical-signal.md) — qué señal física hay medida, qué **no**, y por qué `PHYSICAL_OUTPUT_NOT_VERIFIED`.
- [`handoff-b.md`](handoff-b.md) — contrato propuesto SRC/PCM/MUX/restore para #364 y **por qué no está validado**.
- `logs/` — crudos: A0/A5, REA (doctor/capabilities/decompile), mask-test 2009 casos, mixer, hashes, refs del supervisor, strings/símbolos del vendor, traza I2C previa.

## Limitaciones que no se ocultan

1. **A4 no se ejecutó** (regla del issue: sólo con A0–A3 PASS). Sin instrumentación física ni escucha
   autorizada, un PASS de audio sería falso.
2. **La caída transitoria de WiFi** (22:16–22:21Z) impidió un snapshot en ese lapso; la radio volvió a
   responder **sin reinicio** (mismo `boot=1fa5d1ea`), y el snapshot final A5 se tomó a las 22:21:55Z.
3. **El firmware del MCU está encriptado**: no se puede afirmar qué hace el BASE con el bit al entrar en
   TX, al resetear o al apagar. Queda en el anexo A de `docs/HALLAZGOS.md`.
4. **La traza I2C de `logs/` es un artefacto previo** que estaba en `/tmp` de la radio (no la generó esta
   corrida); se usa sólo como corroboración del framing y así se declara.
5. REA `doctor` marca `node 22.23.1` por debajo del mínimo declarado (`>=22.23.4`); las operaciones usadas
   (capabilities/analyze/decompile) corrieron con **exit 0** y Ghidra 12.1.4 ok — se registra igual.

## Reproducción (resumen)

```sh
# base
git fetch origin main && git rev-parse origin/main           # cb9ea287…
# A0 solo-lectura (SSH, sin escrituras): date/model/hashes, /proc/asound, pactl, amixer,
#   GET remoto {"cmd":"get"}, bluetoothctl, crashlogs, /proc/*/fd para el dueño de i2c-0
# A1/REA
node refs/rea/scripts/rea.mjs doctor | capabilities
node refs/rea/scripts/rea.mjs analyze .qa363/rom/libaether_x6200_control.so.0.15.3 --provider ghidra
node refs/rea/scripts/rea.mjs decompile .qa363/rom/libaether_x6200_control.so.0.15.3 \
     --procedure x6200_control_record_set --provider ghidra     # get/cmd/idle/ptt_set idem
python3 .qa363/mask_test.py                                      # 2009 casos, PTT intacto
# validación del PR documental
asf/scripts/apps/x6200/check-matrix.sh main HEAD /home/pablo/desa/x6200/x6200-audio-route-363
python3 asf/scripts/risk.py x6200 <base> <head>
```

## Próximo paso / condición de desbloqueo

**#364 no arranca.** Para desbloquear A hace falta una fase nueva que demuestre, con evidencia física o
de protocolo del MCU: (a) **readback real** del estado de `voice_rec` (o una fuente de verdad equivalente),
(b) **dueño serializado** del reg 12 con interlock de TX (hoy el único que puede escribir es el proceso
GUI), y (c) **restore selectivo ≤250 ms** probado incluso con crash de la GUI. Si eso no es posible sin
tocar el firmware del BASE (encriptado) o sin instrumentación física, el gate correcto seguirá siendo
`BLOCKED_SAFE_MUX`.
