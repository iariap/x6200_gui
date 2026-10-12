# Auditoría de integración: cómo se llega a cada feature y qué se verificó

Documento de control del programa de port (2026-10-12). Existe por una razón concreta: **dos
cableados fallaron en silencio** durante el port (las apps no eran seleccionables desde la
interfaz, y el script de Field Ops leía variables de entorno con el nombre viejo). Este documento
recorre **feature por feature** cómo se llega a ella, de qué depende y qué se probó, para no
volver a dar por buena una integración sin mirarla.

**Leyenda de verificación:** 🧪 probado sin radio (test nativo, contenedor o consulta) ·
🔌 necesita la radio · 📄 sólo compilación e inspección del binario o de la imagen.

---

## 1. Cómo se abren las apps

Todas van por **Ajustes → acciones de pulsación larga**: se elige la acción y se le asigna un
botón; después se mantiene ese botón para abrirla. La lista es `long_action_items[]` en
`dialog_settings.c`.

| Acción en la lista | App |
|---|---|
| `APP WSPR`, `APP NavTex`, `APP SSTV`, `APP WeFax`, `APP JS8` | modos digitales |
| `APP Cluster` | DX Cluster |
| `APP Field Ops` | SOTA/POTA |
| `APP CW Keyboard` | telegrafía escribiendo |
| `Theme` | cambia el tema (rota por los 6 + los JSON de `/mnt/themes`) |
| `Screen rec`, `Screenshot` | grabación de pantalla |
| `APP RTTY`, `APP FT8`, `APP SWR Scan`, `APP Settings`, `APP Recorder`, `APP Callsign`, `APP WiFi`, `APP EQ`, `QTH Grid`, `About` | las de siempre |

**Corregido en la auditoría**: las entradas de las apps portadas se habían escrito con otro
formato de línea y **nunca entraron en el array** (ver `NEW_FEATURES.md` §22); tampoco estaban
`Callsign`, `WiFi` ni `EQ`, del firmware original. Verificado en el binario: los 12 rótulos están
presentes.

Aparte de esa lista, hay tres apps que se abren de otra forma (como en el X6100):
**mensajes de voz** (pulsación larga del botón de modo en SSB/AM/NFM), **mensajes CW** (igual, en
modo CW) y la **grabación** (acción propia).

---

## 2. Feature por feature

### 2.1 Modos digitales

| Feature | Cómo se llega | Depende de | Verificación |
|---|---|---|---|
| **WSPR** | `APP WSPR` | `cfg.wspr_*` (frecuencia, %, subida a WSPRnet), `/mnt` para logs | 🧪 round-trip del motor (158 comprobaciones, decodifica a −20/−25 dB) |
| **NavTex** | `APP NavTex` | nada extra | 🧪 loopback a +20/+10/+5 dB |
| **SSTV** | `APP SSTV` | `rootfs/usr/share/x6200/sstv/{tx_pics,tx_text}` (10 archivos) | 🧪 los 7 modos, correlación 0,92–0,98 |
| **WeFax** | `APP WeFax` | nada extra | 🧪 300/300 líneas, correlación 0,995 |
| **JS8** | `APP JS8` | `third-party/js8core` (GPLv3), `world_map`, `cty.dat`, `js8_map` | 🧪 **la suite del X6100 pasa entera: 1994 aserciones en 88 casos** |
| **FT8/FT4** | `APP FT8` | red (opcional, para UDP) | 🧪 UDP contra un receptor real en el host |

Alineado/inclinación de **WeFax**: se ajustan con los botones de la app (`Align`, `Tilt`, `Fix`)
y se guardan en `cfg.wefax_align/tilt` ✅. Lo que **no** está portado es la **perilla** (el
registro de controles `CTRL_*` del X6100): 🔌 para probar los botones en la radio.

### 2.2 Cluster, Field Ops y CW

| Feature | Cómo se llega | Depende de | Verificación |
|---|---|---|---|
| **Cluster DX** | `APP Cluster`; la **columna** en la pantalla principal (Ajustes → `Cluster column`) | red; 5 servidores + pota.app | 🧪 50+ comprobaciones con la suite portada de ellos; la columna se toca para sintonizar |
| **Field Ops** | `APP Field Ops` | `x6200-field`, `x6200-pota-spots`, 8 iconos, `cty.dat`, red; variables `X6200_*` | 🧪 32 comprobaciones (referencias, ADIF, bandas) y la consulta a la base |
| **CQ de voz** (Field Ops) | `Rec CQ` / `CQ` en la página 2 | `/mnt/msg` (WAV), micrófono | 🧪 WAV contra la libsndfile del target (el MP3 no era posible); 🔌 nivel de micrófono y audio emitido |
| **CW Keyboard** | `APP CW Keyboard` | teclado vinculado; `msg_cw` en la base | 🧪 consulta a la base real; 🔌 telegrafía real |

**Corregido en la auditoría**: `x6200-field`/`x6200-pota-spots` leían `X6100_CALL`,
`X6100_POTA_API`, `X6100_QRZ_API` mientras el web configurator pasa `X6200_*` → la subida a QRZ
desde la web usaba un indicativo vacío. Ahora leen `X6200_*` y el *User-Agent* dice
`X6200-LU2DSX`.

### 2.3 Pantalla y UX

| Feature | Cómo se llega | Depende de | Verificación |
|---|---|---|---|
| **Aguja de S-meter** | Ajustes → interruptor `S-meter needle` | — | 📄 en Ajustes y en el binario (no verificable sin ver el panel) |
| **Band plan IARU** | Ajustes → interruptor `Band plan` | `band_plan_data.c` | 🧪 datos del plan (test nativo); 🔌 aspecto en pantalla |
| **Temas JSON** | acción `Theme`; temas propios en `/mnt/themes` | `/usr/share/x6200/themes` (12 archivos) → se copian a `/mnt/themes` al arrancar | 🧪 carga de temas incluidos, JSON propio, fallback y lista (test nativo); 🧪 la pantalla principal usa los roles del tema (§28) y la GUI se reinicia al cambiarlo |
| **Fuente** | Ajustes → `Font` (Sony/Roboto/DejaVu/Arial) | se aplica **en el próximo arranque** (igual que el X6100: `fonts_apply()` corre una vez) | 📄 en Ajustes |
| **Grabación de pantalla** | acción `Screen rec` | `/mnt/screenrec` | 🧪 escritor AVI; 🔌 grabación real |
| **Aviso de versión** | automático al arrancar | `/mnt/update_check.conf` (¿activado?, fuente), red | 🧪 parseo de versiones y comparación |

### 2.4 Conectividad

| Feature | Cómo se llega | Depende de | Verificación |
|---|---|---|---|
| **wfview** | Ajustes → interruptor; `/mnt/wfview.conf` | puertos 50001-50003 | 🧪 protocolo completo (handshake, login, CI-V) con sockets reales |
| **Control remoto (web)** | página `/remote` | socket `/tmp/x6200_remote.sock`, ~65 comandos de **todas** las apps (FT8 incluido) | 🧪 protocolo y comandos en el binario; 🔌 punta a punta con la radio |
| **Web configurator** | `http://<ip>/` y `https://<ip>/` | `S96webserver` + paquete `x6100-webserver` | 🧪 19 páginas/endpoints contra la base real |
| **Bluetooth CAT** | Ajustes → `Bluetooth` y `Discoverable (CAT)`; consola: `btcat-setup`, `btcat-ctl` | `S94btcat`, `/etc/default/btcat`, `/mnt/btcat.conf` | 📄 filas en Ajustes y scripts en la imagen; 🔌 adaptador y pairing |
| **Auriculares BT** | app `APP Bluetooth` (lista, scan, vincular, conectar, RX) y `btaudio-ctl` por consola | política de PulseAudio, `/tmp/bt_passkey` | 🧪 parseo de la salida del script (test nativo); 🔌 audio real |
| **Teclados BT/USB** | automático (evdev); el **passkey** se muestra en la app de Bluetooth | — | 🧪 *rollover* con 5 casos; 🔌 teclado real |
| **Pairings** | automático | `S39btpairings`, `/mnt` | 📄 servicio en la imagen |

### 2.5 Sistema

| Servicio | Para qué | Nota |
|---|---|---|
| `S01create_data` | crea la partición `DATA` en `/mnt` | del firmware original |
| `S02update` | aplica actualizaciones desde la SD: busca `x6200_r1cbu_*.tgz` en `/mnt` y lo descomprime sobre `/` | del firmware original; el release publica ese tgz (lo arma `tools/release/make_release_assets.sh`) |
| `S03tune_cache` | caché de sintonía | del firmware original |
| `S04restore_backup_config` | restaura la configuración respaldada | del firmware original |
| `S39btpairings` | restaura los pairings de Bluetooth | portado del X6100 |
| `S94btcat` | levanta el puente de CAT por Bluetooth | portado |
| `S95gui` | arranca la GUI | del firmware original |
| `S96webserver` | arranca el web configurator | portado |

---

## 3. Qué se verificó sin radio y qué queda para el equipo

**Verificado sin radio (🧪):** 401 comprobaciones del arnés nativo del host, las suites de códec
contra las herramientas del propio X6100 (WSPR, NavTex, SSTV, WeFax), la suite completa de JS8,
las 19 páginas del web configurator, el protocolo de wfview con sockets reales, la carga de temas
JSON, el *rollover* del teclado y el formato WAV contra la libsndfile del target.

**Necesita la radio (🔌):**

1. **Bluetooth**: qué transporte usa el chip (`hciconfig -a`, `dmesg | grep -i blue`) y los
   pairings reales (CAT, auriculares, teclado).
2. **Audio de TX**: nivel de excitación de los modos digitales (el X6200 no expone la revisión de
   su base, así que quedó un valor conservador).
3. **CQ de voz**: nivel de micrófono y audio emitido en SSB.
4. **CW Keyboard**: telegrafía real con un teclado vinculado.
5. **Pantalla**: aspecto de la aguja de S-meter y del band plan; temas en la pantalla principal
   (que sigue con estilos fijos, ver `X6100_FEATURE_GAP.md`).
6. **Web remote** (`/remote`) de punta a punta con la radio andando.
7. **DRM/KMS**: el equipo sigue con `fbdev`.
8. **Espacio en `/mnt`**: lo comparten grabación de pantalla, logs, imágenes RX y mensajes de voz.

---

## 4. Deuda de integración conocida

- **Perilla** de alineado/inclinación de WeFax y **perilla con hilo propio**: no portadas.
- **CW Trainer**: la única app del X6100 que no está.
- **Banderas de país en FT8**: falta el header generado por `tools/flags_gen.py` (el `cty.dat` que la imagen instala todavía no lo usa nadie).
- **TX Cal / CESSB**: la librería del X6200 no expone esos comandos.

**Limpiado en esta auditoría**: ocho parámetros que había agregado (`wspr_tx_*`, `wspr_upload`,
`wefax_align/tilt`, `cluster_all_bands/order`, `update_alert`) eran **duplicados muertos** de
valores que ya viven en `cfg.*` o en el archivo de configuración del aviso; como además
colisionaban por nombre con las claves de `cfg` en la misma tabla, se eliminaron para dejar una
sola fuente de verdad.
