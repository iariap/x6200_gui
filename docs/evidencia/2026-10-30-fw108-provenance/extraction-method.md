# Método de validación y extracción — Fase A FW 1.0.8 (#375)

Documento de método. Todo lo descrito se ejecutó **offline**, sin acceso a la radio, sin `sudo`, sin `mount`/`losetup`, sin `dd` a dispositivos y sin ejecutar ningún script o binario del paquete. Las rutas absolutas de scratch se documentan en `manifest.json`.

## 0. Principios

1. **Sólo lectura sobre fuentes**: el ZIP se descargó a scratch; el material histórico (`/home/pablo/lenovo_vieja/.../V1.0.6*` y `/home/pablo/desa/x6200/refs/vendor-fw/`) se hasheó y leyó **en su ubicación original**, sin copiarlo ni modificarlo.
2. **Nunca montar**: las particiones se leyeron de dos formas alternativas: (a) extracción de la partición completa a un archivo en scratch con `7z`, y (b) herramientas de sólo lectura sobre la imagen/partición (`debugfs`, `dumpe2fs`, `sfdisk`, `fdisk -l`, `mdir`, `mtype`, `pdfinfo`, `pdftotext`).
3. **Nunca ejecutar**: ni `install_emmc.sh`, ni `fmt_emmc.sh`, ni la GUI vendor, ni el `.xgf`, ni los `.scr` de U-Boot. Los scripts se leyeron como texto.
4. **Cero acceso a hardware/red de radio**: sin WiFi/SSH/USB hacia el equipo, sin I²C/UART, sin TX/PTT/ATU, sin flash/actualización/factory reset.
5. **Sin binarios propietarios en Git**: los artefactos y las extracciones quedan en scratch fuera del repositorio; al PR sólo van metadatos y hashes.

## 1. Herramientas usadas

| Herramienta | Versión / origen | Uso |
|---|---|---|
| `curl` | sistema | descarga HTTPS oficial, headers, ETag, rangos |
| `sha256sum` | coreutils | hashes calculados (dos pasadas; verificación con `-c`) |
| `unzip` / `zipinfo` | Info-ZIP | listado, `-t` (CRC), extracción del ZIP |
| `7z` | 26.04 (x64) | `7z l`/`7z t` del ZIP, reconocimiento MBR y ext4, extracción de particiones y streaming |
| `file` | sistema | clasificación por magic |
| `sfdisk` / `fdisk` | util-linux | tabla de particiones MBR (**sólo lectura**, `-l`/`-d`) |
| `dumpe2fs` | e2fsprogs 1.47.3 | superbloque ext4 (**sólo lectura**) |
| `debugfs` | e2fsprogs 1.47.3 | listado y extracción de archivos del ext4 (**sin `-w`**, sólo lectura) |
| `mdir` / `mtype` | mtools | listado/lectura de particiones FAT en `imagen@@offset` (sólo lectura) |
| `readelf`, `strings`, `od` | binutils/coreutils | inspección estática de ELF y bytes |
| `pdfinfo`, `pdftotext` | poppler | metadatos y texto de PDFs oficiales (sin abrirlos en visor) |
| `python3` | sistema | validación de entradas ZIP, CRC32, hashes por rango, entropía |

## 2. Validación previa a la descarga (procedencia)

1. Descarga de la página oficial `https://xiegu.eu/downloads/` y extracción del texto con un script Python (strip de `<script>/<style>` + unescape + colapso de espacios) para **citar literalmente** la entrada `X6200_Update_1.0.8 (2026-08-20)` y su `href` `…download_id=10216`.
2. `curl -I -L` sobre el endpoint WP para obtener la cadena 302 → URL final y los headers del objeto (`content-type`, `content-length`, `last-modified`, `etag`).
3. Descarga del anuncio oficial del blog para registrar las versiones declaradas APP/BASE y la fecha del post.
4. Registro del link 1.0.7 (`download_id=10215`) y de su HTTP 500 (no se lo necesitaba, pero se documenta para no simular disponibilidad).

## 3. Descarga y verificación de integridad del ZIP

1. `curl -L --retry 5 --fail --retry-delay 5 --connect-timeout 30 --max-time 3600` con log de métricas (`http_code`, `size_download`, `time_total`, etc.) y `-D` para headers. Se registra el **exit code** (`0`).
2. Comparación `bytes_on_disk == content-length` (371 310 845).
3. `sha256sum` dos veces (coincidentes) y `sha256sum -c` sobre el archivo de hash.
4. Reproducibilidad del servidor: `If-None-Match` (espera 304) y dos peticiones `Range` (primer/último MiB, espera 206) comparadas por hash contra el archivo local.
5. `unzip -t` (verificación CRC de todas las entradas) y `7z t` (verificación independiente).
6. **No** se usó la copia alternativa de Google Drive del blog: requiere login y no se acepta como sustituto.

## 4. Validación de seguridad del ZIP antes de extraer

Script Python con `zipfile.ZipFile.infolist()` que rechaza/aborta si encuentra:

- nombre con path absoluto (`/…`, unidad `C:`);
- segmento `..` (path traversal);
- backslashes;
- entrada symlink (bits de modo `0o120000` en `external_attr`);
- ratio de compresión `file_size/compress_size > 1000` (posible archive bomb);
- bytes extra después del EOCD (`PK\x05\x06`) o comentario de archivo inesperado;
- y comprueba el total descomprimido (856 687 351 B, factor 2,307×) como cota de expansión.

Resultado en 1.0.8: **0 entradas riesgosas**; sólo se procedió a extraer tras el PASS. La extracción se hizo con `unzip -q -o ZIP -d <scratch>/vendor/x6200-fw108-vendor/` (nunca sobre el rootfs del sistema ni sobre un dispositivo).

## 5. Extracción de la imagen microSD y sus particiones

1. `file`, `7z l`, `sfdisk -d`, `fdisk -l` sobre `sdcard.img` para confirmar MBR y offset/tamaño de cada partición.
2. Extracción **sólo de la partición rootfs** (p2) a scratch con `7z x sdcard.img 1.img -o<scratch>/ext108/` (archivo de 419 430 400 B). El hash de la partición extraída se verificó igual al SHA256 del rango correspondiente de `sdcard.img`.
3. Partición p1 (FAT, 16 MiB) y p3 (FAT16, 400 MiB): no se copiaron enteras; se listaron y hashearon sus archivos **por stream** con `mdir -i "sdcard.img@@<offset>"` y `mtype -i "sdcard.img@@<offset>" ::/archivo | sha256sum`.
4. Para el `rootfs.tar` de p3: hash y listado **por stream** (`mtype … ::/rootfs.tar | sha256sum`, `| tar -tvf -`) sin copiar el tar a disco; se extrajeron dos archivos puntuales por stream (`tar -xOf - ./usr/app_qt/x6200_ui_v100`, `…/x6200_mcu_*.xgf`) para comparar sus hashes con los del ext4.

## 6. Inspección del rootfs ext4 (sin montar)

- `dumpe2fs -h` para UUID/features/estado.
- `debugfs -R "ls -l /usr/app_qt"` y `ls -l /usr/firmware` para inventariar los binarios y sus metadatos.
- `debugfs -R "dump <ruta> <destino-en-scratch>"` para materializar los binarios clave (GUI vendor, `.xgf`, `u-boot`, `zImage`, DTB, scripts, `emmc_sources/`) **en scratch** y hashearlos/inspeccionarlos con `file`, `readelf`, `strings`.
- Listado completo del rootfs con `7z l -slt <partición>` (7-Zip reconoce ext4) para búsquedas por nombre/magic sin montar (7 516 paths), guardado como derivado en scratch.

## 7. Inspección estática de ELF y `.xgf`

- `file` y `readelf -h/-S/-s` sobre los ELF ARM: clase, máquina, ABI, entry point, presencia de `.symtab`/`.strtab` (no stripped) y ausencia de DWARF.
- `strings -a -t x`: búsqueda de la constante de versión suelta (`1.0.8` en 1.0.8, `1.0.6` en 1.0.6, mismo patrón/vecindad) y de señales como `slip_encode`/`slip_decode`/`BaseCommIIC`.
- Diff de listas de símbolos (`readelf -sW | awk FUNC/OBJECT | sort -u`) 1.0.6 vs 1.0.8.
- `.xgf`: `file` (=data), primeros 16 B en hex, entropía de Shannon y ratio ASCII para caracterizar que es **opaco**; no se desencriptó ni ejecutó.

## 8. Documentación oficial de instalación

- Descarga del tutorial oficial (`download_id=9241`, PDF 300 348 B) y del PDF histórico del 1.0.6 local.
- `pdfinfo` para metadatos y `pdftotext -layout` para extraer **texto**; el procedimiento se cita en `upgrade-owner-checklist.md` y no se ejecutó.

## 9. Comparación histórica (material local verificado)

- Hash y listado del ZIP 1.0.6 y de su `sdcard.img` (sólo lectura en su ruta original).
- Verificación del hash declarado en su release note (coincide: control positivo).
- Extracción de la partición ext4 1.0.6 a scratch con `7z` y `debugfs dump` de sus binarios; comparación por hash con `refs/vendor-fw/` para **demostrar** que ese material es el del paquete 1.0.6.
- Comparación 1.0.6 ↔ 1.0.8 por tipo (GUI con GUI, BASE con BASE), tamaños y hashes; lo ausente se marca `UNKNOWN`, no se inventa.

## 10. Criterios de STOP

Se detiene y se reporta sin PASS si: la descarga falla de forma reproducible; el tamaño no coincide con `content-length`; un hash no es estable entre métodos; el ZIP no pasa `unzip -t`/`7z t`; hay entradas riesgosas; o **el hash declarado por el fabricante no coincide con el payload** (este caso). En todos ellos el estado es explícito en `README.md`/`manifest.json` y no se habilita RE.
