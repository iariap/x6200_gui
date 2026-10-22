# Upstream: versiones nuevas, rebase de nuestra ROM y mitigación de riesgos

Verificación del 2026-10-11 sobre los repos en los que se basa nuestro X6200. Responde a:
*¿hay versiones más nuevas?, ¿conviene un rebase?, ¿se pueden mitigar los riesgos pendientes
de [`X6200_FEASIBILITY.md`](X6200_FEASIBILITY.md)?*

Clones de referencia (locales, se pueden borrar): `.x6100_ref` (69 MB), `.x6100_br` (4,9 MB),
`.x6100_ctl`, `.x6200_ctl`, `.x6200_docs` (notas del BASE). Total ≈ 75 MB.

---

## 1. Resumen

| Pregunta | Respuesta |
|---|---|
| ¿Hay versiones más nuevas? | **Sí.** GUI **v0.1.2** (ago-2026), buildroot **1 commit**, librería de control **v0.1.1**, y **base OEM 1.0.8** (ago-2026) |
| ¿El rebase es viable? | **Sí y es barato**: los dos repos están *fast-forward* limpio (3 commits + 1). Sólo **2 archivos** con conflicto, y trivial |
| ¿Se mitigan riesgos? | **Sí, varios**: batería/S-meter/espectro (v0.1.2), CW (v0.1.2 + base 1.0.8), EQ/compresión (base ≥1.0.1), BT y audio TX (recetas de la comunidad). CESSB/IF shift/TX cal siguen sin salida |
| ¿Qué falta antes de tocar código? | **Saber la versión de BASE** de tu radio (1.0.7 vs 1.0.8 vs anterior): determina qué versión de GUI es compatible |

---

## 2. Qué versión tenemos y qué hay

| Componente | Nuestra copia | Upstream | Distancia |
|---|---|---|---|
| `gdyuldin/x6200_gui` | `6a29687` = **v0.1.1 + 2** (2025-07-08) | `e1476ece` = **v0.1.2** (2026-08-10) | 3 commits, **fast-forward** |
| `gdyuldin/AetherX6200Buildroot` | `4b8a05b` (2025-06-10) | `8b7ca9d6` (2026-05-08) | 1 commit, **fast-forward** |
| `gdyuldin/X6200Control` | buildroot lo fija en **v0.1.0** | **v0.1.1** (`0ee1ae3`, "Fix flow on 1.0.7 base") | 1 tag |
| Base OEM Xiegu | ? (ver §4) | **1.0.8** (ago-2026, "CW macros & keyer fixes") | — |

### Qué trae lo nuevo

**GUI v0.1.2** (release notes: *"Add support of latest OEM BASE flow format (spectrum/waterfall/battery/etc)"*):

| Commit | Contenido |
|---|---|
| `c74df4a3` "Fixes for 1.0.7 BASE" | Nuevo formato de *flow*: `radio.c` resta offset a las muestras y corrige el dBm (`-dbm + 4`); `dsp.cpp` cambia el promedio del PSD (dB→potencia, +69/-13); `meter.c` corrige el S-meter con PRE/ATT y agrega `meter_set_noise()`; `cfg/mode.c` renombra `zoom`→`fft_width` y usa `FFT_FULL_WIDTH`; ajustes en `buttons.cpp`, `mfk.cpp`, `radio.c` |
| `d6776bd9` "Add CW zoom control" | **CW zoom** en `mfk.cpp`/`buttons.cpp`/`cfg/mode.c`/`dsp.cpp` |
| `e1476ece` "Bump version" | Versión en `main.h` |

**Buildroot**: sube la librería de control a **v0.1.1**, la que a su vez trae *"Fix flow on 1.0.7 base"* → **el par GUI+buildroot+control debe ir junto**.

**Base OEM** (notas oficiales Radioddity, vía [Xiegu-X6200-Research](https://github.com/tom-acco/Xiegu-X6200-Research)):
- 1.0.1: **ecualizador de audio del transceptor**, **compresión de voz**, decoder CW, espectro/waterfall.
- 1.0.3: algoritmo del cargador, ALC, **NR optimizado**, **AGC optimizado**, DFL de CW, bugs de CI-V, y **soporte Bluetooth inicial para FT8CN (app móvil)**.
- 1.0.4 y 1.0.5: nunca se publicaron.
- 1.0.6: **medidor de batería más preciso**, ajustes de red en SYSTEM, AM/NFM corregidos, **NR optimizado**, **nuevo decodificador CW en KEY** con umbral.
- 1.0.7 / **1.0.8** (ago-2026): macros de CW y arreglos del keyer; según pruebas independientes, **1.0.8 sigue sin resolver los problemas principales de 1.0.7**.

---

## 3. Rebase: procedimiento y mapa de conflictos

### Estado ya preparado (2026-10-11)

Los forks están creados y el trabajo está pusheado (el trabajo ya no está sólo en disco):

| Fork | Rama `main` | Nuestra rama |
|---|---|---|
| [`iariap/x6200_gui`](https://github.com/iariap/x6200_gui) | `e1476ece` = **v0.1.2 de upstream** | `lu2dsx/x6200-rom` (`1e6a390`, 3 commits) |
| [`iariap/AetherX6200Buildroot`](https://github.com/iariap/AetherX6200Buildroot) | `8b7ca9d6` = upstream | `lu2dsx/x6200-rom` (`71d127c`, 2 commits) |

Dejar `main` en el fork igual a upstream es a propósito: así el rebase es un comando.

```bash
# GUI
cd x6200_gui
git fetch fork
git rebase fork/main        # nuestros 3 commits sobre v0.1.2 (2 conflictos triviales)

# Buildroot
cd ../buildroot_new/AetherX6200Buildroot
git fetch fork
git rebase fork/main        # nuestros 2 commits sobre el upstream
```

> Los remotos `origin` siguen apuntando a los repos de gdyuldin (el del GUI por SSH):
> **no** conviene pushear ahí. El remoto de escritura es `fork`.

### Antes del rebase (contexto histórico)

Los dos repos eran *fast-forward* desde nuestra base (el padre de `c74df4a3` era nuestro `6a29687`, y el de `8b7ca9d6` era nuestro `4b8a05b`), y el trabajo estaba en 34 archivos sin commitear en el GUI y 7 en buildroot. Eso ya está resuelto: hay commits temáticos en la rama `lu2dsx/x6200-rom` de cada fork.

### Mapa de conflictos (GUI)

Upstream toca 12 archivos; de ellos **sólo dos los tocamos nosotros**:

| Archivo | Estado | Conflicto esperado |
|---|---|---|
| `src/buttons.cpp` | modificado por nosotros (botón GPS) y por upstream (CW zoom) | trivial: son zonas distintas |
| `src/mfk.cpp` | modificado por nosotros (include de `voice_stub.h`) y por upstream (CW zoom) | trivial: una línea |
| `cfg/cfg.c`, `cfg/cfg.h`, `cfg/mode.c`, `cfg/mode.private.h`, `dsp.cpp`, `meter.c`, `meter.h`, `radio.c`, `main.h` | **no los tocamos** | se aplican limpios |

En buildroot no hay conflicto: upstream toca `aether-x6200-control.mk` y nosotros tocamos `X6200_defconfig`, `x6200-gui/Config.in` y `x6200_gui.mk`.
Nuestros borrados (gps.c/h, dialog_gps.c/h, voice.cpp/h) tampoco chocan: upstream no los toca en estos commits.

### Pasos propuestos

```bash
# 0) NO hacer esto sin backup del árbol actual
cd x6200_gui
git switch -c lu2dsx/rom-review          # rama de trabajo
git add -A && git commit -m "X6200: adaptaciones LU2DSX (GPS/TTS out, About, adaptive tuning, audio enh, fixes)"

# 1) traer upstream y avanzar en fast-forward
git remote add upstream https://github.com/gdyuldin/x6200_gui.git 2>/dev/null || true
git fetch upstream --tags
git rebase upstream/main            # 3 commits, conflictos sólo en buttons.cpp y mfk.cpp

# 2) buildroot igual
cd ../buildroot_new/AetherX6200Buildroot
git add -A && git commit -m "X6200 buildroot: RHVoice off, limpiezas, settings"
git fetch origin && git merge --ff-only origin/main     # 1 commit (sube control lib a v0.1.1)

# 3) recompilar y verificar
#    (mismos pasos del skill .agents/x6200-build: dirclean + rebuild GUI + make + copiar imagen)
cd ../../x6200_gui && tests/native/run_native_tests.sh   # nuestros tests deben seguir en verde
```

Después del rebase hay que **re-verificar**: `main.h` (versión), que nuestros diálogos sigan enganchados en Ajustes, que el nuevo `cfg/mode.c` (renombrado `zoom`→`fft_width`) no rompa `dialog_settings.c`/`main_screen.c` (compila o no), y regenerar+validar la imagen como ya hicimos.

### Alternativa más conservadora

Si no querés tocar la ROM que ya funciona: **cherry-pick selectivo** de `d6776bd9` (CW zoom) y `c74df4a3`, dejando el resto. Menos riesgo, pero más divergencia y más conflictos la próxima vez.

---

## 4. Prerequisito crítico: la versión de BASE de tu radio

El salto a v0.1.2 **cambia el parseo del *flow*** (espectro/waterfall/dBm/batería) porque la base
de Xiegu cambió el formato. Consecuencias:

- Si tu radio tiene base **≥ 1.0.7** → v0.1.2 es la versión correcta.
- Si tiene base **1.0.3 / 1.0.6** (o la de fábrica) → v0.1.2 puede mostrar mal espectro/S-meter/batería; nuestra ROM actual (v0.1.1+2) probablemente es la compatible.

**Cómo saberlo** (con la ROM actual, por SSH o consola serie):
```bash
# el GUI muestra la versión de la app; para el BASE:
strings /usr/sbin/x6200_gui | grep -i version
dmesg | grep -i -E "base|stm32"        # o el menú INFO del firmware OEM
```
El blog de EA5JTT probó v0.1.2 sobre base **1.0.7** y funcionó (con las limitaciones de §5).

**Recomendación:** alinear la dupla. Si se hace rebase a v0.1.2, actualizar el BASE a **1.0.7**
(probado con esa GUI) y evaluar 1.0.8 aparte, porque aún no hay GUI que la valide explícitamente.

---

## 5. Estado real del port según un usuario independiente

Prueba publicada ([EA5JTT, agosto 2026](https://ea5jtt.blogspot.com/2026/08/xiegu-x6200-r1cbu.html))
de la **v0.1.2** sobre un X6200 con base **1.0.7**. Útil para calibrar expectativas:

| Funciona | No funciona / limitado |
|---|---|
| WiFi (rápido) y **sincronización NTP** del reloj | **Bluetooth: "no funciona nada"** |
| **FT8/FT4 autónomo** (sin PC), TX hasta 5 W con SWR visible, log ADIF en `DATA` | **WSPR no implementado** |
| Teclado USB en el puerto host | **wfview no implementado** |
| Bandas 0–500 kHz y 26–28 MHz incluidas | **No trabaja en 60 m** |
| ATU (parece funcionar mejor que con el OEM) | Varios ajustes quedan "KO": span, spectrum beta, min/max, AGC, RIT/XIT, charger |
| Batería (V y %) y hora en pantalla | GPS: el autor duda que el X6200 tenga receptor GPS |
| Web configurator (archivos, bandas, modos digitales, hora) | Ratón USB no funciona |

Coincide con nuestra verificación: los riesgos amarillos **no están resueltos aguas arriba**.

---

## 6. Mitigación de los riesgos pendientes

| Riesgo (de `X6200_FEASIBILITY.md`) | ¿Se mitiga? | Cómo |
|---|---|---|
| 🟡 **#1 Bluetooth** | **Parcial, con receta** | [`pier-cbd/xiegu_x6200_stuff`](https://github.com/pier-cbd/xiegu_x6200_stuff) lo logró **en el firmware OEM** (probado con base 1.0.6): puente GStreamer `alsasrc device=mixcapture` → `pulsesink device=bluez_sink.<MAC>.a2dp_sink`, con auto-arranque vía `userapp`. En **nuestra** ROM falta el transporte HCI: **habilitar `CONFIG_BT_HCIUART`** (el X6100 lo tiene; nosotros no) y revisar en qué bus está el chip. Con eso + el script, el riesgo baja mucho |
| 🟡 **#2 Audio TX digital** | **Sí** | [`usb_audio_full_duplex`](https://github.com/pier-cbd/xiegu_x6200_stuff/tree/main/usb_audio_full_duplex) documenta full-duplex por el puerto host (`mixout` para TX, `mixcapture` para RX, 48 kHz), con apps en C (PTT por `/dev/input/event0` o detección de pico) y cross-compilación con nuestro mismo buildroot. Funciona en SSB, U-DIG y L-DIG |
| 🟡 **#3 DRM/KMS** | **Sí (lado kernel)** | Ya está todo habilitado (`DRM_SUN4I`, panel `jlt4013a`); sólo falta el driver LVGL. Pendiente verificar en la radio que el pipeline arranca |
| 🔴 **Batería / S-meter / espectro** | **Sí** | GUI **v0.1.2** + base **≥1.0.6** (medidor de batería más preciso) |
| 🔴 **NR / AGC** | **Sí** | Base 1.0.3 y 1.0.6 optimizaron NR y AGC; nuestro módulo `audio_enhancement` puede apoyarse en eso o complementarlo |
| 🔴 **EQ y compresión de voz** | **Sí** | Existen en el base desde **1.0.1**; la librería del X6200 expone EQ de RX/TX/micrófono y `comp_*` |
| 🔴 **CW decoder / macros / keyer** | **Sí** | Base 1.0.6 (nuevo decoder con umbral) y 1.0.8 (macros y keyer); GUI v0.1.2 agrega **CW zoom** |
| 🔴 **CESSB, IF shift, filtro TX, TX cal/IQ, FM emphasis** | **No** | Viven en el MCU; el X6100 los tuvo con `x6100_base_patch`, **que no existe para el X6200**. Sin ingeniería inversa del base (o que Xiegu los agregue), quedan fuera |
| 🔴 **GPS** | **Se aclara** | Si el X6200 no tiene receptor GPS (duda publicada por EA5JTT), nuestra remoción **no pierde hardware**: el `gps.c` original hablaba con un GPS externo por USB/serie |
| 🟢 **Band plan IARU** | **Sí** | Ya resuelto en la UI OEM por [`Bandplan Display Patch`](https://github.com/tom-acco/Xiegu-X6200-Research/tree/main/Bandplan%20Display%20Patch) (metodología + parches); en nuestra GUI LVGL es trabajo nativo con la misma fuente de datos |

---

## 7. Recomendación estratégica: upstream-first

Nuestro árbol tiene **34 archivos modificados sin commitear** y varios borrados. Cada rebase
manual va a doler más con el tiempo. Propuesta:

1. **Ordenar el trabajo en commits temáticos** (uno por: GPS/TTS out, About, adaptive tuning,
   audio enhancement, fixes de la revisión, limpiezas de buildroot). Así los rebases futuros
   son cherry-picks, no mezclas.
2. **Hacer el rebase ahora**, que es barato (fast-forward + 2 conflictos triviales), **antes** de
   portar features nuevas. Después del rebase, cualquier feature se construye sobre la base nueva.
3. **Alinear versiones**: GUI v0.1.2 + control lib v0.1.1 + base OEM 1.0.7 (probada).
4. **Evaluar contribuir aguas arriba** (gdyuldin): adaptive tuning, audio enhancement, limpieza de
   RHVoice/gpsd y los fixes de esta revisión son candidatos naturales a PR. Eso reduce nuestra
   divergencia y hace que el mantenimiento lo compartamos.
5. **Posponer** CESSB/IF shift/TX cal hasta que exista base patch del X6200 o una versión OEM que
   los incluya.

Si preferís no tocar la ROM estable, el paso 1 y 2 se pueden hacer en una rama y dejar la actual
como respaldo: el costo de hacerlo bien ahora es de minutos; el de no hacerlo, de decenas de
conflictos en el próximo salto.
