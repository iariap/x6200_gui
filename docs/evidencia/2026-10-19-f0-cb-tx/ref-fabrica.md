# F0 CB 27 MHz — gate `fullband-tx` del firmware oficial V1.0.6 (imagen de fábrica, sólo referencia)

**Fecha**: 2026-10-19 · **Tarea**: F0 CB 27 MHz (issue #146), task-2 · **Rama**: `lu2dsx/x6200-rom` (repo `x6200_qa`)
**Objetivo**: documentar con evidencia exacta qué hace el gate `fullband-tx` del firmware oficial Xiegu V1.0.6
(archivo + línea + binario responsable) y si el gate vive en la UI Linux o en el baseband/MCU.

**Regla dura respetada**: NO se flasheó, NO se escribió en ningún dispositivo, NO se montó nada. Todo el
trabajo fue lectura sobre el `sdcard.img` de fábrica ya descargado, extrayendo archivos a un directorio de
trabajo local (`/home/pablo/desa/x6200/.f0-ref/`, fuera del repo). La radio no se tocó.

---

## 1. Material y procedencia

Imagen de referencia (ya existente en el host, provista por el owner):

```
/home/pablo/lenovo_vieja/Downloads/radio/x6200/2025-01-15_Xiegu_X6200_update_V1.0.6/
├── readme.1st
├── 2025-01-15 Xiegu X6200 update V1.0.6.pdf
├── ReleaseNote-250114001(EN_CHS).txt
└── sdcard.img   (856.686.592 B, ya descomprimida)
```

**Procedencia verificada**: el SHA256 del `sdcard.img` coincide exactamente con el que publica el
Release Note oficial de Xiegu/Radioddity para esta versión:

```
$ sha256sum sdcard.img
e507e7ad98b3ca4e30407177f579717318e9718b1542c48c7532556627734514  sdcard.img

ReleaseNote-250114001(EN_CHS).txt:
  Image File SHA256: e507e7ad98b3ca4e30407177f579717318e9718b1542c48c7532556627734514
  APP:  1.0.6 Jan 14 2025,14:13:28
  BASE: 1.0.6 Jan 11 2025,13:46:22
```

Es decir: imagen oficial V1.0.6, APP del 14-ene-2025 y BASE (MCU) del 11-ene-2025. El archivo de firmware
de MCU que aparece dentro del rootfs se llama justamente `x6200_mcu_20250111134652.xgf` (11-ene-2025 13:46).

Herramientas usadas (todas en el host, sin root): `sfdisk`, `parted`, `file`, `dd`, `debugfs`,
`strings`, `readelf`, `arm-linux-gnu-objdump` (binutils cruzado ARM del sistema), `python3`.

---

## 2. Medido — extracción (ningún paso monta ni escribe en dispositivos)

### 2.1 Tabla de particiones (`sfdisk -d`, sólo lectura)

```
$ sfdisk -d sdcard.img
label: dos
label-id: 0x00000000
device: sdcard.img
unit: sectors
sector-size: 512
sdcard.img1 : start=        2048, size=       32768, type=c, bootable   # vfat boot (16 MiB)
sdcard.img2 : start=       34816, size=      819200, type=83              # ext4 -> rootfs Linux (400 MiB)
sdcard.img3 : start=      854016, size=      819200, type=c               # vfat DATA (400 MiB)

$ parted -s sdcard.img unit B print
 1  1048576B    17825791B   16777216B   primary  fat16  boot, lba
 2  17825792B   437256191B  419430400B  primary  ext4
 3  437256192B  856686591B  419430400B  primary  fat16  lba
```

La partición Linux es **p2** (ext4, 400 MiB). p1 es boot vfat y p3 es la DATA vfat.

### 2.2 Extracción de p2 con `dd` (sin montar)

```
$ dd if=sdcard.img of=.f0-ref/rootfs.ext4 bs=512 skip=34816 count=819200 conv=sparse status=progress
819200+0 records in
819200+0 records out
419430400 bytes (419 MB, 400 MiB) copied, 3.09 s, 136 MB/s

$ file .f0-ref/rootfs.ext4
Linux rev 1.0 ext4 filesystem data, UUID=653a142c-a3e4-40df-8050-0fdb3285d601 (extents) (large files) (huge files)
SHA256(rootfs.ext4) = fc330714466f64e0e15f3c5d60926e6815823fe7166bd582a47d704738278936
```

### 2.3 Lectura con `debugfs` y volcado completo con `debugfs rdump` (sin root, read-only)

```
$ debugfs -R "ls -l /etc/xgradio" .f0-ref/rootfs.ext4
    269  40755 (2)  0  0 1024  5-Jun-2023 02:16 .
    104  40755 (2)  0  0 1024 14-Jan-2025 03:16 ..
    270 100644 (1)  0  0  133  9-Nov-2022 01:53 xgradio.conf

$ debugfs -R "cat /etc/xgradio/xgradio.conf" .f0-ref/rootfs.ext4
# XG Radio配置文件
# fullband-tx: 全频段发射开关
# +            enable
# +            disable

[mods]
fullband-tx=disable

$ debugfs -R "rdump / ./.f0-ref/root" .f0-ref/rootfs.ext4   # 5902 archivos, 469 MB
```

El volcado completo permite grepear el rootfs entero como archivos locales (los binarios grandes quedaron
sólo en `.f0-ref/`, no se commitean).

---

## 3. El gate: archivo + línea exactos

**Archivo**: `/etc/xgradio/xgradio.conf` (dentro del rootfs de fábrica p2; inodo 270, 133 B)
**Línea exacta**: **línea 7** → `fullband-tx=disable` (la sección `[mods]` empieza en la línea 6)
**Valor por defecto de fábrica**: `disable` (gate cerrado)

```
 1  # XG Radio配置文件
 2  # fullband-tx: 全频段发射开关        <- "interruptor de TX en plena banda completa"
 3  # +            enable
 4  # +            disable
 5
 6  [mods]
 7  fullband-tx=disable
```

SHA256 de la copia extraída (idéntica): `c5dac539af300130567adc1b3c5c959a61c0d1bf2f77d51e71ccf8466e67fb8d`
→ artefacto [`ref-fabrica/xgradio.conf`](./ref-fabrica/xgradio.conf).

**Quién lee esa clave**: `grep -arl fullband-tx` sobre TODO el rootfs extraído devuelve exactamente 3 archivos:

```
$ grep -arl 'fullband-tx' .f0-ref/root
.f0-ref/root/etc/xgradio/xgradio.conf
.f0-ref/root/usr/app_qt/x6100_ui_v100
.f0-ref/root/usr/app_qt/x6200_ui_v100
```

No existe ningún daemon `xgradio` (no hay binario con ese nombre en `/bin`, `/sbin`, `/usr/bin`, `/usr/sbin`,
`/usr/libexec`, `/opt`; `/etc/xgradio/` sólo contiene el `.conf`). El único consumidor es la **app Qt de la UI**.
En el X6200 la que corre es `/usr/app_qt/x6200_ui_v100`, arrancada por
`/usr/share/support/userapp` (`NAME=x6200_ui_v100`) y mantenida viva por `/etc/monitrc`
(`check process x6200_ui_v100 ... start program = "/usr/share/support/userapp start"`).

---

## 4. Binario responsable y evidencia (símbolos + offsets)

**Binario**: `/usr/app_qt/x6200_ui_v100` (rootfs de fábrica)
**Función**: `XBandPlan::XBandPlan()` — símbolo `_ZN9XBandPlanC1Ev`, **dirección 0x8f708**, tamaño 13792 B
(la dirección 0x8f708 es «XBandPlan ctor», llamada desde `XParamAdmin` ctor @0xaaec4, o sea al arrancar la UI).

```
$ file root/usr/app_qt/x6200_ui_v100
ELF 32-bit LSB executable, ARM, EABI5 version 1 (GNU/Linux), dynamically linked,
interpreter /lib/ld-linux-armhf.so.3, for GNU/Linux 5.4.0, not stripped

$ readelf -h ...
Class: ELF32   Machine: ARM   Entry point 0x29b20   Flags 0x5000400 (EABI5, hard-float)
SHA256(x6200_ui_v100) = b76331c5aa859ae98dc1819b1079a3e40489e9986c9656e0dc2c553aeb28871b
```

### 4.1 Strings y offsets en el binario

```
$ strings -a -t x root/usr/app_qt/x6200_ui_v100 | grep -E 'fullband|xgradio.conf'
  fd19f fullband-tx
  ff04f /etc/xgradio/xgradio.conf
  ff08d found [mods] fullband-tx:

# también, en el mismo binario (otras claves / metadatos):
  ff0b1 found [mods] bands-cfg:
  ff038 XBandPlan::XBandPlan()
  ff06e ../x6200_ui_v100/xbandplan.cpp       <- archivo fuente compilado
  fb758 FullBandTx: %d
  fafe5 labelFullbandTxe
```

Direcciones virtuales ARM (`.rodata`: vaddr = fileoffset + 0x10000):
`fullband-tx` → **0x10d19f**, `/etc/xgradio/xgradio.conf` → **0x10f04f**, `enable` → **0x10c6eb** (el linker
fusionó el sufijo de `sql_enable`), `found [mods] fullband-tx:` → **0x10f08d**.

### 4.2 Cómo se lee el gate (secuencia real, `arm-linux-gnu-objdump` sobre 0x8f708)

Este binario es PIC: la dirección de cada string se calcula como
`target = (pc de la instrucción add + 8) + literal_del_pool`. Relevadas las referencias (script de mapeo
PC-relativo sobre el desensamblado completo; ver artefacto
[`ref-fabrica/02-xbandplan-ctor-gate-disasm.txt`](./ref-fabrica/02-xbandplan-ctor-gate-disasm.txt)):

| insn `add` | destino    | string / significado |
|-----------:|-----------:|----------------------|
| `0x8f778`  | `0x10f04f` | `"/etc/xgradio/xgradio.conf"` |
| `0x8f7a8`  | `0x10f069` | `"mods"` (beginGroup) |
| `0x8f7cc`  | `0x10d19f` | `"fullband-tx"` (QString para `contains()`) |
| `0x8f840`  | `0x10f08d` | `"found [mods] fullband-tx:"` (qDebug) |
| `0x8f860`  | `0x10d19f` | `"fullband-tx"` (QString para `value()`) |
| `0x8f8e8`  | `0x10d19f` | `"fullband-tx"` (segunda `value()`) |
| `0x8f934`  | `0x10c6eb` | `"enable"` (patrón a buscar) |
| `0x8f998`  | `0x10f0a7` | `"bands-cfg"` (otra clave del mismo `[mods]`) |

Secuencia reconstruida del constructor (ARM, no Thumb), con las llamadas que hizo el compilador:

```asm
8f778: add  r1, pc, r1                 ; r1 -> "/etc/xgradio/xgradio.conf"
8f77c: bl   QString::QString(char const*)
8f794: bl   QSettings::QSettings(QString const&, QSettings::Format /*r2=1=IniFormat*/, QObject*)
8f7a8: add  r1, pc, r1                 ; r1 -> "mods"
8f7b8: bl   QSettings::beginGroup(QString const&)
8f7cc: add  r1, pc, r1                 ; r1 -> "fullband-tx"
8f7dc: bl   QSettings::contains(QString const&)   ; ¿existe la clave?
8f7ec: cmp  r5, #0
8f7f4: beq  0x8f990                   ; si NO existe -> sigue con "bands-cfg" (flag queda 0)
...
8f7fc/8f800: QMessageLogger(..., "xbandplan.cpp", /*line*/ 286, "XBandPlan::XBandPlan()")
8f840: add  r1, pc, r1; 8f844: bl QDebug::operator<<(char*)   ; qDebug() << "found [mods] fullband-tx:"
8f860: QString("fullband-tx"); 8f898: QSettings::value(key, QVariant())
8f8e8: QString("fullband-tx"); 8f918: QSettings::value(key, QVariant())
8f924: QVariant::toString()
8f934: QString("enable")
8f950: bl   QString::indexOf(QString const&, int /*r2=0*/, Qt::CaseSensitivity /*CaseInsensitive*/)
8f980: cmn  fp, #1                    ; ¿indexOf(...) != -1 ?
8f984: movne r3, #1
8f988: ldrne r2, [sp, #16]            ; r2 = this (XBandPlan*)
8f98c: strbne r3, [r2, #28]           ; this->m_fullbandTx (offset 0x1c) = 1
```

Al inicio del constructor el byte se inicializa en 0 (`0x8f76c: strb r3, [r0, #28]` con `r3=0`), por lo que:

- clave ausente → **0** (cerrado, default);
- `fullband-tx=disable` → **0**;
- `fullband-tx` con un valor que contenga `enable` (case-insensitive) → **1** (abierto).

**Conclusión del mecanismo**: el gate no compara igualdad contra un literal: hace
`QString::indexOf("enable", 0, Qt::CaseInsensitive) != -1` sobre el valor. Es decir, `enable`, `enabled`,
`ENABLE`, `Enable`, etc. **todas habilitan**; `disable` (y cualquier valor sin `enable`) queda cerrado.

### 4.3 Función de log / línea del fuente (evidencia del `qDebug`)

En `.rodata` están los metadatos del `QMessageLogger` para el log de la clave:
`../x6200_ui_v100/xbandplan.cpp`, línea **286** (`movw r9, #286 @ 0x11e`), función
`XBandPlan::XBandPlan()` (`_ZZN9XBandPlanC4EvE19__PRETTY_FUNCTION__` @0x10f038) y el mensaje
`"found [mods] fullband-tx:"`. El siguiente log de la misma función, para `bands-cfg`, usa la línea **292**
(`movw r9, #292 @ 0x124`). **No hay DWARF** en el binario (sin secciones `.debug_*`), así que 286/292 son
las líneas que el propio binario arrastra para sus logs, no una línea recuperada de la tabla de debug.
Para efectos del gate, la referencia estable y verificable es: **`/etc/xgradio/xgradio.conf` línea 7**, leída
por `XBandPlan::XBandPlan()` en `xbandplan.cpp` (log en línea 286) de `/usr/app_qt/x6200_ui_v100`.

### 4.4 Qué hace el firmware con el flag (`XBandPlan+0x1c`)

`XBandPlan` está embebido en `XParamAdmin` en el offset `+0xd4`; por eso el flag es `XParamAdmin+0xf0` (240).
Se encontraron **4 lectores** del byte (`grep 'ldrb ..., [rX, #240]'` → 0x5ad68, 0x5e928, 0x776b0, 0x7857c):

1. **`MainWidget::updateBandEdge()` (0x5ad64-0x5ad6c)** — copia el flag a la estructura de datos del
   **baseband** (`BaseCommIIC`, objeto en `MainWidget+0x237c`), campo `+0x120` (288):
   ```asm
   5ad64: ldr  r3, [r4, #892]          ; BaseCommIIC* (MainWidget+0x2000+0x37c)
   5ad68: ldrb r2, [r2, #240]          ; m_fullbandTx (XParamAdmin+0xf0)
   5ad6c: strb r2, [r3, #288]          ; pack del baseband, offset 0x120
   ```
   En la misma función vuelca, para cada banda, inicio/fin y el byte `XBandMapItem+0x30` (permiso de TX
   por banda). Inmediatamente después del `updateBandEdge()`, `slot_basebandStateChanged()` llama a
   `BaseCommIIC::updateCalibrateData()` (0x5d6a8 → 0x5d6b0): el UI **baja** el flag y las bandas hacia el
   baseband por el canal `BaseCommIIC` (I2C). El log `FullBandTx: %d` en `slot_basebandStateChanged()`
   (0x5d524-0x5d528) lee ese mismo campo `+0x120`.

2. **`MainWidget::slot_atuTune()` (0x5e928-0x5e948)** — si el flag es 0 y `XBandPlan::isHamBand(freq)` es
   falso, **rechaza el ATU** (muestra mensaje y loguea); si el flag es 1, se saltea ese rechazo:
   ```asm
   5e928: ldrb r3, [r0, #240]   ; m_fullbandTx
   5e92c: cmp  r3, #0
   5e930: bne  0x5ee34          ; fullband-tx activo -> permite sintonizar el ATU fuera de banda ham
   5e940: bl   XBandPlan::isHamBand(long long)
   5e944: cmp  r0, #0
   5e948: bne  0x5ee34
   ... (0x5e94c: mensaje de error/aviso)
   ```

3. **`MainWidget` ctor (0x776b0-0x776c4)** — muestra u oculta el widget `ui->labelFullbandTxe`
   (`Ui_MainWidget+0xb4`) según el flag. El widget lo crea `Ui_MainWidget::setupUi()` con
   `setObjectName("labelFullbandTxe")` (0x807d4-0x80800) y estilo
   `QLabel{border:2px solid red;border-radius:5px;color:red;background:transparent;}` (0x808c4-0x808e4):
   es el indicador rojo de «Full Band TX» en pantalla.

4. **`MainWidget` ctor (0x78574-0x78690)** — si el flag está activo, escribe en `/etc/xgradio/man.conf`,
   sección `[manufacture]`: `fbtx-open-time` (fecha/hora de apertura, `yyyy-MM-dd hh:mm:ss`) si no existía, y
   `fullband-tx=1`. Es un registro de manufactura/fábrica; **no** es el gate operativo (el gate operativo es
   el de `xgradio.conf [mods]`).

**Nota importante sobre `isTxEnable()` / `isHamBand()`**: en este binario `XBandPlan::isTxEnable(long long)`
(0x8f474) **no tiene ningún llamador** (`grep 'bl 8f474'` = 0 resultados); `XBandPlan::isHamBand(long long)`
(0x8f4dc) tiene **un solo llamador**, el ATU (0x5e940). `isTxEnable` devuelve `XBandMapItem+0x30` si la
frecuencia cae en el rango inicio/fin del ítem; `isHamBand` devuelve `+0x30` sólo si además `+0x31` (es banda
ham) es distinto de 0. Esos bytes `+0x30`/`+0x31` son los que `updateBandEdge()` copia al pack del baseband
por banda. Es decir: el flag global `fullband-tx` y el permiso de TX por banda se calculan/leen en la UI y se
le mandan al baseband; el PTT del teclado (`MainWidget::adminBuiltinPTT`) sólo postea el parámetro de PTT al
baseband y no consulta banda/frecuencia en la UI.

Artefactos con el desensamblado recortado de todo esto:
[`ref-fabrica/03-isTxEnable-isHamBand-disasm.txt`](./ref-fabrica/03-isTxEnable-isHamBand-disasm.txt) y
[`ref-fabrica/04-consumidores-flag-disasm.txt`](./ref-fabrica/04-consumidores-flag-disasm.txt).

---

## 5. ¿UI-side o MCU/baseband-side?

**El gate es de la UI Linux. No es del baseband/MCU.** Evidencia:

1. La clave está en un archivo del **rootfs Linux** (`/etc/xgradio/xgradio.conf`, línea 7) y el único código
   que la lee es la app Qt `/usr/app_qt/x6200_ui_v100` (`grep -arl` sólo devuelve el conf y los dos binarios
   de UI; no hay daemon `xgradio` en el firmware).
2. La lectura/parseo/efecto están en la app que arranca al boot (`/usr/share/support/userapp` → 
   `x6200_ui_v100`; monitoreada por monit): `XBandPlan::XBandPlan()`, invocado desde el ctor de `XParamAdmin`.
3. El flag produce efectos observables en la UI: habilita el ATU fuera de banda ham, muestra el indicador
   rojo `labelFullbandTxe`, y además el UI lo **baja** al pack del baseband (`BaseCommIIC`, offset 0x120)
   junto con el permiso de TX por banda.
4. El firmware del MCU/baseband está en `/usr/firmware/x6200_mcu_20250111134652.xgf` (524.288 B) y es
   **opaco**: entropía 7,7484 bits/byte (≈ 8 = cifrado/comprimido), sin strings legibles y sin ninguna
   referencia ASCII a `fullband` (ver [`ref-fabrica/05-mcu-xgf-opaco.txt`](./ref-fabrica/05-mcu-xgf-opaco.txt)).
   No se puede leer sin herramientas/formato que no tenemos.

**Matiz honesto**: que el gate y la configuración sean UI-side **no prueba** que el MCU acepte TX fuera de
banda por sí solo. El UI le manda el flag y los permisos por banda; si el MCU además aplica su propia
restricción es algo que **no se puede concluir con análisis estático** de este firmware (el `.xgf` es opaco).
La única forma de saberlo es F0 en la radio (transmitir y medir). Ver §7.

---

## 6. Valor exacto del flag y discrepancia `enable` vs `enabled`

- **Fábrica**: `fullband-tx=disable` (línea 7 del conf; default del código = 0).
- **README público** (`Full TX`): setear `fullband-tx` a **`enable`**. Cita textual:

  > ## Firmware Version 1.0.1 - 1.0.6
  > 1. Edit xgradio.conf: `nano /etc/xgradio/xgradio.conf`
  > 2. Set `fullband-tx` to `enable`
  >
  > ## xgradio.conf - Default
  > ```
  > [mods]
  > fullband-tx=disable
  > ```
  > ## xgradio.conf - Updated
  > ```
  > [mods]
  > fullband-tx=enable
  > ```

  URL: <https://github.com/tom-acco/Xiegu-X6200-Research/blob/main/Full%20TX/README.md> (snapshot
  [`ref-fabrica/06-github-fulltx-README.md`](./ref-fabrica/06-github-fulltx-README.md)).
  El README raíz del repo: <https://github.com/tom-acco/Xiegu-X6200-Research>.
- **La tarea original decía `disabled→enabled`**. La discrepancia **no importa en la práctica**: el binario
  no compara igualdad, busca la subcadena `enable` case-insensitive (`QString::indexOf("enable", 0,
  Qt::CaseInsensitive) != -1`, §4.2). Por lo tanto `enable` **y** `enabled` (y `ENABLE`, `Enable`, etc.)
  habilitan igual. `disable`/`disabled` quedan cerrados porque no contienen `enable`.
  Canónico/documentado: **`fullband-tx=enable`**.
  (Efecto colateral del `indexOf`: un valor raro como `not-enable` también habilitaría; no usar valores
  raros. `disabled` no habilita.)

---

## 7. Qué se pierde usando el port y qué NO se puede concluir sin F0

**El port no tiene este gate** (verificado en esta misma sesión y en la documentación interna del port):

- `grep -rn -i 'fullband\|xgradio' /home/pablo/desa/x6200/x6200_gui` → sin coincidencias en código;
- el propio diseño del port lo documenta: `x6200_gui/docs/cb27-spec.md` §1.4 «El flag `fullband-tx` del
  firmware oficial no está en esta ROM» (`tar -tf images/rootfs.tar | grep -i xgradio` vacío;
  `grep -a fullband-tx images/x6200_*.img` sin coincidencias; el PTT del GUI llama a `radio_set_ptt()` sin
  consultar banda/frecuencia). Ver `x6200_gui/docs/cb27-spec.md:102-118`.
  (Ese documento atribuye a la «evidencia pública» los símbolos `isTxEnable()`/`isHamBand()`; el README
  público sólo documenta el `.conf`. Los símbolos y su rol están verificados en este informe contra el
  binario, no en el README.)

Consecuencias:

- En el port **no hay nada que «destrabar»** en el rootfs: si el TX en 27 MHz estuviera bloqueado, el
  bloqueo no vendría de un archivo de configuración Linux como en el oficial; sólo podría venir del
  baseband/MCU (que el port no cambia desde Linux).
- Comparar con el oficial sirve como referencia de comportamiento esperado, no como prueba de que el port
  transmita.

**Sin F0 no se puede concluir** (queda como hipótesis a medir):

1. Si el X6200 con firmware oficial y `fullband-tx=enable` **realmente transmite** en 26,965–27,405 MHz
   (CB 27) y con qué potencia/modo; el análisis estático sólo muestra que la UI no bloquea y que manda el
   flag al baseband.
2. Si el **baseband/MCU** tiene o no su propia restricción de banda/frecuencia (el `.xgf` es opaco, no se
   pudo revisar).
3. Si la **cadena de RF/PA/filtros** del X6200 es físicamente capaz de entregar potencia en 27 MHz (el
   firmware no lo prueba).
4. Si en el port la combinación «bandas CB en la BD + `radio_set_ptt()` sin gate» alcanza para TX real en
   27 MHz, o si el baseband rechaza por su cuenta.
5. La línea de fuente exacta del `fullband-tx=...` en `xbandplan.cpp` (no hay DWARF; sólo se conoce la
   línea 286 del log del propio binario, ver §4.3).

---

## 8. Sin fundar (explícitamente)

- **No** se determinó si el MCU/baseband impone límites de TX por banda/frecuencia. El firmware
  `x6200_mcu_20250111134652.xgf` no es analizable con las herramientas disponibles (alta entropía, sin
  strings); cualquier afirmación en ese sentido sería especulación.
- **No** se midió ni se transmitió en 27 MHz: no se tocó la radio (regla dura).
- **No** hay DWARF, así que no hay números de línea de fuente «de tabla de debug»: la línea 286 es la que el
  binario usa para su propio `qDebug` de esa clave.
- **No** se afirma que el gate sea el **único** control de TX: `isTxEnable()` existe pero está sin llamadores
  en este binario, y el PTT de la UI no consulta banda/frecuencia; el control efectivo puede ser parcial o
  total en el baseband (no verificable acá).
- `labelFullbandTxe` es el `objectName` literal que aparece en el binario (con la «e» final, tal cual); no se
  determinó el texto visible exacto de ese label (se setea por traducción en runtime).
- No se analizó la partición DATA (p3, vfat) porque el gate no vive ahí; no se montó.

---

## 9. Reproducibilidad (comandos, sin root, sin montar)

```bash
FW=/home/pablo/lenovo_vieja/Downloads/radio/x6200/2025-01-15_Xiegu_X6200_update_V1.0.6/sdcard.img
mkdir -p /home/pablo/desa/x6200/.f0-ref && cd /home/pablo/desa/x6200/.f0-ref

sfdisk -d "$FW"                                          # p2 = 34816..854015 (ext4, 819200 sectores)
dd if="$FW" of=rootfs.ext4 bs=512 skip=34816 count=819200 conv=sparse status=progress
debugfs -R "cat /etc/xgradio/xgradio.conf" rootfs.ext4
debugfs -R "rdump / ./root" rootfs.ext4
grep -arl 'fullband-tx' root
strings -a -t x root/usr/app_qt/x6200_ui_v100 | grep -E 'fullband|xgradio\.conf'
readelf -sW   root/usr/app_qt/x6200_ui_v100 | grep -iE 'XBandPlan|isHamBand|isTxEnable'
arm-linux-gnu-objdump -d --start-address=0x8f708 --stop-address=0x8f9a0 root/usr/app_qt/x6200_ui_v100
# mapeo de strings: script python que interpreta los 'ldr rX,[pc,#imm]' + 'add rX,pc,rY'
# (los literales del pool se leen del propio ELF; target = (pc_add+8) + literal)
```

---

## 10. Artefactos (chicos, dentro de `ref-fabrica/`; binarios grandes NO commiteados)

| Archivo | Contenido |
|---|---|
| [`ref-fabrica/xgradio.conf`](./ref-fabrica/xgradio.conf) | el conf de fábrica extraído (133 B, idéntico al del rootfs) |
| [`ref-fabrica/00-metodo-procedencia.txt`](./ref-fabrica/00-metodo-procedencia.txt) | sha256 del sdcard.img (== oficial), particiones, dd, inodos, hashes de artefactos |
| [`ref-fabrica/01-grep-strings-rootfs.txt`](./ref-fabrica/01-grep-strings-rootfs.txt) | `grep -arl` sobre todo el rootfs y `strings -t x` de los dos binarios de UI |
| [`ref-fabrica/02-xbandplan-ctor-gate-disasm.txt`](./ref-fabrica/02-xbandplan-ctor-gate-disasm.txt) | tabla de refs PC-relativo + desensamblado del ctor con el gate |
| [`ref-fabrica/03-isTxEnable-isHamBand-disasm.txt`](./ref-fabrica/03-isTxEnable-isHamBand-disasm.txt) | disasm de `isTxEnable`/`isHamBand` |
| [`ref-fabrica/04-consumidores-flag-disasm.txt`](./ref-fabrica/04-consumidores-flag-disasm.txt) | los 4 consumidores del flag + log `FullBandTx: %d` + BaseCommIIC |
| [`ref-fabrica/05-mcu-xgf-opaco.txt`](./ref-fabrica/05-mcu-xgf-opaco.txt) | evidencia de que el firmware del MCU `.xgf` es opaco |
| [`ref-fabrica/06-github-fulltx-README.md`](./ref-fabrica/06-github-fulltx-README.md) | snapshot del README público «Enable Full TX» |
| [`ref-fabrica/06b-github-root-README.md`](./ref-fabrica/06b-github-root-README.md) | snapshot del README raíz del repo público |

Datos permanentes (no se commitean): `.f0-ref/rootfs.ext4` (400 MiB) y `.f0-ref/root/` (rootfs volcado,
~469 MB) en `/home/pablo/desa/x6200/.f0-ref/`.

## 11. Cita

- Información pública sobre el mod `fullband-tx`: **<https://github.com/tom-acco/Xiegu-X6200-Research/blob/main/Full%20TX/README.md>**
  (repo: <https://github.com/tom-acco/Xiegu-X6200-Research>). Texto clave: «Edit xgradio.conf … Set
  `fullband-tx` to `enable`», default `fullband-tx=disable`. Snapshot local:
  [`ref-fabrica/06-github-fulltx-README.md`](./ref-fabrica/06-github-fulltx-README.md).
- Sin cambios operativos en la radio: no se flasheó, no se montó, no se escribió en dispositivos.
