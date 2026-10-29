# Reverse engineering — REA, estático y binarios del X6200 (#363)

**Corrida:** 2026-10-29, host del agente (Fedora 44), sobre **copias de solo lectura** de los artefactos.
**No se flasheó, no se escribió hardware, no se reemplazó la GUI, no se tocó el MCU.**

## 1. Herramientas (disponibilidad real, ejecutadas en esta corrida)

| herramienta | estado | evidencia |
|---|---|---|
| **REA** `rea-agents@6.7.0` | **disponible y ejecutada**. Checkout oficial `refs/rea` (morluto/rea) compilado en el host (`npm run build:cached`, exit 0). Provider **Ghidra 12.1.4** (`tools/ghidra/ghidra_12.1.4_PUBLIC`), JDK 21 local. `rea capabilities` + `rea analyze` + `rea decompile` OK. | `logs/rea-capabilities.txt`, `logs/rea-lib-decompile.txt`; `rea doctor`: `node ok`, `ghidra ok`, `ghidra-installation ok` |
| toolchain ARM de la ROM | `arm-buildroot-linux-gnueabihf-objdump/readelf/nm` 12.3.0 | usado para disassembly de la lib y de la GUI vendor |
| sqlite3 (host) | lee `xparam.db` del vendor y copia read-only de `/mnt/params.db` | sólo lectura |
| Ghidra/Hopper/IDA | Ghidra 12.1.4 presente; Hopper/IDA no; REA cubre el decompilado | — |

**REA no se usó para el firmware del MCU** (ver §5): el único binario existente está encriptado.

## 2. Sujetos analizados (sha256 y por qué)

| sujeto | sha256 | rol |
|---|---|---|
| `libaether_x6200_control.so.0.15.3` **desplegada en la radio** | `80c460e403bfe77684c773a50a054bfe6c4a858753695a01167493e35e126df6` | implementa la escritura del reg 12; byte-idéntica a la del paquete de imagen v0.1.11 |
| `x6200_gui` **desplegada** | `4ee87aefb389b6a6b5d2d90df2c51d0440aeff19ed4464d44649f48757dd66b0` | GUI; binario del build de `main` `fc18a831` (ver #351); nuestra base `cb9ea287` sólo agrega docs sobre ese commit |
| `x6200_daemon` (supervisor) | `07c431cceaf28ac3e5b7f630d36c70f7d04b9af3cb19f078c0728c7f53c99877` | shell; backoff de reinicio de la GUI |
| `x6200_ui_v100` (GUI de fábrica, eMMC) | `b76331c5aa859ae98dc1819b1079a3e40489e9986c9656e0dc2c553aeb28871b` | UI vendor **con símbolos C++**; referencia de cómo controla el códec y el I2C |
| `x6200_mcu_20250111134652.xgf` (fw MCU/FPGA) | `dca90387b70d9550448006310a69ea65625bec87f402c10df789deeb13ff12f3` | **encriptado**, no analizable (§5) |
| `xparam.db` vendor | `b87f51b4ff8310fd2a6f029dcf669b98e4bb7bcc0fed4f94676149a1c173eab0` | tabla `wparam` (parámetros de trabajo del vendor) |
| paquete de imagen v0.1.11 | `746fbc2587e3c375f6bbc2c6b2a32fd874bc3bebb3c125ead05300a16fb1b7ca` | de donde se extrajeron lib/GUI/daemon para comparar |

Provenance de la lib: el paquete de Buildroot del proyecto la fija en `v0.1.0`
(`br2_external/package/aether-x6200-control/aether-x6200-control.mk`); el checkout local
`refs/X6200Control` está en v0.1.1 (`0ee1ae3`). La comparación decisiva es **contra el binario
desplegado**, no contra la fuente.

## 3. REA sobre la lib desplegada (sesión real, esta corrida)

Comandos (recortados; ver `commands.log` y `logs/rea-lib-decompile.txt`):

```sh
node refs/rea/scripts/rea.mjs capabilities                       # provider ghidra: available
node refs/rea/scripts/rea.mjs analyze   .qa363/rom/libaether_x6200_control.so.0.15.3 \
      --provider ghidra --format json                            # evidence_id ev_279ec087…
node refs/rea/scripts/rea.mjs decompile .qa363/rom/libaether_x6200_control.so.0.15.3 \
      --procedure x6200_control_record_set --provider ghidra     # y get/cmd/idle/ptt_set
```

Pseudocódigo REA de las funciones que importan (completo en `logs/rea-lib-decompile.txt`):

```c
void x6200_control_record_set(int on) {
  uVar1 = x6200_control_get(0xc);          /* reg 12 */
  uVar1 = on ? (uVar1 | 8) : (uVar1 & 0xfffffff7);
  x6200_control_cmd(0xc, uVar1);           /* escribe SIEMPRE, ignora el resultado */
}

void x6200_control_ptt_set(int on) {
  uVar1 = x6200_control_get(0xc);          /* MISMO reg 12 */
  uVar1 = on ? (uVar1 | 0x40000) : (uVar1 & 0xfffbffff);
  x6200_control_cmd(0xc, uVar1);
}

undefined4 x6200_control_get(int cmd) { return *(undefined4 *)(&DAT_000250ca + cmd*2); }

undefined4 x6200_control_cmd(int cmd, undefined4 arg) {
  ... local_18 = 0x72; local_10 = 6; ...               /* addr esclavo 0x72, 6 bytes */
  *(undefined4 *)(&DAT_000250ca + cmd*2) = arg;        /* PRIMERO la sombra RAM */
  iVar1 = ioctl(i2c_fd, 0x707, ...);                   /* I2C_RDWR; si falla, devuelve 0 */
}

void x6200_control_idle(void) {
  ... ioctl(fd, 0x707, [{addr=0x72, buf=&DAT_000250c8, len=0xda}]);  /* 218 B = 54 regs */
  /* si falla: close, usleep(1000), open("/dev/i2c-0"), reintenta */
}
```

**Hechos que fija el REA (no inferencia):**
1. `get` = sombra en memoria del proceso, **sin I2C** ⇒ no es readback del BASE.
2. `record_set` y `ptt_set` hacen RMW sobre el **mismo** registro 12; los bits son 0x8 y 0x40000.
3. `cmd` escribe la sombra **antes** del I2C y no propaga el error ⇒ sombra falsa posible.
4. `idle` reescribe **218 B (54 registros)** desde la sombra; si falla reabre el bus y reintenta.
5. `x6200_control_get_fw_version` (`0x2834`) es la única lectura I2C expuesta, y lee el bloque de info.

## 4. GUI desplegada: llamadas al registro compartido (offsets del binario)

Disassembly con el objdump del toolchain de la ROM sobre `x6200_gui` (`4ee87aef…`):

| offset | instrucción | mapeo a la fuente |
|---|---|---|
| `0x2eb48` | `bl x6200_control_record_set@plt` con `r0=0`, precedido de `mic/hmic/imic_set` y seguido de `spmode/vox…` | `src/radio.c:782` (`radio_init`) |
| `0x2e350` | `bl x6200_control_idle@plt` tras `cmp r3,#1000` y `pthread_mutex_lock` | `src/radio.c:337-340` (`IDLE_TIMEOUT=1000`) |
| `0x3dca4/0x3dcac/0x3dcb4` | `hmic_set(0)`, `imic_set(0)`, `record_set(1)` (tail-call) | `src/audio.c:462-469` (`audio_play_en(true)`) |
| `0x3dcbc` | `record_set(0)`; después repone `hmic` | `src/audio.c:473-477` (`audio_play_en(false)`) |
| `0x2fbe4` | `ptt_set` inmediatamente después de `pthread_mutex_lock` | `src/radio.c:1368-1372` (`radio_set_ptt`) |

Esto confirma sobre el **binario realmente desplegado** lo mismo que dice la fuente del repo.

## 5. Firmware del MCU/FPGA del BASE: por qué no se puede hacer REA de él

- El BASE se controla por I2C (`0x72`), pero su firmware **no está en el rootfs Linux**: el único
  binario que lo contiene es `/usr/firmware/x6200_mcu_20250111134652.xgf` (524.288 B), copiado del
  rootfs vendor de la eMMC (medido por el proyecto: `docs/HALLAZGOS.md` §67). En esta corrida se
  verificó sobre la copia local: `file` = `data`, 4.793 strings, ninguno legible (blob encriptado
  con formato propio `.xgf`); sha256 en §2. **No hay clave ni formato documentado** ⇒ el firmware que
  implementa el MUX/PA/clocks **no es analizable** con REA ni con Ghidra.
- Consecuencia honesta: **la semántica física de 0x8 dentro del MCU es UNKNOWN**. Lo único analizable es
  el lado Linux (quién escribe el bit y con qué protocolo), que es lo que cubre este informe.

## 6. Binario de fábrica `x6200_ui_v100` (referencia, no prueba de nuestro equipo)

- ELF ARM 32-bit **no stripped** con símbolos C++: `BaseCommIIC::iicWrite` (`0xe3070`),
  `BaseCommIIC::iicRead` (`0xe2f70`), `BaseCommIIC::workParamAddr` (`0x29308`),
  `BaseCommIIC::updateWorkParams` (`0xe33b8`), `parseParamItem` (`0xdf594`),
  `basebandInfo` (`0x29350`), `updateCalibrateData` (`0xe6220`), `slot_recieveParamItem` (`0xe6158`),
  `DialogVoiceRec`, `XAudioRecorder`, `XAudioPlayer`, `FTxModemAudio`, `AudioWorker`.
- **Medido en strings del binario:** la UI vendor configura el códec con `amixer`:
  `amixer -c 0 sset 'DAC',0 on`, `… 'Mic1',0 3`, `… 'Mic1',0 unmute`, `… 'Mic1 Boost',0 0`,
  `… 'Mic2',0 mute`, `amixer cset numid=19 … 'Mic1 Capture Switch' on`,
  `amixer cset numid=9 … 'AIF1 Slot 0 Digital DAC Playback Switch' on` (ver `logs/vendor-gui-strings.txt`).
  Es el mismo juego de controles que configura nuestro `mixer_setup()` (`src/audio.c:156-174`).
- `xparam.db`: tabla `wparam` con 192 parámetros de trabajo; entre ellos `sple` (OFF/ON), `atue`,
  `rxvol`, `monilevel`, `micsel`, `pttmode`, `hmicg`, `imicg`. **No hay** un wparam llamado
  `voice_rec`/`record` ⇒ el vendor expone el estado de audio por otra vía (work params + comandos), no
  con un rótulo equivalente al de la lib Aether.
- REA también se ejecutó sobre el vendor (`analyze` exit 0, evidence_id `ev_f1d7f5…`): `updateWorkParams`
  arma el paquete de parámetros y `parseParamItem` compara nombres (`"vfoa:freq"`, `"vfoa:agc"`, …).
  `workParamAddr`/`basebandInfo` caen en un `qt_assert` (thunks/inlineados, igual que notó el proyecto en
  §67/§76) ⇒ **el mapa completo workparam→dirección no quedó resuelto en esta corrida** y no hace falta
  para el gate: el punto de seguridad es que el **único** que puede leer/escribir el reg 12 en nuestra
  ROM es el proceso GUI, y no hay readback del registro.

## 7. Hechos vs hipótesis vs unknowns

| afirmación | clase | evidencia |
|---|---|---|
| reg 12 = `sple_atue_trx`, addr I2C `0x30`, 6 B `[addr_be16][arg_le32]` | **medido/código** (desensamblado del binario desplegado) | §3, §4, traza I2C previa |
| bit 0x8 = `voice_rec`; 0x40000 = `iptt`; comparten registro | **código** + REA | §3, `low/control.h:108-125` |
| `get` = sombra RAM; sin readback real del reg 12 | **código** + REA del binario desplegado | §3 |
| `idle` reescribe 218 B (54 regs) cada ≤1 s | **código** + REA + binario GUI | §3, §4 |
| único proceso con `/dev/i2c-0` = `x6200_gui`; sin otro dueño | **medido** en la radio | `logs/a0-preflight.txt` |
| `audio_play_en` cambia ganancias HMic/IMic además del bit 0x8 | **medido** en el binario + código | §4 |
| el supervisor reinicia la GUI con backoff 2/5/15/60 s | **medido/código** (script en la imagen) | `registers.md` §3 |
| `voice_rec=1` conmuta el MUX del parlante al DAC del SoC | **hipótesis externa (Mestre)**, no medida en nuestro equipo | §1.2; anexo A de `HALLAZGOS.md` |
| el PA del parlante necesita `rxvol>0` | **hipótesis externa** + medición previa del proyecto de que `rxvol`=0 mutea | §4 |
| el MCU restaura el bit al entrar en TX o al resetear | **unknown** (firmware encriptado, sin readback) | §5 |
| la captura Linux recibe el audio del BASE después de NR/DFL | **no medido**; sí está medido que el source entrega señal real y que la entrada es `Mic1` ("capture audio from radio") | `registers.md` §4, `physical-signal.md` |
