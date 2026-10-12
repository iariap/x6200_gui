# X6200 LU2DSX ROM — tanda de features del X6100 (2026-10-11)

Segundo prerelease de esta ROM. Trae **todo el programa de port** desde el firmware del X6100 de
[IU1WXY](https://github.com/IU1WXY/X6100-IU1WXY): 18 features, cada una documentada, compilada
para el equipo, verificada en la imagen y con tests donde se puede correr sin radio.

**Versión del firmware**: `v0.1.2-lu2dsx` (base upstream v0.1.2 + este trabajo).

## Qué se incorporó

| # | Feature | Detalle |
|---|---|---|
| 1 | **Decodificados FT8/FT4 por UDP** | Compatible con GridTracker/JTAlert (WSJT-X schema 3) |
| 2 | **Aviso de nueva versión** | Consulta la última release y avisa al arrancar (sin telemetría) |
| 3 | **Aguja de S-meter** | Dial analógico en Ajustes, además de las barras |
| 4 | **Band plan IARU** | Franja de colores sobre el espectro (160 m a 6 m) |
| 5 | **Grabación de pantalla** | AVI MJPEG + audio de RX en `/mnt/screenrec` |
| 6 | **Bluetooth CAT** | Puente SPP con pty sobre el UART de CAT (`btcat`) |
| 7 | **Bluetooth auriculares** | A2DP con loopback y `btaudio-ctl` |
| 8 | **Teclados USB/Bluetooth** | Descubrimiento dinámico, *rollover*, la app **CW Keyboard**, el **CW Trainer** y la app de **dispositivos Bluetooth** (listar/escanear/vincular + passkey) |
| 9 | **wfview** | La radio como IC-705 en la red (Icom LAN) |
| 10 | **Web configurator ampliado** | 15 páginas + HTTPS, gestor de todo el equipo |
| 11 | **Control remoto en vivo** | La página `/remote` maneja la radio en tiempo real, con **todas** las apps |
| 12 | **Field Ops SOTA/POTA** | Referencias, log ADIF, feed RBN y **CQ de voz** (graba el CQ y lo emite en SSB) |
| 13 | **Cluster DX** | Spots, filtros y alertas, más la **columna en la pantalla principal** (se toca un spot y sintoniza) |
| 14 | **WSPR** | Motor y app completos (RX/TX, subida a WSPRnet) |
| 15 | **NavTex** | Decodificador SITOR-B con FEC |
| 16 | **SSTV** | 7 modos, RX y TX, con galería |
| 17 | **WeFax** | Fax meteorológico con modo Auto |
| 18 | **JS8** | Motor y app completos |
| + | **Temas JSON** | 6 temas incluidos + temas propios en `/mnt/themes` |

Todo se abre desde **Ajustes → acciones de pulsación larga**: elegí la acción y asignala a un
botón. Están WSPR, NavTex, SSTV, WeFax, JS8, Cluster, Field Ops, CW Keyboard y Theme (más las
que ya tenía el equipo). El mensaje de voz de Field Ops se graba y se emite desde la app, y todo
se configura también desde el navegador.

## Verificación

Todo se corre con un comando y queda resumido:

```bash
cd x6200_gui && tests/run_all_tests.sh      # --quick omite la suite de JS8
```

Lo que se puede probar **sin la radio**, y se probó:

- **401 comprobaciones** en el arnés nativo del host: aguja/band plan/AVI, UDP, aviso de versión,
  wfview (protocolo por UDP real), cluster, Field Ops, temas JSON, teclado, y el motor de WSPR
  (`tests/native/run_native_tests.sh`).
- **Round-trips de códecs** con las herramientas del propio X6100:
  - WSPR: 158 comprobaciones, decodifica a −20 y −25 dB de SNR.
  - SSTV: los **7 modos**, correlación 0,92–0,98.
  - WeFax: 300/300 líneas, correlación 0,995.
  - NavTex: OK a +20, +10 y +5 dB.
- **Suite completa de JS8** del X6100 contra nuestro port: **1994 aserciones en 88 casos**.
- **Web configurator**: las 19 páginas/endpoints contra la base de datos real.
- **Protocolo de wfview**: handshake, login aceptado/rechazado, puente CI-V.

Corrida completa al cierre de esta tanda:

| Suite | Resultado |
|---|---|
| Arnés nativo (host) | **401 comprobaciones, 0 fallos** |
| WSPR (round-trip) | **158 comprobaciones, 0 fallos** |
| SSTV (7 modos) | **all OK** |
| WeFax (300/300 líneas) | **PASS** |
| NavTex (SITOR-B) | **all OK** |
| JS8 (suite del X6100) | **1994 aserciones en 88 casos** |
| Web configurator (19 páginas) | **19 comprobaciones, 0 fallos** |

Cada imagen se verificó con `e2fsck` y contra el contenido esperado (binarios, assets, base de
datos, dependencias resueltas).

## Lo que falta verificar en el equipo

Estas cosas dependen del hardware y quedan como chequeos pendientes:

1. **Bluetooth**: qué transporte usa el chip (`hciconfig -a`, `dmesg | grep -i blue`), y el
   pairing real de CAT y auriculares.
2. **Audio de TX**: el nivel de excitación de los modos digitales (el X6200 no expone la revisión
   de la base como el X6100, así que quedó un valor conservador).
3. **CQ de voz** (Field Ops): el nivel de micrófono y el audio emitido en SSB.
4. **DRM/KMS**: el equipo sigue con fbdev; el camino DRM del X6100 necesita probarse en hardware.
5. **Espacio en `/mnt`**: la grabación de pantalla, los logs, las imágenes RX y los mensajes de voz lo comparten.

El detalle de cómo se llega a cada feature, de qué depende y qué se probó (y qué queda para
probar en el equipo) está en [`docs/INTEGRATION_AUDIT.md`](INTEGRATION_AUDIT.md).

## Limitaciones conocidas (documentadas en `docs/NEW_FEATURES.md`)

- **Bluetooth**: todo desde la radio (encendido, discoverable, lista de dispositivos, vincular, conectar, audio a los auriculares y el *passkey* de los teclados). La consola (`btaudio-ctl`) sigue disponible.
- **Perilla de alineado/inclinación de WeFax**: su registro `CTRL_*` no está portado; los valores
  se ajustan desde los parámetros.
- **UI principal con temas**: los diálogos de apps siguen el tema; la pantalla principal
  (barra, botones, espectro) todavía usa los estilos fijos.
- **WeFax**: falta la perilla de alineado/inclinación (su registro de controles no está portado);
  esos valores se ajustan desde los parámetros.
- El *snake* del firmware original (un huevo de pascua) no está en esta ROM.

## Instalación

El release trae **dos artefactos**:

| Archivo | Para qué |
|---|---|
| `x6200_<fecha>.img` | imagen completa para grabar en la microSD |
| `x6200_r1cbu_<versión>.tgz` | **actualización en el lugar** (no toca tus datos) |
| `SHA256SUMS` | para verificar las descargas |

### 1. Actualización en el lugar (recomendada: no toca tus datos)

El equipo ya sabe actualizarse solo: `S02update` busca `x6200_r1cbu_*.tgz` en la partición DATA
(`/mnt`), hace un backup y lo descomprime sobre la raíz. Entonces:

1. Copiá `x6200_r1cbu_v0.1.2-lu2dsx.tgz` a `/mnt` (por SSH o sacando la SD).
2. Reiniciá la radio y esperá: el firmware hace el backup y aplica la actualización.
3. La configuración, los logs, los temas de `/mnt/themes`, las grabaciones y los mensajes de voz
   **quedan como estaban**.

Ojo: este camino **no** actualiza el kernel ni u-boot (esos van en la imagen).

### 2. Imagen completa en la microSD

```bash
sudo dd if=x6200_<fecha>.img of=/dev/sdX bs=4M status=progress conv=fsync
```

⚠️ Grabar la imagen recrea las particiones: **respaldá `/mnt`** antes (ahí viven tus ajustes,
logs, temas y grabaciones).

### Verificar lo que bajaste

```bash
sha256sum -c SHA256SUMS
```

## Repositorios

- Firmware: [iariap/x6200_gui](https://github.com/iariap/x6200_gui) (rama `lu2dsx/x6200-rom`)
- Buildroot: [iariap/AetherX6200Buildroot](https://github.com/iariap/AetherX6200Buildroot) (rama `lu2dsx/x6200-rom`)
- Web configurator: [iariap/x6200_webserver](https://github.com/iariap/x6200_webserver)
- Documentación del port: [`docs/NEW_FEATURES.md`](NEW_FEATURES.md),
  [`docs/X6100_FEATURE_GAP.md`](X6100_FEATURE_GAP.md),
  [`docs/X6100_REPO_INDEX.md`](X6100_REPO_INDEX.md)

Créditos: las features portadas vienen del firmware del **X6100 de IU1WXY** (LGPL-2.1-or-later) y
de los proyectos que él usa (js8core de JS8Call-improved, wsprd de K1JT, liquid-dsp, etc.), con
su atribución en cada archivo.
