# FT8/FT4 por UDP (GridTracker, JTAlert, Log4OM)

Implementado el 2026-10-11. Permite que las estaciones que escuchan el puerto UDP de
WSJT-X (~2237) reciban **los decodificados que hace la propia radio**, sin PC ni
tablet: la radio decodifica y emite, el logger de escritorio escucha.

Portado del firmware del X6100 de **IU1WXY** (`src/ft8_udp.c`), que a su vez viene del
PR de **SP2KMO**. Licencia LGPL-2.1-or-later, con atribución en el código.

## Qué emite

| Mensaje | Tipo WSJT-X | Cuándo |
|---|---|---|
| Heartbeat | 0 | cada 15 s (y al abrir la ventana FT8) |
| Status | 1 | al cambiar RX/TX, offset de audio, frecuencia del dial o la estación trabajada, y en cada heartbeat |
| Decode | 2 | con cada decodificado (desde el hilo del decodificador) |
| QSO Logged | 5 | al guardar un QSO |
| Logged ADIF | 12 | junto al anterior, en formato ADIF |

Todos los datagramas usan el esquema **3** de WSJT-X (el que emite WSJT-X 2.x), con la
serialización de `QDataStream` (big endian).

## Configuración

Archivo `/mnt/ft8_udp.conf`, creado con valores por defecto en el primer arranque:

```
# X6200 WSJT-X UDP broadcaster
enabled=1
host=239.255.0.0,gateway
port=2237
id=X6200
```

| Clave | Significado |
|---|---|
| `enabled` | `1`/`0`. Con `0` el módulo no gasta CPU ni abre socket |
| `host` | Hasta 4 destinos separados por coma. Acepta unicast, broadcast, multicast y la palabra `gateway` (la puerta de enlace por defecto) |
| `port` | Puerto UDP (2237 es el de WSJT-X) |
| `id` | Nombre con el que la radio se presenta |

El path se puede cambiar con la variable de entorno `FT8_UDP_CONF` (lo usan los tests y
es cómodo en una imagen de desarrollo).

**Por qué `gateway`**: en una LAN normal el multicast es lo prolijo, pero cuando el
teléfono *es* el punto de acceso, Android no entrega el multicast de su SoftAP a las
apps locales. Un unicast a la puerta de enlace (el teléfono) siempre llega. La lista de
destinos permite las dos cosas a la vez; UDP es barato.

## Uso con GridTracker / JTAlert

1. Copiar el archivo (o dejar que se cree solo) y confirmar `enabled=1`.
2. En GridTracker: *Settings → General → UDP* → puerto `2237` (y escuchar en la misma
   red que la radio).
3. En JTAlert: modo **WSJT-X**, misma red y puerto.
4. Abrir la aplicación **FT8** en la radio: aparecen los decodificados y los QSO.

En redes con varios equipos, apuntar `host` a la IP fija del PC en lugar de `gateway`.

## Detalles de implementación

- El socket es **no bloqueante**: un `sendto()` que se traba (buffer lleno, red lenta)
  congelaría la UI. Si no se puede encolar, el datagrama se descarta — es telemetría.
- Los envíos de *decode* ocurren en el hilo del decodificador; los de *status* en un
  timer LVGL de 250 ms que compara y solo envía si algo cambió.
- Los booleanos/fechas siguen el esquema de WSJT-X (QDateTime: fecha juliana + ms del
  día), y el ADIF se arma con `ft8_udp_build_adif()`, que omite los campos vacíos.
- **Sin UI todavía**: se prende/apaga editando el archivo. El módulo ya expone
  `ft8_udp_is_enabled()` / `ft8_udp_set_enabled()` para una fila en Ajustes cuando la
  agreguemos.

## Pruebas

`tests/native/test_ft8_udp.c` (parte del arnés de `docs/TESTING.md`) cubre:

- mapeo de banda ADIF para 160–6 m y fuera de banda;
- construcción del registro ADIF (call, grid, modo, banda, freq, reportes, potencia,
  fecha/hora, `eor`) y comportamiento con buffer chico;
- **intercambio real de datagramas por loopback**: se levanta un socket UDP en un
  puerto efímero, se apunta el módulo ahí con `FT8_UDP_CONF` y se validan magic,
  esquema, tipo, id `X6200` y el contenido de heartbeat / decode / status / QSO
  logueado (tipos 5 y 12);
- que con `enabled=0` **no** se emita nada (timeout en la recepción).

Total del arnés: 111 comprobaciones, en x86 y en ARM (`qemu-arm-static`).
