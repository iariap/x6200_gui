# Revisión de ROM — Xiegu X6200 (firmware optimizado LU2DSX)

**Fecha de revisión:** 2026-10-11
**Alcance:** las 7 imágenes `.img` de `./images/`, su coherencia con el código fuente local (`./x6200_gui`) y con la documentación (`./docs`, `./images/*.md`).
**Método:** inspección *offline* de las imágenes (sin hardware). No se flasheó ni se midió consumo.

---

## 0b. Después de esto: el programa de port (2026-10-12)

Sobre esa ROM corregida se hizo el **programa de port** del firmware del X6100 de IU1WXY: 18
features (modos digitales, cluster, Field Ops, temas, Bluetooth, wfview, web configurator con
control remoto, etc.). El detalle, el estado de cada una y lo que falta están en
[`NEW_FEATURES.md`](NEW_FEATURES.md), [`X6100_FEATURE_GAP.md`](X6100_FEATURE_GAP.md) y
[`INTEGRATION_AUDIT.md`](INTEGRATION_AUDIT.md). Este informe se conserva como la auditoría de la
ROM de la primera fase.

---

## 0. Actualización 2026-10-11 — correcciones aplicadas y ROM nueva

Los hallazgos de este informe **fueron corregidos en el código y la imagen se regeneró**. Las secciones 1–7 que siguen documentan la auditoría original (builds hasta `035043`) y se conservan como evidencia.

### ROM nueva (recomendada)

| Artefacto | Valor |
|---|---|
| Imagen | `images/x6200_20261011_201456.img` — 571.473.920 B (545 MB) |
| MD5 imagen | `4a8264bf48d75a7eec3cdf712963d9d1` |
| Rootfs | `images/rootfs.tar` — 226.990.080 B (217 MB), MD5 `0bf40ae0642d1c8d85d6dfb6176bcfb9` |
| `x6200_gui` embebido | 538.668 B, MD5 `4be3d719a89632710c99ffa1c7ee9671` |
| Kernel / DTB / `boot.scr` | **idénticos** al build anterior (`4facecfd…` / `84330fc4…` / `2f79b714…`) |
| rootfs ext4 | limpio (`e2fsck -fn`), label `rootfs`, 5151/32768 inodos y 61932/131072 bloques (frente a 5276 y 66027 del build anterior: **~16 MB liberados**) |
| Dependencias del GUI | 41 bibliotecas resueltas con el `ld.so` del propio rootfs bajo `qemu-arm-static`, **0 faltantes** |

### Cambios de código

| Hallazgo | Corrección | Archivos |
|---|---|---|
| §5.1 waterfall battery mode sin uso | Nueva fila en Ajustes **"Waterfall battery mode"** + parámetro persistido `waterfall_battery`; se aplica en `main()` tras crear la pantalla. Por defecto **off** | `waterfall.c/.h`, `params/params.{c,h}`, `dialog_settings.c`, `main.c` |
| §5.2 audio enhancement inerte | Procesado **por bloques** de 256 muestras (el callback entrega ~4410); AF leveler con ganancia calculada una vez por bloque; NR de Speex sólo en tramas completas. Valor de preset fuera de rango acotado | `audio_enhancement.c` |
| §5.2 sin UI para el audio enhancement | Nueva fila en Ajustes **"Audio enhance"** con presets Off / CW weak / SSB voice / AM broadcast / Digital + parámetro persistido `audio_enh_preset`, cargado al arrancar | `dialog_settings.c`, `params/params.{c,h}`, `main.c` |
| §5.3 etiqueta `x%.1f` no se ocultaba | `lv_timer_resume()` al mostrar el multiplicador | `main_screen.c` |
| §5.4 ajuste "Freq acceleration" inerte / evento muerto | El multiplicador adaptativo queda **gobernado por el ajuste**: none = 1×, lite ≤ 10×, strong ≤ 100×. Se eliminó `EVENT_ROTARY` y el callback muerto; `freq_shift()` sigue usándose desde las teclas `+`/`-` | `main_screen.c`, `events.c/.h` |
| §5.5 *cast* con desborde | Paso acotado a ≥1 y grilla de `align_int()` dentro de `uint16_t` (cae al paso base si es mayor) | `main_screen.c` |
| §5.6 peso muerto RHVoice (12,2 MB) | Se quitó `select BR2_PACKAGE_RHVOICE` de la Config.in de la GUI, `rhvoice` de `_DEPENDENCIES` y las opciones del defconfig; limpieza del target | `X6200_defconfig`, `x6200-gui/Config.in`, `x6200_gui.mk` |
| §5.6 restos gpsd (3,5 MB) | Limpieza ampliada del target (clientes `usr/bin/gps*`, `cgps`, `xgps`, módulo Python `gps`, `usr/share/gpsd`, MIBs, reglas udev) | volumen de build |
| §4 documentación obsoleta | Banners de obsolescencia en `images/BUILD_COMPLETE.md`, `README.md`, `BUILD_STATUS.md`, `BUILD_SUMMARY.md`; `docs/TRD.md` actualizado (§1.5.4, §2.3, §4.1.3, §17 y control de versiones 1.3) | docs |

### Verificación de la ROM nueva

- Particiones idénticas (32 MiB FAT `BOOT` + 512 MiB ext4 `rootfs`), u-boot en offset 8 KiB, `e2fsck -fn` limpio.
- **Sin** `gpsd`, `gpsdctl`, `libgps*`, `libgpsdpacket*`, `S50gpsd`, clientes `gps*` ni módulo Python `gps`. Único remanente relacionado: `garmin_gps.ko` (driver del kernel, carga sólo bajo demanda).
- **Sin** RHVoice (ni librerías, ni voces, ni `RHVoice-test`).
- Todas las dependencias dinámicas del GUI están presentes (`readelf -d` sin faltantes) y el binario contiene las filas nuevas ("Waterfall battery mode", "Audio enhance") y el About de LU2DSX.
- `rootfs.tar` corresponde exactamente a la imagen (mismo MD5 de `x6200_gui`).

### Pendiente / no verificado

- **Sin pruebas en hardware**: no hay medición de consumo, de boot ni de audio. Los ahorros siguen siendo estimaciones.
- **Qué sí se probó sin hardware**: ver [`TESTING.md`](TESTING.md) — 55 comprobaciones unitarias nativas y en la ISA ARM (`qemu-arm-static`), prueba de mutación de los bugs corregidos, análisis estático `gcc -fanalyzer` limpio y validación del cierre de dependencias del binario real. Los tests encontraron además un bug latente: `audio_enh_preset_get_current()` estaba declarada sin implementar.
- `clang-format` no está instalado y `$HOME` es de sólo lectura, así que el chequeo de formato de `AGENTS.md` no pudo ejecutarse.
- Los tests existentes (`tests/`) siguen cubriendo sólo ft8 y qth; los nuevos módulos tienen ahora su propio arnés en `tests/native/`.

### Huellas de los builds retirados

Las 7 imágenes anteriores (los 6 builds obsoletos y el primer build verificado) se borraron para liberar ~1,8 GB; sus huellas quedan registradas aquí:

| Imagen | MD5 | Familia |
|---|---|---|
| `x6200_optimized_20260210_214944.img` | `fb52873025d334c10332ca881e50340b` | original (gpsd + GUI con RHVoice) |
| `x6200_20260211_022649.img` | `85fcc749e7bc085e418cb5a63d39d42c` | original |
| `x6200_20260211_025006.img` | `ae93a8c0044b79b457587c9b2d2afcdb` | intermedio (GUI nuevo, con gpsd) |
| `x6200_20260211_025154.img` | `c89e300f34a86bed522df53a303a0c0f` | intermedio |
| `x6200_20260211_030325.img` | `aa3b86b619bce3658d122bac110c16ca` | intermedio |
| `x6200_20260211_033125.img` | `25268352a2e9ec22683f2927b8093169` | intermedio |
| `x6200_20260211_035043.img` | `c1833f9bc0f5b1a25d266903897986d4` | primer build verificado (retirado) |

---

## 1. Veredicto

> Las respuestas siguientes corresponden a la auditoría **previa** a las correcciones (builds hasta `035043`); la ROM vigente es la de §0.

| Pregunta | Respuesta |
|---|---|
| ¿Hay una ROM correcta y completa? | **Sí:** `images/x6200_20260211_035043.img` (hoy superada por `x6200_20261011_201456.img`) |
| ¿La ROM recomendada está "al día" con el código? | **Sí** — último cambio de fuente 00:49, imagen generada 00:50 (hora local) |
| ¿Arranca? (cadena de arranque) | **Correcta a nivel de artefactos** (u-boot, kernel 6.1.82, DTB, `boot.scr`, rootfs ext4 limpio, `root=/dev/mmcblk0p2`). No probada en equipo |
| ¿La documentación describe lo que realmente contiene? | **No** — los `.md` de `./images` (feb-10) afirman optimizaciones que no existen; `docs/TRD.md` (feb-11) sí es preciso |
| ¿La imagen "optimized" que la doc manda flashear es la optimizada? | **No** — `x6200_optimized_20260210_214944.img` es un build **previo** (gpsd activo + GUI original con RHVoice) |
| ¿Funcionan las features nuevas del build final? | **Parcialmente**: el dialogo About y el *adaptive tuning* sí; el *waterfall battery mode* y el *audio enhancement* son código inerte |

**Recomendación corta:** usar `images/x6200_20261011_201456.img` (ver §0) y `images/rootfs.tar`; el resto de los builds quedaron obsoletos. No dar por buenos los porcentajes de ahorro hasta medir en hardware.

---

## 2. Integridad de la imagen recomendada (`x6200_20260211_035043.img`)

| Verificación | Resultado |
|---|---|
| Tamaño / MD5 | 571.473.920 B (545 MB) · `c1833f9bc0f5b1a25d266903897986d4` |
| MBR | válido; p1 `0xC` arrancable (2048, 65536 sectores = 32 MiB), p2 `0x83` (67584, 1048576 sectores = 512 MiB). **Tamaño exacto = tabla de particiones** |
| u-boot | presente en offset 8 KiB (magic sunxi `eGON.BT0`); región 0–1 MiB **idéntica en las 7 imágenes** |
| `/boot` (FAT "BOOT") | `zImage` (5.567.736 B) + `sun8i-r16-x6200.dtb` + `boot.scr`; **idénticos en las 7 imágenes** |
| Kernel | `Linux version 6.1.82` (Buildroot 2023.02.11, gcc 12.3.0) → coincide con `/lib/modules/6.1.82` |
| `boot.scr` | `bootargs ... root=/dev/mmcblk0p2 rootwait panic=10 fbcon=rotate:3 video=VGA:480x800`; carga `zImage` y `${fdtfile}` (u-boot define `fdtfile=sun8i-r16-x6200.dtb`) |
| rootfs ext4 | label `rootfs`, UUID `e8b4aac3-37d2-4b6d-9ea8-cd4546acbd15`, 512 MiB, 4096 B/bloque; `e2fsck -fn` → **limpio** (5276/32768 inodos, 66027/131072 bloques, 258 MB usados) |
| Dependencias del GUI | las 24 `NEEDED` de `usr/sbin/x6200_gui` están presentes en el rootfs (incl. `ld-linux-armhf`, `liblvgl`, `liblv_drivers`, `libliquid`, `libft8`) |
| `rootfs.tar` | 243.660.800 B, MD5 `c1824d81ad318b639a4213d79e427c87`; **corresponde exactamente a esta imagen** (mismo `x6200_gui` MD5, mismas 5759 rutas; sólo difieren `lost+found` y `[SYS]/Journal`) |
| GUI embebido | `usr/sbin/x6200_gui`, 534.548 B, `ARM PIE`, contiene `2.0-opt`, `About`, `73 de LU2DSX`, `Made with love by LU2DSX` → el *override* de fuente local funcionó |

### Observación de layout: falta la partición `/mnt`
`etc/fstab` espera `/dev/mmcblk0p3` (vfat) en `/mnt`, pero `genimage.cfg` sólo crea 2 particiones. En esta imagen `/mnt` es un directorio del rootfs: `params.db` y `/mnt/app_logs` se escriben en la partición raíz (hay ~254 MB libres, no es crítico, pero no es el diseño previsto y no sobrevive a un reflasheo del rootfs).

---

## 3. Qué es cada imagen (auditoría de contenido)

Todas pesan igual y comparten la misma región de arranque; **lo que cambia es el rootfs**:

| Imagen (nombre = hora UTC) | `x6200_gui` (MD5) | gpsd/`S50gpsd` | libgps | libspeexdsp | ntp.conf | Familia |
|---|---|---|---|---|---|---|
| `x6200_optimized_20260210_214944.img` | `8485e50d…` 625.092 B | **sí** | sí | no | 485 B | **Original** (GUI sin About, con RHVoice) |
| `x6200_20260211_022649.img` | `8485e50d…` | **sí** | sí | no | 485 B | **Original** |
| `x6200_20260211_025006.img` | `721fb78f…` 530.396 B | **sí** | **sí** | no | 362 B | intermedio (GUI nuevo, GPS sin quitar) |
| `x6200_20260211_025154.img` | `721fb78f…` | no | no | no | 362 B | intermedio |
| `x6200_20260211_030325.img` | `b89431cb…` 530.396 B | no | no | no | 639 B | intermedio |
| `x6200_20260211_033125.img` | `1571af1e…` 534.508 B | no | no | no | 639 B | intermedio |
| **`x6200_20260211_035043.img`** | **`2234a234…` 534.548 B** | **no** | **no** | **sí** | **639 B** | **final/completo** |

Diferencia de contenido de cada build contra el final (`path` + `size`): 20, 11, 7 y 7 líneas respectivamente — es decir, **sólo difieren el binario del GUI y un puñado de librerías** (`liblvgl.so`, `libspeexdsp*`, `libgps*`, `etc/shadow`, `etc/ntp.conf`). No hay cambios ocultos en el resto del sistema de archivos.

> **Hallazgo grave de documentación:** `images/BUILD_COMPLETE.md` (y `README.md`) declaran `x6200_optimized_20260210_214944.img` como "firmware optimizado, listo para flashear", con GPS y TTS "removidos". Esa imagen es en realidad un build **anterior**: contiene `usr/sbin/gpsd`, `usr/sbin/gpsdctl`, `lib/udev`, `etc/init.d/S50gpsd` (gpsd **arranca en el boot**) y el **GUI original de 625 KB** que enlaza `libRHVoice.so.5`, `libRHVoice_core.so.10`, `libRHVoice_audio.so.2` y `libgps.so.30`. Flashearla no da ninguna de las optimizaciones descritas.

---

## 4. Documentación vs. ROM real

| Afirmación (docs de `./images`, feb-10) | Realidad verificada | Estado |
|---|---|---|
| Profundidad de color 32→16 bit, buffer 1,5 MB→768 KB (−50 %) | `lv_conf.h:27` = `LV_COLOR_DEPTH 32`; `main.c:41` = `800*480*4` (1,5 MB). **No se aplicó** (revertido por pantalla negra; ver TRD 3.1.2) | ❌ **falso** |
| "Ahorro de 120–240 mA (40–50 %)" | `docs/TRD.md` (más nuevo) dice 25–35 % y ~85–190 mA. Números contradictorios; ninguno medido | ⚠️ **inconsistente** |
| RHVoice eliminado del build ("librerías removidas") | `X6200_defconfig` **sigue con** `BR2_PACKAGE_RHVOICE_ENGLISH=y` y `..._SLT=y`; la imagen lleva **12,16 MB en 78 archivos** RHVoice (libs + voces + `RHVoice-test`). Sólo se quitó el enlazado del GUI | ⚠️ **parcial** |
| gpsd eliminado a nivel sistema | `gpsd`, `gpsdctl`, `libgps*`, `S50gpsd` ausentes ✅, pero quedan **45 archivos / 3,51 MB** (clientes `usr/bin/gps*`, `python3.11/site-packages/gps`, `usr/share/gpsd`, `lib/udev/rules.d/25-gpsd.rules`, MIBs). El TRD 1.5.4 avisa de esta limpieza manual; el patrón usado (`/usr/sbin/gps*`) no cubrió `/usr/bin/gps*` | ⚠️ **incompleto** |
| Waterfall battery mode: 75 % menos refresco, ~15 mA | La función existe pero **nunca se llama** (ver §5.1) | ❌ **sin efecto** |
| "Faster boot, no GPS daemon delay" | Cierto en la imagen final (no hay `S50gpsd`) | ✅ |
| Checklist de pruebas en `BUILD_COMPLETE.md` | **todo sin marcar**: sin medición de boot, consumo, ni prueba funcional | ❌ **no validado** |
| Redacción de energía | El ahorro de TTS sólo aplica mientras hablaba (nadie lo usaba → ~0); y quitar el *daemon* GPS no garantiza cortar la alimentación del módulo GPS (hardware) | ⚠️ **estimaciones** |

`docs/TRD.md` es el documento fiable (dice explícitamente que 16-bit está revertido y da 25–35 %). Los cuatro `.md` de `./images` quedaron desactualizados y hoy inducen a error.

---

## 5. Hallazgos en el código que se compila en la ROM

### 5.1 `waterfall_set_battery_mode()` es código muerto — *prioridad media*
`src/waterfall.c:232` / `src/waterfall.h:25`. Únicas apariciones en todo el árbol: definición y declaración. Nunca se invoca desde la UI ni desde `params`. El "ahorro ~15 mA" no existe tal como está.

### 5.2 El *audio enhancement* nunca procesa audio — *prioridad media-alta*
Tres motivos independientes, cada uno suficiente:
1. **Guarda de tamaño:** `audio_enhancement.c:283` → `if (nsamples > AUDIO_ENH_FRAME_SIZE /*256*/) return;`. El callback de captura pide `fragsize = 100 ms @ 44100 Hz = 4410 muestras` (`audio.c:28,116,128`), y `audio.c:56` llama `audio_enh_process(buf, nbytes/2)` → **siempre retorna sin procesar**.
2. **Sin UI:** `audio_enh_set_enabled()` / `audio_enh_preset_load()` sólo se llaman dentro del propio módulo; `audio_enh_init()` deja el preset en `OFF`.
3. **No hay consumidor** de notches/AGC/NR (ninguna función `audio_enh_*` usada desde `dialog_*`).

Conclusión: `docs/AUDIO_ENHANCEMENT_GUIDE.md` ("Features Implemented") describe funcionalidad que en la ROM **no es alcanzable**; el plan (`..._IMPLEMENTATION_PLAN.md`) dice "Ready for Implementation", que es el estado real. Además, si se arreglara la guarda, el bucle por muestra con `log10f`/`powf` en el hilo de PulseAudio es caro para un objetivo QRP (`AGENTS.md` → batería/latencia).

### 5.3 El indicador del multiplicador no se oculta — *prioridad baja (bug visible)*
`main_screen.c:1160-1161` crea el timer de 2000 ms y lo **pausa**; `main_screen.c:849` sólo hace `lv_timer_reset()`, que en LVGL v8 únicamente actualiza `last_run` (`lvgl/src/misc/lv_timer.c:257-260`) y **no despausa**. Resultado: tras el primer giro, la etiqueta `x%.1f` (arriba a la derecha) queda visible para siempre. Falta `lv_timer_resume()`.

### 5.4 `EVENT_ROTARY` dejó de emitirse: se anula el ajuste de usuario "Frequency acceleration" — *prioridad media*
`src/rotary.c:54` ahora sólo envía `EVENT_ROTARY_ENHANCED`. `main_screen_rotary_cb` (`main_screen.c:887`) sigue registrado pero **nunca se ejecuta**; con él quedan muertos `freq_shift()` (`:812`) y `freq_accel()` (`:796`). Pero el parámetro `freq_accel` **sigue expuesto en Ajustes** (`dialog_settings.c:1293-1304`, dropdown None/Lite/Strong) y se guarda en la DB: el usuario lo cambia y no pasa nada, porque el nuevo camino usa exclusivamente el multiplicador adaptativo. Esto rompe el principio "Predictable" de `AGENTS.md`. Además el nuevo camino `hardcodea` el rango `500 kHz – 300 MHz` (`main_screen.c:874-875`), mientras el original no recortaba (los límites de banda los maneja `update_freq_boundaries`); 300 MHz no corresponde a un HF/50 MHz.

### 5.5 *Cast* con desborde en la alineación adaptativa — *prioridad baja*
`main_screen.c:871`: `align_int(freq + df, (int16_t)adaptive_step)`. Con `step=5000` y multiplicador 100 → 500000 → `(int16_t)500000 = 41248`, así que en modo "scan" la grilla de alineación real es de ~41 kHz en vez de los 500 kHz esperados (valores de frecuencia "raros"). `align_int` recibe `uint16_t`; conviene acotar antes de castear.

### 5.6 Peso muerto y limpieza del target — *prioridad baja*
12,16 MB (RHVoice) + 3,51 MB (restos gpsd) = **15,7 MB** en una partición de 512 MB. No consume CPU (nada lo enlaza/ejecuta) pero contradice la documentación y alarga los builds.

### 5.7 Proceso, tests y lint — *prioridad media (proceso)*
- Los módulos nuevos (`adaptive_tuning.c`, `audio_enhancement.c`, `dialog_about.c`) **no tienen tests**: `tests/` sólo cubre `ft8_qso` y `qth`; `run_tests.sh` requiere toolchain cruzada/Buildroot.
- `AGENTS.md` pide verificar linting: **no se pudo** — `clang-format` no está instalado en este entorno (existe `.clang-format`).
- Los `.c` nuevos usan `//` y formato propio; no se validó contra `.clang-format`.

### 5.8 Riesgo operativo heredado
`usr/sbin/x6200_daemon` es un `while true` que relanza el GUI sin *backoff* si éste muere; con código nuevo no probado (adaptive tuning/audio) un fallo del GUI produce reinicios en bucle. Es upstream, pero conviene tenerlo presente.

---

## 6. Acciones recomendadas, por prioridad

| # | Acción | Archivo |
|---|---|---|
| P0 | **No flashear `x6200_optimized_20260210_214944.img`**; usar `x6200_20260211_035043.img` | `images/` |
| P0 | Corregir `BUILD_COMPLETE.md`, `README.md`, `BUILD_STATUS.md`, `BUILD_SUMMARY.md` (16-bit, 768 KB, 40–50 %, "RHVoice removido") o marcarlos como obsoletos y apuntar a `docs/TRD.md` | `images/*.md` |
| P1 | Decidir el *audio enhancement*: arreglar `fragsize`/guarda (procesar por bloques de 256) **y** añadir UI, o sacarlo del build | `audio_enhancement.c`, `audio.c` |
| P1 | Conectar `waterfall_set_battery_mode()` a un ajuste (o quitarlo) | `waterfall.c/.h`, `dialog_settings.c` |
| P1 | `lv_timer_resume()` al mostrar el multiplicador | `main_screen.c:849` |
| P2 | Reactivar `EVENT_ROTARY` o eliminar `freq_accel` de Ajustes y de `params.db`; ajustar el rango de frecuencia al del X6200 | `rotary.c`, `main_screen.c`, `dialog_settings.c` |
| P2 | `BR2_PACKAGE_RHVOICE_*` fuera del defconfig + limpiar restos gpsd/RHVoice del target (el TRD ya documenta el procedimiento) | `X6200_defconfig`, `br2_external` |
| P3 | Acotar `adaptive_step` antes del cast; añadir tests; instalar/ejecutar `clang-format` | `main_screen.c`, `tests/` |

### Limpieza de artefactos
1,8 GB en `images/`. Al conservar sólo la imagen final + `rootfs.tar` se liberan ~1,6 GB. Las imágenes son *sparse* (`ls` 545 MB, `du` ~260 MB): `dd`/balenaEtcher las escriben correctamente.

---

## 7. Evidencia reproducible

```bash
# tabla de particiones y tipo de imagen
file images/*.img

# hashes por partición (boot = 1 MiB off, 32 MiB; rootfs = 33 MiB off, 512 MiB)
dd if=images/x6200_20260211_035043.img bs=512 skip=2048  count=65536   status=none | md5sum
dd if=images/x6200_20260211_035043.img bs=512 skip=67584 count=1048576 status=none | md5sum

# rootfs: extraer, validar y listar
dd if=images/x6200_20260211_035043.img of=/tmp/rootfs.ext4 bs=1M skip=33 count=512
e2fsck -fn /tmp/rootfs.ext4
dumpe2fs -h /tmp/rootfs.ext4 | grep -Ei "volume name|UUID|state|Block count|Free blocks"
7z l -slt /tmp/rootfs.ext4 | grep -E "^(Path|Size) ="

# GUI dentro de la imagen y sus dependencias
7z e -o/tmp/x /tmp/rootfs.ext4 usr/sbin/x6200_gui
readelf -d /tmp/x/x6200_gui | grep NEEDED
strings -a /tmp/x/x6200_gui | grep -E "LU2DSX|2\.0-opt"

# version de kernel del zImage del FAT
7z e -o/tmp/b /tmp/boot.vfat zImage && python3 -c "
import zlib;d=open('/tmp/b/zImage','rb').read();i=d.find(b'\x1f\x8b\x08')
o=zlib.decompressobj(31).decompress(d[i:],1<<26);j=o.find(b'Linux version');print(o[j:j+80])"
```

---

## Anexo — resumen de cifras

- Imágenes: 7 (1,8 GB en disco, 545 MB lógicas c/u) → **conservar 1**.
- ROM recomendada: `images/x6200_20260211_035043.img` — MD5 `c1833f9bc0f5b1a25d266903897986d4`.
- Rootfs asociado: `images/rootfs.tar` — MD5 `c1824d81ad318b639a4213d79e427c87`.
- rootfs: 5759 rutas, 258 MB usados / 512 MB, fs limpio.
- Peso muerto: 15,7 MB (RHVoice 12,16 MB + restos gpsd 3,51 MB).
- Sin validación en hardware: no hay mediciones de corriente, boot time ni pruebas funcionales registradas.
