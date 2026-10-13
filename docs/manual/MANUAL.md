---
title: "Manual del operador — ROM LU2DSX para Xiegu X6200"
subtitle: "Con las funciones del X6100 de IU1WXY, explicadas para radioaficionados"
author: "lu2dsx"
lang: es
---

# Manual del operador — ROM LU2DSX para Xiegu X6200

Esta ROM nace del firmware del X6200 (GUI LVGL + Buildroot) y le incorpora las funciones del
X6100 de IU1WXY que la plataforma permite. La idea es tener **un QRP de campo completo**: los
digitales, CW, el clúster, SOTA/POTA y todo lo que hace falta cuando estás en un cerro con una
batería y un cable de 20 metros.

![La radio operando](img/radio-principal.png)

**Lo que vas a encontrar acá:** el equipo con 8 temas, plan de banda IARU, S-meter con aguja,
WFM, grabación, Bluetooth (CAT y auriculares), wfview, configurador web, control remoto desde el
teléfono, FT8/FT4, WSPR, JS8, SSTV, WeFax, NavTex, RTTY, CW Keyboard y CW Trainer, clúster DX
con spots sobre el waterfall, Field Ops (SOTA/POTA) y anuncios por voz en español.

## 1. Antes de empezar

**Lo que necesitás**

- La Xiegu X6200 y una **microSD**. Ojo: la microSD va **dentro del adaptador**; si no queda
  bien asentada, la radio arranca su firmware interno y parece que "no bootea".
- Una PC con cualquier herramienta de imágenes (para la instalación completa).
- **Cargador**, si vas a actualizar en el lugar: si se corta a mitad, el equipo no arranca.

**Regla de oro:** todo lo tuyo vive en la partición **`DATA`** (`params.db` con los ajustes,
`qso_log.db` con los contactos, `themes/`, `msg/`, `rec/` y `app_logs/`). **Esa partición nunca
se toca al actualizar.** Igual, respaldala antes de cada actualización.

## 2. Instalación

### 2.1 Instalación completa (imagen)

1. **Respaldá `DATA`** a la PC.
2. **Grabá el `.img`** en la microSD (`dd`, balenaEtcher, Raspberry Pi Imager). Trae dos
   particiones: `BOOT` (arranque) y `rootfs` (el sistema).
3. **Ponela en la radio y encendé.** En el primer arranque el equipo crea la partición `DATA`.
   Si ya existe una `DATA` con formato FAT, la monta **sin formatear**: tus datos sobreviven.
4. **Restaurá tus datos** en `DATA` y reiniciá.

### 2.2 Actualización en el lugar

1. Copiá `x6200_r1cbu_<version>.tgz` a la raíz de `DATA`.
2. **Con el cargador puesto**, encendé. El script `S02update` respalda (`backup_*.tgz`), aplica
   la actualización y borra el tgz.
3. Esperá a que termine y arranque.

## 3. La pantalla principal

![Pantalla principal](img/00-principal.png)

| Zona | Qué te dice |
|---|---|
| Arriba a la izquierda | VFO (`VFO-A`/`VFO-B`), modo (LSB/USB/CW/CWR/AM/FM/DIG), `AUTO`, `PRE` (preamplificador) y `ATU1` |
| Arriba a la derecha | Hora **UTC** |
| El arco | **S-meter** de S1 a +40 dB, con la lectura en dBm |
| La franja de colores | **Plan de banda IARU**: CW, DIG y ALL MODES, con los segmentos de la banda |
| Los números grandes | Los extremos de la banda y, en el centro, la frecuencia del VFO |
| La zona oscura | **Waterfall**; arriba, la franja con las **marcas de los spots** del clúster |
| Los botones de abajo | Volumen, Squelch, RF gain, TX power y las páginas de botones de función |

## 4. Los controles

### 4.1 El dial (rueda grande)

Cambia la frecuencia del VFO activo y **anuncia la frecuencia por voz** si el modo de voz está
activo. Con `Freq acceleration` activado, girando rápido salta más. El paso se cambia con las
acciones *Step up* / *Step down* (o desde las páginas de botones).

### 4.2 La rueda de volumen (izquierda) — 12 modos

Girala para el modo activo y apretala para cambiar de modo. Los modos son:

| Modo | Qué ajusta |
|---|---|
| **VOL** | Volumen de audio |
| **RFG** | RF gain del receptor |
| **SQL** | Squelch |
| **FILTER LOW / HIGH / BW** | Filtro de recepción: corte bajo, corte alto y ancho |
| **PWR** | Potencia de transmisión |
| **MIC / HMIC / IMIC** | Ganancia de micrófono: general, de mano y de línea |
| **MONI** | Nivel del monitor (te escuchás en TX) |
| **VOICE LANG** | Idioma de los anuncios (español / inglés) |

### 4.3 El MFK (rueda multifunción, abajo) — la perilla de todo

Girala para cambiar el valor del modo activo, apretala para cambiar de modo. Los modos y sus
parámetros:

| Grupo | Modos del MFK |
|---|---|
| **Espectro** | Min level, Max level, FFT decimation, FFT width, Spectrum beta, Peak hold, Peak speed, Spectrum fill, Spectrum peak |
| **Recepción** | DNF (center frequency, width), NB (level, width), NR (level), AGC (control, hang, knee level, slope level), ATT switcher, Antenna selector |
| **CW** | CW key speed, CW key mode, Iambic mode, CW key tone, CW key volume, CW key QSK time, CW key ratio, CW key train, CW tune, CW decoder (SNR level, noise beta, peak beta) |
| **Otros** | Comp (compresor de audio), Charger mode, Peak hold time |

### 4.4 El botón de power

- **Toque corto:** apaga **sólo la pantalla**. El equipo sigue operando; es lo que usa el ajuste
  de voz *When LCD off* para hablarte sin encender nada.
- **Pulsación larga:** apaga la radio.

### 4.5 El ATU

1. Apretá `ATU` para habilitarlo (aparece `ATU1`).
2. Mantené apretado `ATU` (o usá la acción *ATU tune*) para sintonizar; el equipo busca la mejor
   red de adaptación.
3. El resultado queda guardado por banda y modo, así que al volver no tenés que resintonizar.

## 5. Ajustes: todos los parámetros

Se entra con la acción *APP Settings*. Las filas, por página:

| Página | Parámetros | Para qué |
|---|---|---|
| **Reloj** | Day, Month, Year / Hour, Min, Sec | Hora UTC de los decodificados (también la ajusta el NTP si hay red) |
| **Pantalla** | Timeout, Brightness / Buttons brightness / Clock view / S-meter needle / Band plan | Apagado automático, brillo de pantalla y de botones, vista del reloj, aguja del S-meter y plan de banda |
| **Info** | Mag Freq, Info, ALC | Qué muestra cada zona de la barra superior |
| **Audio** | Audio enhance / Speaker mode / Voice mode / Voice rate, pitch, volume | Realce de audio, modo del parlante, y los anuncios de voz con su velocidad, tono y volumen |
| **Espectro** | Spectrum auto min, max / Waterfall auto min, max / Waterfall line, zoom / Waterfall smooth scroll / Waterfall battery mode | Ajuste automático de los niveles, líneas y zoom del waterfall, desplazamiento suave y modo de ahorro en batería |
| **Operación** | Freq acceleration / Charger / Cluster column / Bluetooth / Discoverable (CAT) | Aceleración del dial, modo del cargador, columna del clúster, Bluetooth y visibilidad para CAT |
| **Apariencia** | Theme / Font | Los **8 temas** y la familia de fuente (se aplican al reiniciar) |
| **Servicios** | wfview server / Update check | Servidor wfview y aviso de firmware nuevo |
| **Field Ops** | Referencias y log | Referencias SOTA/POTA y el log ADIF |
| **Acciones** | Pantalla completa de acciones | Asignar acciones a los botones: Screenshot, Screen rec, Mute, Voice mode, Battery info, ATT, PRE, ATU, ATU tune, APP Bands y las demás apps |

## 6. Las aplicaciones

Se abre con el botón `APP` (o asignando la acción del app a un botón). En las capturas se ve el
diálogo del app sobre la pantalla principal, tal como queda en el equipo.

### 6.1 FT8 / FT4

![FT8](img/01-ft8.png)

El modo digital más usado: ciclos de 15 segundos, decodificado y transmisión automática o
asistida. Sirve para hacer contactos con señales que no escucharías ni en sueños, con 5 W y un
cable cualquiera.

**Cómo se usa:** elegí banda y modo `DIG`; poné tu **indicativo** en Ajustes (Callsign); abrí
FT8 y elegí la frecuencia de llamada de la banda; para llamar CQ usá el botón de CQ, y para
contestar tocá una fila de la lista de decodificados. El app arma la secuencia y transmite
sola. Podés subir los decodificados por UDP a un programa de la PC (ver `docs/FT8_UDP.md`).

### 6.2 WSPR

![WSPR](img/02-wspr.png)

Baliza débil señal: transmite y escucha reportes de 2 minutos para ver hasta dónde llega tu
señal con muy poca potencia. Es la mejor forma de probar una antena nueva.

**Cómo se usa:** elegí el porcentaje de tiempo de TX (`wspr_txpct`) y la banda; el app transmite
en los slots elegidos y decodifica el resto. Subí los spots a **WSPRnet** con tu indicativo y
grilla; los reportes quedan en el log del app.

### 6.3 JS8

![JS8](img/03-js8.png)

Mensajería digital de teclado a teclado, tolerante a señales flojas, con modos de velocidad
normal, rápido y turbo. Sirve para pasar un mensaje corto o tu posición a otro colega.

**Cómo se usa:** elegí el grupo de llamada y la banda, y escribí en el teclado; el app decodifica
las estaciones que te responden y muestra el mapa de estaciones conocidas. Permite mandar tu
**posición** (la toma del teléfono o del web) y guardar el QSO en el log.

### 6.4 SSTV

![SSTV](img/04-sstv.png)

Imágenes por radio en modo analógico: mandás y recibís una foto dibujada línea por línea. Los
modos más usados son R36 y R72 (los que se usan en 20 metros).

**Cómo se usa:** abrí la app y elegí el modo (`R8BW`, `R36`, `R72`, `M1`, `M2`, `S1`, `S2`); para
recibir, sólo dejá el receptor en la frecuencia y el app va dibujando la imagen; para transmitir,
elegí la imagen y arrancá la TX (el app respeta el PTT). Las imágenes quedan guardadas en `DATA`.

### 6.5 WeFax

![WeFax](img/05-wefax.png)

Facsímil meteorológico: las cartas que transmiten las estaciones de radiofax en HF. Se reciben
en blanco y negro y se leen como un mapa del tiempo.

**Cómo se usa:** elegí la frecuencia de la estación y el modo (blanco/negro o escala de grises),
y esperá el comienzo de la carta; el app alinea la imagen. Con la **perilla del MFK** (o los
botones *Align* y *Tilt*) corregís el **alineado** y la **inclinación**: girando se ve cómo se
endereza la imagen; después guardás el PNG.

### 6.6 NavTex

![NavTex](img/06-navtex.png)

Avisos marítimos en 518 kHz: meteorología, avisos a los navegantes y búsquedas. Es la versión en
texto del radiofax y con muy poco ancho de banda.

**Cómo se usa:** sintonizá la frecuencia de la estación y abrí el app; el texto decodificado
aparece en pantalla y se guarda por estación. Es útil también en la costa para saber el estado
del mar.

### 6.7 Clúster DX

![Clúster](img/07-cluster.png)

Los spots de DX en vivo: qué estaciones están en el aire y en qué frecuencia. Es la herramienta
de caza mayor, y con la columna y las marcas sobre el waterfall se convierte en un radar.

**Cómo se usa:** configurá **servidor**, **puerto** y tu **indicativo** (el clúster lo pide para
entrar), elegí los filtros (modos, bandas, actividad y antigüedad) y conectá. Los spots aparecen
en la **columna** de la derecha: al tocar uno, el equipo **sintoniza** esa frecuencia y ajusta el
modo. Las **marcas sobre el waterfall** muestran los que caen dentro del ancho que estás viendo.

### 6.8 Posición (GPS)

![GPS](img/08-gps.png)

Muestra de dónde viene la posición y la grilla QTH, que es lo que Field Ops y JS8 necesitan para
reportar dónde estás. El X6200 **no trae receptor GPS**: la posición la manda el teléfono o el
configurador web.

**Cómo se usa:** mandá la posición desde el teléfono (`{"cmd":"location","text":"-34.60,-58.38"}`)
o desde el web. El app muestra la **fuente** (Phone / None), la **edad** del dato, la fecha y
hora, latitud, longitud y la **grilla**; si no hay nada, lo dice tal cual en vez de inventar
satélites.

### 6.9 Field Ops (SOTA/POTA)

![Field Ops](img/09-field.png)

La app para salir al campo: referencias SOTA y POTA, log de contactos y self-spot. Está pensada
para que en la cumbre no tengas que pelear con nada.

**Cómo se usa:** elegí la **referencia** (la lista viene incluida, se puede usar sin internet),
poné tu **indicativo**, y activá el **self-spot** para avisar a pota.app en qué frecuencia estás.
Cada contacto se guarda en el log ADIF (exportable y subible a **QRZ**). El **CQ de voz** graba
tu llamado una vez y lo emite por PTT cuando lo pedís.

### 6.10 RTTY

![RTTY](img/10-rtty.png)

El digital clásico de los concursos: Baudot a 45 baudios, con shift de 170 Hz. Todavía se escucha
en 40 y 20 metros y sigue siendo divertido de decodificar.

**Cómo se usa:** elegí centro, shift y velocidad (`rtty_center`, `rtty_shift`, `rtty_rate`),
escribí en el teclado y el app transmite; lo que llega se decodifica en pantalla. Es ideal para
probar el filtro angosto del receptor.

### 6.11 CW Keyboard y CW Trainer

![CW](img/11-cw.png)

Dos herramientas: el **CW Keyboard** para transmitir desde un teclado (USB o Bluetooth) sin
manipulador, y el **CW Trainer** para practicar recepción a distintas velocidades.

**Cómo se usa:** en el CW Keyboard escribís y el equipo transmite en CW con la velocidad, el tono
y el peso que tengas configurados (`CW key speed`, `CW key tone`, `CW key ratio`); también
decodifica lo que recibe. El **CW Trainer** genera grupos de caracteres para practicar, con
dificultad creciente.

### 6.12 Bandas

![Bandas](img/13-bands.png)

La vista de bandas: los límites de cada banda y los parámetros guardados por banda y modo. Sirve
para moverse rápido entre bandas sin pasar por el dial.

**Cómo se usa:** abrí la app y elegí la banda; el equipo cambia de banda manteniendo el modo y
sus ajustes. Cada banda recuerda su última frecuencia, filtro y estado del ATU.

### 6.13 Ajustes y las demás

![Ajustes](img/12-settings.png)

La app de **Ajustes** es la puerta a todo lo que vimos en la sección 5 (páginas de reloj,
pantalla, audio, espectro, apariencia, servicios y la lista de **acciones** asignables). Además
del FT8/FT4, WSPR, JS8, SSTV, WeFax, NavTex, Clúster, Field Ops, GPS, RTTY, CW y Bandas, el menú
incluye las apps de estado del sistema, Bluetooth, WiFi, ecualizador, grabador, indicativo,
QTH y el barrido de ROE.

## 7. Guías avanzadas

### 7.1 Control remoto desde el teléfono

El equipo abre un socket en `/tmp/x6200_remote.sock` con unos 65 comandos.

1. Conectá el equipo al WiFi (Ajustes → Bluetooth/WiFi) y averiguá la IP.
2. Abrí `http://<ip>/remote` en el teléfono.
3. Comandos útiles: `{"cmd":"app","text":"FT8"}` abre una app;
   `{"cmd":"band","val":1}` cambia de banda; `{"cmd":"mode","val":1}` cambia el modo;
   `{"cmd":"freq","val":14074000}` sintoniza;
   `{"cmd":"location","text":"-34.6037,-58.3816"}` manda tu posición.
4. Diagnóstico: `{"cmd":"cluster_feed","text":"DX de LU1ABC:  14074.0  LU2DSX  FT8  2313Z GF05"}`
   inyecta un spot del clúster (la línea necesita la hora `HHMMZ`).

### 7.2 Configurador web

1. Averiguá la IP y abrí `https://<ip>/` (certificado autofirmado: aceptá la excepción).
2. Tiene 15 páginas: estado, RF, audio, clúster, digitales, temas, WiFi, Bluetooth,
   actualizaciones y el control remoto.

### 7.3 Bluetooth

1. Ajustes → Bluetooth → activar. Marcá **Discoverable (CAT)** si vas a usarlo desde la PC.
2. **CAT (SPP):** vinculá la PC y elegí el puerto serie que aparece (19200 8N1 o lo que pida tu
   programa). Sirve para cualquier logger o programa CAT.
3. **Auriculares (A2DP):** poné los auriculares en vinculación, buscalos en la app de
   dispositivos y elegí la salida de audio; el RX sale por ahí.
4. **Teclado:** vinculá un teclado Bluetooth o conectá uno USB; se usa en el CW Keyboard y para
   navegar los menús.

### 7.4 El clúster, paso a paso

1. Conseguí los datos del servidor (por ejemplo un nodo DXSpider) y tu indicativo.
2. Abrí la app **Clúster** → configurá servidor, puerto e indicativo → conectá.
3. Ajustá los **filtros**: modos, bandas, actividad y antigüedad máxima.
4. Activá la **columna** del clúster en Ajustes si querés verla siempre.
5. Tocá un spot de la columna para sintonizarlo, o mirá las **marcas** sobre el waterfall.

### 7.5 wfview: la radio como IC-705 en la red

El equipo se anuncia y responde el **protocolo LAN de Icom**, así que se comporta como un
**IC-705** y se puede manejar con **wfview** desde la PC: el waterfall, el audio del receptor y
el control de frecuencia y modo, todo por la red.

**Qué habla**

| Puerto | Uso |
|---|---|
| **UDP 50001** | Control (y el descubrimiento que hace wfview) |
| **UDP 50002** | CI-V (el CAT de la radio por red) |
| **UDP 50003** | Audio (LPCM 16 bits, mono) |

**Paso 1 — Preparar la radio**

1. Conectá el equipo a la **misma red** que la PC (Ajustes → WiFi).
2. Anotá la **IP** del equipo (pantalla de estado, o la lista del router).
3. Ajustes → **wfview server** → activar. Al activarlo se escribe su configuración en
   **`/mnt/wfview.conf`**:

   ```
   enabled=1
   user=x6200
   pass=<tu clave>
   ```

4. Si querés, cambiá el **usuario** y la **clave** desde el **configurador web** (página de
   wfview); la clave se guarda **hasheada**, no en texto plano. Anotá lo que pongas: es lo que
   vas a cargar en wfview.

**Paso 2 — Instalar wfview en la PC**

Instalá wfview desde [wfview.org](https://wfview.org) o el paquete de tu distribución. Sirve
también cualquier cliente que hable el protocolo LAN de Icom.

**Paso 3 — Registrar el equipo (el paso que importa)**

1. Abrí wfview y andá a **Settings → Radios**.
2. wfview **escanea la red** y suele encontrar la radio sola: aparece listada como **IC-705**
   (es lo que emula el equipo). Tocá **Connect**.
3. Si **no aparece** (hay routers que filtran el *broadcast* del descubrimiento), agregala a
   mano con **Manual / Add**:

   | Campo | Qué poner |
   |---|---|
   | **Radio Name** | `X6200` (el que quieras) |
   | **IP Address** | la IP del equipo (paso 1) |
   | **Control Port** | `50001` |
   | **Username** | `x6200` (o el que hayas puesto en el conf) |
   | **Password** | la clave del `wfview.conf` |
   | **Model** | **IC-705** |
   | **CI-V Address** | `A4` (la del IC-705) |

4. **Guárdala**: queda **registrada** para las próximas veces, ya no hay que buscarla de nuevo.

**Paso 4 — Usar**

1. Conectá. En la ventana principal vas a ver el **espectro y el waterfall**, y podés cambiar
   frecuencia y modo (el CAT viaja por red y se comparte con el UART: lo que cambies en la PC o
   en la radio se refleja en los dos lados).
2. Elegí el dispositivo de **audio** de la PC para escuchar el receptor.
3. **Un cliente a la vez**: el servidor atiende una conexión.

**Si algo no anda**

| Síntoma | Qué revisar |
|---|---|
| No aparece en la lista | Que esté en la misma red y que el **firewall** de la PC no bloquee **UDP 50001–50003**. Si el router no deja pasar el *broadcast*, agregala a mano (paso 3.3) |
| Conecta pero no suena | El dispositivo de audio elegido en wfview |
| No hay waterfall | Que el espectro esté activado en el cliente |
| Pide clave y no la sabés | Mirá `/mnt/wfview.conf` (o volvé a ponerla desde el configurador web) |
| Se corta | Es UDP: usá WiFi con buena señal o cable, y evitá otras aplicaciones pesadas en el equipo |

### 7.6 Temas

Hay **8**: los incorporados (IU1WXY, Outdoor, Classic, Outdoor-Negative y Vision) y los del
X6100 que van como archivos (Holo, Material 3 y RS).

1. Ajustes → **Theme** → elegí uno; el equipo guarda y **se reinicia** para aplicar los colores.
2. **Tu propio tema:** copiá un `.json` a `DATA/themes/` (tomá `Holo.json` como modelo) y aparece
   en la lista. También puede ser una **carpeta** con un `theme.json` adentro, como `RS/`.

### 7.7 Anuncios por voz (TTS en español)

1. Ajustes → **Voice mode** → **Off**, **When LCD off** o **Always**. *When LCD off* es lo más
   cómodo en el campo: habla justo cuando no estás mirando la pantalla.
2. Con la pantalla apagada, girando el dial o cambiando una opción te dice qué estás cambiando.
3. **Nunca habla mientras transmitís**, para no salir al aire.
4. Podés ajustar velocidad, tono y volumen en la misma página, y el idioma con el modo
   **VOICE LANG** de la rueda de volumen.

### 7.8 Field Ops con QRZ

1. Poné tu indicativo en Ajustes → Callsign.
2. Abrí Field Ops, elegí la referencia SOTA o POTA y activá el self-spot.
3. Cada QSO se guarda en `DATA/qso_log.db`; exportá el ADIF y subilo a QRZ con tu cuenta.

### 7.9 Grabación de pantalla y capturas

- **Captura:** acción *Screenshot* → un PNG en `DATA`.
- **Grabación:** acción *Screen rec* → video MJPEG + **audio del receptor** en un AVI en `DATA`.

### 7.10 Motor de video DRM/KMS

Está portado y **apagado por defecto**. Se activa creando `/mnt/use_drm` y reiniciando. Si algo
no se ve bien, borralo y volvés al motor clásico.

### 7.11 Actualizaciones y SSH

- Aviso de firmware nuevo en la pantalla de estado (consulta GitHub; no manda datos tuyos).
- **SSH**: usuario `root`. **Cambiá la contraseña** (la de fábrica es conocida). Los logs están
  en `DATA/app_logs/x6200_log_*.txt` y se escriben al cerrar la GUI.

## 8. Solución de problemas

| Síntoma | Qué hacer |
|---|---|
| **Sólo el logo y no arranca** | Casi siempre la actualización cortada a mitad o la microSD mal asentada. Revisá el adaptador; si sigue, grabá la imagen completa. |
| **Arranca el firmware viejo** | La tarjeta no está siendo leída: revisá el adaptador y el contacto. |
| **"No medium found" al grabar** | La microSD no está dentro del adaptador. |
| **No se oyen los anuncios** | *Voice mode* en Off, o estás transmitiendo. |
| **Un tema no aparece** | Tiene que estar en `DATA/themes/` con el JSON válido. |
| **El clúster no muestra spots** | Revisá servidor, indicativo y filtros; una línea de prueba necesita la hora `HHMMZ`. |
| **Sin espacio en `DATA`** | Borrá `rec/`, capturas viejas o los `backup_*.tgz`. |
| **Ajustes raros tras actualizar** | Restaurá el respaldo de `params.db` o borralo (perdés los ajustes, no el log). |

## 9. Referencia

| Partición | Contenido |
|---|---|
| `BOOT` (32 MB, vfat, arrancable) | `zImage`, device tree y `boot.scr` |
| `rootfs` (512 MB, ext4) | El sistema y la GUI |
| `DATA` (vfat, etiqueta `DATA`) | **Tus datos**: ajustes, log, temas, mensajes, grabaciones y logs |

| Archivo | Qué es |
|---|---|
| `DATA/params.db` | Todos los ajustes |
| `DATA/qso_log.db` | El log de contactos |
| `DATA/themes/` | Tus temas |
| `DATA/msg/` | Mensajes de voz (WAV) para el CQ de voz |
| `DATA/rec/` | Grabaciones y capturas |
| `DATA/app_logs/` | Logs de la GUI |

## 9. Créditos

Esta ROM es el trabajo de mucha gente, encadenado. En orden de abajo hacia arriba:

| Quién | Qué aportó |
|---|---|
| **Xiegu** | El hardware X6200 y la documentación del CAT |
| **Belousov Oleg (R1CBU)** | El firmware alternativo original (X6200 y X6100): la arquitectura, la GUI LVGL, el CAT, los modos digitales. La base de todo |
| **Georgy Dyuldin (R2RFE)** — [gdyuldin](https://github.com/gdyuldin/x6200_gui) | El port del firmware de R1CBU al X6200 con buildroot |
| **iariap** | El fork sobre el que está hecha esta ROM: [iariap/x6200_gui](https://github.com/iariap/x6200_gui) y [iariap/AetherX6200Buildroot](https://github.com/iariap/AetherX6200Buildroot) |
| **IU1WXY** | El firmware del X6100 ([IU1WXY/X6100-IU1WXY](https://github.com/IU1WXY/X6100-IU1WXY)) del que se portaron las funciones: aviso de actualización, S-meter, plan de banda IARU, grabación de pantalla, Bluetooth (CAT, auriculares y teclados), wfview, configurador web, FT8/FT4, WSPR, JS8, SSTV, WeFax, NavTex, RTTY, CW Keyboard y Trainer, clúster DX, Field Ops, temas, GPS y los anuncios por voz |
| **Adrian Grzeca (SQ5FOX)** | Aportes al firmware base |
| **LU2DSX** | Esta ROM: el port de las funciones del X6100 al X6200, las correcciones y este manual |

Y las bibliotecas y proyectos de software libre con los que se construye:

| Proyecto | Para qué |
|---|---|
| **LVGL** | La biblioteca gráfica de toda la interfaz |
| **Buildroot** | La compilación cruzada y el armado de la imagen |
| **Dave Gamble y colaboradores (cJSON)** y **jansson** | JSON (clúster, temas, web, control remoto) |
| **Equipo de wfview** | El protocolo y el cliente para controlar el equipo desde la PC |
| **Jonathan Duddington y el equipo de espeak** | El sintetizador de voz de los anuncios |
| **liquid-dsp**, **FFTW**, **GSL** | Procesamiento digital: espectro, modems y filtros |
| **PulseAudio**, **ALSA**, **BlueZ / bluealsa**, **Speex**, **mpg123**, **LAME**, **libvorbis** | Audio y Bluetooth |
| **OpenSSL**, **libcurl**, **NTP**, **NetworkManager**, **wpa_supplicant**, **OpenSSH** | Red, hora, seguridad y SSH |
| **e2fsprogs**, **dosfstools**, **util-linux**, **parted** | Particiones y sistemas de archivos |
| Y todos los demás paquetes de software libre del buildroot | Lo que hace que el equipo ande |

**Licencias:** el firmware es **LGPL-2.1-or-later** (la GUI y los archivos portados conservan su
atribución original en sus cabeceras); cada biblioteca mantiene la suya.


> Documento generado desde `docs/NEW_FEATURES.md` y `docs/X6100_FEATURE_GAP.md`, con capturas
> reales del firmware (banco de pruebas y equipo).
