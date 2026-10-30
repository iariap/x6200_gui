# Registros y ruteo de audio — #363 (Fase A de #362)

**Fecha de la corrida:** 2026-10-29 (snapshot A0 22:00:24Z; cierre de lecturas 22:04Z; radio offline ~22:17Z).
**Base main verificada:** `cb9ea287548534ac5c4438c786b57deb436c7c40` (`git fetch origin main` → `origin/main`).
**Worktree:** `x6200-audio-route-363`, rama `audit/rx-audible-route-363` (head documental, sin cambios en `src/`).
**Radio:** X6200 con ROM de `main` (GUI md5 `80bbe5bd70b345e1e9a563d06d047a1c` / sha256 `4ee87aef…66b0`, la misma del deploy del #351) y BASE MCU **fw 1.0.6** (ver `logs/a0-preflight.txt`).

Este documento es el mapa del **registro compartido** que controla el ruteo de audio (`voice_rec`) y de la
cadena SoC→DAC/MUX→parlante. Separa **medido**, **código** y **sin fundar** (anexo A del registro de
hallazgos). No es una prueba física: la prueba física es A4 y **no se ejecutó** (ver `gates.csv`).

---

## 1. Registro 12 — `x6200_sple_atue_trx` (dirección I2C `0x0030`)

Fuente del mapa: `gdyuldin/X6200Control` (`include/aether_radio/x6200_control/low/control.h:33` define
`x6200_sple_atue_trx = 12`; bits en `:108-125`). La lib desplegada es **byte-idéntica** a la del paquete
de imagen v0.1.11 (`sha256 80c460e4…6df6`, ver §5), y su desensamblado coincide con esa fuente (REA, §6).

| bit (hex) | nombre en la lib | qué declara la lib | evidencia |
|---|---|---|---|
| `0x00002` | `x6200_sple` | split | código; el vendor tiene un wparam `sple` (OFF/ON) en `xparam.db` |
| **`0x00008`** | **`x6200_voice_rec`** | **"Toggle audio stream mic/speaker or RX/TX audio"** (`control.h:44`); único bit que cambia `audio_play_en()` | código + REA (record_set 0x34bc: `orr #8` / `bic #8`) |
| `0x00010` | `x6200_swrscan_trx` | barrido de ROE (TRANSMITE) | código |
| `0x00020` | `x6200_tune_flag` | flag de tuning | código |
| `0x01000` | `x6200_atue` | ATU enable (estado) | código; wparam vendor `atue` |
| `0x02000` | `x6200_atu_tune` | ATU tune (TRANSMITE) | código |
| `0x04000` | `x6200_modem_trx` | modem TX (SSTV/WeFax/JS8) | código |
| `0x08000` | `x6200_calibration` | calibración | código |
| `0x10000` | `x6200_power_off` | apagado por software | código |
| **`0x40000`** | **`x6200_iptt`** | **PTT (TRANSMITE)** | código + medición previa del proyecto (§76 de `docs/HALLAZGOS.md`: PTT por I2C con `iptt` dio `tx=true` en 7.100 MHz) |
| `0x100000` | `x6200_calibration_trx` | calibración TRX | código |
| `0x200000` | `x6200_bias_drive_off` | bias drive off | código |
| `0x400000` | `x6200_bias_final_off` | bias final off | código |

**Framing de escritura** (medido por desensamblado + traza I2C previa del proyecto):
I2C bus 0, esclavo `0x72`, transacción de **6 bytes**: `[addr_be16][arg_le32]`, con `addr = cmd * 4`; para
reg 12 ⇒ `0x00 0x30` + 4 bytes little-endian (REA: `x6200_control_cmd` en `0x275c` arma `rev16(cmd*4)` y
`ioctl(fd, I2C_RDWR=0x707)`; `x6200_control_idle` en `0x28f8` manda 218 B en una sola transacción).
La traza previa `/tmp/i2c-trace.log` (artefacto de una corrida anterior, no de esta — ver
`logs/i2c-trace-prior-artifact.txt`) muestra exactamente ese framing para otros registros.

### 1.1 Escritores del registro 12 en nuestra GUI (código, base `cb9ea287`)

Todas las funciones de la lib writes del registro son **RMW sobre la sombra RAM** (`get` + máscara + `cmd`):

| función de la lib | bit(s) | llamadores en `src/` (base de esta corrida) |
|---|---|---|
| `x6200_control_record_set()` | `0x8` | `src/audio.c:468` y `:473` (`audio_play_en`), `src/radio.c:782` (arranque de `radio_init`) |
| `x6200_control_ptt_set()` | `0x40000` | `src/radio.c:1368` y `:1372` (`radio_set_ptt`, bajo `radio_lock`) |
| `x6200_control_modem_set()` | `0x4000` | `src/radio.c:1400` (`radio_set_modem`, bajo `radio_lock`) |
| `x6200_control_atu_tune()` | `0x2000` | `src/radio.c:262`, `:276` (bajo `radio_lock`) |
| `x6200_control_atu_set()` | `0x1000` | `src/radio.c:724` (observer de `cfg.atu_enabled`) |
| `x6200_control_split_set()` | `0x2` | `src/radio.c:702` |
| `x6200_control_swrscan_set()` | `0x10` | `src/radio.c:1024`, `:1035` |
| `x6200_control_poweroff()` | `0x10000` | `src/radio.c:293` |

`audio_play_en(true)` hace, en este orden: `hmic_set(0)` → `imic_set(0)` → `record_set(true)`
(`src/audio.c:458-478`); `audio_play_en(false)` restaura `record_set(false)` → `hmic_set(params.hmic)` →
`imic_set(params.imic)`. **Medido en el binario desplegado:** en `0x3dca4/0x3dcac/0x3dcb4` están
`hmic_set(0)`, `imic_set(0)` y `record_set(1)` seguidos; el camino de apagado en `0x3dcbc` llama
`record_set(0)` y después repone las ganancias de micrófono (`0x3dcc8`+). Es decir: la única función de la
GUI que prende `voice_rec` **también cambia las ganancias HMic/IMic** y no es una vía RX neutral.

### 1.2 Semántica del bit `voice_rec` — qué está fundado y qué no

- **Fundado (código):** el bit 0x8 se llama `x6200_voice_rec` y la lib lo documenta como "toggle audio
  stream mic/speaker or RX/TX audio"; en nuestro árbol se usa **sólo** en `audio_play_en()` (mensajes de
  voz y grabador: `src/dialog_msg_voice.c`, `src/dialog_recorder.c`) y en el arranque (`radio.c:782`).
- **Documentado externo, NO medido en nuestro equipo:** el proyecto Mestre documenta para el X6200 que
  `voice_rec=1` selecciona **DAC del SoC → MUX → PA → parlante** y `0` el audio del receptor; que el MUX
  está en el MCU/FPGA y se controla por I2C; y que el PA se habilita con `rxvol_set() > 0`
  (<https://git.sa6anw.se/sa6anw/mestre/src/branch/main/audio-architecture.md>). Es una pista de un
  tercero (otro equipo, otra ROM): **no reemplaza la medición física en nuestro X6200**.
- **Sin fundar (anexo A):** cuál es el efecto eléctrico exacto en nuestro equipo; si conmuta el MUX del
  parlante, el del jack, o ambos; si el PA queda habilitado con `rxvol=7` (nuestro estado medido); y si el
  bit tiene efecto en RX sin PTT. La única forma de fundarlo es la prueba física A4 (prohibida sin A2/A3
  PASS), con restore verificado.

---

## 2. `x6200_control_get()` NO es readback: es una sombra de RAM (A2)

- **Código:** `src/low/control.c:466-469` — `return all_cmd.arg[cmd];` (cache en RAM; `all_cmd` se
  siembra en `x6200_control_init`). No hay llamada a I2C en esa función.
- **REA sobre la lib desplegada** (`logs/rea-lib-decompile.txt`):
  `x6200_control_get(0xc)` compila a un único load de `&DAT_000250ca + param*2` ⇒ **memoria del proceso**,
  cero I2C.
- `x6200_control_cmd()` escribe la sombra **antes** del ioctl y devuelve 0/1 sin que `record_set`,
  `ptt_set`, `atu_*`, etc. miren el resultado (`src/low/control.c:452-464`; REA `0x275c`): si el I2C falla,
  la sombra queda "actualizada" y el siguiente RMW parte de un valor falso.
- **Consecuencia:** `x6200_control_get(x6200_sple_atue_trx)` **no puede** usarse para saber si el BASE
  tiene `voice_rec` prendido. Tampoco hay API pública de readback del registro: las únicas lecturas I2C de
  la lib son `x6200_control_init()` (info/calibración/estado) y `x6200_control_get_fw_version()`
  (`0x2834`, lee el bloque de info). El proyecto documentó que **el BASE sí se puede leer** por I2C en
  ciertas direcciones (anexo: `docs/HALLAZGOS.md` §69: 0x2000 estado, 0x0000 info "1.0.6", 0x0100 calib,
  0x1000 work-params con la frecuencia), pero **no hay mapa verificado de dónde se refleja el registro 12**
  y leerlo en crudo desde un segundo proceso está prohibido por este issue.

### 2.1 Dueño y "clobber": por qué un guardián externo no puede ser dueño

- **Medido (radio):** único proceso con `/dev/i2c-0` abierto = `x6200_gui` (pid 4716); además es el único
  con `/dev/ttyS1` (`logs/a0-preflight.txt`). No hay otro lector/escritor del bus.
- **Código + REA:** `radio_thread()` llama `x6200_control_idle()` cada `IDLE_TIMEOUT = 1000 ms`
  (`src/radio.c:45,337-340`, mutex incluido en el binario: `0x2e350` está justo después del
  `pthread_mutex_lock` y del `cmp r3,#1000`). `x6200_control_idle()` reenvía **`sizeof(all_cmd)` = 218 B**
  (54 registros, reg 12 incluido) al BASE desde la sombra, y si falla reabre el bus y reintenta.
- **Consecuencia:** cualquier helper independiente que escriba `voice_rec` (o cualquier reg) es
  **pisado en <=1 s** por el bloque completo de la GUI, o antes si la GUI hace un setter. Un "guardián"
  externo no restaura nada de forma confiable: compite con el dueño real del bus. La propiedad
  serializada sólo existe **dentro del proceso GUI** (`radio_lock` + hilo de radio), y `audio_play_en()`
  ni siquiera toma ese lock (la secuencia `hmic/imic/record` corre desde el hilo de LVGL/UI).
- **Risk concreto del registro compartido:** el bit de PTT (`0x40000`) y el de ATU (`0x2000`/`0x1000`)
  viven en el mismo reg 12. Un RMW externo/crudo (o una sombra desincronizada) puede pisar PTT/ATU. La
  única medición propia del proyecto del PTT por I2C es la del §76 (`iptt` con fullband-tx dio `tx=True`),
  i.e. el bit **sí** transmite.

---

## 3. Restore / fail-safe: qué existe y cuánto tarda (A3)

| situación | mecanismo existente | ¿selectivo? | ¿<250 ms? | evidencia |
|---|---|---|---|---|
| cierre normal de `audio_play_en` | `record_set(false)` + repone hmic/imic | sí (bit 0x8) | sí, pero **depende de que el proceso siga vivo** | código `src/audio.c:473-477`; binario `0x3dcbc` |
| arranque de la GUI | `x6200_control_init()` reenvía `all_cmd` **resetado a defaults** (record=0) y luego `radio.c:782` `record_set(false)`; también reescribe calibración con "Open TX" | **no**: reescribe 54 registros | **no**: supervisor con backoff mínimo **2 s** + init de GUI **1,7–1,9 s** ⇒ **~4 s típico** | código `src/low/control.c:129-...`; `/usr/sbin/x6200_daemon` `delay_for()` 2/5/15/60 s (imagen v0.1.11, sha256 `07c431cc…9877`); `/mnt/app_logs/boot.log`: `GUI inicio 4873.1 s → GUI lista 4874.9 s` (1,8 s) |
| crash de la GUI | el supervisor `x6200_daemon` la relanza con backoff; **no hay proceso que restaure el bit antes** | no | no (2 s + init) | `/usr/sbin/x6200_daemon`; 3 reinicios medidos en `boot.log` (988,3 / 2201,0 / 3213,6 / 4873,1 s) |
| muerte del SoC / power-cut | el BASE conserva su estado interno hasta el reset; no hay watchdog de audio del lado Linux | no | no | sin fundar (anexo A) |
| entrada en TX | `ptt_set(true)` es otro RMW del **mismo** reg; no hay chequeo "record off antes de PTT" en la GUI | — | — | código `src/radio.c:1356-1373`; REA `ptt_set 0x37dc` |

**Conclusión A3:** no existe hoy un guardián independiente del proceso GUI con timeout <=250 ms: el único
camino de restore fuera del proceso es el reinicio de la GUI, que tarda segundos y reescribe todo el
register file (incluida la calibración de TX). Diseñar/probar ese guardián exige escribir el registro
compartido, que es exactamente lo que #363 prohíbe sin restore/readback probados ⇒ `A3_BLOCKED`.

---

## 4. DAC / PA / clocks del SoC (lo que sí está medido de nuestro equipo)

Snapshot de solo lectura (A0, 22:00Z; ver `logs/mixer-live.txt` y `logs/a0-preflight.txt`):

| control | valor medido | nota |
|---|---|---|
| `AIF1 DA0 Playback Volume` | `160,160` | playback app→radio (GUI, `src/audio.c:160`) |
| `Headphone Playback Volume` | `58` | `src/audio.c:158` |
| `Headphone Playback Switch` | `on,on` | |
| `AIF1 Data Digital ADC Capture Switch` | `on,on` | `src/audio.c:171` |
| `AIF1 DA0 Stereo Playback Route` | `Mix Mono` | `src/audio.c:170` |
| `AIF1 Slot 0 Digital DAC Playback Switch` | `on,on` | |
| `DAC Playback Switch` (numid 31) | **`on,on`** (medido) | asound.state guardado dice `off,off`; Mestre documenta que debe quedar **off**. **Discrepancia medida, causa no determinada** (anexo). |
| `DAC Playback Volume` | `0,0` | |
| `Mic1 Capture Switch` | `on,on` | captura desde el BASE (`src/audio.c:163`) |
| `Line In Capture Switch` | `off,off` | |
| `Mixer Capture Switch` | `off,off` | |
| `ADC Gain Capture Volume` | `3` | `src/audio.c:168` |

- **Ruta de la captura (código):** `src/audio.c:156-174` (`mixer_setup`) configura `Mic1` como entrada de
  captura con el comentario `// capture audio from radio`; `capture_device` es
  `alsa_input.platform-sound.stereo-fallback` (`src/audio.c:40`). **Medido:** source RUNNING
  (`pactl list short sources`), PCM capture `RUNNING` con owner PulseAudio; en #347 un `parec` de 2 s del
  source dio RMS −22,44 dBFS / pico −10,51 dBFS / 100 % de muestras ≠ 0 (evidencia previa, ver
  `physical-signal.md`).
- **Ruta de reproducción (código):** `play_stm` escribe `audio_play()` (`src/audio.c:326`) al sink
  `alsa_output.platform-sound.stereo-fallback` (`src/audio.c:37`); **medido:** sink RUNNING con un
  sink-input "X6200 GUI Play" s16le 1ch 44100 Hz del pid 4716 (22:04Z).
- **Clocks/DAC:** tarjeta `sun8i-a33-audio`, card0 device 0 `1c22c00.dai-sun8i-codec-aif1`; ambos PCM
  (capture y playback) `RUNNING`, owner PulseAudio (pids 330/331) — PulseAudio mantiene el PCM abierto.
  La dependencia de `pll-audio` (Mestre) es **externa** y no se midió por dentro; nuestro dato es que el
  PCM está abierto y corriendo.
- **PA:** la documentación externa (Mestre) dice que el PA del parlante se habilita con
  `x6200_control_rxvol_set() > 0`, y el proyecto ya tiene medido que el reg 14 (`rxvol`) es el nivel de
  audio de recepción y que `0` = mudo (`docs/HALLAZGOS.md` §64). En el snapshot el estado remoto declara
  `vol:7` (el registro 14 lo escribe `radio_vol_apply`, `src/radio.c:681-689`). **No está medido** que el
  PA del parlante esté efectivamente habilitado con 7; es hipótesis (anexo).

---

## 5. Artefactos, hashes y provenance

| artefacto | sha256 | origen |
|---|---|---|
| `libaether_x6200_control.so.0.15.3` (radio) | `80c460e403bfe77684c773a50a054bfe6c4a858753695a01167493e35e126df6` | `/usr/lib/…` de la radio; **idéntico** al del paquete de imagen v0.1.11 |
| `libaether_x6200_control.so.0.15.3` (imagen v0.1.11) | `80c460e4…6df6` | extraído de `images/x6200_r1cbu_v0.1.11-lu2dsx.tgz` |
| `x6200_gui` (radio) | `4ee87aefb389b6a6b5d2d90df2c51d0440aeff19ed4464d44649f48757dd66b0` / md5 `80bbe5bd70b345e1e9a563d06d047a1c` | `/usr/sbin/x6200_gui`; coincide con el deploy del #351 (base `fc18a831`) |
| `x6200_gui` (imagen v0.1.11) | `9521b9ee…2c58` | build anterior del mismo paquete |
| `x6200_daemon` (supervisor) | `07c431cceaf28ac3e5b7f630d36c70f7d04b9af3cb19f078c0728c7f53c99877` | shell script, `/usr/sbin/x6200_daemon` |
| imagen ROM v0.1.11 | `746fbc2587e3c375f6bbc2c6b2a32fd874bc3bebb3c125ead05300a16fb1b7ca` | `images/x6200_r1cbu_v0.1.11-lu2dsx.tgz` |
| imagen SD 2026-10-28 | `d3f22d806f5279d533ca86d712c903d911b378be139ea67735141fa06f496d0b` | `images/x6200_20261028_104820.img` (ROM de la corrida según `docs/PRUEBAS.md` fila 29/10) |
| vendor GUI `x6200_ui_v100` | `b76331c5aa859ae98dc1819b1079a3e40489e9986c9656e0dc2c553aeb28871b` | copia local del rootfs vendor (`refs/vendor-fw/`) |
| vendor MCU fw `.xgf` (2025-01-11) | `dca90387b70d9550448006310a69ea65625bec87f402c10df789deeb13ff12f3` | `refs/vendor-fw/x6200_mcu_20250111134652.xgf` (encriptado) |
| vendor `xparam.db` | `b87f51b4ff8310fd2a6f029dcf669b98e4bb7bcc0fed4f94676149a1c173eab0` | `refs/vendor-fw/xparam.db` |

**Provenance de la lib:** el paquete de Buildroot la fija en `v0.1.0`
(`br2_external/package/aether-x6200-control/aether-x6200-control.mk`); el checkout local del proyecto
(`refs/X6200Control`, v0.1.1 `0ee1ae3`) tiene la misma implementación de las funciones de este informe.
La evidencia fuerte es el **desensamblado de la lib desplegada** (REA, §6): los bits, máscaras, el tamaño
218 B del bloque y el framing I2C son los de la fuente citada.

---

## 6. Anclas de código/offsets usadas en este informe

- Lib desplegada (`80c460e4…`): `record_set` `0x34bc` (`orr/bic #8`), `ptt_set` `0x37dc`
  (`orr/bic #0x40000`), `cmd` `0x275c`, `get` `0x281c`, `idle` `0x28f8` (ioctl `0x707`, len `0xda`=218,
  addr `0x72`), `get_fw_version` `0x2834`.
- GUI desplegada (`4ee87aef…`): llamadas a `record_set@plt` en `0x2eb48` (radio_init: antes
  `mic/hmic/imic_set`, después `spmode`…), `0x3dcb4/0x3dcb8` (`audio_play_en` true: `hmic_set(0)`,
  `imic_set(0)`, `record_set(1)`), `0x3dcbc` (`record_set(0)`), `0x3dcc8`+ (restore hmic);
  `ptt_set@plt` en `0x2fbe4` (bajo mutex); `idle@plt` en `0x2e350` (justo después de mutex y del
  `cmp r3,#1000`).
- Firmware del BASE: **no analizable**. El binario del MCU sólo existe como `.xgf` encriptado en el
  rootfs vendor de la eMMC (medido en el proyecto: `docs/HALLAZGOS.md` §67/§69) y no hay fuente ni
  símbolos. Por eso "qué hace el MCU por dentro con 0x8" queda **unknown** declarado.
