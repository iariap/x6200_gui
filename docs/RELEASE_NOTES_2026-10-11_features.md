# X6200 LU2DSX ROM — features del X6100 (pre-release)

> ⚠️ **NO PUBLICAR TODAVÍA / DO NOT PUBLISH YET.** La ROM **arranca y funciona en el equipo**
> (verificado por el usuario), pero la **migración no está terminada**: falta cerrar los puntos
> pendientes (DejaVu/Arial, la perilla de WeFax y las verificaciones que solo se hacen en la
> radio). Los sha256 están en `SHA256SUMS`.
>
> _Not ready to publish: the ROM boots and runs on the radio, but the migration is not finished
> yet. Checksums are in `SHA256SUMS`._

## Qué trae

El **programa de port** desde el firmware del X6100 de [IU1WXY](https://github.com/IU1WXY/X6100-IU1WXY):
las features de la lista, más los extras que aparecieron en el camino.

| Área | Estado |
|---|---|
| Aviso de firmware nuevo (sin telemetría) | ✅ |
| Aguja de S-meter (por tema) | ✅ |
| **Temas**: 6 incorporados (IU1WXY, Classic, Outdoor, Outdoor-Negative, Legacy, VISION) + 3 migrados del X6100 (Holo, Material 3, RS con sus imágenes) | ✅ |
| Band plan IARU sobre el espectro | ✅ |
| Grabación de pantalla (video MJPEG + audio de RX) | ✅ |
| Bluetooth: CAT (hasta 3 clientes), auriculares A2DP, teclados | ✅ |
| wfview (la radio como IC-705 en red) | ✅ |
| Web configurator ampliado: 15 páginas + HTTPS + control remoto en vivo | ✅ |
| Field Ops SOTA/POTA (referencias, log ADIF, subida a QRZ, self-spot, CQ de voz) | ✅ |
| Modos digitales: WSPR, SSTV, JS8, WeFax, NavTex, CW Keyboard y CW Trainer | ✅ |
| **Anuncios por voz (TTS)** | ✅ — espeak en español, respetando Off/When LCD off/Always y sin hablar al aire |
| **App de posición (GPS)** | ✅ — adaptada: sin receptor en el equipo, la posición la manda el teléfono/el web |

**21 apps** (las mismas que el X6100), **63.600 líneas** en `src/` y **8 suites de tests**
nativas (host 476 comprobaciones, WSPR 158, SSTV, WeFax, NavTex, JS8 1994, webserver 19 y el
chequeo de la estructura de configuración).

## El arranque: qué estaba mal y cómo se encontró

La primera imagen **no arrancaba**: quedaba en el logo. Fueron ocho fallos encadenados, todos
del port, todos en el camino del arranque, y ninguno visible sin mirar adentro:

| Qué | Síntoma |
|---|---|
| Dos campos de configuración (`cwt_level`, `cwt_group`) sin inicializar | la GUI moría antes del primer dibujo: la struct se recorre como arreglo y el campo faltante llegaba con punteros NULL |
| Falta el usuario `pulse` en la imagen | pulseaudio no arrancaba y la GUI esperaba para siempre un contexto que nunca llegaba |
| Esperas de audio sin tope y `pa_stream_drain` sin chequear | cuelgue con el stream caído |
| Mixer de ALSA sin chequear retornos ni NULL | aborto si el elemento no existe |
| Encoders (`vol`, `mfk`, `mfk_inner`) desreferenciados sin guarda | crash al abrir el EQ, o sin las perillas presentes |
| GPIO sin reservar (`x6200_gpio_init` falla) | llamada a un puntero NULL: crash al arrancar |
| Reintento de la BASE sin límite | la GUI no dibujaba nunca si la radio no respondía |
| `cfg_sm` global construido antes de que existan los subjects | **el control remoto no hacía nada**, en silencio |

Todo arreglado y verificado. La técnica que los encontró: una **reproducción en chroot** (raíz
de la imagen + framebuffer falso + la base del usuario), una **biblioteca de trazas** por
`LD_PRELOAD`, un **capturador de señales que traduce el PC y el link register** con `addr2line`,
y manejar la GUI **por su propio socket de control remoto** para abrir cada app.

## Cómo se instala

**Sin regrabar (recomendado):** copiar `x6200_r1cbu_v0.1.2-lu2dsx.tgz` a la raíz de la
partición **DATA** de la tarjeta y encender. `S02update` lo aplica al principio del arranque,
**antes** de que se lance la GUI, y respalda lo que reemplaza en `/mnt/backup_*.tgz`. Conserva
`/mnt` completo (ajustes, memorias, logs, temas, grabaciones).

**Imagen completa:** grabar `x6200_*.img` con `dd` en la tarjeta. Se pierde lo que haya en la
partición DATA.

## Limitaciones conocidas

- **WeFax**: el alineado y la inclinación se ajustan por botones; la perilla necesita portar el
  registro de controles del X6100.
- **TX Cal / CESSB / filtro TX**: dependen del baseband del X6100; el X6200 no expone esos comandos.
- **DRM/KMS**: portado y disponible, apagado por defecto hasta validarlo en el equipo.
- **App Android X6100 Link**: no existe; el protocolo del socket de control remoto sí está.
- El mapa de QSO está en JS8, no en FT8.

## Qué falta

La prueba en el equipo. Cuando pase, se saca el aviso y se publica.
