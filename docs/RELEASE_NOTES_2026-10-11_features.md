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
| 8 | **Teclados USB/Bluetooth** | Descubrimiento dinámico y *rollover* |
| 9 | **wfview** | La radio como IC-705 en la red (Icom LAN) |
| 10 | **Web configurator ampliado** | 15 páginas + HTTPS, gestor de todo el equipo |
| 11 | **Control remoto en vivo** | La página `/remote` maneja la radio en tiempo real |
| 12 | **Field Ops SOTA/POTA** | Referencias, log ADIF y feed RBN |
| 13 | **Cluster DX** | Spots, filtros y alertas |
| 14 | **WSPR** | Motor y app completos (RX/TX, subida a WSPRnet) |
| 15 | **NavTex** | Decodificador SITOR-B con FEC |
| 16 | **SSTV** | 7 modos, RX y TX, con galería |
| 17 | **WeFax** | Fax meteorológico con modo Auto |
| 18 | **JS8** | Motor y app completos |
| + | **Temas JSON** | 6 temas incluidos + temas propios en `/mnt/themes` |

Todo se abre desde **Ajustes → acciones de pulsación larga** (WSPR, NavTex, SSTV, WeFax, JS8,
Cluster, Field Ops, Theme) y se configura desde el navegador.

## Verificación

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

Cada imagen se verificó con `e2fsck` y contra el contenido esperado (binarios, assets, base de
datos, dependencias resueltas).

## Lo que falta verificar en el equipo

Estas cosas dependen del hardware y quedan como chequeos pendientes:

1. **Bluetooth**: qué transporte usa el chip (`hciconfig -a`, `dmesg | grep -i blue`), y el
   pairing real de CAT y auriculares.
2. **Audio de TX**: el nivel de excitación de los modos digitales (el X6200 no expone la revisión
   de la base como el X6100, así que quedó un valor conservador).
3. **DRM/KMS**: el equipo sigue con fbdev; el camino DRM del X6100 necesita probarse en hardware.
4. **Espacio en `/mnt`**: la grabación de pantalla, los logs y las imágenes RX lo comparten.

## Limitaciones conocidas (documentadas en `docs/NEW_FEATURES.md`)

- Control remoto de **FT8** por la web: nuestro diálogo FT8 es el upstream y no tiene los
  enganches del suyo (las otras seis apps sí).
- **UI de pairing Bluetooth**: hoy se vincula por consola (`bluetoothctl`/`btaudio-ctl`).
- **Perilla de alineado/inclinación de WeFax**: su registro `CTRL_*` no está portado; los valores
  se ajustan desde los parámetros.
- **UI principal con temas**: los diálogos de apps siguen el tema; la pantalla principal
  (barra, botones, espectro) todavía usa los estilos fijos.
- **Cluster**: los spots están en la app, pero **no** como columna en la pantalla principal, y
  falta el *spot snap* (que el VFO se detenga en un spot girando despacio).
- **Field Ops**: el **CQ de voz** en SSB no funciona (el API de mensajes de voz de su fork no se
  portó; el de CW sí).
- **WeFax**: falta la perilla de alineado/inclinación (su registro de controles no está portado);
  esos valores se ajustan desde los parámetros.
- El **teclado CW** y el *snake* del firmware original no están en esta ROM.

## Instalación

Grabar la imagen en una microSD (el equipo arranca de la SD) o actualizar el rootfs. La partición
`DATA` con las configuraciones, logs, temas y grabaciones **se conserva**.

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
