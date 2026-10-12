# Features portadas del X6100 (IU1WXY) al X6200

Registro del programa de port: qué se incorporó del firmware del X6100 de **IU1WXY**,
cómo se usa y en qué estado está. Se actualiza a medida que se suman features.

| Feature | Estado | Dónde |
|---|---|---|
| Decodificados FT8/FT4 por **UDP** (GridTracker, JTAlert) | ✅ | [FT8_UDP.md](FT8_UDP.md) |
| **Aviso de nueva versión** | ✅ | §2 |
| **Aguja de S-meter** | ✅ | §3 |
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
