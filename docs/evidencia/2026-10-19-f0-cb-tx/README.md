# F0 — TX en 27 MHz (CB) en el X6200 del owner: recon, gate de fábrica y prueba en vivo

**Fecha**: 2026-10-19 (hora host 11:00–11:4x -03; hora radio 13:59–14:37 UTC)
**Issue**: [#146 — Módulo CB 27 MHz](https://github.com/iariap/x6200_gui/issues/146) §11 (fase F0)
**Rama**: `lu2dsx/x6200-rom` de `iariap/x6200_gui`
**Radio**: X6200 del owner, `192.168.0.236` (la `192.168.18.67` no respondía)
**Build desplegado** (medido): GUI md5 `9a094c421b0e`, 13.844.268 B, `BASE fw version: 1.0.6`

---

## 0. Veredicto

**El equipo SÍ transmite en 27 MHz. El MCU/baseband no bloquea el TX en CB.** Medido en vivo en
la radio: `tx:true` y el puente de ROE del equipo midió RF en **43 de 51 puntos entre 26,935 y
27,435 MHz** (barrido a **0,1 W** forzada por firmware, ~1,2 s). La hipótesis de "gate en el
MCU" queda **refutada**.

**Pero la F0 completa sigue sin poder declararse PASS**: no hay medidor de potencia real ni
analizador de espectro, así que **potencia efectiva y armónicos NO están medidos**; tampoco se
pudo accionar un PTT de AM/FM a potencia configurada (el socket no tiene PTT; la inyección de
tecla no llegó a la GUI; el PTT del micrófono es del MCU). **No se publica TX como validado**
(regla del issue #146).

| Fase | Estado | Fundamento |
|---|---|---|
| 1. Recon READ-ONLY en la radio | **medido** ✓ | [`recon-radio.txt`](recon-radio.txt) |
| 2. Gate del rootfs de fábrica (archivo + línea) | **documentado** ✓ | [`ref-fabrica.md`](ref-fabrica.md): `/etc/xgradio/xgradio.conf` línea 7, binario `x6200_ui_v100`, **UI-side** |
| 3. Camino de TX del port (código) | **código** ✓ | [`f0-protocolo.md`](f0-protocolo.md): sin gate Linux en TX |
| 4. ¿El MCU/baseband bloquea el TX en 27 MHz? | **NO** (medido) ✓ | [`live-tx/transcript.md`](live-tx/transcript.md): `tx:true` + ROE 43/51 |
| 5. F0 completa: potencia real + armónicos | **BLOCKED** ✗ | Sin carga fantasma ni instrumentos de RF; no se midió potencia ni armónicos |
| 6. TX AM/FM a potencia configurada / PTT del micrófono | **NO MEDIDO** (requires-radio) | Sin PTT accionable por software para AM/FM; el PTT del micrófono es del MCU |

**Consecuencia para el #146**: la rama de contingencia "si el MCU no transmite → RX-only" **no se
activa**: el módulo CB puede diseñarse con TX. Lo que **no** se puede hacer todavía es
**publicar el TX como validado** ni prometer potencia/armónicos: eso exige F0 con instrumentos
(§5). Mantener igual el tope de 4 W y el interlock que define el issue.

**La radio quedó como estaba**: 27.185.000 MHz, LSB, CB C, RX, vol 0, app cerrada, misma GUI
(PID 13127), sin sesión de barrido. Ver §4 de
[`live-tx/transcript.md`](live-tx/transcript.md).

---

## 1. Qué se hizo (y qué no)

- **Recon read-only** por SSH (ninguna escritura, ningún reinicio). Transcripción cruda en
  [`recon-radio.txt`](recon-radio.txt).
- **Extracción del firmware de fábrica V1.0.6** en el host (paquete oficial del owner; sin montar
  ni flashear) y **lectura del rootfs de fábrica interno de la propia radio** (eMMC, sin montar,
  por bloques con `dd` + parseo ext4). En [`ref-fabrica.md`](ref-fabrica.md) y §2.5.
- **Análisis del código del port** (camino de TX, control remoto, telemetría) sin ejecutar nada.
  En [`f0-protocolo.md`](f0-protocolo.md).
- **Prueba en vivo** (autorizada por el owner): barrido de ROE a 0,1 W en 27.185 MHz, ~1,2 s,
  con lectura de la ROE por el puente del equipo. En [`live-tx/transcript.md`](live-tx/transcript.md).
- **NO se desplegó, NO se flasheó, NO se editó código** (solo documentos y `/tmp` de la radio).
- **No se tocaron los ajustes del owner**: frecuencia/modo/potencia/volumen restaurados; ATU y
  parámetros de `DATA` intactos.

---

## 2. Recon en la radio: el knob `fullband-tx` no existe en el port (medido)

Comandos y salida real (transcripción completa en [`recon-radio.txt`](recon-radio.txt)):

```text
$ ls -la /etc/xgradio/
ls: /etc/xgradio/: No such file or directory

$ grep -rn "fullband" /etc /usr/local /opt /usr/share /lib 2>/dev/null | head -20
(vacío)

$ grep -r -a -l "fullband" /usr /lib /etc /opt 2>/dev/null | head -20
(vacío)

$ ps aux | grep -i xgradio | grep -v grep
(vacío; el proceso es /usr/sbin/x6200_gui)

$ grep -a -c "fullband" /usr/sbin/x6200_gui      → 0
$ grep -a -c "xgradio"  /usr/sbin/x6200_gui      → 0
$ grep -a -c "fullband" /usr/lib/liblvgl.so      → 0
```

- El rootfs en ejecución está en la **microSD**: `cat /proc/cmdline` → `root=/dev/mmcblk0p2`;
  `mmcblk0p2` ext4 512 MB (`LABEL=rootfs`), `p1` BOOT 32 MB, `p3` DATA 3,2 GB. No hay `mtd`/`ubi`
  (`/proc/mtd` no existe) ni `fw_printenv`.
- `/etc/init.d/` tiene `S11radio_daemon`, `S20gui`, … **ningún** script de `xgradio`.
- La GUI desplegada es la del port: `md5sum /usr/sbin/x6200_gui` →
  `9a094c421b0eb05c103abf782ea00db6` (13.844.268 B); `BASE fw version: 1.0.6` en el log.
- **No está** `/usr/app_qt` (la UI Qt de fábrica no se shippea en el port).

### 2.5 Sí hay un rootfs de fábrica en la radio… en el eMMC interno (medido)

La radio tiene un **segundo dispositivo**: eMMC interna `mmcblk2` (8 GB, `8GTF4R`), **no montada**,
con `p1` vfat 48 MB y `p2` ext4 de **7,2 GiB**, `dumpe2fs`: filesystem limpio, `Last mounted on: /`.
Leyendo **bloques puntuales con `dd` y parseando ext4 localmente** (sin montar nada; UUID
verificado `68c7cb26-…`), el recon encontró:

- Es un **rootfs de fábrica**: Buildroot **2020.02.9**, `/boot/sun8i-r16-x6200.dtb` + `zImage`,
  `/usr/app_qt/x6200_ui_v100`.
- **Tiene `/etc/xgradio`** (inodo 130807): `xgradio.conf` (132 B) con
  **`fullband-tx=enable`**, `man.conf` (80 B) con **`fullband-tx=1`**,
  `fbtx-open-time=2025-12-13 19:53:25`, `exec-counter=392`, más `xgradio.conf.save`,
  `xgradio.conf.save.1` (editados a mano), `man.conf.ouBKDB`, `mcontext.conf` y `wfserver.ini`.

**Lectura**: el sistema de fábrica de **esta** radio ya tenía el **full-band TX habilitado** (el
owner lo activó el 2025-12-13 en el firmware original). El port **no lee** ese archivo: el knob del
firmware de fábrica **no aplica** a la ROM alternativa. La radio puede volver a fábrica
arrancando desde el eMMC (sin flashear) — no se probó ese arranque.

**Conclusión**: en la radio que corre el port, el archivo y la clave del mod **no existen** (no
están deshabilitados: no están). No hay nada que "destrabar" en el rootfs del port. Coincide con
el issue #146 §3.3.

---

## 3. El gate del firmware oficial, documentado con evidencia

Resumen de [`ref-fabrica.md`](ref-fabrica.md) (fuente: `sdcard.img` oficial V1.0.6, extraída
read-only con `dd`+`debugfs`, sin montar; sha256 de la imagen == el del ReleaseNote oficial):

1. **Archivo y línea del gate**: `/etc/xgradio/xgradio.conf`, sección `[mods]` (línea 6),
   **línea 7 → `fullband-tx=disable`** en la imagen oficial (default de fábrica).
2. **Binario que lo lee**: `/usr/app_qt/x6200_ui_v100` (ELF32 ARM, no stripped;
   sha256 `b76331c5…`). **No existe un daemon `xgradio`**: el nombre es solo el directorio.
3. **Función**: `XBandPlan::XBandPlan()` @ `0x8f708` (símbolo `_ZN9XBandPlanC1Ev`; fuente
   `../x6200_ui_v100/xbandplan.cpp`, `qDebug` línea 286), llamada desde el ctor de `XParamAdmin`
   al arrancar la UI.
4. **Mecanismo exacto**: `QSettings` IniFormat → `beginGroup("mods")` →
   `contains("fullband-tx")` → `value().toString()` →
   `QString::indexOf("enable", 0, CaseInsensitive) != -1` → setea `XBandPlan+0x1c = 1`.
   Si la clave falta o el valor no contiene `enable`, queda 0.
5. **Valor exacto**: el canónico es **`enable`**; `enable` y `enabled` funcionan igual porque es
   búsqueda de subcadena case-insensitive, no igualdad. `disable` = cerrado. La premisa de la
   tarea (`disabled` → `enabled`) se resuelve así: **`disable` → `enable`**. En la radio del
   owner el rootfs de fábrica ya tenía `enable` (§2.5).
6. **Es UI-side, no MCU-side** (con matiz): el parseo y los efectos están en la UI: muestra el
   indicador rojo `labelFullbandTxe`, permite ATU fuera de banda ham, y **baja el flag + el permiso
   de TX por banda al baseband** (`BaseCommIIC` offset `+0x120`, por I2C, en
   `updateBandEdge`/`updateCalibrateData`). El firmware del MCU
   (`/usr/firmware/x6200_mcu_20250111134652.xgf`, 512 KiB) es **opaco** (entropía 7,75, sin
   strings): el análisis estático no puede decidir si el MCU además restringe. **F0 lo midió: no
   bloquea** (§4).
7. **Dato operativo**: `/usr/share/emmc_sources/install_emmc.sh` (líneas 68/86) respalda y
   restaura `/etc/xgradio` al actualizar por SD → la edición del conf sobrevive a la actualización.

Referencia pública del mod:
[`Full TX/README.md` (tom-acco/Xiegu-X6200-Research)](https://github.com/tom-acco/Xiegu-X6200-Research/blob/main/Full%20TX/README.md).

---

## 4. Prueba en vivo en 27.185 MHz (autorizada por el owner): el MCU NO bloquea

Detalle completo (comandos, estados, capturas) en
[`live-tx/transcript.md`](live-tx/transcript.md).

- **Autorización**: el owner pidió "hacer lo posible por validar la transmisión en CB con la
  radio como está". Se usó el **barrido de ROE**, el único camino de TX accionable por software:
  el firmware fuerza **0,1 W** y **LSB**, **una sola pasada** (~1,2 s).
- **Medido**: `tx:true` durante el barrido; la pantalla del barrido quedó con
  **`min 1.0 @ 26.935.000 kHz (n=43/51)`** y curva de ROE en **26,935–27,435 MHz**. Es decir, el
  puente de ROE del equipo midió **RF real en 43 frecuencias de la banda CB**, con ~1,1 en 27.185.
  Captura: [`live-tx/06-swrscan-run.png`](live-tx/06-swrscan-run.png).
- **El barrido termina en `Error: timeout de RF`** (misma firma que en 40 m, issues #133/#125):
  es un defecto del barrido, **no** un bloqueo de TX.
- **Lo que NO se pudo medir**: potencia real, armónicos, AM/FM a potencia configurada, PTT del
  micrófono, ATU off (el indicador siguió en `ATU1`). Sin instrumentos, la F0 no puede ser PASS.

---

## 5. Qué falta para cerrar la F0 (checklist)

1. **Carga fantasma 50 Ω** en la salida de antena (hoy sólo hay antena; la prueba en vivo fue con
   la antena del owner y 0,1 W).
2. **Medidor de potencia/RF calibrado** y, para armónicos, **analizador de espectro** con
   atenuador (2.º/3.er armónico). Sin esto no hay PASS.
3. **ATU desactivado** y **XPA125B desconectado**; registrar el estado inicial.
4. **PTT de AM/FM a potencia configurada**: hoy no hay PTT por software; opciones: PTT físico
   (tecla superior o micrófono) con autorización, o habilitar wfview/CAT externo. El PTT del
   micrófono lo maneja el MCU y hay que probarlo físicamente en 27.185 y en 26.515 (§7.2 del
   issue).
5. **Restaurar** frecuencia/modo/potencia/ATU y RX; registrar las lecturas del instrumento.
6. Recién entonces, veredicto PASS/FAIL de la F0 y publicación del TX como validado.

---

## 6. Legal (Argentina) — obligatorio

- El **Servicio de Banda Ciudadana (SBC)** está en **26.960–27.410 MHz** y exige **autorización por
  estación** y **equipo homologado/registrado** (Res. SC 47/1982; CABFRA 2025 de ENACOM; trámite de
  Alta con formularios F1/F2/F23, D3/COPITEC y responsable técnico).
  Fuentes: [CABFRA 2025 (ENACOM, PDF)](https://enacom.gob.ar/multimedia/noticias/archivos/202501/archivo_20250127023814_6458.pdf),
  [trámite SBC (ENACOM)](https://www.enacom.gob.ar/tramites/autorizaciones-solicitud-alta-operar-servicio-banda-ciudadana-sbc-_t16).
- El **X6200 no es un equipo CB homologado**; transmitir en 27 MHz con él queda fuera del régimen
  del SBC y fuera del servicio de aficionados (10 m empieza en 28,000 MHz). **Modificar el firmware
  no cambia la homologación.**
- La prueba en vivo transmitió en 27,185 MHz (canal 19 CEPT), 0,1 W, ~1,2 s, con autorización
  explícita del owner y su equipo; **no habilita a operar** en CB.
- Los grids A, B y D (26,065–26,955 y 27,415–27,855) **no** son SBC argentino: el default
  `cb_tx_scope = 0` del issue los deja en RX. Modos y potencia del SBC argentino **no confirmados**
  (el texto de la Res. 47/82 no está online); usar AM/FM y **4 W** como valores conservadores.

---

## 7. Hallazgo colateral (defecto del control remoto, fuera del alcance de #146)

1. **Estado `pwr` congelado**: el state line reporta `"pwr":50` siempre
   (`remote_server.cpp` usa `cfg_sm.p_pwr.get()`, que devuelve `value_`; el valor real está en
   `getf()`/`cfg.pwr`). No permite verificar la potencia por control remoto.
2. **NFM mal mapeado**: `mode_names` tiene 8 entradas con `"NFM"` en el índice 7, pero el enum real
   es `x6200_mode_sam = 7` y `x6200_mode_nfm = 8`
   (`aether-x6200-control/.../low/control.h:88-99`). `{"cmd":"mode","val":"NFM"}` **selecciona
   SAM**, y el state line (`src/remote_server.cpp:623`) lee `mode_names[8]` fuera del array cuando
   el modo es NFM.
3. **`{"cmd":"app_cmd",...}` no existe**: los comandos de app se mandan con el nombre de la app
   como `cmd` (p. ej. `{"cmd":"swrscan","val":1}`). Queda documentado en
   [`f0-protocolo.md`](f0-protocolo.md).

Son defectos **de código** distintos del F0; no se arreglan en esta tarea (issue #146 prohíbe
tocar `remote_server.cpp`). Se registran en un issue aparte.

---

## 8. Evidencia y trazabilidad

| Archivo | Qué contiene |
|---|---|
| [`recon-radio.txt`](recon-radio.txt) | Transcripción cruda del recon SSH read-only (comando + salida), incluido el eMMC de fábrica |
| [`ref-fabrica.md`](ref-fabrica.md) | Gate del firmware oficial V1.0.6: archivo/línea, binario, mecanismo, UI-side vs MCU |
| [`ref-fabrica/`](ref-fabrica/) | Artefactos chicos: `xgradio.conf` extraído + sha256, strings, desensamblado, snapshot del README público |
| [`f0-protocolo.md`](f0-protocolo.md) | Camino de TX del port, comandos remotos, protocolo F0 y restauración, criterios PASS/FAIL/BLOCKED |
| [`live-tx/transcript.md`](live-tx/transcript.md) | Prueba en vivo: estados `tx:true`, ROE 43/51, restauración |
| [`live-tx/00-antes-rx.png`](live-tx/00-antes-rx.png) … [`live-tx/07-despues-rx.png`](live-tx/07-despues-rx.png) | Capturas de pantalla antes/durante/después (incluye el barrido con la curva de ROE) |
| `docs/PRUEBAS.md` | Fila y sección de esta corrida |

- Procedencia del firmware de fábrica: `sha256(sdcard.img)` =
  `e507e7ad98b3ca4e30407177f579717318e9718b1542c48c7532556627734514`, igual al publicado en el
  ReleaseNote oficial (APP 1.0.6, BASE 1.0.6) — ver [`ref-fabrica.md`](ref-fabrica.md) §1.
- Binarios grandes de la extracción (rootfs.ext4 400 MiB y volcado) quedaron **fuera del repo** en
  `/home/pablo/desa/x6200/.f0-ref/`; no se commitean.

## 9. Sin fundar (pendiente, no es hallazgo)

| Hipótesis | Por qué no está fundada | Qué haría falta |
|---|---|---|
| El PA entrega la potencia configurada en 27 MHz | Solo se midió el barrido a 0,1 W; no hay wattmeter | F0 con medidor de potencia |
| Los armónicos de 27 MHz están dentro de norma | No hay analizador de espectro | F0 con analizador + atenuador |
| El TX en AM/FM a potencia de operador funciona | No se pudo accionar PTT de AM/FM por software; el barrido fuerza LSB/0,1 W | PTT físico/wfview/CAT + F0 |
| El PTT del micrófono pasa por el MCU y funciona en 27 MHz | No se probó el PTT físico | Probar el PTT físico en 27.185 y 26.515 |
| El arranque desde el eMMC de fábrica sigue operativo | No se probó (no se reinició la radio) | Probar el arranque desde fábrica cuando el owner lo autorice |
