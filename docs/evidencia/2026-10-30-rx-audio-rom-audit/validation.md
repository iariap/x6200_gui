# Validación — #362 (auditoría ROM Linux completa)

**Esta fase NO validó audio: validó que el informe está fundado en el código/ROM y que las condiciones objetivas del
veredicto son comprobables.** No hubo radio, ni deploy, ni I2C, ni MUX, ni TX/PTT/ATU, ni medición física. Todo lo de
abajo es **estático** (lectura de fuentes/artefactos de build) o **host** (herramientas que corren en esta máquina).

**Fecha:** 2026-10-30 (America/Argentina/Buenos_Aires) · **Worktree:** `x6200-rom-audio-audit` @ `6850d671`
· **Build auditado:** kernel Linux 6.1.82, Buildroot 2023.02.11, `AetherX6200Buildroot` `e85e97a` (último cambio:
HIDRAW) con la GUI construida `4ee87aef…66b0`.
**Log completo con comandos, salidas y exit codes:** [`commands.log`](commands.log).

## 1. Resultado global

| Resultado | Valor |
|---|---|
| Comandos del log | **~115**, todos de lectura; **todos exit 0** salvo el único intento documentado de `risk.py` en modo *diff* (§6), que no resuelve las refs en el checkout ASF y se reemplaza por `--files` |
| Pruebas host ejecutadas | **suite nativa completa** (`tests/native/run_native_tests.sh`): **exit 0**, **25 stages con “0 fallos”**, **0 stages con fallos** |
| Asserts de condiciones del veredicto | **8/8 PASS** (DTB sin MUX/GPIO, i2c0 sin hijos, codec A33/simple-card, GUI enlaza SpeexDSP/PA/ALSA, PA con loopback/null-sink, supervisor sin I2C/audio, `play_stm` único y aislado de la captura, rootfs sin `.xgf`) |
| Pruebas con radio / audio físico | **NINGUNA** (prohibido por la fase; no es un faltante accidental) |
| QEMU del binario ARM (sin radio) | **NO ejecutado**: preflight del banco falla por falta de `tools/remote/x6200-remote` (§4) |
| Cambios de código/producto | **CERO**: el diff del PR es sólo documentación |

## 2. Pruebas estáticas ejecutadas (mapa a `commands.log`)

| # | Qué verifica | Comandos (sección del log) | Resultado |
|---|---|---|---|
| V1 | Base real, worktrees, ramas, commits y suciedad | §0.1–§0.8 | `origin/main = 6850d671`; buildroot `e85e97a` (construido: `b44eb812` según `last-built.txt`); GUI `fc18a831`; Buildroot `2023.02.11` |
| V2 | Hashes de provenance de todos los artefactos | §1.1–§1.6 | GUI `4ee87aef…66b0` (== desplegada), lib `80c460e4…6df6`, daemon `07c431cc…9877`, DTB `4d73f8a5…cc38`; entre `fc18a831` y `6850d671` sólo cambiaron `docs/` |
| V3 | Config del kernel: ASoC/códec/I2C/watchdog | §2.1–§2.3 | `CONFIG_SND_SUN8I_CODEC=y`, `_CODEC_ANALOG=y`, `SND_SIMPLE_CARD=y`, `SND_DUMMY/ALOOP=m`, `SND_USB_AUDIO=m`, `I2C_CHARDEV=y`, `I2C_MV64XXX=y`, `SUNXI_WATCHDOG=y` |
| V4 | No hay driver de audio propio de la placa | §2.4 | grep negativo en `sound/` y en DTS (único DTS que nombra la placa: el del board) |
| V5 | DTB construido: `sound`, códec A33, codec-analog, DAI, i2c0 sin hijos, sin MUX/amp/GPIO | §2.5–§2.9, §8.1–§8.3 | dtc exit 0; `simple-audio-card` name `sun8i-a33-audio`; `allwinner,sun8i-a33-codec`; `i2c@1c2ac00` sin nodos hijos; **0** coincidencias de MUX/amp/gpio-hog |
| V6 | Rutas ASoC del códec (HP/LineOut/ADC/Mic1) | §2.11–§2.12 | `sun8i-codec-analog.c:319-389` (HP), `:526-565` (Line Out), `:223-309` (ADC/Mic1); `sun8i-a33.dtsi:183-195` |
| V7 | Servicios del rootfs y ausencia de guardián de audio | §3.1–§3.6, §3.11–§3.12 | `S11radio_daemon`, `S12pulseaudio`, `S20gui`, `S50alsa`; **no** hay broker/guardián; el supervisor no menciona I2C/audio (0 matches) |
| V8 | PulseAudio 16.1 con módulos para A2DP/loopback/null-sink | §3.2, §3.6–§3.7, §8.5 | `module-loopback.so`, `module-null-sink.so`, `module-bluez5-*`; `btaudio.pa` A2DP-only (sin `switch-on-connect`); loopback de `btaudio-ctl` confirmado |
| V9 | Ruta GUI: captura → SpeexDSP → recorder/decoders; reproducción separada | §4.1–§4.12 | `read_callback` entrega sólo a `audio_enh_process`/`screen_rec`/`dsp`; `play_stm` se escribe en un único lugar (`audio.c:369`) y sólo desde `audio_play()`; `audio_enhancement.c` no llama a BASE/ALSA/PA (sólo un comentario) |
| V10 | Protocolo del BASE: sombra RAM, RMW, idle, reg 12 compartido, rxvol/spmode, I2C crudo | §5.1–§5.8 | `get()` = `all_cmd.arg[cmd]`; `cmd()` sombra-antes-de-ioctl; `idle()` 218 B; `record_set`/`ptt_set` RMW del **mismo reg 12**; `/dev/i2c-0` slave `0x72`, `I2C_RDWR` |
| V11 | Evidencia previa que se reutiliza (no se repite) | §6.1–§6.7 | #363 `BLOCKED_SAFE_MUX`; #369 `R_INCONCLUSIVE`; #371 tres claims NO; guía de audio; HALLAZGOS §25/§70/§77/§78 |
| V12 | Tests host nativos (callback de captura + enhancement) | §7.1–§7.3 | `test_audio_callback.c` + `test_audio_enhancement.c` incluidos; suite **exit 0**, 25 stages `0 fallos`, **0** stages con fallos; stage “Captura #351” 174 comprobaciones 0 fallos |
| V13 | Integridad del rootfs: sin firmware del BASE, con flasher | §3.9–§3.10, §8.8 | rootfs sin `.xgf`; `stm32flash` presente; `.xgf` vendor (encriptado) fuera del build, sha256 `dca90387…12f3` |

## 3. Asserts de las condiciones del veredicto (`commands.log` §8)

| Assert | Condición | Resultado |
|---|---|---|
| 8.1 | El DTB **no** tiene nodo de MUX/amplificador/GPIO de audio | **PASS** (`matches=0`) |
| 8.2 | `i2c0` (bus del BASE) **no** tiene nodos hijos en el DTB | **PASS** (`hijos_i2c0=0`) |
| 8.3 | La placa usa **el códec A33 + simple-audio-card** (no un códec externo) | **PASS** (1/1/1) |
| 8.4 | El binario GUI del build enlaza SpeexDSP + PulseAudio + ALSA | **PASS** (`libspeexdsp.so.1`, `libpulse`, `libasound`) |
| 8.5 | El PulseAudio del rootfs tiene `module-loopback` + `module-null-sink` | **PASS** (2) |
| 8.6 | El supervisor `x6200_daemon` **no** contiene I2C/audio (no puede restaurar el MUX) | **PASS** (0) |
| 8.7 | Un solo escritor de `play_stm`; la captura no escribe ahí | **PASS** (1 escritor en `audio.c:369`; `audio_play_en` 1 definición) |
| 8.8 | El rootfs compilado no incluye firmware `.xgf` del BASE | **PASS** (`xgf_en_rootfs=0`) |

## 4. Pruebas que NO se ejecutaron y por qué (limitaciones)

1. **Radio / audio físico / MUX / instrumentación:** prohibido en esta fase (#362/#371). `AUDIO_OUTPUT_VERIFIED`,
   `MUX_SELECT_PHYSICALLY_VERIFIED` y `RESTORE_VERIFIED` siguen **NO**. No se inventan instrumentos: el plan físico y
   el equipamiento faltante están en `docs/evidencia/2026-10-30-rx-audio-physical-prep/equipment.md`.
2. **Banco QEMU del binario ARM sin radio:** el preflight falla por entorno, no por el cambio:
   `tools/captura-banco.sh --check` → `FALTA: falta …/tools/remote/x6200-remote (compilar con …/tools/remote/compila.sh)`
   → `== 1 cosa(s) faltan: el banco no puede correr ==`. Se declara **BLOCKED**; no se compiló el remote porque no es
   necesario para la evidencia (y no forma parte del alcance de esta fase).
3. **Banco #351 con `libspeexdsp` real:** no se repite (requiere Docker con Debian pinned); su resultado ya está
   integrado en `docs/evidencia/2026-10-29-audio-capture-351/` y se cita como evidencia previa.
4. **Medición de latencia del pipeline / CPU R16 / calidad audible:** no medibles sin radio/instrumentación.
5. **Equivalencia byte a byte de la `sdcard.img`:** el build actual fue rehecho después del flasheo (hash distinto);
   la equivalencia de **código** se acredita con los hashes de archivos (GUI/lib/daemon/DTB) y con `last-built.txt`,
   no con el hash de la imagen.

## 5. No-claims (lo que este informe NO afirma)

- **NO** afirma que la ruta Linux→parlante esté implementada ni validada.
- **NO** afirma que el audio procesado se escuche en ningún lado hoy (sólo va a recorder/decoders).
- **NO** afirma que A2DP valide el stream procesado (el loopback actual toma la captura pre-Speex).
- **NO** declara GO por QEMU, por ALSA `RUNNING`, por un `parec`, por un sink BT ni por escucha informal.
- **NO** afirma que el registro 12 tenga readback ni que el MUX se haya conmutado (no se hizo).
- **NO** afirma nada sobre el efecto eléctrico del bit `voice_rec` en nuestro ejemplar (UNKNOWN; firmware encriptado).
- **NO** se editó `docs/HALLAZGOS.md` (sin medición nueva no se agrega nada a ese registro).

## 6. Validación del PR documental (ejecutada post-commit, en `commands.log` §9)

| Validación | Comando | Resultado |
|---|---|---|
| Matriz de checks, base **real** del PR | `asf/scripts/apps/x6200/check-matrix.sh fork/main HEAD <worktree>` | **exit 0**; `[SIN GATE] ningún bloque cubre el diff` + `(sólo documentación: no hay gate que correr)`. `fork/main = 6850d671`, la base real de `main` |
| Matriz de checks, base literal del issue | `… check-matrix.sh main HEAD <worktree>` | **exit 0**, mismo resultado (el `main` local está en `fc18a831`; el diff incluye la documentación de fases previas + ésta, todo `docs/`, sin gate) |
| Clasificación de riesgo (modo diff) | `asf/scripts/risk.py x6200 fork/main HEAD` | **exit 1**: el checkout configurado del ASF (`asf/work/x6200_gui`) está en `cb9ea287` y **no tiene** las refs del PR → no resuelve; se documenta el fallo |
| Clasificación de riesgo (fallback por archivos) | `asf/scripts/risk.py x6200 --files $(git diff --name-only fork/main HEAD)` | **exit 0**: `"risk": "low"`, reviewers `["reviewer-frontend"]`, `"human_approval": false`, `"auto_approve": true` (los 9 archivos son `docs/evidencia/…`) |
| Espacios/errores del diff | `git diff --check fork/main HEAD` | **exit 0** |
| Contenido del diff | `git diff --stat fork/main HEAD` | 9 archivos, **1987 inserciones** al cierre del commit (1917 en la medición intermedia §9.7), 0 borrados; sólo `docs/evidencia/2026-10-30-rx-audio-rom-audit/` |

**No hay merge automático** y no se abren issues. El PR es [#373](https://github.com/iariap/x6200_gui/pull/373)
(head inicial `e66d2aa6`), declara en el cuerpo `Refs #362; Refs #363; Refs #367; Refs #369; Refs #371; Refs #372`
y no toca producción/radio.
