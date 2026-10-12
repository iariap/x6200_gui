# X6100 (IU1WXY) vs X6200: features y estado del port

Empezó como el análisis de qué tenía el X6100 que no teníamos (2026-10-11) y hoy es el
**seguimiento del programa de port**: cada fila dice en qué estado está.

**Leyenda:** ✅ hecho · 🔶 parcial (dice qué falta) · ❌ pendiente

Fuente del X6100: [`IU1WXY/X6100-IU1WXY`](https://github.com/IU1WXY/X6100-IU1WXY) (release
**v0.117** del 2026-10-11, LGPL-2.1), clonado como referencia en `./.x6100_ref` (se puede borrar
con `rm -rf .x6100_ref`; para volver a tenerlo:
`git clone --depth 1 https://github.com/IU1WXY/X6100-IU1WXY.git .x6100_ref` y después
`git submodule update --init --recursive --depth 1`).

Comparación hecha contra `./x6200_gui` (nuestro árbol) + `./buildroot_new/AetherX6200Buildroot`.
Cada feature portada tiene su detalle, configuración y tests en
[NEW_FEATURES.md](NEW_FEATURES.md).

---

## 0. Estado del programa de port

| Feature | Estado | Detalle |
|---|---|---|
| Decodificados FT8/FT4 por UDP | ✅ | [FT8_UDP.md](FT8_UDP.md) |
| Aviso de firmware nuevo | ✅ | NEW_FEATURES §2 |
| Aguja de S-meter | ✅ | §3 |
| Band plan IARU | ✅ | §4 |
| Grabación de pantalla | ✅ | §5 — video MJPEG + audio de RX |
| Bluetooth CAT | ✅ | §6 |
| Auriculares Bluetooth | ✅ | §6 y §25/§26 — loopback + A2DP y la app de dispositivos (listar, vincular, conectar y enrutar el RX), con la política de PulseAudio |
| Teclados USB/Bluetooth | ✅ | §18 entrada, §21 CW Keyboard y §26 app de dispositivos |
| wfview | ✅ | §7 |
| Web configurator ampliado | ✅ | §8 — 15 páginas + HTTPS |
| App de posición (**GPS**) | ✅ | §40 — adaptada: el X6200 **no tiene receptor**, la posición la manda el teléfono/el web por el socket |
| Control remoto en vivo | ✅ | §19 — socket con ~65 comandos, las apps incluido FT8 (`ft8_cq`, `ft8_stop`, `ft8_auto`, `ft8_hold`) |
| WSPR | ✅ | §9 |
| NavTex | ✅ | §10 |
| SSTV | ✅ | §11 |
| WeFax | ✅ | §12 y §38 — incluida la perilla de alineado/inclinación |
| JS8 | ✅ | §16 |
| Cluster DX | ✅ | §14 y §27 — con la columna en la pantalla principal |
| Field Ops SOTA/POTA | ✅ | §15 y §20 — incluye el CQ de voz |
| Temas JSON | ✅ | §17 y §28 — apps **y** pantalla principal |

---

## 1. Contexto: no es "la versión anterior del mismo firmware"

Ambos descienden del mismo linaje (**R1CBU** para la GUI, **R2RFE/gdyuldin** para el buildroot),
pero el fork del X6100 se llevó mucho más trabajo encima — y este programa de port cerró buena
parte de esa diferencia:

| Métrica | X6100 (IU1WXY v0.117) | X6200 (al empezar) | X6200 (hoy) |
|---|---|---|---|
| Líneas en `src/` (solo raíz) | ~69.900 en 193 archivos | ~22.100 en 122 archivos | **~63.600 en 202 archivos** |
| Apps (`ACTION_APP_*`) | 21 | 10 | **21** |
| Módulos de `src/` | 12 | 5 | **13** (`wspr`, `cluster`, `navtex`, `sstv`, `wefax`, `field`, `js8` + los 6 de base) |
| Base | requiere `x6100_base_patch` r13+ para varias funciones | baseband propio (`libaether_x6200_control`) | igual |
| Estado | "todo beta, primer release" (según su README) | port estable de R1CBU | + este programa de port |

**Ya no queda ninguna app del X6100 sin portar**: `CW_KBD` y `CWTRAINER` se incorporaron
(§21 y §23 de [NEW_FEATURES.md](NEW_FEATURES.md)). Lo único que no está es el selector `MODES`
(acá las apps se abren desde las acciones de pulsación larga), `SNAKE` (un huevo de pascua) y
`GPS` (que este ROM quitó a propósito por consumo).

(En sentido inverso, el X6200 tiene `EQ` de hardware y `ABOUT`, que el X6100 no.)

---

## 2. Estado por feature

### 2.1 Modos digitales y CW

| Función | X6100 | X6200 | Estado / qué falta |
|---|---|---|---|
| **JS8** (4 velocidades, inbox, mensajes guardados, APRS/status, mapa) | ✅ | ✅ | motor + app; **la suite del X6100 pasa entera** (1994 aserciones) |
| **WSPR** RX + TX (baliza aleatoria, subida a WSPRnet) | ✅ | ✅ | motor + app + 11 frecuencias en la base |
| **SSTV** RX + TX (VIS, modo auto, dibujo línea a línea) | ✅ | ✅ | 7 modos, verificado con su loopback |
| **WeFax** RX + TX | ✅ | ✅ | códec y app ✅; el alineado/inclinación anda por **botones** y por la **perilla** (registro de controles portado, [NEW_FEATURES.md](NEW_FEATURES.md) §38) — falta el visto bueno en la radio |
| **NavTex** RX (lista de estaciones, guarda mensajes) | ✅ | ✅ | SITOR-B con FEC, verificado |
| **CW Keyboard** (teclado USB/BT que telegrafía) | ✅ | ✅ | app portada ([NEW_FEATURES.md](NEW_FEATURES.md) §21), con los mensajes CW guardados y control remoto; en **CW** la velocidad que muestra es la configurada, no la medida como en el X6100 |
| **CW Trainer** | ✅ | ✅ | entrenador de prefijos de indicativos ([NEW_FEATURES.md](NEW_FEATURES.md) §23) |
| Selector de apps/modos (**MODES**) | ✅ | ❌ | acá las apps se abren desde acciones de pulsación larga |
| **RTTY** | ✅ | ✅ | — |
| **FT8 / FT4** | ✅ | ✅ | se cambia dentro del diálogo FT8 |

### 2.2 DX Cluster

| Función | X6100 | X6200 | Estado / qué falta |
|---|---|---|---|
| Spots, filtros (modo, banda, todas las bandas), orden, alertas | ✅ | ✅ | app completa, con tests portados de su suite |
| 5 servidores en paralelo + pota.app, sin duplicados | ✅ | ✅ | `cluster/client.c` |
| País/banderas de cada spot | ❌ | ❌ | **ninguno de los dos** los muestra en el cluster: en su firmware las banderas (16x12 RGB embebidas, generadas por `tools/flags_gen.py`) están en el diálogo **FT8**, no en el cluster |
| **Columna lateral** de spots en la pantalla principal | ✅ | ✅ | portada ([NEW_FEATURES.md](NEW_FEATURES.md) §27), apagada por defecto y con **sintonía al toque** (su recorrido con la perilla necesita el registro de controles que no está) |
| **Spot snap** (sintonizar un spot) | ✅ | ✅ | **tocando la fila** en la columna (§27); su versión con la perilla necesita su registro de controles |
| Decodificados por **UDP** para GridTracker/JTAlert | ✅ | ✅ | [FT8_UDP.md](FT8_UDP.md) |
| Mapa de QSO | ✅ | 🔶 | **JS8 sí** (`js8_map.c` + mapa mundial); FT8 no (nuestro diálogo FT8 es el upstream) |
| Banderas de país en **FT8** | ✅ | ✅ | portadas: el país se resuelve con `cty.dat` y la **bandera** se dibuja en cada fila ([NEW_FEATURES.md](NEW_FEATURES.md) §31) |

### 2.3 Field Ops (SOTA / POTA)

| Función | X6100 | X6200 | Estado / qué falta |
|---|---|---|---|
| Páginas de la app (S-meter, CQ, QSO, tiempo; Bands; Log) | ✅ | ✅ | `dialog_field.c` portado |
| Referencias offline (parks/summits), actualizables desde la web | ✅ | ✅ | `x6200-field` + `/api/field` del web configurator |
| `CQ SOTA/POTA` en **CW** | ✅ | ✅ | botón CQ + `cw_encoder` |
| **CQ de voz** | ✅ | ✅ | grabación y emisión (PTT) por la API `msg_voice_*`, portada en NEW_FEATURES §20; en **WAV** porque la libsndfile de la imagen no trae MPEG |
| Anuncios de voz del equipo (**"Voice mode"**) | ✅ | ✅ | **portado con TTS** ([NEW_FEATURES.md](NEW_FEATURES.md) §39): `espeak` en español, en un hilo propio, respetando Off/When LCD off/Always y **sin hablar durante la transmisión**; el X6100 usaba su propio módulo (que no está en `.x6100_ref`), acá se reimplementó |
| Log de activación ADIF + **subida a QRZ Logbook** | ✅ | ✅ | `x6200-field qrz-upload` |
| **Self-spot a pota.app** y a los clusters DX, con confirmación | ✅ | ✅ | `cluster_client_send_spot()` + doble pulsación de confirmación |

### 2.4 Pantalla, UX y accesorios

| Función | X6100 | X6200 | Estado / qué falta |
|---|---|---|---|
| **Temas JSON** (8 temas, incluidos "outdoor") | ✅ | ✅ | motor completo + **6 incorporados** (IU1WXY, Classic, Outdoor, Outdoor-Negative, Legacy oculto y VISION) + **3 migrados del X6100** (Holo, Material 3 y RS con sus PNG), y **también la pantalla principal** ([NEW_FEATURES.md](NEW_FEATURES.md) §28, [THEMES.md](THEMES.md)); los de la ROM se buscan en `/usr/share/x6200/themes` además de `/mnt/themes` (el del usuario gana), con test nativo; al cambiar de tema la GUI se reinicia, como en el X6100 |
| Aguja de S-meter dibujada | ✅ | ✅ | Ajustes → S-meter needle (por tema, como el original) |
| **Band plan IARU** sobre el espectro | ✅ | ✅ | Ajustes → Band plan |
| **Grabación de pantalla** AVI | ✅ | ✅ | los dos graban **video MJPEG + audio de RX** (el análisis inicial decía "incluye tu voz en TX": su propio `screen_rec.c` dice "Screen + RX audio recorder", así que no era una diferencia) |
| Aviso de firmware nuevo al arrancar | ✅ | ✅ | sin telemetría |
| **Motor de video DRM/KMS** | ✅ | 🔶 | **portado y disponible** ([NEW_FEATURES.md](NEW_FEATURES.md) §32): el kernel tiene DRM y el driver del panel, y el backend está en la GUI; queda **apagado por defecto** hasta validarlo en el equipo |
| Perilla principal con cada detent en su propio hilo (menos latencia) | ✅ | ✅ | portado ([NEW_FEATURES.md](NEW_FEATURES.md) §33): el dial se lee en un hilo que marca el tiempo de cada detent; el *adaptive tuning* de este ROM se sigue alimentando desde ahí |
| CW tune indicator | ✅ | ✅ | `cw_tune_ui.c` |

### 2.5 Conectividad

| Función | X6100 | X6200 | Estado / qué falta |
|---|---|---|---|
| **wfview server** (la radio como IC-705 en red) | ✅ | ✅ | puertos 50001-50003, con tests de protocolo |
| **Bluetooth CAT** (hasta 3 clientes) | ✅ | ✅ | `btcat` (pty sobre el UART de CAT + SPP) + encendido y discoverable en Ajustes (§25) |
| **Auriculares BT** A2DP | ✅ | ✅ | app de dispositivos (listar, scan, vincular, conectar, RX a los auriculares y passkey) + política de PulseAudio ([NEW_FEATURES.md](NEW_FEATURES.md) §25 y §26) |
| **Teclados BT** | ✅ | ✅ | descubrimiento dinámico y *rollover* ✅; la app de dispositivos muestra el **passkey** para vincularlos |
| Pairings respaldados en `/mnt` y restaurados al boot | ✅ | ✅ | `S39btpairings` |
| **Web configurator**: Bands, Digital modes, Cluster, wfview, Memories, Remote, Backup, Gallery, Field, Files, Time, Logs, CW messages, Channels + **HTTPS** | ✅ 12 | ✅ **15** | HTTP + HTTPS con certificado autofirmado; tests de las 19 páginas |
| **Control remoto en vivo** (página `/remote`) | ✅ | ✅ | socket `/tmp/x6200_remote.sock`, ~65 comandos, **todas las apps** (FT8 incluido) |
| App Android **X6100 Link** | ✅ | ❌ | la app compañera no existe; el protocolo del socket sí está |

### 2.6 Configuración de radio y sistema

| Función | X6100 | X6200 | Estado / qué falta |
|---|---|---|---|
| **TX Cal** (IQ, ganancia por banda, **CESSB**), compresor, filtro TX, FM emphasis | ✅ | ❌ | depende del baseband del X6100 (`x6100_base_patch`); el X6200 no expone esos comandos |
| Selector de **fuente** de pantalla, tab INFO con versiones | ✅ | 🔶 | tenemos About con versión; no el selector de fuente |
| Cluster filters/view, key long-press configurables | ✅ | ✅ | long-press configurable con las acciones de las apps |
| AGC configurable, VOX, charger, transverters | ✅ | ✅ | — |

---

## 3. Lo que el X6200 tiene y el X6100 no

Para que la comparación no sea engañosa:

- **EQ de hardware** (`dialog_eq.cpp`) — el X6100 no lo tiene (usa TX Cal en su lugar).
- **WFM / broadcast FM y Air** — incorporados en el upstream del X6200.
- **Adaptive tuning** y **audio enhancement** — añadidos nuestros (2026-10-11).
- **About** con versión y autor.
- **GPS y TTS**: existían en el upstream del X6200 y **los quitamos a propósito** en este build
  por consumo (ver `TRD.md` §1 y §2). En el X6100 sí existen (GPS + QTH automático), aunque acá
  el QTH se puede fijar desde el teléfono o la web.
- **Restauración del estado de radio** al cerrar las apps que lo cambian (frecuencia y modo
  vuelven solos), que el X6100 ya tenía y acá se portó junto con el servidor remoto.

---

## 4. Por qué no es copiar y pegar

1. **Capa de radio distinta.** El X6200 habla con el equipo por `libaether_x6200_control`
   (GPIO/gpiod + serie propios); el X6100 usa su propio control + `x6100_base_patch`.
   Todo lo que toque PTT, banda, modo o niveles necesita adaptación — y en algunos casos no se
   puede: **no hay comandos de filtro de TX, de apagado del DSP de RX, ni de RF gain**.
2. **Baseband/firmware base distinto.** TX Cal, CESSB o el porcentaje de batería dependen de la
   revisión del *base firmware* del X6100; en el X6200 no aplican igual. El X6200 tampoco expone
   la revisión de su base, así que el nivel de excitación de los modos digitales quedó en un
   valor conservador **a validar en la radio**.
3. **Pantalla distinta.** El fork del X6100 pasó a DRM/KMS; el X6200 usa framebuffer `fbdev`
   con panel RGB666 a 32 bpp (ver `TRD.md` §3.1.2). Cambiar el motor de video es un riesgo de
   hardware sin verificación posible en el banco de pruebas.
4. **Hardware distinto.** GPS, sensores y RF no son los mismos; el transporte Bluetooth de cada
   equipo hay que confirmarlo en el equipo (`hciconfig`, `dmesg`).
5. **Licencia:** LGPL-2.1 en ambos → se reutiliza código con atribución y manteniendo la licencia.
   `js8core` es GPLv3 y `wsprd` tiene la suya; ambas están vendorizadas con su licencia.
6. **Madurez:** el propio README del X6100 avisa que es beta y que la app compañera es alpha.

---

## 5. Pendientes conocidos (y qué haría falta)

Por orden de valor:

1. **Probar en el equipo**: es la última puerta. La ROM arranca y dibuja en la reproducción
   (sin radio, sin audio, sin encoders y sin GPIO), pero falta el visto bueno en la radio: es lo
   que habilita publicar el release.
2. **WeFax**: la perilla de alineado/inclinación necesita portar el registro de controles
   (`CTRL_*` + `mfk_set_ctrl`, ~500 líneas de `controls.cpp`); los ajustes ya funcionan por botones.
3. **Crash del camino de comandos de apps** (visto en la reproducción, falta atribuirlo en el
   equipo): al abrir una app con el UART de CAT ausente salta una llamada a puntero NULL después
   de cargar los parámetros de banda. El sujeto NULL que lo acompañaba ya está cubierto. Se
   persigue con `depurar-por-red.sh` (bucle por SSH, sin regrabar) y `diagnostico-por-red.sh`.
4. **Auriculares Bluetooth**: el motor y la app están, falta la UI de selección de salida.
5. **TX Cal / CESSB / filtro TX**: depende del baseband del X6100; el X6200 no expone esos comandos.
6. **DRM/KMS**: portado y disponible, apagado por defecto hasta validarlo en el equipo.
7. **App Android X6100 Link**: la app compañera no existe; el protocolo del socket sí está.

El detalle de cómo se llega a cada feature, de qué depende y qué se probó está en
[INTEGRATION_AUDIT.md](INTEGRATION_AUDIT.md), que también lista la deuda de integración.

Chequeos que solo se pueden hacer **en el equipo**: transporte Bluetooth real, nivel de
excitación de TX, **nivel de micrófono y audio emitido del CQ de voz**, DRM, y espacio en `/mnt`
para grabaciones/logs y mensajes de voz.

---

## 6. Resumen en una frase

El X6100 de IU1WXY ya no tiene ninguna **app** que acá falte: lo que queda son **detalles de
integración** (la columna de spots con su *spot snap*, los **temas en la pantalla principal**, la
**UI de pairing Bluetooth**, la **perilla** de WeFax) y **mejoras de base** (**DRM/KMS**,
**TX Cal/CESSB**) que dependen del hardware o del baseband. En funcionalidad, el X6200 cubre
**los cinco modos digitales, el cluster, Field Ops, los temas, la grabación, Bluetooth, wfview
y el web configurator con control remoto**, que era el grueso de la diferencia. Lo que queda
pendiente es UI y lo que depende del baseband o del motor de video.
