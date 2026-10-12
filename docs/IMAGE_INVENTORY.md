# Inventario de la imagen (qué lleva el release, verificado)

Comprobación del artefacto que se publica, hecha **sobre la imagen misma** (partición extraída y
montada con 7z/sqlite) y no sobre el árbol de compilación. Es la evidencia del "verificada en la
imagen" de cada feature.

## La imagen

| | |
|---|---|
| Archivo | `x6200_20261012_013612.img` (545 MB) |
| sha256 | `8d9868da4804b9c7c30b87680986306034becb11a244c1c69ec2f27111975ad7` |
| Rootfs | `e2fsck -fn`: limpio · 5217/32768 inodos |
| Contenido | 5702 entradas (5288 archivos, 413 carpetas) |
| GUI | `usr/sbin/x6200_gui` — md5 idéntico al del `target` del build |

## Ejecutables y scripts

| Archivo | |
|---|---|
| `usr/sbin/x6200_gui` | la GUI |
| `usr/sbin/x6200-field` | Field Ops (POTA/SOTA), Python |
| `usr/sbin/x6200-pota-spots` | spots de pota.app |
| `usr/sbin/btcat-ctl`, `btcat-setup` | puente de CAT por Bluetooth |
| `usr/sbin/btaudio-ctl` | auriculares A2DP (y el pairing desde la app de Bluetooth) |
| `usr/sbin/x6200_daemon` | reinicia la GUI (lo usa el cambio de tema y DRM) |

## Assets

| Ruta | Contenido |
|---|---|
| `usr/share/x6200/themes/` | 13 archivos: el tema `RS` (10 PNG) + `Holo.json` y `Material 3.json` |
| `usr/share/x6200/sstv/` | 12 archivos: imágenes y textos de TX |
| `usr/share/x6200/field/` | 8 iconos de la app |
| `usr/share/x6200/cty.dat` | 343 KB (AD1C), para el país y las banderas de FT8 |
| `usr/share/x6200/params.default.db` | la base por defecto |
| `usr/lib/libdrm.so*` | 3 archivos, para el camino DRM/KMS (opcional) |
| `etc/init.d/` | 23 scripts de arranque (los del firmware + los portados) |

## La base de datos

| | |
|---|---|
| Modos digitales | **31** filas |
| De ellos, WSPR (`type=4`) | **11** frecuencias (1.8 MHz a 50 MHz) |
| `msg_cw` | mismo esquema que el X6100 (`id`, `val`) |
| Bandas | tabla `bands` poblada |

## Compilado dentro del binario (no son assets)

| | |
|---|---|
| Fuentes | 51 familias alternativas en `src/fonts/` (Roboto, DejaVu, Arial) — `fonts_apply()` |
| Banderas de país | los 16×12 RGB de `flags_data.h` (≈200 banderas) |
| Motor JS8 | `js8core` + `src/js8` |
| Mapas | `world_map` para el mapa de estaciones de JS8 |

## La actualización en el lugar

`x6200_r1cbu_v0.1.2-lu2dsx.tgz` (97 MB, sha256
`36bdddee60f58406b289cbedabf459ab5c42a0fd067603b99d0c1075017da093`):

- trae **el mismo binario GUI** que la imagen (comparado byte a byte; lo verifica
  `tools/release/make_release_assets.sh` y otra vez `publish_release.sh`);
- 25 scripts de arranque;
- `S02update` lo descomprime sobre la raíz al arrancar, conservando `/mnt`.

## La verificación completa, al día

Corrida sobre este mismo código: **7 suites, 0 fallos** — 469 comprobaciones del host, 158 de WSPR,
SSTV (7 modos), WeFax, NavTex, **1994 aserciones de JS8 en 88 casos** y las 19 páginas del web
configurator.

## Cómo se reprocesa

```bash
# inventario del rootfs
dd if=<imagen>.img of=/tmp/e.img bs=1M skip=33 count=512
7z l -slt /tmp/e.img | awk '$1=="Path" && $2=="=" {print $3}' | wc -l

# un archivo concreto
7z e -y -o/tmp /tmp/e.img usr/sbin/x6200_gui

# la base
7z e -y -o/tmp /tmp/e.img usr/share/x6200/params.default.db
sqlite3 /tmp/params.default.db "SELECT count(*) FROM digital_modes WHERE type=4"
```
