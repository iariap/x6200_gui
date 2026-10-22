# Features portadas del X6100 (IU1WXY) al X6200

Registro del programa de port: qué se incorporó del firmware del X6100 de **IU1WXY**,
cómo se usa y en qué estado está. Se actualiza a medida que se suman features.

| Feature | Estado | Dónde |
|---|---|---|
| Decodificados FT8/FT4 por **UDP** (GridTracker, JTAlert) | ✅ | [FT8_UDP.md](FT8_UDP.md) |
| **Aviso de nueva versión** | ✅ | §2 |
| **Aguja de S-meter** | ✅ | §3 |
| **CB 27 MHz**: dial por canal (4 grids A–D), `CH nn` y TX con tope/interlock | 🔶 RX y dial medidos en host/banco; TX (RF real) `requires-radio` (F0) | §42 |
| **Band plan IARU** sobre el waterfall | ✅ | §4 |
| **Grabación de pantalla** (AVI: video MJPEG + audio de RX) | ✅ | §5 |
| **Bluetooth CAT** (SPP, hasta 3 clientes) | ✅ | §6 |
| **Bluetooth auriculares** (A2DP + loopback) | 🔶 funciona por consola (`btaudio-ctl`); falta la UI | §6 |
| **wfview**: la radio como IC-705 en red | ✅ | §7 |
| **Web configurator ampliado** (15 páginas + HTTPS) | ✅ | §8 |
| **WSPR completo**: motor + UI (spots, banda, TX, subida) | ✅ | §9 |
| **NavTex**: decodificador SITOR-B + app | ✅ | §10 |
| **SSTV**: 7 modos (RX/TX), app y assets | ✅ | §11 |
| **WeFax**: fax meteorológico (RX/TX, Auto) | 🔶 falta la perilla de alineado/inclinación | §12 |
| **Cluster DX**: spots, filtros, alertas y app | 🔶 falta la columna en la pantalla principal y el *spot snap* | §14 |
| **Field Ops** SOTA/POTA: referencias, log ADIF y RBN | ✅ incluye el **CQ de voz** | §15 |
| **JS8** (motor + app completa) | ✅ | §16 |
| **Temas JSON** (motor, 6 temas y temas propios) | 🔶 los diálogos de apps siguen el tema; la pantalla principal no | §17 |
| **Teclados USB/Bluetooth** (detección y *rollover*) | 🔶 entrada ✅ y app **CW Keyboard** ✅ (§21); falta la UI de pairing | §18 |
| **Control remoto en vivo** (socket para la página `/remote`) | ✅ sin control remoto de FT8 | §19 |
| **DRM/KMS** + perilla con hilo propio | ❌ pendiente | [X6100_FEATURE_GAP.md](X6100_FEATURE_GAP.md) §5 |
| **Propagación** (beacons IBP/IARU R2, solo RX) | ✅ | §43 |

**Leyenda:** ✅ hecho · 🔶 parcial (dice qué falta) · ❌ pendiente. El detalle de cada pendiente y de
cada parcial está en [X6100_FEATURE_GAP.md](X6100_FEATURE_GAP.md) §5.

Todo lo portado mantiene la licencia LGPL-2.1-or-later y la atribución al autor original
en el código.

---

## 2. Aviso de nueva versión

Al arrancar, en un hilo aparte, la radio consulta la última release publicada y avisa con
un mensaje si es más nueva que la versión que está corriendo:

```
Firmware v0.1.3 available
```

Configuración: `/mnt/update_check.conf` (si no existe, se usan los valores por defecto)

```
# X6200 new release alert
enabled=1
source=https://api.github.com/repos/iariap/x6200_gui/releases/latest
```

| Clave | Significado |
|---|---|
| `enabled` | `1`/`0`. Con `0` no se crea ni el hilo |
| `source` | URL que devuelve un JSON con `tag_name` (cualquier API de releases sirve). También acepta que devuelva la versión pelada |

El path se puede cambiar con `$UPDATE_CHECK_CONF` y el comando de consulta con
`$UPDATE_CHECK_CMD` (esto último lo usan los tests, que no tocan la red).

Detalles:

- Usa `python3` (ya presente en la imagen para el web configurator) vía `popen`, con un
  timeout de 15 s; espera hasta ~20 minutos a que la WiFi esté lista y reintenta.
- La comparación de versiones es numérica, no alfabética: `v0.112` es más nueva que
  `v0.99`. Una versión vacía o inválida nunca se considera más nueva.
- El módulo **no sabe de LVGL**: recibe un callback de notificación, así que se testea
  entero en el host. Del original **no se portó el "ping" de telemetría** a un sitio
  de terceros.
- Tests: `tests/native/test_update_check.c` (comparador de versiones, parser del JSON de
  GitHub, y el flujo completo con un comando de consulta falso).

---

## 3. Aguja de S-meter

`Ajustes → S-meter needle` cambia el medidor de barras por un **dial analógico**: un arco
con marcas y números (S1…9 y +20/+40), la zona de +20/+40 en rojo y más gruesa, y la aguja
indicando el nivel actual, con el valor en dB debajo.

- La geometría es la del X6100 (el pivote queda por debajo del recuadro, así se ve solo la
  parte superior del arco), adaptada a la paleta y las fuentes de esta GUI.
- Es un parámetro persistido (`smeter_needle`), así que queda guardado en `/mnt/params.db`.
- **Sin verificar visualmente**: no tenemos pantalla en el banco de pruebas; la geometría
  sigue la del original, pero conviene mirarla en la radio y ajustar si hace falta.

---

## 4. Band plan IARU

Una **franja fina sobre la parte baja del espectro**, alineada con la fila de frecuencia, que
muestra qué es cada tramo del span visible (IARU Región 1, 160 m a 6 m):

| Color | Significado |
|---|---|
| Azul | CW |
| Verde | Modos digitales |
| Gris | Todos los modos / SSB |
| Ámbar | Balizas |

- `Ajustes → Band plan` la prende/apaga (persistido como `band_plan`); por defecto **encendida**.
- Sigue el mismo span que las etiquetas de frecuencia, así que se mueve con el dial y con el
  ancho del espectro. Los segmentos se recortan al borde y las etiquetas ("CW", "DIG", "BCN",
  "SSB") solo se dibujan si entran en el hueco; si no, queda solo el color.
- 24 segmentos simultáneos como máximo (un span muy ancho puede mostrar varias bandas juntas).
- La tabla de frecuencias y la geometría están separadas del dibujo (`band_plan_data.c`, sin
  LVGL) para poder testearlas: **72 comprobaciones** cubren el mapeo a píxeles, el recorte, los
  spans anchos, fuera de banda y los casos degenerados.
- Los colores en el X6100 salen del sistema de temas; acá son fijos hasta que portemos los
  temas JSON.

---

## 5. Grabación de pantalla

Graba la pantalla **y el audio de RX** en un archivo AVI (MJPEG + PCM mono) en la partición
`DATA`:

```
/mnt/screenrec/screen-YYYYMMDD-HHMMSS.avi
```

- Se prende/apaga desde la acción **Screen rec** (Ajustes → acciones de pulsación larga, junto
  a `Screenshot`). Mientras graba se ve un indicador con un punto rojo.
- 8 fps, calidad JPEG 70, audio de captura a 44,1 kHz en el mismo AVI, con índice `idx1` para
  que se pueda buscar/adelantar en cualquier reproductor.
- Antes de arrancar verifica que queden al menos 50 MB libres; si el disco se llena corta sola y
  avisa. También cierra el archivo al apagar (`atexit`).
- La captura se engancha en el `flush_cb` de la pantalla: mientras graba guarda una copia del
  cuadro y después lo dibuja por el camino normal (fbdev). No agrega trabajo cuando está apagada.

**No portado del original**: el camino DRM/KMS (acá la pantalla es fbdev), los colores del tema
(usa la paleta fija) y el "monitor mientras graba" — el X6100 sube el nivel del monitor al
grabar y lo baja al terminar; esta GUI lo deja en manos del ajuste `MONI` del usuario.

Tests: `tests/native/test_avi_writer.c` escribe un AVI real con cuadros y audio falsos y valida
la estructura RIFF (`RIFF`/`AVI `/`hdrl`/`strl`/`MJPG`/`auds`/`movi`/`00dc`/`01wb`/`idx1`), el
tamaño de RIFF y que abrir en una ruta inválida falle limpiamente.

---

## 6. Bluetooth: CAT por SPP y auriculares A2DP

Portado del X6100 de IU1WXY (scripts + `btcat.c`, LGPL). **Todo el soporte que hacía falta
ya estaba en la imagen**: los módulos de PulseAudio para Bluetooth, `bluez5-utils`
(`bluetoothctl`, `btattach`, `hciattach`) y los drivers USB `btusb`+`btrtl`.

### Kernel

Se agregó el transporte **HCI por UART**, que faltaba:

```
CONFIG_BT_HCIUART=m + H4, 3WIRE, INTEL, BCM, RTL
```

Así el equipo funciona con el chip en USB (lo que ya cubrían `btusb`/`btrtl`) **o** en un
UART (Realtek/Broadcom/Intel), que era el hueco. `RFCOMM` y `HIDP` ya estaban.

### Bluetooth CAT: `btcat`

`/usr/sbin/btcat` abre el UART de CAT real (`/dev/ttyS2`), crea una **pty y monta su slave
sobre `/dev/ttyS2`**, y escucha en RFCOMM canal 1 (SPP). Así la GUI (sin cambios) habla con
`btcat` en lugar del UART, y se multiplexan los clientes USB y Bluetooth: una consulta por vez
a la GUI, respuesta al que preguntó, y los *transceive* (dst=0x00) a todos.

- `S94btcat` lo arranca antes de `S95gui` (espera a que el bind-mount esté listo) y
  `btcat-setup` configura el adaptador: alias, `pairable`, registro SDP y agente de pairing.
- Ajustes: `/etc/default/btcat` (valores por defecto) y `/mnt/btcat.conf` (estado en runtime,
  sobrevive al reflash porque vive en la partición DATA).
- `btcat-ctl` permite ver/editar esa configuración y el estado.
- Sirve para apps de teléfono tipo **FT8CN** que manejan la radio por CAT sobre Bluetooth.

### Auriculares A2DP: `btaudio-ctl`

- `/etc/pulse/system.pa.d/btaudio.pa` fija la política: **solo A2DP** (nada de HFP/HSP, que en
  esta familia de equipos no da audio y además corta el WiFi), sin *auto-switch* de perfil y
  **sin** loopear a TX el audio de un teléfono que se conecta como fuente A2DP.
- `btaudio-ctl` (253 líneas, adaptado) maneja todo desde consola:

```
btaudio-ctl list | scan | pair MAC | connect|disconnect|remove MAC
btaudio-ctl rx on|off          # loopback del audio de RX a los auriculares
btaudio-ctl volume 0..100 | mute on|off
btaudio-ctl autoconnect MAC on|off
btaudio-ctl boot | monitor     # lo usa btcat-setup y un monitor de fondo
```

- El audio llega por `module-loopback source=alsa_input.platform-sound.stereo-fallback
  sink=bluez_sink.<MAC>.a2dp_sink` (la misma fuente PulseAudio que ya usa la GUI).
- Las vinculaciones se respaldan en `/mnt/backups/bluetooth.tgz` y `S39btpairings` las
  restaura antes de que arranque `bluetoothd`, así una imagen nueva no las olvida.

### Lo que NO se portó (queda pendiente)

- **UI de Bluetooth en Ajustes → Wireless**: en el X6100 el pairing y todos estos ajustes se
  manejan desde la GUI. Acá los scripts funcionan por consola/SSH (y `btcat-setup` ya deja el
  adaptador listo al arrancar), pero todavía no hay pantalla de pairing.
- **Teclado Bluetooth**: los scripts lo detectan y lo reportan como rol `kbd`, pero la GUI abre
  dispositivos de entrada fijos (`/dev/input/event0/4`) y no tiene el camino dinámico de evdev
  que el X6100 sí tiene. Requiere ese trabajo en la GUI.

### Verificación

- `hci_uart.ko` y `rfcomm.ko` presentes en la imagen; `btcat` compilado e instalado en
  `/usr/sbin/btcat`.
- Scripts validados con `sh -n`, y el fragmento de PulseAudio cae en
  `/etc/pulse/system.pa.d/` (nuestro `system.pa` incluye ese directorio).
- **Requiere probar en la radio**: qué transporte usa el chip (USB o UART) se confirma con
  `hciconfig -a` y `dmesg | grep -i blue`; el pairing real, con `bluetoothctl`.

---

## 7. wfview: la radio como IC-705 en la red

El equipo se anuncia y se comporta como un **IC-705** con el protocolo LAN de Icom, así que se
puede manejar con **wfview** (u otro cliente compatible) desde una PC en la misma red.

- Puertos: **control 50001, CI-V 50002, audio 50003** (UDP).
- El **CAT se comparte** con el UART: los clientes de red entran al mismo motor de CAT por un
  mutex, y las tramas *transceive* (cambios de frecuencia) se copian a la red además del UART.
- El audio de RX se transmite con `pa_simple` tomando la misma fuente de PulseAudio que usa la GUI.
- Configuración en `/mnt/wfview.conf`:

```
enabled=1
user=x6200
pass=<tu clave>
```

  `Ajustes → wfview server` prende y apaga el servicio (usa ese mismo archivo, así que la GUI y
  el futuro web configurator coinciden); las credenciales se editan en el archivo.
  `$WFVIEW_CONF` permite apuntar a otro archivo (lo usan los tests).

**Cambios que hice sobre el original** (y por qué):

- Los hilos ahora **duermen más cuando el servicio está apagado** (poll de 1 s en vez de 100 ms,
  sin keepalives, y el de audio 300 ms en vez de 50 ms): el X6100 mantiene el ritmo de 10 Hz
  siempre. Es la regla de batería del proyecto.
- Ajusté la truncación de las credenciales leídas del archivo (el `snprintf` original podía
  truncar en silencio).
- Expuse `wfview_server_passcode()` (la codificación de clave de Icom) para poder testear el
  handshake y para el futuro web configurator.

### Tests

`tests/native/test_wfview.c` compila el **servidor real** en el host (con stubs de CAT y
PulseAudio) y habla el protocolo por UDP en loopback:

- Defaults y persistencia del archivo de configuración (encender lo reescribe).
- Los tres puertos quedan escuchando (un segundo `bind` falla con `EADDRINUSE`).
- Handshake: *are you there* (→ 0x04), *are you ready* (→ 0x06) y *ping* (eco del secuencia).
- **Login aceptado y rechazado**: con las credenciales correctas la respuesta trae código 0 y
  `wfview_server_client()` devuelve el nombre del cliente; con la clave mal, código de error y
  ningún cliente.
- **Puente CI-V**: una trama del cliente llega al motor de CAT y su respuesta vuelve al cliente.
- ***Transceive***: el *sink* que el servidor instala en el CAT entrega las tramas espontáneas a
  la red.

Son 37 comprobaciones nuevas (280 en total, 0 fallos).

**Requiere probar con hardware**: la latencia del audio y el comportamiento con un cliente real
(wfview) en la red.

---

## 8. Web configurator ampliado (con HTTPS)

El navegador web de la radio pasa de 6 a **15 páginas + API JSON**. Es el webserver extendido
del X6100 de IU1WXY, que a su vez es un fork del `x6100_webserver` de Georgy Dyuldin — el mismo
que ya traíamos en su versión upstream, así que **el esquema de base es idéntico**
(`bands`, `band_params`, `digital_modes`, `memory`, `msg_cw`, `params`, `transverter`).

Páginas: índice, **bandas**, **modos digitales**, **memorias y canales**, **mensajes de CW**,
**cluster DX**, **servidor wfview**, **Field Ops**, **logs** (con export ADIF), **navegador de
archivos**, **galería**, **backup/restore**, **fecha/hora (NTP)** y **control remoto**.

### Repo propio y adaptaciones

El fork vive en **[iariap/x6200_webserver](https://github.com/iariap/x6200_webserver)** y el
paquete de buildroot lo trae fijado a un commit. Cambios sobre el original:

- Identidad visible: títulos, pie, nombres de backup/export de log, sujeto del certificado
  autofirmado (`CN=x6200`, `DNS=x6200`) y credenciales por defecto del servidor wfview.
- Rutas propias: `/tmp/x6200_remote.sock` (control remoto en vivo) y `x6200-field`
  (+ `X6200_FIELD_DIR`/`X6200_CALL`) para Field Ops.
- El módulo Python conserva el nombre `x6100_webserver` (imports, script y arranque) para no
  romper la integración con la imagen.

### HTTPS

`S96webserver` ahora arranca también **HTTPS en el puerto 443** con un certificado autofirmado
que se genera con `openssl` en `/mnt/https` (persiste en la partición DATA). Para eso agregué
`BR2_PACKAGE_OPENSSL=y` + `BR2_PACKAGE_LIBOPENSSL_BIN=y` al defconfig: la librería ya estaba,
faltaba el binario. Si la generación falla, el servidor sigue en HTTP.

### Tests

`tests/run_pages_test.py` del fork arranca **la app real** con la base de datos que la imagen
distribuye (`/usr/share/x6200/params.default.db`) y pide las **19 páginas/endpoints**:

```
19 comprobaciones, 0 fallos
```

Cubre las 14 páginas, 3 endpoints JSON, el CSS estático y que ninguna página se caiga cuando su
subsistema todavía no está (el control remoto en vivo, el cluster y Field Ops se dibujan con
estado vacío/deshabilitado). El test encontró dos rutas mal supuestas por mí (`/files/` con
barra final y que los canales se sirven por `/api/channels`, no como página) y me obligó a
inyectar el shim de Python 3.11 para `bottle-sqlite` del wheel de PyPI (la imagen ya lo trae
parcheado por buildroot, así que ahí funciona sin shim).

### Lo que todavía no funciona en el navegador

- **Control remoto en vivo** (página `/remote`): depende de `remote_server.cpp` de la GUI
  (40 KB: socket, eventos, audio y video), que es una feature aparte todavía pendiente.
- **Field Ops** (`/field`) y **cluster** (`/cluster`): las páginas renderizan, pero los
  subsistemas (`x6200-field`, configuración del cluster) todavía no están portados.
- **CW trainer**: la página lo usa el firmware del X6100; acá no existe ese módulo.

---

## 9. WSPR: motor de decodificación

Primera pieza de los modos digitales, y la más grande de las que se pueden verificar **sin
radio**: el motor completo de WSPR.

### Qué se portó

- `src/wspr/wspr.c/h`: codificador de mensajes tipo 1, sintetizador 4-FSK, **grabador de
  banda base** (NCO + remuestreador de liquid-dsp, de 11.025 kHz de audio a los 375 Hz de I/Q) y
  el pegamento con el decodificador.
- `src/wspr/wsprd/` (K1JT): el motor de decodificación (sync, demodulación, Fano, tablas),
  vendorizado con su licencia y su `UPSTREAM.md`. Es **código C autocontenido**: solo necesita
  FFT en float (`libfftw3f`), que la imagen ya traía.
- `src/wspr/wsprnet.c`: formateo y envío de spots a WSPRnet (sockets propios, sin libcurl).

### El test: round-trip completo

`tests/native/test_wspr.c` hace el camino entero —**codificar → sintetizar audio → banda base →
decodificar**— y lo corre en el contenedor del arnés (necesita `libfftw3f` y `liquid-dsp`, que
ahora quedan instalados en `x6200-uiharness`):

```
spot: IU1WXY JN35 37 dBm, 14097130.0 Hz, snr -20, dt 0.07
158 comprobaciones, 0 fallos
```

Cubre:

- Round-trip a **−20 dB** de SNR: el spot decodificado tiene el indicativo, el locator, la
  potencia y la **frecuencia exacta** (dial 14.0956 MHz + 1530 Hz de audio).
- Segundo round-trip con otro indicativo y **−25 dB**.
- **Solo ruido: 0 spots** (no inventa decodificaciones).
- Mapeo de potencia a los 19 niveles que WSPR puede transmitir, monotonía, y el recorte por
  debajo de 1 mW.
- Límites del codificador: los indicativos compuestos (`DL/IU1WXY`) no entran en un mensaje
  tipo 1; locators completos sí, medios no.
- El grabador de banda base: cuánto acumula, que no desborda con más de un ciclo y que el
  `reset` lo vacía.

### Detalle de la investigación

Perdí un rato con "declaraciones implícitas" de liquid hasta darme cuenta de que **yo mismo
había borrado el `#include <liquid/liquid.h>`** creyendo que no se usaba: los nombres
(`nco_crcf_mix_down`, `msresamp_crcf_execute`) están detrás de macros de *mangling* del header
amalgamado, así que un `grep` del nombre literal no los encuentra. Restaurado.

Para que el test se pueda correr en cualquier lado, el `Dockerfile` del arnés ahora compila e
instala **liquid-dsp 1.4.0** (la misma versión que usa la imagen).

### La UI: portada completa

El diálogo (`dialog_wspr.c`) quedó integrado: tabla de spots, selección de banda y modo digital,
control de TX (ciclo de transmisión, siguiente slot, subida a WSPRnet) y el estado en la línea
inferior. Se abre desde las **acciones de pulsación larga → `WSPR`** (no tenemos las páginas de
botones de apps del X6100, así que se entra por donde entran las demás apps nuestras).

Para que compile y funcione hubo que resolver la lista de diferencias que había relevado:

| Diferencia | Solución aplicada |
|---|---|
| Sistema de temas (`TC(rol)`, `theme_shape`) | `theme.c/h`: shim con la paleta por rol del tema por defecto del X6100. **No afecta** nuestras pantallas |
| `cfg/cfg_api.h` (`cfg_*`, `param_i_get`, `cparam_i_get`, `subject_i_*`) | Shim sobre `cfg_cur`/`cfg`, con los accesores con los nombres del X6100 |
| Ajustes de WSPR (`cfg_wspr_tx_freq/tx_pct/upload`) | Tres `cfg_item_t` nuevos persistidos en la tabla `params` |
| `cluster/spots.h` | Tabla IARU R1 local (`cluster/spots.c`): el cluster no está portado, pero la tira muestra la banda |
| `remote_server.h` | No-ops documentados: el control remoto en vivo es otra feature pendiente |
| `tx_level.c` | Portado tal cual (control de excitación con ALC) |
| `x6100_control_get_base_ver().rev` | Nuestra lib solo da el string de versión: valor conservador (r3) **a validar en la radio** |
| `button_data_t` | Es nuestro `button_item_t` renombrado: alias + dos campos (`mark`, `disabled`) en nuestro struct |
| `.subj` (suyo `Subject*`, nuestro `Subject**`) | Adaptadas las 3 asignaciones del diálogo |
| `AUDIO_DECIM` | Definido en `audio.h` (44100/4 = **11025 Hz**, la tasa que los modos digitales esperan) |
| Callback de audio (`float*` vs nuestros `cfloat*`) | Adaptador que toma la parte real en bloques de 2048 |
| `gps_last_fix`/`gps_phone_fix` | `gps.c/h` stub: este ROM no tiene GPS, el locator sale del QTH guardado |
| `dsp_set_spectrum_enabled`/`waterfall_enabled` | Aceptadas y sin efecto: nuestras apps son diálogos sobre la pantalla principal |
| `radio_set_modem` | Estaba escrita y comentada en nuestro `radio.c`: activada |
| `CFG_DIG_TYPE_WSPR` + frecuencias | Agregado al enum y **11 frecuencias WSPR** a la semilla `sql/digital_modes.csv` |

**Pendiente y documentado**: `dialog.restore_radio` se acepta pero todavía no restaura
frecuencia/modo al cerrar (el mecanismo del X6100 es aparte), y el nivel de excitación de TX
necesita validación en la radio.

### Lo que exigió (relevamiento)

La UI es `dialog_wspr.c` (tabla de spots, banda, controles de TX y subida automática) más la
infraestructura que comparten los cinco diálogos de apps (`app_frame`, `freq_strip`,
`tx_level`, `tx_player`, `station`). Hice el relevamiento completo y esto es lo que hay:

**Lo que se adapta con shims chicos** (ya identificado, cada uno de pocas líneas):

| Falta en nuestro árbol | Solución |
|---|---|
| Sistema de temas (`theme.h`, `TC(rol)`, `theme_shape`) | Shim que devuelve la paleta fija por rol; el código portado no se toca. Cuando se porten los temas JSON, el shim se reemplaza solo |
| `cfg/cfg_api.h` (estado de radio como `cfg_*` + `param_i_get`) | Shim sobre nuestro `cfg_cur` (`fg_freq`, `bg_freq`, `mode`, `pwr`) y los ajustes de WSPR nuevos en `cfg_t` |
| `cluster/spots.h` (nombre de banda en la tira de frecuencia) | Tabla IARU R1 local; el cluster no está portado |
| `remote_server.h` (publicar estado al control remoto web) | No-ops documentados; el servidor remoto es otra feature pendiente |
| `tx_level.c` (control de excitación con ALC) | Es puro y chico: se porta directo |
| `x6100_control_get_base_ver().rev` en `tx_player` | Nuestra lib solo expone la cadena de versión: queda un valor conservador **a validar en la radio** |

**El bloqueante real: el framework de botones.** `dialog_wspr.c` usa el API `button_data_t`
(páginas de botones de apps) que el X6100 introdujo y aplicó a **18+ diálogos**, incluidos los
que ya tenemos (FT8, mensajes, SWR scan, grabador, WiFi, Ajustes). Portarlo implicaría
refactorizar toda nuestra UI; el camino elegido es **reescribir la capa de botones del diálogo
de WSPR contra nuestro API de botones**, que es una fracción del trabajo y sin riesgo para el
resto de las pantallas.

**Estado**: el motor (RX/TX de audio, codificación y decodificación) está portado, testeado y en
la imagen; la UI queda para la próxima tanda, empezando por los shims de la tabla de arriba.

---

## 10. NavTex (SITOR-B)

El receptor de NavTex completo: decodificador SITOR-B con FEC, presentación de mensajes y
guardado, más la lista de frecuencias.

### Qué se portó

- `src/navtex/`: el decodificador (`decoder.cpp`, `navtex_rx.cpp` con el demodulador FSK a
 100 baudios y ±85 Hz, la tabla CCIR 476, el *phasing* y la FEC de SITOR-B) y `fftfilt.cxx` con
  `gfft.h` (la FFT que usa).
- `src/freq_list.{c,h}`: la lista de frecuencias en pantalla, **compartida con SSTV y WeFax** —
  portarla habilita los tres.
- `src/dialog_navtex.c`: la app (mensajes recibidos, estación, tipo, hora, guardado en
  `/mnt/navtex`, y los controles de la barra inferior).
- Se abre desde **acciones de pulsación larga → `NavTex`**.

### Adaptaciones

- El callback de audio: nuestro DSP entrega banda base **compleja** y el decodificador trabaja
  sobre audio real, así que un adaptador toma la parte real en bloques de 2048 (el mismo patrón
  que WSPR).
- `theme.c/h`: se sumaron los roles `TC_near_bg` y `TC_warn`.
- `freq_list.c` usa el enum de modos con los nombres viejos (`x6100_mode_*`): quedaron como
  alias de los nuestros en `cfg/cfg_api.h`, junto con `cparam_i_set` y `remote_text`.
- `DIALOG_H` se movió a `styles.h` (como en el X6100) para que lo vean todos los archivos
  portados, no solo los que incluyen `app_frame.h`.

### Test: el loopback del X6100, portado

`tests/native/run_navtex_tests.sh` compila el **códec real** con
`tests/native/test_navtex_loopback.cpp` (el loopback del X6100, adaptado): sintetiza una
transmisión SITOR-B completa —CCIR 476, cada carácter dos veces a 5 posiciones, *phasing*
alpha/rep, FSK continuo a 100 baudios con ruido— y la decodifica.

```
SNR +20.0 dB, 1 = mark: OK    "ZCZC FA01|TEST MESSAGE FROM X6200 ROM 123 456|NNNN|"
SNR +10.0 dB, 1 = mark: OK
SNR  +5.0 dB, 1 = mark: OK
polarity: a 1 bit is the mark (higher frequency)
all OK
```

También verifica **cuál polaridad** toma el decodificador (un bit 1 = frecuencia alta) y corre
sin fallas a +20, +10 y +5 dB de SNR. A 0 dB —el borde de SITOR-B— muestra los errores de
carácter pero no cuenta como fallo, igual que en el original.

---

## 11. SSTV (7 modos, RX y TX)

El modo de imagen lenta completo: recepción con detección de VIS, transmisión de imágenes y
texto, y la app con la previsualización.

### Qué se portó

- `src/sstv/`: `encoder.c`, `decoder.c`, `linesync.c`, `vis.c` y `preview.c` — **Robot 8 BW, Robot
  36, Robot 72, Martin M1/M2 y Scottie S1/S2**, con la sincronización de línea, la detección de
  VIS y el generador de previsualización PNG.
- `src/dialog_sstv.c` (~4.100 líneas): la app, con la galería de imágenes TX, las plantillas de
  texto, el modo TEST (sin RF), el progreso y el guardado de las RX.
- **Assets**: las 4 imágenes y las 6 plantillas de texto del X6100, en
  `/usr/share/x6200/sstv/{tx_pics,tx_text}` (el diálogo trabaja sobre `/mnt/sstv`, que es del
  usuario; estos son los de fábrica).
- Se abre desde **acciones de pulsación larga → `SSTV`**.

### Adaptaciones

- `disp_btn`: en su fork el botón expone el objeto de la etiqueta con ese nombre; en el nuestro
  se llama `label_obj`. En vez de renombrar nuestro API, los dos nombres son ahora una **unión
  anónima** en `button_item_t`, así los diálogos portados y nuestro `buttons.cpp` siguen igual.
- `DIALOG_H_MAX` agregado a `styles.h` (para los buffers estáticos del diálogo).
- El botón compartido `btn_dig_pwr` (de las páginas de apps del X6100) queda vacío en la página
  donde aparecía.
- Los bloques que publicaban el estado por `cJSON` al control remoto en vivo se reemplazaron por
  un comentario: ese servidor todavía no está portado, así que no hay nada que publicar.

### Test: el loopback del X6100, portado

`tests/native/run_sstv_tests.sh` compila el **códec real** con
`tests/native/test_sstv_loopback.c` (el loopback del X6100): codifica una imagen de prueba en
**cada modo**, le agrega ruido, la decodifica por el mismo camino que la app (VIS + el decoder en
RAM) y compara con la original.

```
Robot 8 BW  VIS   2 160x120, 120 rows, luma correlation 0.920, mean error 10.6: OK
Robot 36    VIS   8 320x240, 240 rows, luma correlation 0.959, mean error  6.9: OK
Robot 72    VIS  12 320x240, 240 rows, luma correlation 0.975, mean error  5.5: OK
Martin M1   VIS  44 320x256, 256 rows, luma correlation 0.974, mean error  4.5: OK
Martin M2   VIS  40 320x256, 256 rows, luma correlation 0.939, mean error  7.6: OK
Scottie S1  VIS  60 320x256, 256 rows, luma correlation 0.982, mean error  3.7: OK
Scottie S2  VIS  56 320x256, 256 rows, luma correlation 0.956, mean error  5.9: OK
all OK
```

Los 7 modos pasan, con correlación de luminancia entre 0,92 y 0,98. Necesita `libpng` y
`libsndfile` (ambas ya en la imagen y en el contenedor del arnés).

---

## 12. WeFax (fax meteorológico, RX y TX)

El receptor/transmisor de fax: codificación y decodificación **Auto** (detección de APT de
arranque y parada, alineación de *phasing*, polaridad y velocidad de línea), más la app con la
previsualización y el guardado.

### Qué se portó

- `src/wefax/{encoder,decoder}.c`: el códec (APT, IOC 576, alineación de phasing, ajuste de
  inclinación) con modo Auto, que es el que usa la app.
- `src/dialog_wefax.c` (~4.300 líneas): la app, con previsualización, TEST (sin RF), progreso y
  guardado en `/mnt/wefax`.
- Se abre desde **acciones de pulsación larga → `WeFax`**.

### Adaptaciones

- `button_data_t` ganó los campos de su framework de perilla: `ctrl` (unión anónima con nuestro
  `data`) y `encoder_allowed`. Los ids `CTRL_WEFAX_ALIGN/TILT` existen en `controls.h`.
- `DIALOG_Y` agregado a `styles.h` y alias `panel_style` → `pannel_style` (así se llama en
  nuestro árbol).
- Ajustes nuevos: `wefax_align` y `wefax_tilt`, persistidos como los de WSPR.
- `mfk_update()` en nuestro árbol lleva un segundo argumento (`voice`): adaptadas las 4 llamadas.
- Bloques `cJSON` del control remoto en vivo reemplazados por un comentario (ese servidor no está
  portado).
- **Adaptador de audio** (parte real de nuestra banda base compleja) igual que en los otros
  modos. De paso corregí el mismo problema en SSTV, donde lo había dejado como *warning*: en
  runtime el decodificador habría recibido punteros del tipo equivocado.

### Pendiente y documentado

El **control por perilla** del alineado/inclinación (el registro `CTRL_*` + `mfk_set_ctrl` de su
fork, 512 líneas de `controls.cpp`) no está portado: los botones muestran los valores y existen
los ajustes, pero la perilla todavía no los mueve. `channel_overlay_show()` (el panel de canales
del X6100) se resuelve mostrando el nombre como mensaje, porque el diálogo de canales tampoco
está portado.

### Test: el loopback del X6100, portado

`tests/native/run_wefax_tests.sh` compila el **códec real** con
`tests/native/test_wefax_loopback.c`: codifica una imagen en la tasa de reproducción de la
radio, la decima a la del decodificador y la decodifica **en modo Auto**, como hace la app.

```
WEFAX AUTO: APT START DETECTED - provisional fax (300.0 Hz, line=0.996)
WEFAX AUTO: FAX CONFIRMED
WEFAX AUTO: PHASING aligned - pulse at x=1731..11 (width 90 px, 5/5 groups agree)
WEFAX AUTO: APT STOP DETECTED (432.0 Hz, line=0.859)
start generations 1, confirmed 1, stop 1, rows kept 393
phasing pulse at column 0, picture from kept row 58, 300 of 300 lines, correlation 0.9953, mean abs error 1.8
PASS
```

Verifica la detección de APT de arranque y parada, la confirmación del fax, la alineación de
phasing y que la imagen vuelve completa: **300 de 300 líneas con correlación 0,995**.

---

## 13. JS8: qué necesita (relevado)

> **Nota:** este apartado es el relevamiento previo a implementarlo. JS8 quedó **implementado**
> más abajo, en §16, y su suite de tests del X6100 pasa entera. Se deja como registro de lo que
> hizo falta.

Último modo digital que queda. Es, por lejos, el port más grande, así que lo relevé antes de
comprometer una ronda:

| Pieza | Tamaño | Notas |
|---|---|---|
| `third-party/js8core/` | 380 archivos, **156.565 líneas** | El módem y protocolo JS8, vendorizado por ellos desde `JS8Call-improved/Android-port`, commit `1f8d390a…` (2026-09-23), **GPLv3**, C++20 |
| `src/js8/` | 4.711 líneas | Alertas, ensamblador de mensajes, autorrespuesta, clasificación, comandos, dirigidos, inbox, RX |
| `src/dialog_js8.c` | 5.413 líneas | La app |
| `tools/js8_wavgen` + `test-audio/js8_test.wav` | — | Generador de WAV y audio de prueba para verificar sin radio |

**El bloqueante es una dependencia**: `js8core` incluye `<boost/crc.hpp>`, o sea necesita
**Boost**, que nuestra imagen no tiene. Hay que agregar el paquete de buildroot
(`BR2_PACKAGE_BOOST`) y reconstruir; recién entonces la librería compila.

**Verificado en esta ronda**: copié `js8core` a nuestro árbol, lo puse en el build y **compiló
hasta la dependencia de Boost** (`legacy_decoder.cpp:30: boost/crc.hpp: No such file`). Después
lo reverti para no dejar 21 MB de código vendorizado sin poder construir y mantener la imagen
verde (build de nuevo en 0 errores).

**Plan para la próxima ronda**: agregar Boost al defconfig → construir `js8core` → portar
`src/js8` (adaptando `cfg_api`/`app_frame`/`freq_list`/`tx_player`, que ya están) → el diálogo
(con la lección del framework de botones ya resuelta) → verificar con `js8_wavgen` y el WAV de
prueba.

---

## 14. Cluster DX

El cliente de cluster completo: conexión al servidor, parseo de spots, filtros por banda y
modo, alertas, y la app con la tabla de spots.

### Qué se portó

- `src/cluster/spots.c/h`: el parseo de las líneas del cluster (formato `DX de`, filas `sh/dx`
  con fecha, la lista de 300 spots con expiración por antigüedad, el mapeo banda/modo y la
  detección de modo por el comentario) y `client.c/h` con la conexión.
- `src/dialog_cluster.c` (~1.000 líneas): la app, con la tabla, los filtros, las alertas y el
  cambio de banda/modo al tocar un spot.
- Se abre desde **acciones de pulsación larga → `Cluster`**.
- **Reemplaza mi stub de bandas**: ahora `cluster_band_name()`/`cluster_band_of()` son los
  reales, así que la tira de frecuencia de las apps muestra la banda de verdad.
- Se sumaron los roles de tema que usa (`TC_alert*`, `TC_vfo*`, `TC_new`, `TC_veil`) y los
  ajustes `cluster_all_bands` y `cluster_order`.

### Fuentes: `font_select` y las familias alternativas

El diálogo usa `fonts/font_select.h`, que es el **cambio de familia de fuente en runtime** del
X6100 (mantiene los `sony_N` y les copia el contenido de arial/dejavu/roboto antes de armar la
UI). Portarlo requería esas familias, así que copié las **51 fuentes alternativas** (5,6 MB) y
agregué `fonts_apply()`/`fonts_scale()`/`font_down()`; el tema puede elegir familia como en el
original.

### Boost y los headers de desarrollo (172 MB)

Para JS8 hacía falta Boost, que agregué al defconfig (`BR2_PACKAGE_BOOST=y`, sin librerías: sólo
headers al sysroot). Pero Buildroot **también copia los headers al target**: 172 MB de cabeceras
de C++ que la radio nunca lee. Agregué un **post-build script** (`board/X6200/post-build.sh`) que
borra `/usr/include` del rootfs y los `.la`; el runtime no necesita nada de eso.

### Test: los casos del X6100, portados

`tests/native/test_cluster.c` es la versión en C de `tests/test_cluster.cpp` (8 casos) y corre en
el **host** (el parser no depende de LVGL):

- Líneas `DX de` (frecuencia, indicativo, spotter, comentario, minuto UTC) y nodos `-#`.
- Filas `sh/dx` con fecha → marca de tiempo UTC exacta; spots en vivo → sin fecha.
- Seis tipos de línea que **no** son spots (WWV, login, resumen del día…) y deben rechazarse.
- El almacén: orden por hora (más viejo primero, como llega el listado), reemplazo del mismo
  indicativo en la misma frecuencia, que un spot viejo **no** pise al nuevo, y el tope de 300.
- Antigüedad y expiración (incluyendo el cruce de medianoche) y el mapeo banda/nombre/modo.

```
330 comprobaciones, 0 fallos
```

El test encontró dos supuestos míos equivocados: el orden del almacén (es más viejo primero, no
más nuevo) y que `cluster_band_name(-1)` devuelve cadena vacía, no `NULL`.

---

## 15. Field Ops (SOTA/POTA)

La app de operación en el campo: referencias SOTA/POTA cercanas, log ADIF de la activación y
el feed RBN de "quién me escucha".

### Qué se portó

- `src/field/{refs,flog,rbn,bands}.c`: búsqueda de referencias por cercanía y por código
  (con `cty.dat`), escritura/lectura del log ADIF (incluye los campos que completa la consulta
  QRZ del teléfono) y el cliente RBN con el estado de actividad por banda.
- `src/dialog_field.c` (~1.900 líneas) y `src/dialog_bands.c`: la app y el editor de bandas,
  con sus *chips* de estado y el mapa de spots.
- **Assets y scripts**: los 8 iconos, `cty.dat` (343 KB) y los helpers `x6200-field` +
  `x6200-pota-spots` (adaptados de `x6100-*`), que son los que usan tanto la app como el web
  configurator (`/api/field`).
- Se abre desde **acciones de pulsación larga → `Field Ops`**.

### Adaptaciones

- **`knobs_info()`**: la app dibuja sus propias etiquetas de perilla. En vez de portar su módulo
  `knobs.cpp` (que pertenece al refactor de controles), escribí `knobs.c/h` como vista de sólo
  lectura sobre nuestros `vol.c`/`mfk.cpp`, con dos getters nuevos (`vol_get_mode()`,
  `mfk_get_mode()`).
- **`theme_png()`**: implementado con libpng y una caché (los iconos son PNG); la API de tema
  del X6100 queda así un poco más completa.
- **Perillas**: `main_screen_mfk()`, `main_screen_vol_press()`, `vol_change_ctrl()` y
  `vol_update(diff, voice)` mapeados a nuestro modelo de vol/mfk, y `main_screen_close_app()`,
  `main_screen_freq_locked()`, `main_screen_mode_locked()` implementados de verdad sobre el
  estado de la pantalla principal.
- **Mensajes de voz**: `msg_voice_rec_toggle/exists/send_toggle/poll` quedan como no-ops
  documentados (el refactor de mensajes de voz de su fork no está portado); el diálogo de
  mensajes de voz que ya teníamos sigue funcionando.
- Ajustes nuevos `field_boot` y `voice_msg_gain`, y los roles de tema que usa la app
  (`TC_info`, `TC_chip`, `TC_btn_mark`, `TC_panel_border`).
- De paso, acoté dos `snprintf` de `cluster/client.c` que podían truncar en silencio las líneas
  de filtros.

### Test: los casos del X6100, portados

`tests/native/test_field.c` es la versión en C de `tests/test_field.cpp`; refs, log y bandas no
dependen de LVGL, así que corre en el **host**:

- Referencias: cercanía ordenada por distancia (con `km` y altitud/puntos de la SOTA), búsqueda
  por código **sin distinguir mayúsculas**, y archivo inexistente como error (`-1`).
- ADIF: la línea exacta (`<CALL>`, `<QSO_DATE>`, `<TIME_ON>`, `<BAND>`, `<FREQ>`, `<MODE>`,
  `<MY_SOTA_REF>`, `<eor>`), que los campos desconocidos **no** se escriban, y que la consulta
  QRZ complete nombre/QTH/locator/país.
- El log: alta de dos QSO, lectura en orden con los tiempos correctos y dos registros ADIF en el
  archivo.
- Bandas: nombre de banda por frecuencia, frecuencia de banda para CW y las palabras de nivel.

```
362 comprobaciones, 0 fallos
```

---

## 16. JS8 (motor y app)

El último modo digital, y el más grande. Queda completo: motor, app y la **suite de tests del
X6100 pasando entera**.

### Qué se portó

- `third-party/js8core/`: el módem y protocolo JS8 (380 archivos, C++20, Eigen y CRCpp
  vendorizados) desde `JS8Call-improved/Android-port`, commit `1f8d390a`, GPLv3.
- `src/js8/` (19 archivos): resampler, velocidades, render, ensamblador de mensajes, clasificación,
  dirigidos, receptor, WAV, TX, comandos, estaciones, autorrespuesta, log de QSO, inbox, alertas,
  macros y la capa `js8_rx`/`js8_ops` que usa la app.
- `src/dialog_js8.c` (~5.400 líneas) y `src/js8_map.c` (el mapa de estaciones), con el mapa
  mundial (`world_map.h`).
- Se abre desde **acciones de pulsación larga → `JS8`**.

### Adaptaciones

- **cJSON**: en vez de reescribir a mano los bloques que publican al control remoto en vivo (que
  no está portado), agregué `cJSON.h` como **stub con la misma forma** (no-ops): los diálogos
  quedan legibles y el día que se porte el servidor remoto se reemplaza por el cJSON real.
- **Parámetros**: 16 ajustes nuevos de JS8 (`js8_auto`, `js8_hb`, `js8_speed`, `js8_alerts`,
  `js8_custom_hz`…) con los mismos valores por defecto que el X6100. Eso necesitó un *loader* y
  *saver* de `int32` que nuestro sistema de params no tenía (`params_load_int32`,
  `params_save_int32`, `params_int32_set`).
- **Roles de tema de las filas** de spots (`TC(row_heard)`, `TC(row_tx)`, `TC(row_to_me)`…), los
  filtros del `cfg` (`cfg_cur_filter_low/high` sobre nuestro `cfg_cur.filter`) y los ajustes
  `tx_filter_low/high`.
- **Lo que la lib del X6200 no expone**, documentado como aceptado-y-ignorado:
  `radio_set_tx_filter()` y `radio_set_rx_dsp_off()` (el X6200Control no tiene comandos de filtro
  de TX ni de apagado del DSP de RX) y `radio_speaker_play()`. Sí implementé de verdad
  `keypad_set_long_time()` (nuestro keypad ahora usa esa variable).
- Mismo **adaptador de audio** que los otros modos (parte real de nuestra banda base compleja).

### Test: la suite del X6100, entera

`tests/native/run_js8_tests.sh` compila `tests/test_js8.cpp` (2.756 líneas, Catch2) contra nuestro
`src/js8` y el `js8core` vendorizado, dentro del contenedor del arnés (que ahora trae **Catch2
3.5.2** y los headers de **Boost**):

```
All tests passed (1994 assertions in 88 test cases)
```

Cubre la compresión/descompresión JSC, el resampler 11025→12000 Hz (longitud, tonos en banda y
supresión de imágenes que caerían dentro de la banda JS8), el renderizado y reensamblado de
tramas de JS8Call, todos los caracteres imprimibles, los comandos del gateway APRS, los frames
compuestos, el ensamblador de texto multiparte, la clasificación, el log de QSO, el inbox, las
alertas y las velocidades.

Detalle del armado: `js8core` y `src/js8` **ambos** tienen un `resampler.cpp`, así que el runner
compila cada uno en su propio directorio para que no se pisen los `.o` (el primer intento falló
justamente por eso).

---

## 17. Temas JSON

El sistema de temas completo: **6 temas incluidos** (IU1WXY, Legacy, Outdoor, Classic,
Outdoor-Negative, VISION), temas **JSON propios** en `/mnt/themes`, imágenes por superficie
(PNG reescalado) y la elección desde la radio.

### Qué se portó

- **`theme.c/h` del X6100, tal cual**: carga de temas por nombre (built-in o JSON con `cJSON`),
  tabla de **128 roles de color** con cuatro variantes cada uno, formas por tema (radios, bordes,
  gradientes, tipografía, paleta del waterfall), y las funciones de superficie (`theme_image()`
  con *slicing*/bilineal y `theme_png()`).
- **`cJSON.c/h`** vendorizado: reemplaza el stub que había usado para los diálogos que publican al
  control remoto (misma API, así que los diálogos portados siguen igual).
- **Assets**: el tema `RS` (10 PNG) y dos temas de ejemplo (`Holo.json`, `Material 3.json`) en
  `/usr/share/x6200/themes`, y al arrancar se copian a `/mnt/themes` una sola vez (igual que el
  X6100: lo que ya está no se pisa).
- **Selección**: acción de pulsación larga **`Theme`**, que recorre `theme_list()` y aplica el
  elegido (se guarda en `theme_name`).

### Lo que esto habilita

Los diálogos de apps que venimos portando (WSPR, NavTex, SSTV, WeFax, JS8, Cluster, Field Ops)
**ya usaban `TC(rol)`** a través del shim que escribí en su momento; al reemplazar el shim por el
motor real, esos diálogos pasan a **seguir el tema elegido** — incluidos los colores de las filas
de spots y los de las bandas del band plan.

**Pendiente y documentado**: la **UI principal** (barra superior, botones, espectro, waterfall)
sigue usando los estilos fijos de esta GUI (428 referencias de color en 42 archivos). Llevarla al
sistema de temas es un refactor aparte y sin verificación visual posible acá.

### Test

`tests/native/test_theme.c` corre en el **host** (theme.c sólo necesita unos pocos tipos de LVGL,
que el stub del test provee, y libpng):

- Los temas incluidos: init por nombre, paletas distintas entre temas, y que un nombre
  desconocido **cae al tema por defecto** en vez de quedar en basura.
- `theme_list()`: los incluidos (nota: `Legacy` queda fuera de la lista a propósito; se puede
  elegir por nombre).
- Un **tema JSON propio**: se escribe en un directorio temporal (con `$THEMES_DIR`), se carga, y
  se verifican los colores que define, que los que **no** define conservan el valor del tema base,
  que las formas (`radius`) se aplican, y que aparece en la lista.
- La paleta del waterfall siempre existe.

```
376 comprobaciones, 0 fallos
```

---

## 18. Teclados USB y Bluetooth

El soporte de entrada para teclados externos (USB y Bluetooth, que en Linux aparecen igual como
dispositivos de entrada).

### Qué se portó

- **`keyboard.c` del X6100**: busca el teclado primero por `/dev/input/by-path/*-kbd` y, si no
  encuentra, **cae a `/dev/input/event*`** comprobando que el dispositivo *parezca* un teclado (la
  fila superior de letras Q..P y la barra espaciadora). Eso es lo que hace que un **teclado
  Bluetooth** funcione, porque no siempre aparece con el nombre `-kbd` en `by-path`.
- **Identidad del dispositivo** (`kbd_rdev`/`kbd_ino`) y un **re-escaneo temporizado**: el evento
  "USB conectado" puede llegar **antes** de que exista el nodo del teclado (el arreglo de
  randal007 en el X6100).
- **`kbd_rollover.c/h`**: cuando una tecla baja antes de que se suelte la anterior, el *rollover*
  convierte eso en los pares press/release que LVGL espera (y así el teclado no manda "ab" cuando
  se escribió "a" y después "b").

### Test

`kbd_rollover.c` es lógica pura sobre un callback de fuente, así que
`tests/native/test_keyboard.c` lo maneja con una **fuente falsa** en el host:

- Una tecla: press, release y la repetición del último estado cuando no hay nada nuevo
  (`continue_reading = false`).
- ***Rollover***: con A sostenida y B bajando, primero se reporta el release de A y el press de B
  en la lectura siguiente.
- **Auto-repeat** del kernel: se ignora (LVGL repite por su cuenta).
- Teclas sin carácter (Shift/Ctrl, `key == 0`) se saltean.
- Un release de una tecla que no es la sostenida no se confunde con ella.

```
401 comprobaciones, 0 fallos
```

**Pendiente**: la **UI de pairing** en Ajustes (el X6100 tiene su sección de Bluetooth en
Wireless). Hoy la vinculación se hace con `bluetoothctl`/`btaudio-ctl` por consola — los scripts
ya dejan el adaptador listo — y el teclado empieza a andar en cuanto el sistema lo ve como
dispositivo de entrada.

---

## 19. Control remoto en vivo (página `/remote` del navegador)

El servidor de control que faltaba para que la página `/remote` del web configurator deje de
mostrar la radio como desconectada: un **socket UNIX** (`/tmp/x6200_remote.sock`) por el que
pasan objetos JSON, uno por línea, con ~60 comandos (banda, modo, frecuencia, filtros, AGC, ATT,
PRE, volumen, squelch, NB/NR, PTT, CW, y los comandos de cada app abierta) y eventos hacia el
navegador, además del **alcance** (el espectro) y el audio.

### Qué se portó y cómo

- `remote_server.cpp/h` del X6100 (1.055 líneas) con el mismo protocolo y el mismo formato de
  eventos, apuntando a `/tmp/x6200_remote.sock` (el que ya usa nuestro fork del webserver).
- **`{"cmd":"swrscan","val":1|0}`** (issue #133, con la app `SWRScan` abierta): Run/Stop del
  barrido de ROE por el **mismo camino que el botón**. Es el único comando de app que inicia
  TX desde el socket (0.1 W, el piso del equipo): sólo con autorización del owner y en RX.
  El estado se lee en el `ax` de `get` (`swrscan_state`, `passes`, `pass`, `points`,
  `swrscan_tx`); contrato y guardas en [`swr133-spec.md`](swr133-spec.md).
- **Adaptador `cfg_sm`**: el módulo lee y escribe la radio a través del `SettingsManager` del
  X6100 (`cfg_sm.p_xxx.get()/set()`, `cp_xxx`, `current_band_id()`, `is_valid_hw_freq()`).
  Escribí `cfg/settings_manager.{h,cpp}` como capa de compatibilidad sobre nuestro `cfg`,
  `cfg_cur` y `params` (25 miembros), y `cfg/db.h/.cpp` con la `BandsTable` sobre nuestro
  `cfg_band_read_all_bands()`.
- Un **puente en C** (`cfg/cxx_bridge.c`): las cabeceras de esta GUI no tienen guardas
  `extern "C"`, así que los dos símbolos que el shim necesita se exponen con funciones puente en
  vez de redeclararlos (C++ no puede enlazarlos directo).
- Los diálogos portados (WSPR, SSTV, WeFax, NavTex, JS8, Field Ops, Cluster) **publican ahora
  eventos de verdad**: antes llamaban a los no-ops que había dejado como marcador.
- **Restauración del estado de radio**: implementé `dialog_restore_radio_on_close()` y
  `dialog_restore_mode_on_close()` (que necesitaba este servidor) sobre `cfg_memory_save/load`
  en el slot de respaldo que ya tenía esta GUI. Con eso se cierra el pendiente que venía
  documentado en WSPR/WeFax/JS8: al cerrar esas apps la frecuencia y el modo vuelven solos.

### Diferencias con el original (documentadas)

- **Sin control remoto de FT8**: nuestro diálogo FT8 es el upstream y no tiene los enganches
  `dialog_ft8_remote()/state()` de su fork (son ~100 líneas con una tabla de mensajes RX). Las
  otras seis apps sí responden a comandos.
- No están las apps que este ROM no tiene: **teclado CW** y el huevo de pascua *Snake* (sus
  entradas salieron del servidor); `RTTY` sí queda.
- La radio no expone por su librería de control el **RF gain** ni el **IF shift** de banda: esos
  comandos guardan el valor en memoria y responden, pero no cambian nada en el equipo.
- `cw_tune_recent()` (la última sintonización de CW) no se guarda en esta GUI: responde "nada".

### Verificación

Compila limpio para ARM y el binario lleva el socket configurado
(`/tmp/x6200_remote.sock`), que es el que espera la página `/remote` del web configurator. **La
prueba de punta a punta necesita la radio** (el servidor arranca con la GUI y habla con el
navegador), así que queda como verificación de hardware pendiente.

---

## 20. CQ de voz (mensajes de voz para Field Ops)

Con esto se cierra el último hueco funcional de Field Ops: la activación **en SSB**, donde se
graba el CQ una vez y la radio lo emite sola (con PTT), que es como se activa un parque o un
pico cuando no se quiere estar hablando todo el rato.

### Qué faltaba

El diálogo de mensajes de voz ya estaba portado (graba, lista, reproduce, y su botón *Send*
transmite con PTT), pero la **API con la que Field Ops lo usa** —`msg_voice_rec_toggle()`,
`msg_voice_exists()`, `msg_voice_send_toggle()`, `msg_voice_poll()`— había quedado inerte en el
port, así que los botones *Rec CQ* y *CQ* no hacían nada.

### La causa de fondo: MP3 no se podía ni escribir ni leer

El módulo original graba **MP3** (`SF_FORMAT_MPEG`). La libsndfile de esta imagen
(**1.2.0**) está compilada **sin soporte MPEG** (le falta libmp3lame), así que `sf_open()` para
escribir fallaba y el módulo entero quedaba muerto — incluido el botón *Rec* del propio diálogo,
que ya existía.

Comprobado contra la libsndfile del target, corriendo el binario ARM:

```
WAV  OK
MP3  File contains data in an unimplemented format.
round-trip: 200 muestras, o[5]=500 (esperado 500)
```

Se pasó a **WAV PCM 16 bits** (11025 Hz mono): no necesita dependencias nuevas y evita el coste
de CPU del codificador, que en un equipo a batería importa. Los mensajes viven en `/mnt/msg` y
aparecen en el diálogo de mensajes de voz como cualquier otro (que se abre con una **pulsación larga del botón de modo** en SSB/AM/NFM).

### Cómo se usa

En **Field Ops → página 2**: `Rec CQ` graba (el nombre es `CQ SOTA` o `CQ POTA` según el
programa; máximo 60 s, como el original), y después `CQ` lo emite en SSB. Una segunda pulsación
corta lo que está sonando; mantener `CQ` ajusta el nivel.

### Implementación

- `msg_voice_rec_toggle()` graba con nombre fijo (o para si ya estaba grabando).
- `msg_voice_send_toggle()` lanza el envío en un hilo: **PTT + reproducción**, el mismo camino
  que el botón *Send* del diálogo (no el de los modos digitales, que colorea la señal con su
  tono de 1325 Hz), y baja PTT al terminar o al abortar.
- `msg_voice_poll()` corta la grabación pasados los 60 s o al cerrar la app, para no dejar nunca
  una grabación abierta ni el PTT puesto.
- La grabación del diálogo y la de Field Ops comparten `create_file()`/`play_file()` (que además
  quedó como un único camino de reproducción, en vez de dos copias).

**Falta probarlo en el equipo**: el nivel de micrófono y el audio emitido son cosas que no se
pueden verificar sin la radio.

---

## 21. CW Keyboard: telegrafiar con el teclado

La app que le da sentido al soporte de teclados de §18: se escribe en la pantalla y la radio
**telegrafía** lo que se escribe, con el codificador de CW que ya tenía la GUI.

### Qué se portó

- `dialog_cw_kbd.c/h` (470 líneas) del X6100, con la pantalla de texto, el envío en vivo, los
  **mensajes CW guardados** (los mismos que edita Ajustes, ahora con una lista para elegirlos) y
  el indicador de progreso.
- Se abre desde **Ajustes → acciones de pulsación larga → `CW Keyboard`**.
- Al abrirla **pone el modo en CW** (si no estaba bloqueado) y al cerrarla **devuelve la
  frecuencia y el modo** anteriores, que es el comportamiento del original.
- **Control remoto**: la app responde por la web (`cw_send`, `cw_stop`, `cw_wpm`, `cw_dec`) y
  publica su estado, así que la página `/remote` puede telegrafiar por vos.

### Lo que hizo falta del lado de nuestra GUI

- `params_msg_cw_list()`: la API para enumerar los mensajes CW guardados (el X6100 la tiene; la
  nuestra sólo tenía cargar/crear/editar/borrar). Es la misma consulta que el original
  (`SELECT val FROM msg_cw ORDER BY id`) y la tabla tiene el mismo esquema que la suya.
- `cw_encoder_sent()`: cuántos caracteres del mensaje en curso ya salieron (para el progreso).
- `cw_get_wpm()`: la velocidad de telegrafía. **Diferencia documentada**: el X6100 informa la
  velocidad que está *mandando* (la mide); la nuestra devuelve la **configurada**, que es la que
  la app muestra.
- Restauré en el servidor remoto los cuatro comandos de CW y su estado, que había sacado cuando
  la app no existía.

### Verificación

- Compila limpio para ARM y la app queda en el binario.
- La consulta de la lista de mensajes se probó contra la **base de datos real de la imagen**
  (`params.default.db`): la tabla `msg_cw` existe con el mismo esquema que el X6100 y la consulta
  corre sin error.
- **Falta probarlo en el equipo**: la telegrafía real necesita la radio (y un teclado vinculado).

**Sigue pendiente**: el **CW Trainer** (el entrenador de oído), que es otra app aparte.

---

## 22. Cómo se abren las apps (y un bug que arreglamos acá)

Todas las apps portadas se abren desde **Ajustes → acciones de pulsación larga**: se elige la
acción y se le asigna a un botón. La lista es `long_action_items[]` en `dialog_settings.c`.

**Bug encontrado y corregido:** las entradas de las apps portadas se habían escrito con otro
formato de línea (`.label = " JS8 "` en vez de `.label = " APP RTTY "`), así que el texto
**nunca entró en el array** y **ninguna de las apps nuevas era alcanzable** desde la interfaz,
aunque el manejo de la acción sí estaba en `main_screen.c`. Se agregaron las nueve entradas
(WSPR, NavTex, SSTV, WeFax, JS8, Cluster, Field Ops, CW Keyboard, Theme) y, de paso, tres apps
del firmware original que tampoco estaban en la lista (**Callsign, WiFi, EQ**).

Verificado en el binario: cada etiqueta nueva está presente y las nueve acciones están manejadas
en `main_screen.c`. El ajuste de la **aguja de S-meter** y el **band plan** ya estaban en Ajustes
como interruptores, así que esos dos sí eran alcanzables.

---

## 23. CW Trainer: entrenador de CW

La última app del X6100 que faltaba. Es un entrenador de **prefijos de indicativos**: arma
indicativos al azar a partir de 55 prefijos reales (`IU1`, `IK2`, `DL#`, `EA8`, `LU#`, `JA#`,
`K#`…) según el **nivel** (de 1 a 5) y los emite en CW con el codificador de la GUI, para que
los copies de oído.

- **Escuchar** (RX): la app emite el grupo; el texto sale **decodificado** en pantalla para que
  te corrijas, y podés **repetirlo**.
- **Telegrafiar** (TX): también podés picar el grupo con la paleta y ver si el decodificador
  entendió lo que mandaste.
- **Grupo**: cambia entre 5 conjuntos de prefijos. **Nivel**: largo del indicativo.
- Respeta el modo de práctica del equipo (`cfg.key_train`): si está apagado, no emite (te lo dice).
- Se abre desde **Ajustes → acciones de pulsación larga → `APP CW Trainer`**.

### Lo que hizo falta portar

- `dialog_cw_trainer.c/h` (1.257 líneas) y `panel.cpp/h` (222 líneas, el panel de texto que usa;
  lo incluí con nuestro `pannel_style`).
- Tres enganches en el módulo de CW, **todos apagados por defecto** para no cambiar el
  comportamiento normal:
  - `cw_trainer_set(bool)`: hace que el DSP de CW corra aunque el decodificador esté apagado
    (la app necesita decodificar lo que ella misma emite).
  - `cw_decoder_set_sink(texto, wpm)`: el texto decodificado va al entrenador en vez del panel;
    si no hay sink, sigue yendo al panel **y ahora también al control remoto** (`cw_text`), así
    que la página `/remote` muestra el CW decodificado.
  - `cw_encoder_require_train(bool)`: cuando el entrenador lo pide, la llave no cierra si el
    modo de práctica del equipo está apagado.
- Dos ajustes nuevos en `cfg` (`cwt_level`, `cwt_group`) que sólo usa esta app.
- `rtty.h` tenía funciones sin guardas `extern "C"`, lo que rompía a los archivos C++ que las
  llaman (`panel.cpp`): se agregaron.

### Verificación

Compila limpio para ARM y la app queda en el binario con sus 19 apps. **Falta probarlo en el
equipo**: el entrenamiento real necesita la radio (que emite el CW) y, para la parte de TX, una
paleta o el teclado.

Con esto **no queda ninguna app del X6100 sin portar** (la única que no está es *Snake*, que es
un huevo de pascua, y el selector `MODES`, que acá se reemplaza por las acciones de pulsación
larga).

---

## 24. Control remoto de FT8

Con esto el servidor remoto cubre **todas** las apps: FT8 era la única que faltaba porque su
diálogo en el X6100 es un fork con enganches propios. La adaptación fue más simple de lo que
parecía: nuestro diálogo FT8 ya tenía todo lo que la API necesita (`cq_enabled`, `tx_cq_en_dis_cb`,
`tx_call_off`, `tx_msg`, `mode_auto_cb`, `hold_tx_freq_cb`, `filter_low/high`).

### Comandos

| Comando | Para qué |
|---|---|
| `ft8_cq` | activa/desactiva CQ |
| `ft8_stop` | corta la transmisión y limpia el mensaje |
| `ft8_auto` | modo automático |
| `ft8_hold` | retener la frecuencia de TX |
| `ft8_txfreq` | frecuencia de audio de TX (200–2900 Hz) |
| `ft8_reply <id>` | **contesta un mensaje decodificado** |
| `app_base` | carga la banda de la app |

Además publica su estado en JSON (`cq`, `txen`, `txing`, `auto`, `hold`, `txfreq`, `base`) para la
página `/remote`.

### Cómo se resuelve `ft8_reply`

Para contestar un mensaje decodificado hay que identificarlo, así que se guardan los **últimos 64**
decodificados con un id y su metadata (igual que el X6100). Se registran en el camino de RX
(`add_rx_text`), con mutex porque el decodificador corre en otro hilo.

De paso unifiqué el arranque de QSO en `start_qso()`: lo usaban el toque en la fila y ahora también
el control remoto, en vez de tener dos copias de la misma lógica.

---

## 25. Ajustes de Bluetooth (encendido, discoverable y cargador)

La app de mensajes de voz y los scripts de Bluetooth ya estaban; lo que faltaba en la GUI eran las
filas que el X6100 sí tiene en Ajustes. Portadas:

| Fila | Qué hace |
|---|---|
| **Bluetooth** | prende/apaga el adaptador: corre `btcat-ctl bt on\|off` en un hilo aparte (no bloquea la UI) |
| **Discoverable (CAT)** | hace visible la radio para el puente de CAT (`btcat-ctl discoverable on\|off`); queda **deshabilitada** cuando el Bluetooth está apagado |
| **Charger** | Off / On / Shadow. En On carga mientras está encendida; **Shadow** carga al apagarla (ya lo manejaba `radio_poweroff()`); se aplica al instante con `radio_set_charger()` |

Los dos interruptores **leen su estado real** de `/etc/default/btcat` y `/mnt/btcat.conf` (mismo
esquema que el X6100), así que reflejan lo que hay, no un valor inventado.

### Lo que sigue pendiente en Bluetooth

Su Ajustes tiene además la **lista de dispositivos** con scan, pairing, el globito de
*connected* y el **passkey** (el código que hay que tipear en un teclado para vincularlo), todo
sobre `btaudio-ctl`. Son ~200 líneas más un mini-framework de filas (`wl_row_t`, `wl_pill`,
`run_async`) que también usa su página de WiFi. **Es el único pedazo de la UI de Bluetooth que
falta**; mientras tanto, `btaudio-ctl` hace lo mismo desde la consola (`scan`, `pair MAC`,
`connect MAC`, `rx on`).

---

## 26. App de dispositivos Bluetooth

Con esto Bluetooth queda completo **desde la radio**: se listan los dispositivos, se escanea, se
vincula y se maneja el audio de los auriculares sin consola.

### Cómo funciona

- **Lista**: `btaudio-ctl list` trae los dispositivos vinculados con su rol (auriculares, teclado,
  CAT), si están conectados y si reconectan solos. La tabla muestra **nombre, rol y estado**; si el
  dispositivo todavía no tiene nombre, muestra la MAC (es normal: `bluetoothctl` sólo le pone alias
  cuando lo conoce).
- **Tocar una fila** hace lo que corresponde: si no está vinculado, **vincula**; si está vinculado
  pero desconectado, **conecta**; si está conectado, **desconecta**.
- **Scan** (15 s) busca auriculares y teclados nuevos y los agrega a la lista como `new`.
- **RX to HP** y **Speaker** prenden/apagan el audio a los auriculares y el silencio del parlante,
  con el estado real leído del script.
- **Passkey**: mientras se vincula un teclado, se lee `/tmp/bt_passkey` y la app dice **qué código
  hay que tipear en el teclado**.
- Se abre desde **Ajustes → acciones de pulsación larga → `APP Bluetooth`**.

El script **no se llama desde el hilo de la UI** (bluetoothd puede tardar): se lanza aparte y deja
su salida en `/tmp/bt_ui_*.txt`; un timer lee esos archivos, y sólo redibuja cuando el contenido
cambió. Los comandos que no imprimen nada se encadenan con un `list` para que el archivo quede
siempre con la lista fresca.

### Tests

El parseo vive en `src/bt_devices.c`, sin LVGL, y se prueba en el host
(`tests/native/test_bt_devices.c`): la lista real del script, los dispositivos **sin nombre** (que
son la mayoría al principio), los recién descubiertos por `scan`, la línea de estado de los
auriculares (HP/RX/MUTE), un **mensaje de error** del script (que no debe parecer un dispositivo) y
el caso de más dispositivos que los que entran en la lista.

```
437 comprobaciones, 0 fallos
```

---

## 27. Columna de spots del cluster (con sintonía al toque)

Los spots del DX Cluster ahora se pueden ver **sobre el waterfall**, en la pantalla principal, sin
abrir la app: una columna en el borde derecho con frecuencia, indicativo, modo y antigüedad, con
las alertas coloreadas y el spot seleccionado resaltado.

### Cómo se usa

- Se prende en **Ajustes → `Cluster column`** (por defecto **apagada**, así que no cambia nada si
  no la querés).
- Muestra los spots de la **banda actual** (o de todas, según el filtro del cluster) y respeta el
  orden elegido en la app (por frecuencia o por hora) y las alertas.
- **Tocar una fila sintoniza ese spot** (el *spot snap* del X6100). Mientras una app está abierta
  la columna se esconde sola, para no taparle el waterfall.

### Diferencias con el X6100 (documentadas)

En su firmware la columna se activa desde el ítem **CLUSTER del menú MFK**, se recorre con la
perilla y el *snap* es una pulsación larga del MFK. Esta GUI **no tiene ese registro de controles**
(el `controls.cpp`/`mfk.cpp` de su fork), así que la adapté a la pantalla táctil: se prende desde
Ajustes y **se toca la fila** para sintonizar. No hay recorrido con perilla: la lista se centra
sola en el spot más cercano a la frecuencia actual.

### Implementación

- `cluster_column.c/h` del X6100 (382 líneas), con el enganche en la pantalla principal
  (`cluster_column_init/layout` después del waterfall y del band plan) y el `hide()` mientras hay
  una app abierta.
- A la selección por toque la implementé yo: cada fila visible guarda **a qué spot corresponde**
  (`row_spot[k]`), así el toque no depende de cuentas de índices sobre las filas dibujadas —que en
  el orden por frecuencia incluyen la fila del marcador del VFO—. Al tocar, se selecciona y se
  sintoniza con `dialog_cluster_tune_spot()`.
- El ajuste nuevo (`cfg.cluster_column`) va en `cfg`, no en `params`, así que su interruptor en
  Ajustes es propio y no un `switch_bool()`.
- **Consumo**: el timer de la columna (250 ms) **se pausa** cuando la columna está apagada u
  oculta, y se reanuda cuando hace falta. Antes latía cuatro veces por segundo para siempre,
  incluso con la función desactivada — justo lo que no conviene en un equipo a batería. Al
  activarla también queda **un spot seleccionado** desde el principio, que es lo que el *snap*
  necesita como blanco.

---

## 28. La pantalla principal sigue el tema

Hasta acá los temas solo cambiaban los diálogos de las apps portadas: la pantalla principal (barra,
botones, espectro, waterfall, S-meter) usaba colores fijos. Ahora **también la sigue**.

### Qué se cambió

Veinte colores fijos de la UI principal, ahora roles del tema:

| Dónde | Antes | Ahora |
|---|---|---|
| Fondo de la pantalla (modos Simple y Legacy) | `0x27313a` / `0x0040A0` | `TC(bg)` |
| Fondo y borde del espectro y del waterfall | negro / `0xAAAAAA` | `TC(bg)` / `TC(panel_border)` |
| LED del botón activo y botón deshabilitado | `0x00FF00` / `0x101010` | `TC(btn_mark_active)` / `TC(text_mute)` |
| **S-meter** (arco, aguja, marcas, barra, rojo) | 4 grises/rojos fijos | `TC(meter_bar)`, `TC(meter_over)`, `TC(meter_low)`, `TC(meter_weak)` |
| Traza del espectro, picos y TX | `0xAAAAAA` / `0x555555` / `0xFF0000` | `TC(text_dim)`, `TC(text_mute)`, `TC(tx)` |
| Línea del medio del waterfall | `0xAAAAAA` | `TC(panel_border)` |
| Colores de velocidad de sintonía (fina/rápida/scan) | azul/amarillo/rojo fijos | `TC(info)`, `TC(warn)`, `TC(err)` |

Y si el tema trae **su propia paleta de waterfall** (los JSON la pueden definir), la usa; si no,
quedan las dos de siempre (Simple/Legacy).

### Aplicar el tema

Los estilos se arman **una vez al arrancar**, así que al elegir un tema la GUI **se reinicia sola**
(el aviso dice "restarting" y el `x6200_daemon` la vuelve a levantar). Es exactamente lo que hace
el X6100: "the GUI saves and restarts at once". Para eso expuse `params_flush()`, que guarda los
parámetros cambiados en el momento en vez de esperar al guardado periódico.

---

## 29. App Status: lo que no se puede verificar sin la radio, en pantalla

Los chequeos que quedaron pendientes para el equipo (transporte del Bluetooth, nivel de TX,
espacio en `/mnt`…) necesitaban comandos por consola. Ahora hay una app que los muestra:

| Fila | De dónde sale | Para qué chequeo sirve |
|---|---|---|
| **GUI** | `VERSION` del firmware | confirmar qué ROM está corriendo |
| **Kernel** | `/proc/sys/kernel/osrelease` | — |
| **Bluetooth** | ¿existe `/sys/class/bluetooth/hci0`? + qué módulo `hci` está cargado (`btusb` = USB, `hci_uart` = UART) | **el transporte del adaptador**, que era el dato que faltaba |
| **Pairings** | `/mnt/backups/bluetooth.tgz` con su fecha | si el respaldo de vinculaciones está hecho |
| **TX level** | `tx_player_base_gain_offset()` | el **nivel de excitación** que dejé conservador y hay que validar |
| **/mnt** | `statvfs("/mnt")` | el **espacio libre** de la partición DATA |
| **wfview** | `enabled=` de `/mnt/wfview.conf` | si el servidor está habilitado |
| **Web** | IP de `wifi_get_ipaddr()` + si hay certificado en `/mnt/https` | la dirección para entrar, y si HTTPS está listo |
| **Update** | `enabled=` de `/mnt/update_check.conf` | si el aviso de versión está activo |

Se abre desde **Ajustes → acciones de pulsación larga → `APP Status`**.

### Y un arreglo

El diálogo **About** mostraba `Version 2.0-opt`, un literal que no tenía nada que ver con el
firmware: ahora muestra la versión real (`main.h`), la misma que compara el aviso de actualización
y la que aparece en el release.

---

## 30. Pasada de calidad: lo que encontró el analizador estático

Al sumar los módulos nuevos al análisis estático del arnés (`gcc -fanalyzer`), aparecieron cosas
reales que conviene registrar:

- **Fuga de descriptores en `wfview_server`**: si el `bind()` de un puerto fallaba, los sockets de
  los puertos anteriores quedaban abiertos y la función salía. Ahora se cierran y, además, se
  chequea que `socket()` no devuelva error. (El analizador sigue marcando los tres sockets que el
  servidor **mantiene abiertos a propósito** mientras vive el proceso, así que ese archivo quedó
  fuera del análisis, con el motivo escrito en el arnés.)
- **Cuatro truncamientos posibles** en `cluster/client.c`: los nombres y hosts de los servidores se
  copiaban a campos de 32 bytes sin acotar; ahora van con `%.*s`.
- **Rutas del motor de temas**: componer `TEMAS/<nombre>/theme.json` en buffers fijos podía
  truncar. Ahora cada `snprintf` **verifica su resultado** y rechaza lo que no entra (que es lo
  correcto: un tema con un nombre imposible no se puede leer). El test de temas sigue pasando.
- **Código muerto** en la app Status (una variable sin uso) y el literal de versión viejo del
  *About*, que se fue en §29.

El análisis ahora corre limpio sobre la lista de módulos, y la suite sigue igual: **437
comprobaciones del host**, más las de códec, la de JS8 y las 19 páginas del web configurator.

---

## 31. Resolvedor de países (cty.dat) y el país al arrancar un QSO

La imagen distribuía `cty.dat` (343 KB, AD1C) desde el port de Field Ops, pero **no lo leía
nadie**: era peso muerto. Ahora se usa.

### Qué se portó

- `dxcc.c/h` del X6100 (400 líneas): parsea `cty.dat` al primer uso, arma la lista de entidades y
  de alias (prefijos y llamados exactos, los que en el archivo van con `=`), y resuelve
  `dxcc_lookup(indicativo, &país, &prefijo)`.
- En **FT8**, al arrancar un QSO el aviso ahora dice el país: `Start QSO with LU1WXY (Argentina)`.
- Se respeta `$CTY_PATH` para apuntar a otro archivo (lo usan los tests).

### Detalles que aparecieron al probarlo

- El parser **normaliza algunos nombres**: `United States` sale como `USA` y `Fed. Rep. of
  Germany` como `Germany`. Los tests esperan eso, no lo que dice el archivo.
- El **prefijo primario** de una entidad es el que se devuelve, pero para *matchear* valen los
  alias listados: un indicativo que empiece con el prefijo primario y no esté en la lista no
  resuelve. Por eso el test no usa un archivo inventado: corre contra el **mismo `cty.dat` que
  ship la imagen**.
- `ZZ` es de **Brasil** (no es un prefijo libre, como suponía mi primer caso negativo).

### Tests

`tests/native/test_dxcc.c` corre contra el `cty.dat` real (el runner lo pasa por `CTY_PATH`):
Argentina, USA, Japón, Alemania y España salen bien, y un prefijo inexistente (`QQ`) y la cadena
vacía no resuelven.

```
452 comprobaciones, 0 fallos
```

### Y ahora también la bandera

Las filas de FT8 muestran la **bandera del país** a la izquierda del mensaje:

- `flags_data.h` es el header generado del X6100 (16×12 desde flagcdn.com, dominio público,
  indexado por el prefijo primario de `cty.dat` de AD1C, códigos ISO de k0swe/dxcc-json bajo MIT).
  Está en **RGB888**; LVGL dibuja **RGB565**, así que `flags.c` convierte cada bandera la primera
  vez que se pide y guarda el descriptor (≈77 KB en total, sólo las usadas).
- La tabla es de **una sola columna** (el mensaje ocupa todo el ancho), así que la bandera se pinta
  en el **hueco del padding izquierdo**, que se agrandó a 26 px: no se superpone con el texto.
- El índice de la bandera sale del país ya resuelto (`dxcc_lookup` → `flags_index_of_prefix`), así
  que no hay una segunda tabla de países.

**Test**: la conversión de color es lógica pura y se prueba en el host
(`tests/native/test_flags.c`): los extremos (negro, blanco, rojo, verde, azul), que los bits bajos
se **truncan** y no se redondean, un verde real de las banderas, y la tabla de prefijos (que `LU`
existe, que un prefijo inexistente no se inventa y que el mismo prefijo da siempre la misma
bandera).

```
469 comprobaciones, 0 fallos
```

---

## 32. DRM/KMS: disponible y **opt-in**

El motor de video del X6100 es DRM/KMS; el nuestro usaba `fbdev`. Investigando si la plataforma lo
permite, la respuesta es **sí**:

- el kernel ya trae `CONFIG_DRM=y`, `CONFIG_DRM_SUN4I=y` y el driver del panel
  (`CONFIG_DRM_PANEL_JINGLITAI_JLT4013A=y`) — comprobado en el `.config` del kernel construido;
- `lv_drivers` trae el backend (`display/drm.c`, 658 líneas) y nuestro grabador **ya esperaba**
  este caso: su `screen_rec_out` es un puntero con `fbdev_flush` por defecto, y con DRM pasa a
  `drm_flush` (su diseño, para que la grabación siga capturando la pantalla);
- hizo falta `libdrm` (agregado al defconfig ✓) y sus flags de compilación para el subproyecto
  `lv_drivers` (el header `xf86drm.h` incluye `drm.h`, que vive en `usr/include/libdrm`).

### Por qué queda apagado por defecto

Cambiar el motor de video es lo único de todo el port que **no se puede verificar sin la radio**, y
si el camino DRM inicializara "bien" pero dibujara mal, la pantalla quedaría negra. Así que:

- **Por defecto sigue `fbdev`**, exactamente como antes.
- Para probarlo: crear `/mnt/use_drm` (o arrancar con `X6200_DRM=1`). El log dice
  `Display: DRM/KMS` o `Display: fbdev`.
- Para volver: **borrar `/mnt/use_drm`** y reiniciar (no hace falta regrabar nada).
- Si `drm_init()` falla (por ejemplo no hay `/dev/dri/card0`), cae solo a `fbdev`.

Cuando se valide en el equipo, alcanza con dejarlo por defecto; hasta entonces, el camino probado
es el de siempre.

---

## 33. Perilla principal con hilo propio (latencia de sintonía)

Era el último pendiente de "base" y resultó **acotado**: el `rotary.c` del X6100 (193 líneas) es
autocontenido — no arrastra su registro de controles, que era lo que me frenaba.

### Qué arregla

Su comentario lo dice: *"so the speed (ms per detent) does not depend on how busy the GUI loop
is"* (arreglo `upstream ae2af22`). Con la perilla leída desde el bucle de la GUI, el tiempo entre
detents incluye lo que el bucle estaba haciendo: si la GUI está ocupada, la **velocidad medida es
falsa** y con ella el *adaptive tuning*. Ahora:

- un **hilo** lee `/dev/input/event1` con `poll`, marca cada detent con su tiempo y lo encola;
- la GUI toma todo lo encolado como **un evento por cuadro** (los detents sumados, el tiempo por
  detent promediado ✓), y un **cambio de dirección corta el lote**;
- si la cola se llena, se descartan detents (la GUI está trabada igual).

### Cómo convive con nuestro *adaptive tuning*

El suyo manda el tiempo en el evento (`rotary_data_t {diff, dt}`); el nuestro mantiene un
historial de eventos, uno por detent. Para no cambiarle la semántica, el lote **se desempaca**: se
agrega una entrada por detent, fechada hacia atrás con el tiempo por detent que midió el hilo, así
la velocidad que calcula es la real. El evento que se manda a la GUI sigue siendo el nuestro
(`EVENT_ROTARY_ENHANCED`, con el multiplicador), así que el resto del camino no cambia.

Los otros dos encoders (volumen y MFK interno, `event2`/`event4`) siguen con el camino clásico.

**Falta probarlo en la radio**: la sintonía es la interacción más usada y es lo único que valida
este cambio.

---

## 34. Por qué la imagen no arrancaba (y el arreglo)

La imagen quedaba en el **logo de arranque** en el equipo. La causa salió de la traza que
quedó en la tarjeta instrumentada:

1. la GUI **no se colgaba en el arranque del sistema**: los 23 scripts corren en orden y
   terminan (~10 s ✓, verificado también en un chroot con QEMU ✓), el kernel monta la raíz
   y la partición DATA se recrea **sin formatear** ✓ (los datos del usuario quedan ✓);
2. la GUI arrancaba **una sola vez** ✓ (los procesos que aparecían eran los `amixer` hijos
   del mixer ✓), registraba `main: Display: fbdev` ✓ —o sea, el display estaba bien— y
   **nunca** llegaba a abrir `/dev/input/event*` ni `/mnt/params.db` ✗;
3. se quedaba en `audio_init()`, en esta espera:

```c
pa_context_connect(ctx, NULL, 0, NULL);
while (PA_CONTEXT_READY != pa_context_get_state(ctx))  {
    pa_threaded_mainloop_wait(mloop);      /* sin límite */
}
```

   Si el contexto de PulseAudio pasa a **FAILED**, esa espera **no termina nunca** ✗.
4. ¿Y por qué fallaba? El log del arranque lo dice:

```
pulseaudio[284]: [pulseaudio] main.c: Failed to find user 'pulse'.
```

   `S50pulseaudio` lo arranca en **modo sistema** (`--system`), que necesita el usuario
   `pulse` — y la imagen **no lo tenía** ✗. Sin daemon, el cliente queda FAILED ✗ y la GUI
   se quedaba esperando **antes de dibujar** ✗. Y sin PulseAudio tampoco había **audio**:
   WSPR, SSTV, la grabación y el CQ de voz dependen de él ✗.

### Los dos arreglos

- **La GUI nunca espera para siempre**: si el contexto no es bueno, avisa y sigue **sin
  audio** ✓. Además, las funciones que usan los objetos de PulseAudio (`audio_play`,
  `audio_play_wait`, `audio_play_en`, `record_monitor_setup`) chequean el estado primero ✓.
- **El usuario `pulse` se crea** en `post-build.sh` del árbol de Buildroot ✓ (aditivo y
  versionado ✓): así el daemon arranca de verdad y hay audio ✓.
- De paso, `mixer_setup()` (diez `amixer` + ALSA) corre en su propio hilo ✓ y los elementos
  del mixer se chequean contra NULL ✓, porque ALSA **aborta el proceso** si el elemento no
  existe ✓.

### Cómo se verificó sin la radio

Con la reproducción en chroot (raíz de la imagen + **framebuffer falso** por `LD_PRELOAD` +
el `params.db` del usuario): antes se colgaba en esa espera ✗; ahora registra
`pulseaudio no disponible (Connection refused): la GUI sigue, pero sin audio` ✓ y sigue con
el resto del arranque ✓.

---

## 35. El crash que dejaba la radio en el logo (y cómo se encontró)

El hallazgo más importante de este port: **la primera imagen no arrancaba**. Quedaba en el logo
del equipo y el daemon reiniciaba la GUI en bucle (38 intentos, uno cada dos segundos ✓, todos
con el mismo log ✓). Lo que costó encontrarlo y lo que quedó como red de seguridad:

### La causa

`init_params_cfg()` recorre **toda** la struct `cfg` como si fuera un arreglo de `cfg_item_t`:

```c
cfg_item_t *cfg_arr  = (cfg_item_t *)&cfg;
uint32_t    cfg_size = sizeof(cfg) / sizeof(*cfg_arr);
init_items(cfg_arr, cfg_size, ...);      /* subject_add_observer(cfg_arr[i].val, ...) */
```

Es un truco que exige que **todos** los campos estén inicializados. La struct tiene 69 y yo
inicializaba 67: faltaban **`cwt_level` y `cwt_group`** (los parámetros del CW Trainer que agregué).
En el slot 68 `val` es NULL, `subject_add_observer(NULL, ...)` y **el proceso muere antes de
dibujar**: de ahí el logo eterno. Determinista e independiente de la base de datos del usuario
(comprobado con la de fábrica también).

### Cómo se encontró (sin la radio)

1. la traza de la tarjeta instrumentada mostró que la GUI arrancaba **una sola vez** y moría
   después de `/mnt/params.db`, sin llegar a abrir los `/dev/input/event*`;
2. con la **reproducción en chroot** (raíz de la imagen + framebuffer falso + el `params.db` del
   usuario) el crash se reproducía igual;
3. **marcas de progreso** temporales acotaron el crash a `init_params_cfg`, y de ahí salió la
   comparación entre los campos de la struct y los `fill_cfg_item()`;
4. para ver más allá hizo falta una **biblioteca de trazas** por `LD_PRELOAD` (log sin buffer +
   cada `open` de `/dev` y `/mnt` + cada hilo) y un **capturador de señales** que deja el PC del
   fallo para traducirlo con `addr2line`.

### Los arreglos

- **`cwt_level` y `cwt_group`** se inicializan ✓ (`cwt_group` desde 1, su rango ✓).
- El recorrido **saltea** los ítems sin inicializar ✓: un campo nuevo sin su `fill_cfg_item()` ya
  no puede matar la GUI ✓.
- `tests/check_cfg_fills.py` lo comprueba **en la suite** ✓: compara, en orden, los campos de
  `cfg_t` con las inicializaciones ✓ y falla si no coinciden ✓ (probado contra el código roto:
  señala exactamente los dos que faltaban ✓).

### Los arreglos del mismo tipo que aparecieron en el camino

| Qué | Síntoma |
|---|---|
| `pa_context_connect` sin límite de espera | la GUI esperaba para siempre un contexto FAILED (pulseaudio no arrancaba porque **faltaba el usuario `pulse`** ✗) |
| Usuario `pulse` ausente en la imagen | sin daemon no hay audio: WSPR, SSTV, grabación y CQ de voz ✗ |
| Mixer de ALSA sin chequear retornos ni NULL | aborto del proceso si el elemento no existe ✗ |
| Esperas de audio con `usleep(1000)` sin tope | mil despertares por segundo y cuelgue si el stream se cae ✗ |
| Encoders NULL (`vol`, `mfk`, `mfk_inner`) | desreferenciados en el arranque, en el EQ y en varios diálogos ✗ |
| Encoders por número de `eventN` fijo | si un USB se enumera antes, las perillas quedan muertas ✗ → ahora se buscan **por nombre** (`rotary0`..`rotary3` del device tree ✓) |

---

## 36. El control remoto no hacía nada, y cómo se encontró

Otro hallazgo salido de perseguir un crash de la reproducción, y probablemente el más silencioso
de todos: **el control remoto no cambiaba nada**. No fallaba con un error: simplemente no hacía
efecto.

### La causa

```cpp
SettingsManager cfg_sm;            // global: se construye antes de main()
...
Param subject_param(Subject *s) {
    return Param(subject_reader(s), subject_writer(s));   // captura el puntero
}
```

`cfg_sm` es un **global**, así que su constructor corre **antes** de que la carga de la
configuración cree los subjects `cfg_cur.*`. `subject_param()` guardaba esos punteros en el
momento de construir, cuando todavía eran NULL: todos los `cp_*` (frecuencia, modo, banda,
filtros, AGC, atenuador, preamplificador) quedaron apuntando a NULL **para toda la vida del
proceso**. Leerlos devolvía el valor neutro y **escribirlos era un no-op silencioso**, así que la
página `/remote` del web configurator y la app del teléfono no podían cambiar nada.

### El arreglo

`SettingsManager::init_live()` rehace ese mapeo **una vez que los subjects existen**, y `cfg_init`
lo llama después de `cfg_band_params_init` y `cfg_mode_params_init`. Como `cfg.c` es C y no puede
llamar a un método de C++, hay un puente `extern "C" void cfg_sm_init_live(void)`.

### Los otros dos que aparecieron detrás

- **`table_draw_part_begin_cb`** (`dialog_ft8.c`) llamaba a `dsc->draw_ctx->draw_img` sin mirarlo.
  LVGL deja ese puntero en NULL en algunos contextos de dibujo y la llamada **mataba la GUI** al
  redibujar la tabla del FT8 (se veía al cambiar de modo). Ahora se avisa una vez y se sigue: la
  bandera no se dibuja, la GUI no muere.
- **Los setters** (`subject_set_int/uint64/float`) tenían la misma desreferencia que los getters:
  ahora avisan y vuelven.

### Cómo se encontró (la técnica que sirve para lo que venga)

1. el barrido de apps por el **socket del servidor remoto** (`{"cmd":"app","text":"FT8"}`) con el
   progreso escrito a un **archivo** que sobrevive a la muerte del contenedor;
2. el capturador de crashes registrando el **PC** y el **link register**: si el fallo es un salto a
   NULL (`PC: (nil)`), `lr` es el sitio de la llamada, y como el binario es PIE se traduce con
   `addr2line` descontando la base de carga;
3. eso dio `table_draw_part_begin_cb`, y de ahí salió el resto.

---

## 37. El bug que dejaba la radio en el logo: el índice de fuente, y las fuentes `const`

Fue el fallo más caro de todo el port porque **no se veía desde el código**: la GUI moría antes
del primer frame, el daemon la relanzaba en bucle y la pantalla se quedaba con el logo.

### La causa

```c
/* Ajustes -> Display -> INTERFACE -> Font ofrece: Sony Sketch, Roboto, DejaVu, Arial (0..3) */
obj = dropdown_uint8_custom_cb(grid, &params.font, " Sony Sketch \n Roboto \n DejaVu \n Arial", ...);
```

y el valor se pasaba **crudo** a `fonts_apply()`, cuyo enum es
`FONT_SONY, FONT_ROBOTO, FONT_DEJAVU, FONT_ARIAL` (0..3) — así que elegir **"Roboto"** (`font = 1`)
aplicaba **DejaVu**. Con `font = 0` funcionaba **de casualidad** (aplicaba Roboto), y por eso el
equipo arrancaba recién salido de fábrica y moría cuando alguien tocaba la fuente.

### Y detrás había un segundo bug

`fonts_apply()` copia la fuente alternativa **dentro** de cada `sony_N`
(`*sony[i] = *alt[i];`), porque el resto del código usa `&sony_N`. Pero con LVGL 8 esas fuentes
se generan **`const`** (en `.data.rel.ro`, sólo lectura) y la copia moría ahí. Fueron **20**
archivos de fuentes; los dos últimos (`sony_20.c` y `sony_30.c`) usan
`#if LVGL_VERSION_MAJOR >= 8` en lugar de `LV_VERSION_CHECK`, y se escaparon de los dos primeros
intentos — con una verificación mía que también los salteaba. Ahora las 20 quedan en `.data`
(escribibles) y las cuatro opciones de fuente andan.

### Cómo se encontró

Con el capturador de señales mejorado: registra el **PC**, la **base de carga**, el **offset para
`addr2line`**, la **dirección que falló** y los **registros**. El fallo en `0xe9a624` con
`r5 = 0xe9a624` mostró que se escribía en `base + 0xa9a624`; `readelf` ubicó esa dirección dentro
de `.data.rel.ro`, `readelf -s` dijo qué fuentes estaban ahí (dos), y el patrón del `#if`
explicó por qué se habían escapado.

---

## 38. El registro de controles de las apps (la perilla de WeFax)

El X6100 tiene un **registro de controles** (`controls.cpp`): la app "toma" un control con
`mfk_set_ctrl(CTRL_X)` y los giros del MFK pasan a mover ESE control, no el parámetro del MFK de
la pantalla principal.

En este port faltaba, y el diálogo de WeFax lo tapaba de la peor manera: al activar Align o Tilt
llamaba a `mfk_update(±8 / ±2 / +1)`, que es el **manejador del MFK de la pantalla principal**
(nivel de espectro, decimación de FFT, velocidad CW...). O sea que girar la perilla del WeFax
**movía otra cosa** y el fax no se alineaba nunca. El campo `encoder_allowed` de los botones
estaba, pero nadie lo leía.

Portado, con el alcance que este port necesita (dos controles):

- **`controls_ids.h`**: los ids (`CTRL_WEFAX_ALIGN`, `CTRL_WEFAX_TILT`), un header mínimo que
  también usan los tests.
- **`controls_step.c`**: el paso y los topes del X6100 (`-1300..1300` y `-400..400`), sin
  dependencias, para poder probarlo en el host.
- **`controls_encoder.c`**: aplica el giro sobre la configuración (`cfg_wefax_align`/`tilt`).
- **`dialog_wefax.c`**: los cuatro sitios que llamaban a `mfk_update()` ahora llaman a
  `controls_encoder_update()`.

`tests/native/test_controls.c` cubre el paso, los topes y que un control desconocido no cambie
nada: las comprobaciones del host pasaron de 476 a 488.

---

## 39. Anuncios por voz (TTS con espeak)

El firmware base traía `voice_stub.h`: las **12 funciones de voz como no-ops** ("*power-efficient
builds without TTS*"), así que el desplegable **Voice mode** (Off / When LCD off / Always) y la
acción del mismo nombre no hacían nada. Ahora hay TTS de verdad, con **espeak** (que ya venía en
el árbol de buildroot, 23 KB de binario + los datos de idioma).

- **`voice.h` / `voice.c`**: los mismos 12 nombres que el stub, así los **109 llamados** del
  código no cambian. El texto se sintetiza en un **hilo propio con cola** (la GUI nunca espera) y
  el hilo **duerme en una condición** cuando no hay nada que decir: en reposo no cuesta nada.
- **Cómo habla**: `espeak --stdout` (por `fork` + `execv`, **sin shell**: el texto va por `stdin`)
  escribe un WAV en `/tmp` y lo reproduce `aplay`.
- **Dos guardas**: respeta el ajuste **Voice mode** (`voice_should_speak()`), y **no habla
  mientras se transmite** (`radio_get_state() == RADIO_TX`), para que no salga al aire.
- **Español**: los textos de la GUI están en inglés, así que `voice_text.c` traduce por
  **palabras** con un diccionario (alcanza con las palabras de las 119 cadenas, no con las
  frases). Las frecuencias se **deletrean dígito por dígito** ("7 punto 1 7 0, megahertz"), que
  es lo que se entiende al oído.
- **Prueba sin radio**: `X6200_VOICE_TEST="texto"` dice esa frase al arrancar. Verificado en la
  reproducción: el log muestra la frase y `aplay` intenta abrir el audio (falla sólo porque el
  chroot no tiene placa de sonido).
- **Tests**: `tests/native/test_voice.c` cubre las tres formas del ajuste, una frecuencia y el
  diccionario, sin arrastrar el motor.

---

## 40. La app de posición (GPS)

El X6100 tiene una app GPS (`dialog_gps.c`) que muestra satélites, fijación, fecha/hora,
latitud, longitud y la grilla QTH. El **X6200 no tiene receptor GPS**: no hay nodo en el device
tree y el firmware base sacó `gpsd`. Por eso la app se portó **adaptada** y dice la verdad:

- la **fuente** es el **teléfono o el configurador web**, que mandan la posición por el socket de
  control remoto (`{"cmd":"location","text":"-34.60,-58.38"}` → `gps_phone_set()`); el campo
  *Position source* lo muestra tal cual, y con "None: this radio has no GPS" cuando no hay nada;
- las filas son **Fijación** (Phone/Internal/None), **Edad** ("12 s", "2 min", "1 h 5 min"),
  **Fecha y hora** (del reloj del equipo, que lo pone el NTP: no hay RTC), **Latitud**,
  **Longitud** y **QTH Grid**;
- `gps_format.c` formatea las coordenadas como se leen en radio (`45 07.4074'N`) y la antigüedad;
  no tiene dependencias, así que `tests/native/test_gps_format.c` lo cubre en el host;
- se abre desde **Ajustes → acciones** ("APP GPS") y también por el control remoto
  (`{"cmd":"app","text":"GPS"}`), que es como se verificó: abre, dibuja y no se cae.

### 39.1 Por qué al principio no se oía nada (y cómo se verificó)

La primera versión reproducía el WAV con **`aplay`**, y en la radio eso **no puede funcionar**: la
GUI tiene tomada la placa de sonido por su propio stream. El log del equipo lo mostró apenas se
probó:

```
audio_init: Conected
audio_play: pa_stream_write() failed: No such entity
```

El arreglo fue mandar el audio por **`audio_play()`**, que es el stream de la GUI (el mismo que ya
usa el audio de FT8/JS8 y que **sí** suena en la radio), en trozos de 2048 muestras como hace
`dialog_js8.c`. espeak entrega 22050 Hz y el stream es de 44100 Hz, así que se **duplica cada
muestra** (`voice_resample()`), que para esa relación es exacto.

Además, **Ajustes no llamaba a la voz ni una vez** (y la referencia del X6100 tampoco), así que
cambiar una opción no decía nada. Ahora los cuatro callbacks comunes de `dialog_settings.c`
(`bool_update_cb`, `uint8_dropdown_update_cb`, `uint8_spinbox_update_cb` y el del tema) anuncian
el **nombre del ajuste y su valor**, y si no encuentran la etiqueta dicen sólo el valor (queda en
el log).

**Verificación hecha en el chasis** (no en la radio): con pulseaudio en modo sistema y un **sink
nulo llamado como el dispositivo que pide la GUI**
(`alsa_output.platform-sound.stereo-fallback`), la GUI diciendo una frase deja:

```
State: RUNNING                                     <- el sink despertó porque le llegó audio
sink-input: s16le 1ch 44100Hz                      <- el stream de la GUI, mono 44.1k
escrituras fallidas: 0
```

También se corrigió que al arrancar decía cuatro frases: las llamadas de voz de la pantalla
principal ya existían (mudas por el stub) y ahora suenan; `voice_ready()` las habilita recién
cuando la GUI está en pie.

---

## 41. Marcas de los spots del cluster sobre el waterfall

El X6100 dibuja los spots del cluster que pasan los filtros como una etiqueta con el indicativo
y una línea hasta su frecuencia, en dos filas escalonadas arriba del waterfall. Este port tenía
sólo la columna de la derecha.

Portado de `spot_marks.c` (277 líneas) con cuatro adaptaciones:

- colores: el tema de este port no tiene `TC_spot`/`TC_alert`, así que los spots van con
  `TC(ok)` (el cian del original: *"Cyan on the waterfall"*) y las alertas con `TC(err)` (su rosa);
- sin `cfg_cluster_spots_wf`: si el cluster no tiene spots, no se dibuja nada;
- la vista (centro y ancho) sale de un `waterfall_view()` nuevo, porque este waterfall guarda su
  centro y su ancho adentro;
- el envío al control remoto sólo ocurre si hay spots o cambiaron: no gasta batería avisando de
  nada.

**Verificado en el banco** (`captura-banco.sh` + `{"cmd":"cluster_feed",...}` para inyectar
líneas): con cuatro spots, se dibuja `RA0ABC` en cian con su línea hasta 7074.0 dentro del ancho
visible (7.035–7.083) y **no** se dibujan los que quedan fuera (`EA3DEF` en 7010.0 y los dos de
14 MHz), que es lo correcto. Sin crashes.

Ojo con el formato de las líneas del cluster: el parser exige la hora `HHMMZ` (lo que la red
siempre manda), y sin ella la línea se descarta en silencio.

### Herramientas de banco que quedaron

- **`captura-banco.sh`**: corre la GUI en la reproducción y convierte el framebuffer falso
  (480x800 a 32 bits) en un PNG. Es la forma de **ver** un cambio antes de grabarlo en la
  tarjeta.
- **`{"cmd":"cluster_feed","text":"<linea>"}`**: mete una línea del cluster sin servidor.
- **`X6200_VOICE_TEST="texto"`**: dice una frase por voz al arrancar.

---

## 42. Módulo CB 27 MHz (Banda Ciudadana): dial por canal y TX con interlock (issue #146)

La banda ciudadana de 27 MHz se usa **sin app nueva**: las cuatro bandas `CB A`–`CB D` ya
existen en la base de bandas y se llega con los `UP/DOWN` largos, como a cualquier banda. Lo
que agrega este módulo es que el **dial cuente canales de 40**, que la barra de información
diga en qué canal estás y que el TX en 27 MHz tenga **tope de potencia e interlock**.

### 42.1 Los cuatro grids y el plan de 40 canales

Cada grid es un plan CEPT/FCC/Argentina completo de 40 canales con paso nominal de 10 kHz,
cinco huecos (26,995 / 27,045 / 27,095 / 27,145 / 27,195) y los canales **23, 24 y 25 fuera de
orden** (23 = 27,255; 24 = 27,235; 25 = 27,245). La misma secuencia de offsets vale para los
cuatro grids, cada uno desde su canal 1:

| Grid | Banda | Canal 1 | Canal 40 |
|---|---|---|---|
| A | `CB A` (id 201) | 26,065 | 26,505 |
| B | `CB B` (id 202) | 26,515 | 26,955 |
| **C** | `CB C` (id 203) | **26,965** | **27,405** |
| D | `CB D` (id 204) | 27,415 | 27,855 |

El grid C es el **SBC argentino / CEPT / FCC**. No se agregaron grids E–H: E empieza en 27,865 y
se solaparía con 10 m (28,000).

La lógica vive en `src/cb.c` / `src/cb.h` (`cb_plan_of_freq`, `cb_channel_of_freq`,
`cb_freq_of_channel`, `cb_step_channel`, `cb_tx_allowed`), sin LVGL, sin subjects y sin SQLite;
`tests/native/test_cb.c` la cubre en el host.

### 42.2 Cómo se usa

- **Dial**: en CB cada detent mueve **un canal**, sin aceleración y sin wrap. En los extremos
  1 y 40 no da la vuelta.
- **Canal 23 → 24**: el número sube y la frecuencia **baja** (27,255 → 27,235). Es correcto: el
  dial cuenta canales, no Hz.
- **Fuera de canal** (un hueco o una frecuencia entrada a mano): la celda VFO muestra `CH --`.
  El primer giro **engancha** el primer canal en la dirección del giro (no el más cercano).
- **Celda VFO** de la barra: `CH 19` si la frecuencia es un canal exacto; fuera de CB sigue
  mostrando `VFO-A` / `SPL-B`. La fila grande de frecuencia no cambia.
- **Voz**: al cambiar de canal dice `Channel 19`.
- **Espectro/waterfall, CAT, memorias y entrada directa de frecuencia** siguen funcionando por
  frecuencia; el canal se deriva. Cargar una memoria en un canal exacto muestra su número.

### 42.3 Entrada y salida de CB (políticas)

Al entrar a un grid CB:

1. **Split = off** (se guarda y se restaura al salir) para que el PTT no termine en otro
   VFO/banda.
2. **Modo AM** si el modo actual no es AM/SAM/NFM; un FM elegido por el usuario se respeta.
3. **Potencia** recortada a `cb_pwr_max` (4,0 W por defecto).
4. **XIT a 0** y bloqueado mientras esté en CB (mueve el TX ±1,5 kHz y saca del canal sin que el
   operador lo vea). El **RIT** sigue disponible.

La política de modo/split se reaplica al cambiar de grid (A↔B↔C↔D). Al salir se restauran el
split y la potencia que había antes; el modo lo recarga la banda destino. En el arranque, si la
última banda guardada es CB, la entrada se aplica una vez.

### 42.4 Parámetros (sin UI en v1)

| Parámetro | Tipo | Default | Significado |
|---|---|---|---|
| `cb_tx_scope` | int 0..2 | `0` | 0 = solo SBC argentino (26,960–27,410); 1 = cualquier grid A–D; 2 = sin interlock |
| `cb_pwr_max` | float | `4.0` | Tope de potencia en CB, en watts |

Se guardan en `/mnt/params.db` como el resto y se editan por el **configurador web / SQLite**
(no hay fila en Ajustes). El interlock actúa sobre el **PTT de software** (teclado y apps
digitales): trabaja por ventana de frecuencia, no exige canal exacto. Fuera de 26–28 MHz no toca
nada, así que las bandas de aficionado quedan iguales. Si bloquea, muestra
`TX blocked: not a CB channel`, lo dice por voz y apaga el PTT.

### 42.5 Migración de una radio actualizada en el lugar

Las radios que ya tienen `/mnt/params.db` conservan la base. La migración **`_4_cb_bands`**
(`src/params/migrations.c`) corrige los cuatro rangos a los grids canónicos y recorta a la base
del grid la frecuencia guardada que quedó fuera; es idempotente y no toca ninguna otra tabla. Una
base nueva ya trae los rangos corregidos desde `sql/bands_cb.csv` (canal 1 de `CB A` = 26,065).

### 42.6 Límites legales (Argentina) — leer antes de transmitir

- El **SBC** ocupa **26,960–27,410 MHz** y exige **autorización por estación** y **equipo
  homologado/registrado** (Res. SC 47/1982; CABFRA 2025 de ENACOM; alta con formularios
  F1/F2/F23, D3/COPITEC y responsable técnico).
- El **X6200 no es un equipo CB homologado**: transmitir en 27 MHz con él queda fuera del
  régimen del SBC y fuera del servicio de aficionados. Modificar el firmware **no** cambia la
  homologación.
- Los grids **A, B y D** (26,065–26,955 y 27,415–27,855) **no** son SBC argentino: el default
  `cb_tx_scope = 0` los deja en recepción.
- Los modos y la potencia del SBC argentino **no están confirmados**; se usan AM/FM y 4 W como
  valores conservadores. El tope es configurable.
- **El PTT del micrófono** probablemente lo maneja el MCU y el interlock de software no
  necesariamente lo frena: hay que medirlo en la radio (F0). La protección real es no
  sintonizar fuera de canal.
- **No se publica el TX como validado** hasta completar F0 y medir armónicos con carga fantasma.

### 42.7 Fuera de alcance en v1

Escaneo de canales, memorias de canal 1–40, comando CAT de canal, **regla de canales** (ticks cada
10 kHz y canal resaltado) en la franja del plan de banda, listar CB en la app Bands y grids E–H.
Tampoco se toca el baseband/MCU ni el flag `fullband-tx` (no está en esta ROM).

### 42.8 Estado de la verificación

Medido en el host y en el banco QEMU: tests nativos de los cuatro grids, de los cinco huecos y
del orden 23/24/25; celda `CH 19` en los seis temas sin recorte; migración sobre una base real;
dial/`CH --`/modo AM/potencia 4,0 W y `tx=false` en el banco. **F0 (RF real en 27 MHz,
armónicos, potencia medida y PTT del micrófono) queda `requires-radio`** y lo hace el owner con
autorización: este módulo **no** declara el TX validado.

### 42.9 El canal en la fila de frecuencia y la barra CB del plan de banda (issue #169)

Mejora de presentación, **solo UI** (no cambia el dial, el TX, la potencia, el interlock, la
migración ni la tabla de canales). En CB, el operador ve de un vistazo el canal que está
seleccionando y la franja del plan de banda pintada de CB.

- **Fila de frecuencia.** En CB los tres labels pasan a: `27.185` (frecuencia del grid, formato
  `MHz.kHz`) · `CH 19` (o `CH --` fuera de canal) · `CB C` (el grid, `CB A`..`CB D`). Los tres usan
  la familia tipográfica **normal** (`sony_30`/`sony_38`), así `CH`, `CB` y `--` se ven también en
  los temas con `font_digits` (Cathode, Sprint), y el **«8» fantasma** se oculta. La fuente y el
  fantasma se cambian **solo en la transición** CB ↔ no-CB (un `static bool`), no en cada cambio de
  frecuencia. En CB no se toca el overlay `msg_tiny`; al salir, la fila vuelve a la frecuencia grande
  con los bordes del span y el fantasma. La lógica pura `cb_plan_name()` vive en `src/cb.c`/`cb.h`.
- **Franja del plan de banda.** Se agrega el tipo `BP_CB` y **cuatro segmentos** (los rangos
  completos de los grids A–D, 26,065–27,855) entre 12 m y 10 m, con color **violeta `0x5A2CA0`** y
  rótulo `CB`. La barra respeta el ajuste *Band plan*, se recorta en los bordes de un grid y no se
  dibuja en el hueco de 10 kHz entre grids. `redraw()` y `band_plan_segments()` no cambian.
- `info.c` **no** se toca: la celda VFO sigue mostrando `CH nn` (redundancia aceptada: estado +
  valor principal).

Medido: tests nativos de `band_plan_segments` (los seis casos CB, con la geometría de 800 px) y de
`cb_plan_name`; arnés de UI con las fixtures `home-cb`, `home-cb-offchannel`, `home-cb-cathode`,
`home-cb-a` y `home-cb-d` (deterministas, A/B con cambios solo en la fila y la franja); banco QEMU
con capturas en CB C, off-channel, grids A/D y el tema Cathode (con `font_digits`), todas con
`tx=false`. La etiqueta blanca del segmento CB mide **9,12:1** de contraste sobre el violeta puro
(11,5:1 sobre fondo oscuro y 5,8:1 sobre fondo claro con la franja al 80 % de opacidad): supera AA.
---

## 43. Propagación: beacons IBP/IARU R2 por banda actual (solo RX)

App nueva (issue #145): se abre desde la página **APP 5:6** o por control remoto
(`{"cmd":"app","text":"Propagation"}`), corre **a demanda** y mide los beacons del
**IBP (NCDXF/IARU)** de la **banda actual**. **Nunca transmite**; al terminar,
cancelar o cerrar devuelve el VFO (frecuencia y modo) exactamente como estaba.

### Qué se muestra

- **Paso 1/4 · Preparando**: la banda actual (nombre del registro de bandas), cuántos
  beacons IBP aplican y el plan de los próximos cinco slots.
- **Paso 2/4 · Sincronizando**: estado del reloj NTP y qué beacon está
  transmitiendo en ese instante (`propagation_slot()`).
- **Paso 3/4 · Midiendo**: beacon por beacon, con la cuenta regresiva a su slot,
  barra de progreso y la escalera **100 → 10 → 1 → 0.1 W** en vivo (✓/✗ y SNR).
  **Cancelar** está siempre en la barra de botones.
- **Paso 4/4 · Resultado**: veredicto **ABIERTA / MARGINAL / CERRADA**, cobertura
  sobre los 18 beacons de la banda, evidencia por beacon (indicativo, frecuencia,
  SNR, escalera y hora UTC del slot) y **dirección** (DXCC + grilla → rumbo y
  distancia desde el QTH de Ajustes).

La banda dice la verdad en los casos raros: **40/30/80 m no tienen beacons IBP**
(la IARU no los respalda debajo de 14 MHz) y la app lo dice, con los spots
recientes del cluster como fuente complementaria y su edad; **sin NTP** avisa que
el turno puede estar corrido; un **slot no escuchado** se informa como
"no se escuchó" (el silencio no se convierte en "banda cerrada").

### Registro y turno (sin red)

- **IBP**: 18 beacons, 5 bandas (14.100 / 18.110 / 21.150 / 24.930 / 28.200 MHz),
  slots de 10 s y ciclo de 15 min para las cinco bandas. El turno implementado
  (`(slot − banda) mod 18`) es el de la tabla oficial de NCDXF/IARU, que se fija
  en `tests/native/test_propagation.c` con casos conocidos (00:00 = 4U1UN en
  14.100, 00:00 = YV5B en 18.110, 00:40 = 4U1UN en 28.200).
- **IARU R2 (10 m)**: tabla empaquetada generada desde el ODS oficial
  (`https://storage.iaru-r2.org/index.php/s/mDCqpdaf8AXt3An`, sha256
  `72e9dab3...ef0628`) con `tools/gen_propagation_iaru.py`: **983 radiofaros,
  313 activos**, ordenados por frecuencia. Se descartan solo los marcadores que
  no son radiofaros (`TENTATIVE`, `K7RA(SK)`) y los indicativos de más de 10
  caracteres; la O con barra (`Ø`) de dos indicativos se normaliza a cero (el
  registro tiene campo de 10 bytes, así que entran los indicativos largos como
  `HI7/N2CQR`). No hay consultas a internet.

### Medicion

- **SNR** = S-meter (dBm) − piso de ruido. El piso se mide **a +5 kHz** de la
  frecuencia del beacon: la banda del IBP tiene un beacon en **todos** los slots,
  así que no hay un hueco libre en la propia frecuencia donde medir ruido.
- **Escalera**: el indicativo y el primer dash van a 100 W y el slot cierra con
  tres dashes de 1 s a 10, 1 y 0.1 W. La v1 usa **ventanas fijas** (0–5 s para
  100 W, 5–6 s, 7–8 s y 9–10 s para el resto; `propagation_ladder_window()`) y
  cuenta **escalones seguidos** por encima del piso de detección (3 dB). Es una
  **heurística declarada**: queda pendiente validar el formato real de los dashes
  en la radio (`requires-radio`); si no coincide, se corrige la ventana en
  `src/propagation.c` y no se toca la UI.
- **Umbrales**: presets 12/6 (fábrica), 15/8, 18/10, 21/12 y 9/4 dB, persistidos en
  `params.db` (`propagation_open_db`, `propagation_marginal_db`). El botón
  **Umbrales** los recorre y, si ya hay resultado, reinterpreta el veredicto sin
  volver a barrer.
- **Params**: además de los umbrales, `propagation_run_on_open` (por defecto
  **apagado**: abrir la app no mueve el VFO sin pedirlo) y `propagation_cluster`
  (por defecto encendido: usa los spots ya en memoria como evidencia secundaria).

### Hilo, timer y consumo

El barrido corre en un **hilo aparte** y no toca LVGL: publica el progreso bajo un
mutex y despierta a la GUI con `scheduler_put_noargs()`. No se usa `lv_async_call()`
porque en LVGL 8 crea un timer desde el hilo que llama y no es thread-safe
(docs/HALLAZGOS.md 58). El **único timer** de la app (1 s) se crea al arrancar el
barrido y se borra al terminar/cancelar/cerrar: con la app cerrada no queda timer
ni hilo propio.

El dial queda **bloqueado** mientras la app está abierta (frecuencia, banda y
modo); el volumen y los botones siguen andando. El barrido sintoniza por hardware
(`radio_set_freq`, solo RX) y al final vuelve a la frecuencia guardada; el modo lo
devuelve el snapshot del marco de diálogos (#94). **Escuchar** sintoniza el mejor
beacon escuchado hasta que el operador pare, repita o cierre; cerrar siempre
devuelve el VFO.

### Control remoto y tests

- `{"cmd":"cmd","name":"propagation","val":1|0}`: Run / Cancelar (idempotente,
  solo con la ventana abierta). El estado va en el campo `ax` de `{"cmd":"get"}`
  (`propagation_state`, `propagation_band`, `propagation_plan`, `propagation_done`,
  `propagation_heard`, `propagation_snr`, `propagation_ladder`,
  `propagation_listening`).
- `tests/native/test_propagation.c`: registro IBP (18/5, frecuencias y unicidad),
  tabla oficial, bordes de slot y ciclo, `propagation_snr()`, veredicto
  abierta/marginal/cerrada y sin datos, banda sin IBP, ventanas y escalera,
  plan, posiciones/rumbo y registro IARU R2 (983/313, indicativos de 10).
- `tests/check_actions.py` y `tests/check_apps_menu.py` cubren el enganche en
  Ajustes/menú; `tests/check_manual_apps.py` exige la sección del manual.
