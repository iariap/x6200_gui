# Mapa de la ruta de audio RX — #362 (ROM Linux completa)

**Alcance:** mapa físico/lógico `BASE → captura Linux → SpeexDSP → DAC del SoC → MUX del BASE → parlante`, con
**archivo:línea** y separación explícita entre **código**, **medido (fase previa)** y **UNKNOWN**. Esta fase no mide
física: no se conmutó el MUX ni se tocó la radio. Ver [`croquis.md`](croquis.md) para los diagramas.

**Clases de evidencia usadas en este documento:**

- **CODE** — leído en el árbol que se compila: `x6200_gui` (main `6850d671`), `AetherX6200Buildroot` (commit
  construido `b44eb812`), `X6200Control` (paquete del build, v0.15.3) y el kernel 6.1.82 del build.
- **MEASURED-PRIOR** — medido en la radio en fases anteriores ya integradas en `main` (#347/#351/#363/#369/#371);
  se cita el documento, **no se repite** el sondeo.
- **UNKNOWN** — no verificado; no se especula.

---

## 1. Cadena RX completa (BASE → Linux → procesamiento)

| # | Tramo | Quién lo implementa | Evidencia (archivo:línea) | Estado |
|---|---|---|---|---|
| R1 | RF → audio demodulado de recepción | **BASE** (RF/DSP propietario) | No hay fuente ni esquema; el firmware del MCU/FPGA está encriptado (`.xgf`, sha256 `dca90387…12f3`) | **UNKNOWN** (interno) |
| R2 | Audio RX del BASE → pin **MIC1** del códec A33 | **BASE** (cableado de placa) | El árbol configura **Mic1** como entrada de captura: `x6200_gui/src/audio.c:163` (`amixer sset 'Mic1',0 0,0 cap`), `:165` (`Mic1 Boost`); el driver expone `SND_SOC_DAPM_INPUT("MIC1")` (`build/build/linux-6.1.82/sound/soc/sunxi/sun8i-codec-analog.c:475`) | **CODE** del lado Linux; el net exacto MIC1↔BASE es **UNKNOWN** (sin esquemático) |
| R3 | MIC1 → Mic1 Amp → ADC Mixer → ADC | códec A33 (ASoC mainline) | `sun8i-codec-analog.c:223-309` (ADC, mixers y rutas `Mic1 Capture Switch`), controles `Mic1 Capture Switch` / `Mic1 Boost Volume` (`:170,:211`); `sun8i-codec.c` (ADC/AIF1) | **CODE**; ruta armada por `mixer_setup()` |
| R4 | ADC → AIF1 AD0 → DAI I2S → simple-audio-card | ASoC + DT | Board DTS `&codec`/`&dai`/`&sound` (`br2_external/board/X6200/dts/sun8i-r16-x6200.dts:223-229,507-513`); nodo `sound` en `sun8i-a33.dtsi:183-195` (`simple-audio-card,name = "sun8i-a33-audio"`); DTB compilado `images/sun8i-r16-x6200.dtb` sha256 `4d73f8a5…cc38` | **CODE** + **MEASURED-PRIOR**: `card0 = sun8i-a33-audio`, PCM capture/playback `RUNNING` (#363 A0) |
| R5 | ALSA hw:0,0 capture | ALSA lib 1.2.8 + kernel | `pcm.mixin`/`pcm.mixcapture` en `rootfs-overlay/etc/asound.conf:34-83`; PulseAudio toma el PCM | **CODE** + **MEASURED-PRIOR** (#363 A0) |
| R6 | Source PulseAudio | PulseAudio 16.1 (modo sistema, `S12pulseaudio`) | `x6200_gui/src/audio.c:40` (`capture_device = "alsa_input.platform-sound.stereo-fallback"`); nombre derivado del device platform `sound` + perfil stereo-fallback de PulseAudio (comportamiento estándar de PA) | **CODE** + **MEASURED-PRIOR** (source `RUNNING`) |
| R7 | Captura → copia y entrega | GUI | `src/audio.c:74-83` (`capture_deliver`): `audio_enh_process()` → `screen_rec_put_audio_samples()` → `dsp_put_audio_samples()`; `read_callback()` en `:85-146` | **CODE** |
| R8 | **SpeexDSP** (notch → AF leveler → NR) | GUI + `libspeexdsp` 1.2.1 | `src/audio_enhancement.c:208` (`speex_preprocess_state_init(441, 44100)`), `:490-538` (`speex_preprocess_run`), `:556-596` (`audio_enh_process`); tasas/trama en `audio_enhancement.h:27-28` y `audio.h:14-15`; dependencia en `CMakeLists.txt`/`src/CMakeLists.txt` y `readelf -d` del binario (`libspeexdsp.so.1`) | **CODE** + **MEASURED-PRIOR** (#351: delay 882 muestras=20 ms con NR, partición invariable) |
| R9 | Destino del PCM procesado | GUI | `screen_rec_put_audio_samples()` (recorder AVI) y `dsp_put_audio_samples()` (decoders/waterfall) — `src/audio.c:80-82`; **no hay llamada a `audio_play()` desde `read_callback()`** | **CODE**; coincide con `docs/AUDIO_ENHANCEMENT_GUIDE.md:28-40` |
| R10 | Vuelta al parlante | **NO EXISTE hoy** en la ROM | `play_stm` lo escribe sólo `audio_play()` (`src/audio.c:326,369`) y sus llamadores son voz/recorder/FT8/JS8/SSTV/TX; ver `commands.log` §4.3 | **CODE** (ausencia de ruta) |

> **Nota de no-confusión:** `platform-sound` es el *device* de plataforma del nodo `sound` del DT; **no** es el MUX
> físico del BASE. `stereo-fallback` es un perfil de PulseAudio. El nombre de la tarjeta es `sun8i-a33-audio`.

---

## 2. Cadena de reproducción (la que existiría para el DAC del SoC)

| # | Tramo | Quién | Evidencia | Estado |
|---|---|---|---|---|
| P1 | GUI escribe PCM al sink | GUI | `src/audio.c:37` (`play_device = "alsa_output.platform-sound.stereo-fallback"`); único escritor del stream: `pa_stream_write(play_stm, …)` en `src/audio.c:369` | **CODE** |
| P2 | Sink PulseAudio → ALSA hw:0,0 playback | PulseAudio | `system.pa` (rootfs overlay) + sink medido `RUNNING` en #363 A0 | **CODE** + **MEASURED-PRIOR** |
| P3 | AIF1 DA0 → DAC → Mixer → Headphone Route | códec A33 | `sun8i-codec-analog.c:229-231` (`Left/Right DAC`), `:248-299` (Mixer y `DAC Playback Switch`), `:319-389` (HP volume/switch, `Headphone Source Playback Route=DAC`, `Headphone Amp`, `HP`); `sun8i-codec.c` para el DAC/AIF1 | **CODE**; la GUI arma `Headphone=58` y `AIF1 DA0=160` (`src/audio.c:158-160`) |
| P4 | HP (pines) → enlace analógico → BASE | cableado de placa | Sin esquemático; el asound.state guardado deja `Headphone Source Playback Route=DAC` y `Headphone Playback Switch=true` (`rootfs-overlay/var/lib/alsa/asound.state`, controles 7/8 y 515); **discrepancia conocida**: `DAC Playback Switch` guardado `false,false` vs **medido `on,on`** en la radio (#363 `registers.md §4`, causa no determinada) | **UNKNOWN** (net/pin exacto) + **MEASURED-PRIOR** (mixer) |
| P5 | **MUX del BASE: fuente = RX o DAC del SoC** | **MCU/FPGA del BASE**, por I2C | Reg 12 bit `x6200_voice_rec = 0x00008` (`refs/X6200Control/…/low/control.h:113`; paquete del build: `build/build/aether-x6200-control-v0.1.0/include/…`); `x6200_control_record_set()` hace RMW sobre la sombra y `cmd(reg12)` (`…/src/control.c:69-81`); la GUI es el único llamador vía `audio_play_en()` (`src/audio.c:458-478`). Efecto físico documentado por un tercero (Mestre, otro equipo/ROM) y **no medido en nuestro ejemplar** | **CODE** del protocolo; **UNKNOWN** el efecto físico |
| P6 | Volumen del parlante | **BASE**, reg 14 `rxvol` | SDK: `x6200_control_rxvol_set()` = "Speaker volume" (`control.h:43`, implementación `src/control.c:64-67`); GUI lo escribe en `radio_vol_apply()` (`src/radio.c:869-873`); prior medido: `rxvol=0` deja mudo el RX (`HALLAZGOS §64`) | **CODE** + **MEASURED-PRIOR** |
| P7 | PA + selección parlante/auricular | **BASE**, reg 21 bit 5 `spmode` | SDK: `x6200_control_spmode_set(bool phone)` = "Toggle Speaker/Phone audio out" (`control.h:45`; impl. `src/control.c:83-88`); GUI lo aplica en `radio_init` (`src/radio.c:783`) y en Ajustes "Speaker mode" (`src/dialog_settings.c:1727-1765`, `src/radio.c:963-973`). Manual X6200 Rev.05: conector S/P con modo `EAR PHONE`/`SPEAKER` (#371 `wiring.md`) | **CODE** del control; semántica física exacta en nuestro equipo **UNKNOWN** |
| P8 | PA → parlante (4 Ω) o S/P | **BASE** | Manual: `Audio output 0.5 W (4 Ω)`, S/P AUDIO/NC/GND (#371 `wiring.md`); sin medición propia | **UNKNOWN** (no medido) |

**Conclusión del tramo P5:** la selección **BASE vs DAC del SoC no la controla ningún dispositivo/driver/DT/GPIO de
Linux**. El nodo `i2c@1c2ac00` del DT (donde vive el BASE) **no tiene hijos** y no hay nodo de MUX/amplificador/GPIO de
audio en el DTB compilado (verificación en `commands.log` §2.8/§2.9/§8.1/§8.2). El control es un `ioctl(I2C_RDWR)` crudo
a `/dev/i2c-0`, slave `0x72`, desde la lib de control (`build/build/aether-x6200-control-v0.1.0/src/low/control.c:37,48-50,70-94`),
con framing `[addr_be16][arg_le32]`, `addr = cmd*4` ⇒ reg 12 = `0x0030` (`…:445-456`). El readback del estado real **no
existe**: `get()` devuelve la sombra RAM (`…/src/low/control.c:459-462`).

---

## 3. Registros/controles del BASE que tocan audio (y su estado de verificación)

**Mapa de registros:** `refs/X6200Control/include/aether_radio/x6200_control/low/control.h:20-75` (enum de comandos,
reg = índice) y bits del reg 12 en `:108-125`.

| Registro | Campo | Función declarada por el SDK | Quién lo escribe en la GUI | Verificación |
|---|---|---|---|---|
| **12** (`0x0030`) | `x6200_sple` 0x2 | split | `radio_set_split` | CODE |
| **12** | **`x6200_voice_rec` 0x8** | "Toggle audio stream mic/speaker or RX/TX audio" (`control.h:44`) | sólo `audio_play_en()` (`src/audio.c:458-478`) | CODE; efecto físico **UNKNOWN** |
| **12** | `x6200_iptt` 0x40000 | **PTT (TRANSMITE)** | `radio_set_ptt` bajo `radio_lock` | CODE + medido en fases previas (`HALLAZGOS §76`) |
| **12** | `x6200_atue` 0x1000 / `atu_tune` 0x2000 | ATU estado / tune (TRANSMITE) | observers de `cfg.atu_enabled` y `radio_set_atu_tune` | CODE |
| **12** | `swrscan` 0x10, `modem` 0x4000, `calibration` 0x8000, `power_off` 0x10000, `calibration_trx` 0x100000, `bias_*` | varias | varios setters | CODE |
| **14** (`0x0038`) | `x6200_rxvol` | **"Speaker volume"** | `radio_vol_apply()` (`src/radio.c:869-873`) | CODE + **MEASURED-PRIOR** (`rxvol=0` mudo, `HALLAZGOS §64`); lectura real **R_INCONCLUSIVE** (#369) |
| **21** (`0x0054`) | `x6200_micsel_pttmode_chge_spmode_auxiqgen_sqlthr` bit 5 | **"Speaker/Phone audio out"** | `radio_init` y Ajustes "Speaker mode" | CODE; efecto físico **UNKNOWN** |
| **20** (`0x0050`) | `ling_loutg_imicg_hmicg` | ganancias line-in/line-out/mic | `radio_init` + observers (`src/radio.c:793-794`) | CODE; uso físico **UNKNOWN** |
| 0/0x100/0x2000 (lectura) | info/calibración/estado | lecturas del `init()` de la lib | `x6200_control_init()` (`…/src/low/control.c:130-160`) | CODE + **MEASURED-PRIOR** (`HALLAZGOS §69`: 0x0000/0x0100/0x1000/0x2000 leen) |

**Riesgo estructural:** `voice_rec`, `iptt` (PTT), `atue`/`atu_tune` (ATU) y `power_off` **comparten el mismo registro
12**. No hay comando de escritura por bit evidenciado: se escribe el word de 32 bits completo (`send_regs` de 6 bytes
por `I2C_RDWR`, `…/src/low/control.c:70-94,445-456`). Cualquier restore de `voice_rec` desde fuera de la GUI tiene que
hacer RMW de ese word, y el único valor "actual" disponible es una **sombra RAM** que la propia GUI actualiza antes de
saber si el I2C funcionó (`…:445-456`) y que reenvía entera cada ≤1 s (`idle()`, `…:473-481`; `src/radio.c:45,338`).

---

## 4. Controles ALSA vs MUX físico: la confusión a evitar

| Nombre que aparece en ALSA/mixer | Qué es realmente | Qué **no** es | Evidencia |
|---|---|---|---|
| `AIF1 DA0 Playback Volume` | ganancia digital AIF1→DA0 del códec A33 | no es el volumen del parlante | `src/audio.c:160`; DTB `sound` |
| `Headphone Playback Switch` / `Volume` | amplificador **HP del códec** del SoC | no es el PA del parlante del X6200 | `sun8i-codec-analog.c:319-389`; `src/audio.c:158` |
| `DAC Playback Switch`, `Headphone Source Playback Route` | mux interno de la **ruta de audio del codec** (JSON/ALSA DAPM) | no es el MUX BASE-vs-DAC del recibo | `sun8i-codec-analog.c:118-142,369-389`; asound.state |
| `Mic1 Capture Switch` / `Mic1 Boost Volume` | entrada MIC1 del códec (donde el BASE entrega el RX) | no selecciona fuente de parlante | `sun8i-codec-analog.c:170,211`; `src/audio.c:163-165` |
| `DAC Playback Volume` | nivel del DAC del códec | no es `rxvol` (reg 14 del BASE) | `sun8i-codec-analog.c:118-124` |
| `spmode` (reg 21), `rxvol` (reg 14), `voice_rec` (reg 12) | **controles del MCU/FPGA del BASE por I2C** | **no son controles ALSA** | `X6200Control`, ver §3 |

**Corolario:** abrir el PCM o ver el sink `RUNNING` **no** dice nada del MUX físico. La fuente de verdad del MUX
(**UNKNOWN** en nuestro ejemplar) sólo podría venir de un readback real del reg 12, de documentación OEM de una
ventana de lectura, o de una sonda física no invasiva (#371 P1). Ninguna existe hoy.

---

## 5. Qué parte del mapa está medida y qué no (declaración explícita)

| Afirmación | Estado | Base |
|---|---|---|
| El códec A33 y su simple-audio-card son la interfaz de audio del Linux del X6200 | **CODE + MEDIDO-PREVIO** | DT/ASoC + #363 A0 (`/proc/asound`) |
| La captura Linux recibe audio real del BASE y SpeexDSP lo procesa | **MEDIDO-PREVIO** (#347/#351) + **CODE** | RMS −22,44 dBFS #347; banco #351 |
| El PCM procesado va sólo a recorder/decoders, no al parlante | **CODE** (verificado esta fase) | `src/audio.c:74-83`; `commands.log` §4.2 |
| El DAC del SoC se puede escribir (sink/PCM `RUNNING`) | **MEDIDO-PREVIO** | #363 A0 |
| El DAC del SoC **suena** en el parlante con el MUX por defecto | **NO** (evidencia débil previa: `speaker-test` sin sonido 12/10, sin captura conservada) | `HALLAZGOS §1`; no repetido |
| `voice_rec=1` conmuta el MUX a DAC en **nuestro** X6200 | **UNKNOWN** (documentado por Mestre, otro equipo; firmware encriptado; sin readback) | #363 `registers.md §1.2` |
| Existe una ruta Linux→parlante que no escriba el reg 12 | **NO EXISTE** (análisis de todas las salidas disponibles en `alternatives.md`) | esta fase |
| Parlante/PA/S-P: nivel, impedancia, efecto del PA | **UNKNOWN** (requiere instrumentación; #371 P0/P2) | #371 |

**Datos que NO se deben usar como evidencia de este mapa:** un `pactl` con sink/source `RUNNING`; un `parec` (es
captura pre-procesamiento); escuchar A2DP (es otro sink); "suena" sin registro; documentación de otro equipo/ROM
(Mestre) como si fuera medición de nuestro ejemplar; la sombra `all_cmd.arg[]` como si fuera readback.
