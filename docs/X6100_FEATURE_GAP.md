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
| Auriculares Bluetooth | 🔶 | §6 — loopback + A2DP; falta la UI |
| Teclados USB/Bluetooth | 🔶 | §18 — entrada ✅ y app **CW Keyboard** ✅; falta la UI de pairing |
| wfview | ✅ | §7 |
| Web configurator ampliado | ✅ | §8 — 15 páginas + HTTPS |
| Control remoto en vivo | ✅ | §19 — sin control remoto de FT8 |
| WSPR | ✅ | §9 |
| NavTex | ✅ | §10 |
| SSTV | ✅ | §11 |
| WeFax | 🔶 | §12 — falta la perilla de alineado/inclinación |
| JS8 | ✅ | §16 |
| Cluster DX | 🔶 | §14 — sin la columna en la pantalla principal |
| Field Ops SOTA/POTA | ✅ | §15 y §20 — incluye el CQ de voz |
| Temas JSON | 🔶 | §17 — apps sí; falta la pantalla principal |

---

## 1. Contexto: no es "la versión anterior del mismo firmware"

Ambos descienden del mismo linaje (**R1CBU** para la GUI, **R2RFE/gdyuldin** para el buildroot),
pero el fork del X6100 se llevó mucho más trabajo encima — y este programa de port cerró buena
parte de esa diferencia:

| Métrica | X6100 (IU1WXY v0.117) | X6200 (al empezar) | X6200 (hoy) |
|---|---|---|---|
| Líneas en `src/` (solo raíz) | ~69.900 en 193 archivos | ~22.100 en 122 archivos | **~51.700 en 180 archivos** |
| Apps (`ACTION_APP_*`) | 21 | 10 | **18** |
| Módulos de `src/` | 12 | 5 | **13** (`wspr`, `cluster`, `navtex`, `sstv`, `wefax`, `field`, `js8` + los 6 de base) |
| Base | requiere `x6100_base_patch` r13+ para varias funciones | baseband propio (`libaether_x6200_control`) | igual |
| Estado | "todo beta, primer release" (según su README) | port estable de R1CBU | + este programa de port |

Apps que tiene el X6100 y **no** el X6200: `CW_KBD` (teclado que telegrafía), `CWTRAINER`,
`GPS`, `MODES` (selector de apps), `SNAKE`. Las demás (`BANDS`, `CLUSTER`, `FIELD`, `JS8`,
`NAVTEX`, `SSTV`, `WEFAX`, `WSPR`) están portadas o cubiertas por equivalentes: `BANDS` por el
editor de bandas del diálogo de Field Ops, y los modos digitales por acciones de pulsación larga
en lugar del menú de apps.

(En sentido inverso, el X6200 tiene `EQ` de hardware y `ABOUT`, que el X6100 no.)

---

## 2. Estado por feature

### 2.1 Modos digitales y CW

| Función | X6100 | X6200 | Estado / qué falta |
|---|---|---|---|
| **JS8** (4 velocidades, inbox, mensajes guardados, APRS/status, mapa) | ✅ | ✅ | motor + app; **la suite del X6100 pasa entera** (1994 aserciones) |
| **WSPR** RX + TX (baliza aleatoria, subida a WSPRnet) | ✅ | ✅ | motor + app + 11 frecuencias en la base |
| **SSTV** RX + TX (VIS, modo auto, dibujo línea a línea) | ✅ | ✅ | 7 modos, verificado con su loopback |
| **WeFax** RX + TX | ✅ | 🔶 | códec y app ✅, con alineado/inclinación por **botones** (se guardan en `cfg.wefax_*`); falta la **perilla** (su registro `CTRL_*` no está portado) |
| **NavTex** RX (lista de estaciones, guarda mensajes) | ✅ | ✅ | SITOR-B con FEC, verificado |
| **CW Keyboard** (teclado USB/BT que telegrafía) | ✅ | ✅ | app portada ([NEW_FEATURES.md](NEW_FEATURES.md) §21), con los mensajes CW guardados y control remoto; en **CW** la velocidad que muestra es la configurada, no la medida como en el X6100 |
| **CW Trainer** | ✅ | ❌ | pendiente |
| Selector de apps/modos (**MODES**) | ✅ | ❌ | acá las apps se abren desde acciones de pulsación larga |
| **RTTY** | ✅ | ✅ | — |
| **FT8 / FT4** | ✅ | ✅ | se cambia dentro del diálogo FT8 |

### 2.2 DX Cluster

| Función | X6100 | X6200 | Estado / qué falta |
|---|---|---|---|
| Spots, filtros (modo, banda, todas las bandas), orden, alertas | ✅ | ✅ | app completa, con tests portados de su suite |
| 5 servidores en paralelo + pota.app, sin duplicados | ✅ | ✅ | `cluster/client.c` |
| País desde `cty.dat` | ✅ | ✅ | resolución de país en el parser |
| **Banderas** de país | ✅ | ❌ | faltan los assets generados por `tools/flags_gen.py` |
| **Columna lateral** de spots en la pantalla principal | ✅ | ❌ | `cluster_column.c` no está portado: los spots se ven en el diálogo |
| **Spot snap** (el VFO se detiene en el spot girando despacio) | ✅ | ❌ | vive en el manejo de perilla de su `main_screen.c` |
| Decodificados por **UDP** para GridTracker/JTAlert | ✅ | ✅ | [FT8_UDP.md](FT8_UDP.md) |
| Mapa de QSO | ✅ | 🔶 | **JS8 sí** (`js8_map.c` + mapa mundial); FT8 no (nuestro diálogo FT8 es el upstream) |

### 2.3 Field Ops (SOTA / POTA)

| Función | X6100 | X6200 | Estado / qué falta |
|---|---|---|---|
| Páginas de la app (S-meter, CQ, QSO, tiempo; Bands; Log) | ✅ | ✅ | `dialog_field.c` portado |
| Referencias offline (parks/summits), actualizables desde la web | ✅ | ✅ | `x6200-field` + `/api/field` del web configurator |
| `CQ SOTA/POTA` en **CW** | ✅ | ✅ | botón CQ + `cw_encoder` |
| **CQ de voz** | ✅ | ✅ | grabación y emisión (PTT) por la API `msg_voice_*`, portada en NEW_FEATURES §20; en **WAV** porque la libsndfile de la imagen no trae MPEG |
| Log de activación ADIF + **subida a QRZ Logbook** | ✅ | ✅ | `x6200-field qrz-upload` |
| **Self-spot a pota.app** y a los clusters DX, con confirmación | ✅ | ✅ | `cluster_client_send_spot()` + doble pulsación de confirmación |

### 2.4 Pantalla, UX y accesorios

| Función | X6100 | X6200 | Estado / qué falta |
|---|---|---|---|
| **Temas JSON** (8 temas, incluidos "outdoor") | ✅ | 🔶 | motor completo + **6 temas** y JSON propios ✅; la **pantalla principal** sigue con estilos fijos (428 referencias de color en 42 archivos) |
| Aguja de S-meter dibujada | ✅ | ✅ | Ajustes → S-meter needle (por tema, como el original) |
| **Band plan IARU** sobre el espectro | ✅ | ✅ | Ajustes → Band plan |
| **Grabación de pantalla** AVI | ✅ | ✅ | los dos graban **video MJPEG + audio de RX** (el análisis inicial decía "incluye tu voz en TX": su propio `screen_rec.c` dice "Screen + RX audio recorder", así que no era una diferencia) |
| Aviso de firmware nuevo al arrancar | ✅ | ✅ | sin telemetría |
| **Motor de video DRM/KMS** | ✅ | ❌ | el X6200 usa `fbdev`; cambiarlo es un riesgo de hardware (ver §4) |
| Perilla principal con cada detent en su propio hilo (menos latencia) | ✅ | ❌ | `rotary.c` clásico |
| CW tune indicator | ✅ | ✅ | `cw_tune_ui.c` |

### 2.5 Conectividad

| Función | X6100 | X6200 | Estado / qué falta |
|---|---|---|---|
| **wfview server** (la radio como IC-705 en red) | ✅ | ✅ | puertos 50001-50003, con tests de protocolo |
| **Bluetooth CAT** (hasta 3 clientes) | ✅ | ✅ | `btcat` (pty sobre el UART de CAT + SPP) |
| **Auriculares BT** A2DP | ✅ | 🔶 | `btaudio-ctl` + política de PulseAudio ✅; **falta la UI** (se maneja por consola) |
| **Teclados BT** | ✅ | 🔶 | descubrimiento dinámico y *rollover* ✅; **falta la UI de pairing** |
| Pairings respaldados en `/mnt` y restaurados al boot | ✅ | ✅ | `S39btpairings` |
| **Web configurator**: Bands, Digital modes, Cluster, wfview, Memories, Remote, Backup, Gallery, Field, Files, Time, Logs, CW messages, Channels + **HTTPS** | ✅ 12 | ✅ **15** | HTTP + HTTPS con certificado autofirmado; tests de las 19 páginas |
| **Control remoto en vivo** (página `/remote`) | ✅ | ✅ | socket `/tmp/x6200_remote.sock`, ~60 comandos; **sin FT8** (ver §5) |
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

1. **UI de pairing Bluetooth** (Ajustes → Wireless): hoy se vincula por consola
   (`bluetoothctl` / `btaudio-ctl`). Falta portar su sección de Bluetooth.
2. **Perilla de alineado/inclinación de WeFax**: requiere portar el registro de controles
   (`CTRL_*` + `mfk_set_ctrl`, ~500 líneas de `controls.cpp`).
3. **Temas en la pantalla principal**: llevar las 428 referencias de color al sistema de temas.
4. **Columna de spots del cluster** en la pantalla principal (`cluster_column.c`) y **spot snap**.
6. **Control remoto de FT8** por la web: faltan los enganches `dialog_ft8_remote()/state()` en
   nuestro diálogo FT8 (upstream). Las otras seis apps sí responden.
7. **CW Trainer** (el entrenador de oído): es la única app del X6100 que queda sin portar.
8. **Banderas** de país en el cluster (assets de `tools/flags_gen.py`).
9. **DRM/KMS + perilla con hilo propio**: mejora de base, riesgo de hardware.
10. **TX Cal / CESSB / filtro TX**: depende del baseband; en el X6200 no hay comandos.

El detalle de cómo se llega a cada feature, de qué depende y qué se probó está en
[INTEGRATION_AUDIT.md](INTEGRATION_AUDIT.md), que también lista la deuda de integración.

Chequeos que solo se pueden hacer **en el equipo**: transporte Bluetooth real, nivel de
excitación de TX, **nivel de micrófono y audio emitido del CQ de voz**, DRM, y espacio en `/mnt`
para grabaciones/logs y mensajes de voz.

---

## 6. Resumen en una frase

El X6100 de IU1WXY sigue teniendo cosas que acá no hay —el **CW Trainer**, la **columna de spots** con su *spot snap*, los **temas en la pantalla
principal**, **DRM/KMS** y **TX Cal/CESSB**— pero tras este programa de port el X6200 cubre
**los cinco modos digitales, el cluster, Field Ops, los temas, la grabación, Bluetooth, wfview
y el web configurator con control remoto**, que era el grueso de la diferencia. Lo que queda
pendiente es UI y lo que depende del baseband o del motor de video.
