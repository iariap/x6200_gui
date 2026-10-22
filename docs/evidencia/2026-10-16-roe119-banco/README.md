# Banco QEMU — SWR Scan con el fondo del chart (issue #119)

**Fecha**: 2026-10-16 · Rama `factory/roe-staircase-119` · HEAD `4be469da`
**Binario probado**: `md5 ade7a32163fa` (13.747.976 B, "es el firmware del port", 3/3 marcas)
**Fuente**: `src/ + lvgl/src` → `md5 99d175824138` · QEMU_CPU `cortex-a7`

## Método

```bash
cd <worktree>
export CHROOT=/home/pablo/desa/x6200/.vfix3 \
       BUILDROOT=/home/pablo/desa/x6200/buildroot_new/AetherX6200Buildroot
tools/remote/compila.sh                      # cliente del socket de control
tools/captura-banco.sh --check               # preflight: docker+imagen+volumen+chroot+qemu+PIL ok
tools/captura-banco.sh docs/evidencia/2026-10-16-roe119-banco/02-swrscan-abierto.png \
    '{"cmd":"app","text":"SWRScan"}'
# el contenedor banco-apps queda corriendo: se encadena por docker exec (un SEGUNDO
# captura-banco.sh reinicia la GUI y se pierde la app abierta)
docker exec banco-apps sh -c 'chroot /t /tmp/x6200-remote "{\"cmd\":\"btn\",\"val\":0}"'   # Run
docker exec banco-apps sh -c 'chroot /t /tmp/x6200-remote "{\"cmd\":\"get\"}"'             # estado
# y la captura del framebuffer con el mismo paso PIL del script, leyendo $CHROOT/tmp/fakefb0
```

## Resultado

```
OK: .../usr/sbin/x6200_gui es el firmware del port (ELF32 ARM, 13747976 B, 3/3 marcas)
  respuesta: {"t":"ok"}
  captura: 02-swrscan-abierto.png (278 colores distintos, 20790 B)
  version: 4be469da docs: hallazgo del #119 (la curva escalonada es un
  fuente: md5 99d175824138 (src/ + lvgl/src)
  binario: md5 ade7a32163fa   qemu-cpu: cortex-a7
```

Estado de la GUI por el control remoto, en el mismo contenedor:

```
tras Run        : app=SWRScan tx=True  mode=LSB
tras Stop (remoto): app=SWRScan tx=True  mode=LSB   <- la orden se DESCARTA (ver abajo)
tras app off    : app=''      tx=False mode=CW      <- cierre del usuario, radio restaurada
crash.txt       : no existe (no hubo crash)
```

### El fantasma: antes y despues, medido

`02-swrscan-abierto.png` (esta corrida) contra `../2026-10-16-roe113-banco/01-swrscan-abierto.png`
(la corrida del #113, mismo dialogo, mismo banco, mismo tema):

| Captura | Dentro del area del chart |
|---|---|
| #113 (`01-swrscan-abierto.png`) | se ve la **fila de frecuencia de la home** a traves del dialogo: la caja redondeada con `21.226 / 21.250.000 / 21.274` y la tira de bandas `ALL MODES / SSB` |
| #119 (`02-swrscan-abierto.png`) | el chart pinta su propio panel: **no se ve nada de la home** y el eje (`4.5..1.5`, `21.237.500 / 21.250.000 / 21.262.500`) queda en el color de texto del tema, legible |

Es el defecto 3 del #119: no era un repintado (en `frame-2.png` de la radio, con el chart
invalidado ~28 veces por segundo, el fantasma sigue ahi), es el panel semitransparente del dialogo
(alpha 204/255 en `images/dialog.bin`) y el chart sin fondo propio. Detalle en
`docs/HALLAZGOS.md` 47.

### Lo que el banco NO puede probar (requires-radio)

El banco no tiene radio: no hay paquetes del flow, asi que `dialog_swrscan_update()` nunca corre y
`data[]` queda en `1.0` → **la curva es una recta plana en el borde inferior del chart**. Por eso
queda **requires-radio**:

- la **forma de la curva dibujada** con muestras reales (el escalon del #119): el RED/GREEN esta
  en el test nativo con el dataset real de 496 puntos (`tests/native/test_swrscan.c`);
- que el **barrido real termine sin cerrar la app**: el banco solo prueba que abrir/Run/off no
  crashea y que la radio queda restaurada;
- la **potencia real de TX** del barrido (0.1 W): el banco no tiene equipo, la escritura del
  registro no es observable.

### Una orden del control remoto que el banco deja medidaApretar **Stop por el control remoto con el barrido transmitiendo no hace nada**: el comando `btn`
se descarta porque `apply_cmd()` (`src/remote_server.cpp`) solo deja pasar los comandos de parada
mientras la radio transmite (`radio_get_state() != RADIO_RX && !is_stop(c)`), y `btn` no es uno de
ellos. El estado no cambia (`app=SWRScan tx=True mode=LSB` tras la orden) y el barrido sigue. El
`{"cmd":"app","text":"off"}` si pasa (es un comando de parada) y cierra el dialogo como el usuario
—y restaura la radio: `app='' tx=False mode=CW`—. El Stop del equipo (boton fisico) no pasa por
ese camino.

Esto es tambien el **candidato fundado** del defecto 2 del #119 (la app "se cierra sola al terminar
el barrido"): el cierre remoto `app off` **si** se acepta mientras el barrido transmite, y las
herramientas del QA mandan exactamente esa orden (`tools/prueba-apps-radio.sh`,
`tools/captura-pantallas.sh`). No quedo el script de esa corrida ni el log de la GUI, asi que el
camino exacto sigue **sin fundar** (anexo A del registro de hallazgos).

---

## `parabola-roe118.png` — la parábola que el owner no vio (y la escalera de la ROM flasheada)

1600x950, md5 `cd6fa0e24b5266f94202c9dc06e22588`. Es el grafico que faltaba: las capturas del banco
salen planas porque el banco **no tiene radio** (`data[]` queda en 1.0), asi que la forma de la
curva solo se puede ver manejando el codigo con los datos reales.

**Las dos lineas, y de donde sale cada una** (ninguna la dibuja el script: las dos salen de codigo
del repo, y por eso son lo que la radio pinta):

| Linea | Que es | Como se genero |
|---|---|---|
| **clara, llena** (`arbol`) | el fit del #117 + el resample actual: la forma que va a pintar la radio al reflashear con el trunk | `src/dialog_swrscan.c` + `src/roe_curve.c` del arbol, con el dialogo real (construct → Run → un paquete por punto → `draw_cb`) y los stubs del test nativo; los segmentos los imprime el mismo stub que usa `tests/native/test_swrscan.c::test_swrscan_render` |
| **gris, a rayas** (`binario flasheado`) | el resample del **#104** (Catmull-Rom) con el barrido de 50 puntos pre-#113: exactamente lo que corre la ROM flasheada (`md5 e294229dd32d`, HALLAZGOS 46) | el mismo driver con `src/roe_curve.c` y `src/dialog_swrscan.c` de `aa05ef9c` (el #104) |

Las dos comen **las mismas muestras**: los 496 puntos reales (`roe-muestras-extraidas.csv`, commit
`b39d6014`), muestreados en el centro de cada intervalo de muestreo igual que el test nativo (el
dataset es la curva del binario, cuyas muestras son nudos exactos). La derivacion y el camino son
los del test, no una re-implementacion: el script no calcula ningun ajuste.

**El estilo** es el del chart: fondo del **panel** con el rol del tema y el eje en el rol `text`
—el look nuevo sale del fix del **#123** (`src/dialog_swrscan.c`: el chart pinta su propio fondo
opaco y el eje deja de ser blanco fijo, para que no se vea la home a traves del panel
semitransparente y el eje siga legible en los temas claros)—, span 50 kHz con las tres etiquetas
de frecuencia del chart y la escala Log. El eje esta **recortado al rango de las muestras**
(0,9..1,7): en la radio el chart va 1,5..4,5 y la curva ocupa la franja de abajo (por eso la
parabola se lee mal en el equipo; el rango del eje es otro tema, no se toco).

**Lo que se lee** (todo medido sobre el codigo, en px del chart):

- **paso maximo entre puntos dibujados**: 1 px en el arbol, **8 px** en el binario flasheado (la
  escalera: mesetas planas + escalones verticales). Es el defecto 1 del #119.
- **minimo medido** (de las muestras): SWR **0,96** en 7.014,8 kHz (muestra 9).
- **vertice del fit** (la parabola del arbol): SWR **1,18** en 7.024,9 kHz (muestra 17,08).
- **separacion**: **8,12 muestras = 10,15 kHz** — el PENDIENTE declarado para el owner: el valle
  que dibuja el fit queda 10 kHz arriba del minimo medido (el test nativo lo imprime; en la
  imagen se mide 8,08 muestras por la cuantizacion a px, el mismo numero dentro de un pixel).

En la radio los escalones de la ROM flasheada se ven **mas anchos** que aca (mesetas de 16/32/47/62
px = 1/2/3/4 intervalos de muestreo, medido en `frame-2.png`): el binario es pre-#113 y muestrea 50
puntos (intervalo 780/49 = 15,9 px), y las muestras reales llegan cuantizadas en decimas de ROE
(`radio.c`: `pack->vswr * 0.1f`), que es lo que deja corridas largas iguales. Aca las muestras se
derivan del dataset con la misma derivacion del test, para que el numero declarado (8,12 muestras)
sea el mismo que el test mide.
