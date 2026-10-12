# Features portadas del X6100 (IU1WXY) al X6200

Registro del programa de port: qué se incorporó del firmware del X6100 de **IU1WXY**,
cómo se usa y en qué estado está. Se actualiza a medida que se suman features.

| Feature | Estado | Dónde |
|---|---|---|
| Decodificados FT8/FT4 por **UDP** (GridTracker, JTAlert) | ✅ 2026-10-11 | [FT8_UDP.md](FT8_UDP.md) |
| **Aviso de nueva versión** | ✅ 2026-10-11 | §2 de este documento |
| **Aguja de S-meter** | ✅ 2026-10-11 | §3 |
| **Band plan IARU** sobre el waterfall | ✅ 2026-10-11 | §4 |
| **Grabación de pantalla** (AVI) | ✅ 2026-10-11 | §5 |
| **Bluetooth: CAT (SPP) y auriculares A2DP** | ✅ 2026-10-11 | §6 |
| **wfview**: la radio como IC-705 en red | ✅ 2026-10-11 | §7 |
| **Web configurator ampliado** (+HTTPS) | ✅ 2026-10-11 | §8 |
| **WSPR**: motor de decodificación (encoder, baseband, decoder) | ✅ 2026-10-11 | §9 |
| Temas JSON | ⏳ pendiente | — |
| Field Ops SOTA/POTA | ⏳ pendiente | — |
| Modos digitales: WSPR (motor ✅, UI ⏳), SSTV, JS8, WeFax, NavTex | 🔶 en curso | §9 |
| DRM/KMS + perilla con hilo propio | ⏳ pendiente | — |

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

### Lo que falta de WSPR

La **UI**: `dialog_wspr.c` (tabla de spots, selección de banda, controles de TX, subida
automática) y la infraestructura compartida por todos los diálogos de apps del X6100
(`app_frame`, `freq_strip`, `tx_player`), que es la misma que necesitan SSTV, WeFax, NavTex y
JS8 — por eso conviene portarla de una vez en la próxima tanda.
