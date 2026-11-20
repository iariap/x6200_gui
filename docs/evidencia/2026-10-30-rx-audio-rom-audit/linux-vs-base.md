# Linux vs BASE — qué controla la ROM que compilamos y qué queda del lado propietario (#362)

**Pregunta:** ¿qué partes de la ROM Linux completa controlamos (y podríamos cambiar) y qué queda en el MCU/FPGA
propietario del BASE, fuera del alcance de Linux?

**Respuesta corta:** controlamos **todo el lado Linux** (kernel 6.1.82, device tree, U-Boot, Buildroot/rootfs,
ASoC/ALSA, PulseAudio, GUI, SpeexDSP, supervisor, stack BT/USB). **No controlamos** el MCU/FPGA del BASE: su firmware
existe sólo como binario encriptado, no hay fuente ni documentación de su protocolo interno, y ahí viven la selección
de audio (MUX), el PA y el estado físico del parlante. Linux sólo lo gobierna por **I2C crudo** (`/dev/i2c-0`, slave
`0x72`, registro = comando × 4) y por **UART** (`/dev/ttyS1`). Esa frontera es la que bloquea la ruta segura al parlante.

---

## 1. Matriz de propiedad

| Componente | ¿Quién lo define? | ¿Lo controlamos / podemos cambiarlo? | Evidencia |
|---|---|---|---|
| Kernel Linux | ROM (Buildroot 2023.02.11), Linux **6.1.82** | **Sí**: defconfig propio, parches de board, se compila en el build | `br2_external/configs/X6200_defconfig:23-29`; `build/build/linux-6.1.82/Makefile:1-5` |
| Device Tree | board `br2_external/board/X6200/dts/sun8i-r16-x6200.dts` | **Sí**: nodos SoC, codec, I2C, GPIO, etc. | DTS completo; DTB construido `4d73f8a5…cc38` |
| U-Boot + boot script | board (`u-boot/…`, `boot.cmd`) | **Sí** | `X6200_defconfig:184-201`; `boot.cmd:1-5` |
| Rootfs / Buildroot | `X6200_defconfig` + overlay + paquetes | **Sí**: paquetes, servicios, config de PA, asound, scripts | `X6200_defconfig` completo; `rootfs-overlay/` |
| ASoC del SoC (A33) | mainline: `sun8i-codec`, `sun8i-codec-analog`, `sun4i-i2s`, `simple-audio-card` | **Sí** (es mainline, parcheable) — hoy **sin parches propios de audio** | `sound/soc/sunxi/sun8i-codec-analog.c`; grep de audio en `commands.log` §2.4 |
| ALSA lib/plugins/utils | Buildroot | **Sí** | `X6200_defconfig:39-49,99` |
| PulseAudio + módulos | Buildroot **16.1** en modo sistema | **Sí**: `system.pa`, `system.pa.d/btaudio.pa`, `S12pulseaudio` | `rootfs-overlay/etc/pulse/…`; `commands.log` §3.2-3.6 |
| GUI + SpeexDSP | repo `x6200_gui` + paquete `speexdsp 1.2.1` | **Sí**: `src/`, `rootfs/`, CMake; binario construido `4ee87aef…66b0` | `src/audio.c`, `src/audio_enhancement.c`; `readelf -d` |
| `x6200_daemon` (supervisor) | repo `x6200_gui` (`rootfs/usr/sbin/x6200_daemon`) | **Sí** (script; hoy sólo reinicia la GUI) | `rootfs/usr/sbin/x6200_daemon` |
| Librería de control (`X6200Control`) | upstream `gdyuldin/X6200Control`, paquete Buildroot v0.1.0 (lib **v0.15.3**) | **Sí** en cuanto a empaquetado/versión; **no** es nuestra: es GPL y describe el protocolo del BASE | `br2_external/package/aether-x6200-control/*.mk`; `build/build/aether-x6200-control-v0.1.0/CMakeLists.txt:3-5` |
| BT (bluez5, bluez-alsa) / USB | Buildroot + kernel | **Sí** (A2DP, USB audio host) | `X6200_defconfig:50,135-141`; `commands.log` §2.1 |
| **MCU/FPGA del BASE: firmware** | **Xiegu** — binario encriptado | **NO**: sin fuente, sin símbolos, no analizable | `refs/vendor-fw/x6200_mcu_20250111134652.xgf` (512 KB, sha256 `dca90387…12f3`); #363 §A1 |
| **MUX de audio del BASE (BASE vs DAC)** | **MCU/FPGA** | **NO** desde Linux; sólo indirecto por I2C reg 12 bit `voice_rec` | `audio-route-map.md` §2-3; `commands.log` §2.8/§8.1/§8.2 |
| **PA / volumen del parlante / `spmode`** | **MCU/FPGA** | **NO**; sólo indirecto por regs 14 y 21 | `commands.log` §3.8/§5.5; `audio-route-map.md` §3 |
| **Estado físico del MUX/PA** | **MCU/FPGA** | **NO** hay fuente de verdad desde Linux (readback real inexistente) | #369 `R_INCONCLUSIVE`; `commands.log` §5.1 |

---

## 2. Dónde está exactamente la frontera física

```text
        LADO LINUX (lo compilamos)                      FRONTERA                 LADO PROPIETARIO DEL BASE
┌────────────────────────────────────────┐   ┌──────────────────────┐   ┌──────────────────────────────────┐
│ CPU A33/R16 · kernel 6.1.82 · DT        │   │ códec A33 (pines)    │   │ MCU/FPGA + RF + PA + MUX         │
│ ASoC sun8i-codec/-analog + i2s          │◄─►│ MIC1  ← audio RX     │◄─►│ firmware .xgf encriptado         │
│ ALSA hw:0,0 · PulseAudio 16.1           │   │ HP    → audio DAC    │   │ reg 12/14/21 vía I2C slave 0x72  │
│ GUI + SpeexDSP · supervisor             │   │ I2C0, UART3 (ttyS1)  │   │ sin driver/DT/GPIO de Linux      │
└────────────────────────────────────────┘   └──────────────────────┘   └──────────────────────────────────┘
        [C] controlado / reproducible             nets exactos [U]              [B] caja negra (firmware encriptado)
```

El DT del board declara `&codec`/`&dai`/`&sound` (`sun8i-r16-x6200.dts:223-229,507-513`) pero **no** declara ningún
dispositivo del BASE colgando de `i2c0` (el nodo `i2c@1c2ac00` no tiene hijos en el DTB compilado) ni un nodo de MUX,
amplificador o `gpio-hog` de audio. Es decir: **Linux controla el códec del SoC y su mezcla interna; el ruteo al
parlante lo decide el BASE, y Linux sólo le manda números por I2C**.

---

## 3. Superficies de control Linux→BASE (las únicas que existen)

| Superficie | Detalle | Evidencia |
|---|---|---|
| **I2C0 crudo** `/dev/i2c-0` | slave `0x72`; `ioctl(I2C_RDWR)`; escritura = 6 bytes `[addr_be16][arg_le32]` con `addr = cmd*4` (reg 12 ⇒ `0x0030`); lectura = selector 2 B BE + N bytes (`get_regs`) | `build/build/aether-x6200-control-v0.1.0/src/low/control.c:37,48-50,70-94,96-126,445-456`; framing corroborado por REA/traza en #363 |
| **Lectura de bloques** | el `init()` lee `0x2000` (estado/ready), `0x0000` (info "BASE ver1/ver2/fw version/date"), `0x0100` (calibración) | `…/src/low/control.c:130-160`; `HALLAZGOS §69` (direcciones 0x0000/0x0100/0x1000/0x2000 verificadas en fases previas) |
| **UART** `/dev/ttyS1` | `S11radio_daemon` manda `s` al parar (apaga el BASE); el SDK tiene `x6200_control_modem_set()` ("Toggle serial port for SMT32 firmware update") | `rootfs-overlay/etc/init.d/S11radio_daemon:10-13`; `refs/X6200Control/include/…/control.h:65`; `build/…/src/low/control.c:113` (`/dev/ttyS1`) |
| **GPIO del BASE** | reset por GPIO 204 (`gpioset gpiochip1 204=1/0`) usado en un experimento de #244; **no** hay nodo DT que lo modele | `HALLAZGOS §70`; DTS sin ese `reset-gpios` |
| **Flasher del MCU** | el rootfs lleva `stm32flash` (para el STM32 del BASE), pero **no** lleva ningún `.xgf`/imagen de firmware | `build/target/usr/bin/stm32flash`; `find target -name '*.xgf'` vacío (`commands.log` §3.9/§8.8) |

**Lo que no existe:** función de readback del estado real de los registros de audio (la `get()` es sombra RAM), comando
de escritura por bit (siempre word completo), interlock de TX en el protocolo, ACK con estado, ni API de "restaurar
audio". Nada de eso está documentado por el fabricante; el firmware está encriptado.

---

## 4. Provenance del build

**Estado del build que se auditó (no es un tag publicado):** el clone local `rom-build/clone-buildroot` está en la rama
`feat/kernel-hidraw-unifying` en `e85e97a` (único cambio sobre `b44eb812`: `CONFIG_HIDRAW=y` +
`CONFIG_HID_LOGITECH_DJ=m`, 2 líneas del defconfig del kernel; `git diff --stat b44eb812..HEAD`), con un cambio local
en `br2_external/package/x6200-gui/x6200_gui.mk` que apunta `SITE` al clone local (necesario para compilar localmente;
el commit del repo apunta a GitHub) y el submódulo `buildroot` con contenido modificado (árbol de build). El `.config`
del kernel del build (mtime 2026-10-29) **ya incluye** `CONFIG_HIDRAW=y`, así que el árbol se rehizo después de
`e85e97a`. La GUI construida es la del estado `fc18a831` (clone-gui HEAD): su binario **coincide byte a byte** con el
documentado como desplegado en #363 A0, y entre `fc18a831` y `main 6850d671` sólo hay cambios en `docs/`.
`rom-build/last-built.txt` (28/10) declara build `b44eb812` y GUI `e7369a4d` (tag de release); se cita como historia,
no como identidad del árbol actual.

| Artefacto | Fuente | Versión / commit | sha256 |
|---|---|---|---|
| Kernel | Buildroot 2023.02.11 (submódulo `cc34b5b2…`) | Linux **6.1.82** | `.config` `8749a618…b570` |
| Kernel defconfig board | `br2_external/board/X6200/linux/sun8i-r16-x6200_defconfig` | — | `71a6012b…d766` |
| Device Tree board | `br2_external/board/X6200/dts/sun8i-r16-x6200.dts` | — | `51e2b3e8…9633` |
| DTB construido | `build/images/sun8i-r16-x6200.dtb` | — | `4d73f8a5…cc38` |
| zImage | `build/images/zImage` | 6.1.82 | `8b09458d…512f` |
| Defconfig Buildroot | `br2_external/configs/X6200_defconfig` | — | `496255a8…4508` |
| GUI construida | `build/target/usr/sbin/x6200_gui` | `x6200_gui` main (sin cambios de código desde `fc18a831`) | **`4ee87aef…66b0`** |
| Lib control (build) | paquete `aether-x6200-control` v0.1.0 (lib v0.15.3) | `X6200Control` | **`80c460e4…6df6`** |
| Supervisor | `rootfs/usr/sbin/x6200_daemon` | — | **`07c431cc…9877`** |
| `btaudio-ctl` | `rootfs/usr/sbin/btaudio-ctl` | — | `8483941f…f01b` |
| SpeexDSP | paquete Buildroot `speexdsp 1.2.1` | `libspeexdsp.so.1.5.2` | `bb57a91c…09dc` |
| PulseAudio | paquete Buildroot 16.1 (modo sistema) | 16.1 | módulos en `target/usr/lib/pulseaudio/modules/` |
| Imagen SD del build | `build/images/sdcard.img` | — | `0ed4cd32…7cf7` (**no** coincide con la flasheada `d3f22d80…6d0b`: rebuild posterior; la equivalencia de código se ancla a los hashes de archivos) |

---

## 5. Consecuencias para la decisión

1. **Todo lo que podríamos implementar en Linux (GUI, daemon, PulseAudio, kernel, DT, ASoC) no alcanza** para decidir el
   parlante: la selección es del BASE y sólo se cambia con el reg 12 bit `voice_rec`.
2. **El MCU/FPGA no es actualizable por nosotros con garantías**: no hay fuente ni imagen en la ROM; el único binario
   disponible está encriptado; `stm32flash` existe pero sin payload ni documentación de protocolo. Cambiar su
   comportamiento no es parte de este épico (y no se hace).
3. **La liberación de la dependencia** (si alguna vez se quiere) tendría que venir de afuera: documentación OEM de una
   ventana de lectura/escritura por bit con semántica conocida, o una sonda física del estado del MUX, o firmware
   alternativo para el MCU. Las tres están fuera de alcance y hoy no existen (#371 P1).
4. **Lo que sí es 100 % nuestro y no toca el BASE** es la validación del pipeline Linux (captura + SpeexDSP) y la
   salida audible por un sink que no sea el parlante (BT A2DP, USB audio): eso se puede implementar/probar sin
   escribir un solo registro del BASE. Ver [`alternatives.md`](alternatives.md).
