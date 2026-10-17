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

### Una orden del control remoto que el banco deja medida

Apretar **Stop por el control remoto con el barrido transmitiendo no hace nada**: el comando `btn`
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
