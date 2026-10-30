# #362 — Auditoría de la ROM Linux completa: ruta de audio RX, alternativas y recuperación

**Fecha de la corrida:** 2026-10-30 (host America/Argentina/Buenos_Aires, UTC−3). **Sólo lectura.**
**Issue:** [#362](https://github.com/iariap/x6200_gui/issues/362) (Epic) · **Fase:** auditoría de la ROM Linux completa
(no repite #363/#367/#369/#371, los integra) · **Sin abrir issues nuevos, sin merge, sin tocar la radio.**

**Base `main` real verificada al empezar:** `6850d671d430a1693dbcaf593579a3d98b037995`
(`git ls-remote https://github.com/iariap/x6200_gui.git refs/heads/main` → `6850d671`; el `main` local del clone
estaba en `fc18a831`, y entre `fc18a831` y `6850d671` **sólo hay cambios en `docs/`**, verificado con
`git diff --name-only fc18a831..6850d671`).
**Worktree/rama:** `x6200-rom-audio-audit` / `audit/rx-audio-rom-audio-362` (base `6850d671`).
**PR documental:** creado desde esta rama, cuerpo `Refs #362; Refs #363; Refs #367; Refs #369; Refs #371; Refs #372`,
**sin merge automático**. Este directorio no modifica firmware, UI, BASE, PulseAudio ni radio.

---

## Veredicto

| Pregunta | Respuesta | Evidencia |
|---|---|---|
| ¿La ROM Linux completa que compilamos permite **reproducir audio RX procesado por SpeexDSP en el parlante del X6200 de forma segura**? | **NO — `BLOCKED_SAFE_MUX`** | El único camino Linux→parlante pasa por el MUX del BASE, seleccionado por `voice_rec` (bit `0x8` del registro 12) — un registro compartido **sin readback real, sin dueño serializado fuera de la GUI y con PTT/ATU en el mismo word**. No hay ruta Linux alternativa al parlante. |
| ¿Se puede declarar GO a #364/#365? | **NO_GO** | Sigue vigente `GATE_A = BLOCKED_SAFE_MUX` de #363; esta auditoría **no lo levanta**. |
| ¿Hay alguna alternativa segura hoy? | **Sí, pero no es el parlante**: validación audible A2DP (auriculares BT, mantiene parlante BASE) y validación offline del pipeline. Ver [`alternatives.md`](alternatives.md). |
| ¿Existe recuperación independiente ≤250 ms? | **NO** en la ROM actual (`x6200_daemon` ≳4 s; sin broker; watchdog del SoC no toca el BASE; GPIO 204 del BASE es invasivo y no verificado). Ver [`recovery-analysis.md`](recovery-analysis.md). |
| ¿Se tocó la radio / se conmutó el MUX / se hizo deploy? | **NO.** Cero conexión, cero escritura I2C, cero TX/PTT/ATU, cero flash. Toda la evidencia es estática/host o reutiliza evidencia medida ya integrada. | [`validation.md`](validation.md) · [`commands.log`](commands.log) |

> **Frase de cierre:** la ROM que compilamos **sabe capturar** el RX del BASE y **sabe procesarlo** con SpeexDSP
> (medido en banco con la librería real en #351), pero **no tiene una ruta segura a su parlante**: esa ruta
> depende de un bit de un registro compartido del MCU/FPGA del BASE que no tiene verdad de estado ni restore
> verificable. Declarar GO por QEMU, por un sink PulseAudio `RUNNING` o por escuchar algo por A2DP sería falso.

---

## Las cinco preguntas obligatorias (respuesta corta)

1. **Mapa físico.** `BASE (RX demodulado) → MIC1 del códec A33 → ADC → AIF1 → ASoC simple-audio-card "sun8i-a33-audio"
   → ALSA hw:0,0 → PulseAudio source alsa_input.platform-sound.stereo-fallback → GUI read_callback → SpeexDSP`.
   El retorno `GUI → PulseAudio sink → AIF1 DA0 → DAC → mixer → HP → pines del códec → (analog link unknown) → MUX del
   BASE → PA → parlante`. **La selección BASE vs DAC no la controla ningún device/driver/DT/GPIO de Linux**: la hace
   el MCU/FPGA del BASE por I2C (reg 12 bit `voice_rec`), escrita sólo por la GUI. Detalle:
   [`audio-route-map.md`](audio-route-map.md) + [`croquis.md`](croquis.md).
2. **Linux vs BASE.** Controlamos kernel 6.1.82, DT, U-Boot, rootfs/Buildroot, ASoC (`sun8i-codec` + `sun8i-codec-analog`
   + simple-card), ALSA/PulseAudio 16.1, GUI+SpeexDSP 1.2.1 (`libspeexdsp.so.1.5.2`), `x6200_daemon`, stack BT/USB.
   **No controlamos** el MCU/FPGA del BASE: su firmware existe sólo encriptado (`.xgf`, 512 KB, sha256 `dca90387…12f3`)
   y el MUX/PA/estado de audio viven ahí. Detalle: [`linux-vs-base.md`](linux-vs-base.md).
3. **Alternativas.** (a) ruteo Linux **sin** escribir reg 12: **no existe** hacia el parlante; (b) coordinación del MUX
   desde Linux con restore independiente verificable: **BLOCKED** (requisitos hoy ausentes); (c) **A2DP BT como
   validación audible** manteniendo el parlante BASE: **GO acotado** (componentes ya en la ROM, medido con auriculares
   reales el 13/10, `HALLAZGOS §25`), con el límite de que el loopback toma la captura **pre-SpeexDSP**. Detalle:
   [`alternatives.md`](alternatives.md).
4. **Recuperación.** No hay mecanismo independiente ≤250 ms. `x6200_daemon` sólo reinicia la GUI (backoff mínimo 2 s +
   init ~1,7–1,9 s ⇒ **≈4 s**, y reescribe todo el register file, no un bit). Watchdog del SoC (`CONFIG_SUNXI_WATCHDOG=y`)
   resetea el SoC, **no** el BASE; el reset del BASE por GPIO 204 ya se probó en otra fase y **no** recuperó el audio con
   la GUI andando; un broker nuevo no tiene readback ni puede escribir el bit sin tocar PTT/ATU del mismo registro.
   Detalle: [`recovery-analysis.md`](recovery-analysis.md).
5. **Decisión.** **`BLOCKED_SAFE_MUX` (NO_GO)** para el parlante; **GO condicionado y acotado** sólo para validación
   audible A2DP/offline sin MUX. Condiciones de desbloqueo objetivas en [`decision.md`](decision.md).

---

## Hallazgos clave (todos con archivo:línea o comando)

**H1 — El camino Linux→códec existe y está completo; el MUX del parlante no está en Linux.**
El DTS del board habilita `&codec`, `&dai` y `&sound` (`br2_external/board/X6200/dts/sun8i-r16-x6200.dts:223-229,507-513`),
los tres son mainline (`allwinner,sun8i-a33-codec`, `simple-audio-card` name `sun8i-a33-audio`, `sun8i-a23-codec-analog`;
DTB compilado `images/sun8i-r16-x6200.dtb`, sha256 `4d73f8a5…cc38`). El kernel **no tiene driver propio de la placa**
(no hay archivo en `sound/` que la nombre). El nodo `i2c@1c2ac00` (bus del BASE) **no tiene nodos hijos** en el DTB y
**no existe** nodo de MUX/amplificador/`gpio-hog` de audio (grep negativo en el DTB decompilado; ver `commands.log` §2).
La selección la hace el MCU/FPGA por I2C: `x6200_voice_rec = 0x00008` en el reg 12 de `X6200Control`
(`…/low/control.h:113`), escrito sólo por `x6200_control_record_set()` (`build/…/aether-x6200-control-v0.1.0/src/control.c:69-81`).

**H2 — No hay ruta Linux al parlante que evite escribir el registro 12.**
El DAC del SoC llega (por el códec analógico) a los pines HP, y de ahí al BASE; con el MUX en "audio de RF" ese audio
no sale por el parlante. La observación previa de `speaker-test` sin sonido con la GUI parada (12/10, `HALLAZGOS §1`,
sin captura conservada) apunta a eso, pero es evidencia débil: **no verificada en esta corrida**. Un sink PulseAudio
`RUNNING` sólo prueba que el PCM entra al códec, **no** que el MUX esté en DAC. Los únicos "sinks" alternativos del
rootfs son BT A2DP y USB audio (si se enchufa un DAC USB), ninguno es el parlante del X6200.

**H3 — La GUI de hoy NO manda el RX procesado a la reproducción.**
`read_callback()` entrega la captura procesada sólo a `screen_rec_put_audio_samples()` y `dsp_put_audio_samples()`
(`src/audio.c:74-83`); `audio_enh_process()` es el único punto donde corre SpeexDSP (`src/audio.c:76`,
`src/audio_enhancement.c:208,518,556`). El stream de reproducción lo escribe **sólo** `audio_play()` (`src/audio.c:326,369`),
llamado por voz/recorder/FT8/JS8/SSTV/TX (`src/dialog_msg_voice.c`, `src/dialog_recorder.c`, `src/audio.c:458-478`).
`audio_enhancement.c` no llama a `x6200_control_*`, ALSA ni PulseAudio (sólo un comentario los menciona; conteo en
`commands.log` §4.7). Esta mitad ya estaba documentada en `docs/AUDIO_ENHANCEMENT_GUIDE.md:28-40`.

**H4 — Control seguro del MUX: imposible hoy (no es un problema de ALSA).**
`x6200_control_get()` devuelve la sombra RAM (`…/src/low/control.c:459-462`), `cmd()` actualiza la sombra **antes** del
ioctl y no propaga el error (`…:445-456`), `idle()` reenvía los 54 registros / 218 B (`…:473-481`) y `radio_thread()` lo
hace cada `IDLE_TIMEOUT=1000 ms` (`src/radio.c:45,338`). `record_set` (`voice_rec`, 0x8) y `ptt_set` (`iptt`, 0x40000)
son RMW sobre **el mismo reg 12** (`…/src/control.c:69-81` y `:156-166`). Un escritor externo no puede: (i) leer el
estado real (R′ dio `R_INCONCLUSIVE`, `0x0030/0x0038` → `00 00 00 00`), (ii) evitar el clobber del idle, (iii) actualizar
el bit sin arrastrar/cegar PTT/ATU. El firmware que implementa el MUX es propietario y está encriptado.

**H5 — Recuperación ≤250 ms: no existe en la ROM.**
Inventario: `audio_play_en(false)` (sólo si el proceso vive; `src/audio.c:458-478`); reinicio por `x6200_daemon`
(backoff 2/5/15/60 s, `rootfs/usr/sbin/x6200_daemon:80-87,219-271`) + init de GUI 1,7–1,9 s medido en radio
(#363 A3) ⇒ **≈4 s** y reescribe defaults; PulseAudio/ALSA no tienen control del MUX; `CONFIG_SUNXI_WATCHDOG=y`
resetea el SoC (no el BASE) y tarda el boot; reset por GPIO 204 del BASE ya probado sin recuperar audio con la GUI
andando (`HALLAZGOS §70`); el único comando serie es apagar el BASE (`S11radio_daemon:10-13`). No hay broker ni servicio
de audio en `etc/init.d` (inventario en `commands.log` §3.1). Detalle cuantificado en [`recovery-analysis.md`](recovery-analysis.md).

**H6 — A2DP: la única validación audible real ya medida, pero no del stream procesado.**
`btaudio-ctl` carga `module-loopback source=alsa_input.platform-sound.stereo-fallback sink=bluez_sink…a2dp_sink`
(`rootfs/usr/sbin/btaudio-ctl:23,80-116`); `btaudio.pa` desactiva `module-switch-on-connect` y HFP/HSP
(`rootfs/etc/pulse/system.pa.d/btaudio.pa:5-18`); el rootfs compilado tiene `module-loopback.so`, `module-null-sink.so`
y los módulos bluez5. Con auriculares reales se midió A2DP conectado y `loop=1` el 13/10 (`HALLAZGOS §25`).
**Límite:** el loopback toma la captura ALSA cruda (pre-SpeexDSP) y suena en los auriculares BT, **no** en el parlante
del X6200; además mutear el parlante es una escritura de `rxvol` (reg 14) en la práctica (`radio.c:869-873`,
`btaudio.c:133-141`), no del MUX.

**H7 — Provenance del artefacto auditado (para que el informe sea reproducible).**
Kernel Linux **6.1.82** (`build/build/linux-6.1.82/Makefile:1-5`), Buildroot **2023.02.11** (submódulo
`cc34b5b2…`), AetherX6200Buildroot en `e85e97a` (sobre `b44eb812`, único delta = 2 líneas de HIDRAW en el defconfig
del kernel; `rom-build/last-built.txt` del 28/10 declara `b44eb812`), GUI construida
`target/usr/sbin/x6200_gui` sha256 **`4ee87aef…66b0`** (idéntica a la documentada como desplegada en #363 A0),
lib de control **`80c460e4…6df6`** (v0.15.3), `x6200_daemon` **`07c431cc…9877`**, `btaudio-ctl` **`8483941f…f01b`**,
PulseAudio **16.1**, SpeexDSP **1.2.1** (`libspeexdsp.so.1.5.2`, sha256 `bb57a91c…09dc`), DTB `4d73f8a5…cc38`.
Tabla completa en [`linux-vs-base.md`](linux-vs-base.md#4-provenance-del-build) y [`validation.md`](validation.md).
La imagen `sdcard.img` del build actual (`0ed4cd32…7cf7`) **no** coincide con la flasheada el 28/10
(`d3f22d80…6d0b`) porque el build se rehizo después; por eso la equivalencia se ancla a los **hashes de archivos**
(GUI/lib/daemon) y no al hash de la imagen.

**H8 — No se repitieron sondeos agotados ni REA.**
No hubo I2C, ni lectura del reg 12, ni REA nueva (no apareció hipótesis nueva sobre binarios: el firmware del BASE
sigue encriptado). Se reutiliza la evidencia medida ya integrada (#363, #369, #371) y se agrega evidencia estática
nueva de ROM/DT/kernel/rootfs/GUI. Sin invención de instrumentos ni de mediciones.

---

## Artefactos de esta evidencia

| Archivo | Contenido |
|---|---|
| [`README.md`](README.md) | este resumen y veredicto |
| [`audio-route-map.md`](audio-route-map.md) | mapa físico/lógico con archivo:línea, registros y confusión ALSA vs MUX |
| [`linux-vs-base.md`](linux-vs-base.md) | frontera Linux vs MCU/FPGA, provenance del build, superficies de control |
| [`alternatives.md`](alternatives.md) | (a) ruteo Linux sin reg 12, (b) coordinación del MUX, (c) A2DP; veredictos |
| [`recovery-analysis.md`](recovery-analysis.md) | mecanismos de recuperación, latencias y evaluación ≤250 ms |
| [`validation.md`](validation.md) | pruebas estáticas/host ejecutadas, asserts, provenance y no-claims |
| [`decision.md`](decision.md) | decisión GO/BLOCKED/NO_GO, condiciones y propuesta mínima |
| [`commands.log`](commands.log) | comandos ejecutados con salida y exit code |
| [`croquis.md`](croquis.md) | croquis Markdown de la ruta RX, la reproducción y las alternativas |

## Evidencia previa que se usa (no se repite)

- [#363 / PR #366](https://github.com/iariap/x6200_gui/pull/366) — `GATE_A = BLOCKED_SAFE_MUX`; REA de la lib
  desplegada, sombra vs readback, idle 218 B, único dueño de `/dev/i2c-0`; A4 `NOT_EXECUTED`.
- [#369 / PR #370](https://github.com/iariap/x6200_gui/pull/370) — `R_INCONCLUSIVE`: lectura real del reg 12
  (`0x0030/0x0038`) devolvió `00 00 00 00`; no se repite el sondeo.
- [#367 / PR DRAFT #368](https://github.com/iariap/x6200_gui/pull/368) — `A_BLOCKED_READBACK`; guardián no habilitado.
- [#371 / PR #372](https://github.com/iariap/x6200_gui/pull/372) — plan físico y contrato de los tres claims
  (`AUDIO_OUTPUT_VERIFIED`, `MUX_SELECT_PHYSICALLY_VERIFIED`, `RESTORE_VERIFIED`), todos **NO**.
- `docs/AUDIO_ENHANCEMENT_GUIDE.md` y `docs/evidencia/2026-10-29-audio-capture-351/` — cadena SpeexDSP real medida
  (20 ms de delay con NR, partición invariable, destino=recorder+decoders) y **no** ruta al parlante.
- `docs/HALLAZGOS.md` §25 (A2DP medido), §64–§70 (rxvol/mute/I2C/GPIO204), §77–§78 (reg 12 sin readback ni restore).

## Limitaciones declaradas

1. **No hay medición física nueva**: no se midió el parlante, ni el MUX, ni el S/P, ni el PA. `AUDIO_OUTPUT_VERIFIED`,
   `MUX_SELECT_PHYSICALLY_VERIFIED` y `RESTORE_VERIFIED` siguen **NO**.
2. **No se tocó la radio**: todo el trabajo es lectura estática de repos/artefactos de build y pruebas host. No hubo
   deploy ni prueba en QEMU del binario ARM (el banco QEMU de la app requiere `tools/remote/x6200-remote` compilado,
   que falta en este worktree: `tools/captura-banco.sh --check` → `FALTA`; ver `commands.log` y `validation.md`).
3. El famoso "efecto físico del bit `voice_rec` en nuestro ejemplar" sigue **UNKNOWN**: depende de firmware encriptado
   y de instrumentación presencial (#371 P1/P2).
4. La semántica exacta del MUX interno, del PA y del `spmode` (parlante/auricular) no está documentada por el
   fabricante; se marca **UNKNOWN** y no se usa como evidencia de PASS.
5. El checkout local de `AetherX6200Buildroot` está en la rama `feat/kernel-hidraw-unifying` (`e85e97a`, HIDRAW en el
   defconfig del kernel) con un cambio local en `x6200-gui/x6200_gui.mk` (SITE local, propio del flujo de build local)
   y el submódulo `buildroot` con contenido modificado; **`last-built.txt` (28/10) declara `b44eb812`** y el `.config`
   del kernel del build (29/10) ya tiene `CONFIG_HIDRAW=y`, es decir el árbol se rehizo sobre `e85e97a`. Se documenta
   tal cual; el delta no toca audio.
6. No se edita `docs/HALLAZGOS.md` (regla de la fase: sólo con medición nueva). Esta auditoría no agrega hallazgos
   medidos a ese registro.

## Reproducción

```sh
# base real
git ls-remote https://github.com/iariap/x6200_gui.git refs/heads/main   # 6850d671…
# worktree
cd x6200-rom-audio-audit && git rev-parse HEAD                          # 6850d671…
# evidencia: todos los comandos y exits están en commands.log
# (dtc del build host, greps de kernel/DT/rootfs, readelf del binario, hashes, tests nativos host)
sh tests/native/run_native_tests.sh /tmp/x6200-native-audit            # exit 0 en esta corrida
```
