# X6100 (IU1WXY) vs X6200: qué features tiene el predecesor que no tenemos

Análisis del 2026-10-11. Fuente del X6100: [`IU1WXY/X6100-IU1WXY`](https://github.com/IU1WXY/X6100-IU1WXY)
(release **v0.117** del 2026-10-11, LGPL-2.1), clonado como referencia en `./.x6100_ref`
(se puede borrar con `rm -rf .x6100_ref`; para volver a tenerlo:
`git clone --depth 1 https://github.com/IU1WXY/X6100-IU1WXY.git .x6100_ref`).

Comparación hecha contra `./x6200_gui` (nuestro árbol) + `./buildroot_new/AetherX6200Buildroot`.

---

## 1. Contexto: no es "la versión anterior del mismo firmware"

Ambos descienden del mismo linaje (**R1CBU** para la GUI, **R2RFE/gdyuldin** para el buildroot),
pero el fork del X6100 se llevó **mucho más trabajo encima**:

| Métrica | X6100 (IU1WXY v0.117) | X6200 (nuestro) |
|---|---|---|
| Líneas en `src/` (solo raíz) | ~69.900 en 193 archivos | ~22.100 en 122 archivos |
| Apps en el menú (`ACTION_APP_*`) | **21** | **10** |
| Submódulos de `src/` | 12 (`cluster`, `cw_decoder`, `field`, `ft8`, `js8`, `navtex`, `sstv`, `wefax`, `wspr`, …) | 5 (`cfg`, `fonts`, `ft8`, `params`, `qth`, `widgets`) |
| Base | requiere `x6100_base_patch` r13+ para varias funciones | baseband propio (`libaether_x6200_control`) |
| Estado | "todo beta, primer release" (según su README) | port estable de R1CBU |

Apps que tiene el X6100 y no el X6200: `BANDS`, `CLUSTER`, `CW_KBD`, `CWTRAINER`,
`FIELD`, `GPS`, `JS8`, `MODES`, `NAVTEX`, `SNAKE`, `SSTV`, `WEFAX`, `WSPR`.
(En sentido inverso, el X6200 tiene `EQ` y `ABOUT`, que el X6100 no.)

---

## 2. Features del X6100 ausentes en el X6200

### 2.1 Modos digitales y CW (lo más grande)

| Función | X6100 | X6200 | Portabilidad |
|---|---|---|---|
| **JS8** (compatible JS8Call, 4 velocidades a la vez, inbox, mensajes guardados, APRS/status) | ✅ | ❌ | Código propio (`src/js8/`), portable; requiere integrar audio/PTT |
| **WSPR** RX + TX (baliza aleatoria por %, subida a WSPRnet) | ✅ | ❌ | módulo propio, portable |
| **SSTV** RX + TX (VIS automático, modo auto sin VIS, dibujo línea a línea) | ✅ | ❌ | módulo propio (`src/sstv/`), portable |
| **WeFax** RX + TX | ✅ | ❌ | módulo propio, portable |
| **NavTex** RX (lista de estaciones, guarda imágenes) | ✅ | ❌ | módulo propio, portable |
| **CW Keyboard** (teclado USB o Bluetooth que telegrafía) | ✅ | ❌ | portable |
| **CW Trainer** | ✅ | ❌ | portable |
| Selector de apps/modos (**MODES**) | ✅ | ❌ (menú de apps clásico) | portable |
| **RTTY** | ✅ | ✅ | — |
| **FT8 / FT4** | ✅ | ✅ (se cambia dentro del diálogo FT8) | — |

Dato curioso: nuestro `main_screen.h` ya reserva `MEM_WSPR_ID 300` y `MEM_SSTV_ID 400`
(herencia del linaje), pero las apps no existen: los huecos de memoria están preparados
para modos que nunca se portaron.

### 2.2 DX Cluster

| Función | X6100 | X6200 |
|---|---|---|
| Spots sobre el waterfall + columna lateral | ✅ | ❌ |
| Filtros (modo, banda, actividad POTA/SOTA), "newest first", todas las bandas | ✅ | ❌ |
| *Spot snap* (el VFO se detiene en el spot al girar despacio) | ✅ | ❌ |
| 5 servidores en paralelo + pota.app, sin duplicados | ✅ | ❌ |
| País/bandera desde `cty.dat` (trabajo de SP2KMO) | ✅ | ❌ |
| Decodificados por **UDP** para GridTracker/JTAlert | ✅ | ✅ **desde 2026-10-11** ([FT8_UDP.md](FT8_UDP.md)) |
| Mapa de QSO en FT8/JS8 | ✅ | ❌ |

### 2.3 Field Ops (SOTA / POTA) — pensado para QRP portátil

| Función | X6100 | X6200 |
|---|---|---|
| Página que se abre sola al entrar en Field Ops | ✅ | ❌ |
| Referencias offline (parks/summits), actualizables desde la web | ✅ | ❌ |
| `CQ SOTA/POTA` en CW y **CQ de voz** en SSB | ✅ | ❌ |
| Log de activación + subida a QRZ Logbook, self-spot a pota.app | ✅ | ❌ |
| Posteo a todos los clusters conectados, cierre con doble pulsación | ✅ | ❌ |

### 2.4 Pantalla, UX y accesorios

| Función | X6100 | X6200 |
|---|---|---|
| Motor de video **DRM/KMS** (del upstream) | ✅ | ❌ (usa `fbdev`) |
| **Temas JSON** (8 temas, incluidos "outdoor"; aguja de S-meter por tema) | ✅ | solo Simple/Legacy |
| Aguja de S-meter dibujada | ✅ | ❌ |
| **Grabación de pantalla** AVI (incluye tu voz en TX) | ✅ | ❌ |
| Aviso de firmware nuevo al arrancar | ✅ | ❌ |
| **Band plan IARU** sobre el waterfall, escala/lista de frecuencias | ✅ | ❌ |
| Perilla principal con cada detent temporizado en su propio hilo (menos latencia) | ✅ | ❌ (`rotary.c` clásico) |
| CW tune indicator | ✅ | ✅ (`cw_tune_ui.c`) |

### 2.5 Conectividad

| Función | X6100 | X6200 |
|---|---|---|
| **wfview server** (la radio aparece como un IC-705 en red) | ✅ | ❌ |
| **Bluetooth CAT** (hasta 3 clientes simultáneos) | ✅ | ❌ |
| **Auriculares BT** A2DP (volumen propio, mute del parlante) | ✅ | ⚠️ parcial: `bluez-alsa` está en el rootfs, falta la UI y el loopback de PulseAudio |
| **Teclados BT** (para el CW Keyboard) | ✅ | ❌ |
| **Web configurator**: 12 páginas (Bands, Digital modes, Cluster, wfview, Memories, Remote con espectro/waterfall y audio RX, Backup, Gallery, Field, Files, Time), HTTP+**HTTPS** | ✅ | ⚠️ tiene webserver (Bands, Digital modes, Files, Time) sin HTTPS ni control remoto |
| App Android **X6100 Link** (control total + PTT) | ✅ | ❌ |
| Bluetooth CAT / pairings respaldados en `/mnt` y restaurados al boot | ✅ | ❌ |

### 2.6 Configuración de radio y sistema

| Función | X6100 | X6200 |
|---|---|---|
| **TX Cal** (IQ, band output gain, codec gain, **CESSB**), compresor, filtro TX, FM emphasis | ✅ | ❌ (depende del baseband del X6100) |
| Selector de **fuente** de pantalla, tab INFO con versiones de app y base | ✅ | parcial (About) |
| Cluster filters/view, Key long-press configurables | ✅ | parcial (long-press sí) |
| AGC configurable, VOX, charger, transverters | ✅ | ✅ |

---

## 3. Lo que el X6200 tiene y el X6100 no

Para que la comparación no sea engañosa:

- **EQ de hardware** (`dialog_eq.cpp`) — el X6100 no lo tiene (usa TX Cal en su lugar).
- **WFM / broadcast FM y Air** — incorporados en el upstream del X6200 (commit "Add Broadcast FM and Air").
- **Adaptive tuning** y **audio enhancement** — añadidos nuestros (2026-10-11).
- **About** con versión y autor.
- **GPS y TTS**: existían en el upstream del X6200 y **los quitamos a propósito** en este build
  por consumo (ver `TRD.md` §1 y §2). En el X6100 sí existen (GPS + QTH automático).

---

## 4. Por qué no es copiar y pegar

1. **Capa de radio distinta.** El X6200 habla con el equipo por `libaether_x6200_control`
   (GPIO/gpiod + serie propios); el X6100 usa su propio control + `x6100_base_patch`.
   Todo lo que toque PTT, banda, modo o niveles necesita adaptación.
2. **Baseband/firmware base distinto.** TX Cal, CESSB o el porcentaje de batería dependen
   de la revisión del *base firmware* del X6100; en el X6200 no aplican igual.
3. **Pantalla distinta.** El fork del X6100 pasó a DRM/KMS; el X6200 usa framebuffer `fbdev`
   con panel RGB666 a 32 bpp (ver `TRD.md` §3.1.2), así que el motor de video no se traslada directo.
4. **Hardware distinto.** GPS, teclado, sensores y RF no son los mismos.
5. **Licencia:** LGPL-2.1 en ambos → se puede reutilizar código con atribución y manteniendo la licencia.
6. **Madurez:** el propio README del X6100 avisa que es beta y que la app compañera es alpha.

---

## 5. Qué conviene portar, por relación valor/esfuerzo

| Prioridad | Feature | Valor | Esfuerzo | Notas |
|---|---|---|---|---|
| 1 | ~~UDP de decodificados (GridTracker/JTAlert)~~ **hecho** · queda: aviso de actualización + aguja S-meter | alto/inmediato | bajo | el UDP salió del port del X6100 (IU1WXY) con tests |
| 2 | Temas JSON + band plan IARU sobre el waterfall | alto visual | bajo-medio | no toca DSP |
| 3 | DX Cluster (spots + columna + spot snap) | muy alto | medio | necesita red y UI; el webserver ya existe |
| 4 | Grabación de pantalla (AVI) | medio | medio | `avi_writer.c` + captura |
| 5 | Bluetooth CAT / auriculares / teclado + wfview | alto | medio-alto | `bluez-alsa` ya está en el rootfs |
| 6 | Web configurator completo (HTTPS, remotas, backup, galería) | medio | medio | base Python/bottle ya presente |
| 7 | **Field Ops SOTA/POTA** | muy alto para QRP | alto | depende de log, QTH y clusters |
| 8 | Modos digitales, de a uno: WSPR → SSTV → JS8 → WeFax/NavTex | muy alto | alto | requieren audio/PTT del X6200 |
| 9 | Base: DRM/KMS + perilla con hilo propio | alto (latencia/CPU) | alto | riesgo: el kernel del X6200 debe soportar DRM |

**Antes de portar cualquier cosa conviene verificar el soporte en la base del X6200**
(DRM en el kernel, disponibilidad de audio bidireccional para TX digital, acceso a CAT/serial),
porque el fork del X6100 asume cosas que la base del X6200 puede no tener.

---

## 6. Resumen en una frase

El firmware del X6100 (IU1WXY) es hoy un **superconjunto funcional** del nuestro: modos
digitales completos (JS8/WSPR/SSTV/WeFax/NavTex + CW Keyboard), DX Cluster sobre el waterfall,
Field Ops SOTA/POTA, temas, grabación de pantalla, Bluetooth CAT/audio, wfview y un
web configurator mucho más rico. Nuestro X6200, en cambio, tiene EQ de hardware,
WFM/broadcast y las mejoras de esta revisión (adaptive tuning, audio enhancement, limpieza
de GPS/TTS/RHVoice). La mayor parte de lo que falta es **software portable**; lo que no se
traslada directo es lo que depende del baseband y de la pantalla.
