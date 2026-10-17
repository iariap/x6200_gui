# Banco QEMU — barrido de ROE multipasada (issue #125)

**Fecha**: 2026-10-17 · Rama `factory/swrscan-multipass-125` · base `52a0204e`
**Binario probado**: `md5 a931216d9592` (13.753.312 B, "es el firmware del port", 3/3 marcas)
**Fuente**: `src/ + lvgl/src` → `md5 31e3f713ca17` · QEMU_CPU `cortex-a7`
**Chroot**: `/home/pablo/desa/x6200/.vfix3` · `BUILDROOT=/home/pablo/desa/x6200/buildroot_new/AetherX6200Buildroot`

## Método

```bash
cd <worktree>
export CHROOT=/home/pablo/desa/x6200/.vfix3 \
       BUILDROOT=/home/pablo/desa/x6200/buildroot_new/AetherX6200Buildroot
tools/remote/compila.sh                      # cliente del socket de control
tools/captura-banco.sh --check               # preflight: docker+imagen+volumen+chroot+qemu+PIL ok
RECONSTRUIR=si tools/captura-banco.sh \
    docs/evidencia/2026-10-17-swr125-banco/01-swrscan-abierto.png \
    '{"cmd":"app","text":"SWRScan"}'
# el contenedor banco-apps queda corriendo: se encadena por docker exec (un SEGUNDO
# captura-banco.sh reinicia la GUI y se pierde la app abierta)
docker exec banco-apps sh -c 'chroot /t /tmp/x6200-remote "{\"cmd\":\"btn\",\"val\":0}"'   # Run
docker exec banco-apps sh -c 'chroot /t /tmp/x6200-remote "{\"cmd\":\"get\"}"'             # estado
# y la captura del framebuffer con el mismo paso PIL del script, leyendo $CHROOT/tmp/fakefb0
```

Salida del rebuild y de la primera captura:

```
limpiando la GUI (dirclean): el rebuild reusa una copia propia del repo
reconstruyendo la GUI: RECONSTRUIR=si
OK: /home/pablo/desa/x6200/.vfix3/usr/sbin/x6200_gui es el firmware del port (ELF32 ARM, 13753312 B, 3/3 marcas)
  respuesta: {"t":"ok"}
  captura: 01-swrscan-abierto.png (334 colores distintos)
  fuente: md5 31e3f713ca17 (src/ + lvgl/src)
  binario: md5 a931216d9592   qemu-cpu: cortex-a7
```

## Resultado

| Captura | Qué muestra |
|---|---|
| `01-swrscan-abierto.png` | La ventana real de barrido de ROE abierta por `main_screen_start_app(ACTION_APP_SWRSCAN)` sobre el binario ARM: estado **`Idle`**, readout **`sin datos medidos`** (nunca un `1.0` inventado), la grilla con sus etiquetas de eje y la fila `Run / Scale: Log / Span: 50 kHz / Passes: 1`. El **`Passes: 1`** es la prueba en el binario de la ROM de que la multipasada sigue apagada (`SWRSCAN_MULTIPASS_TX_ENABLED` no se compila en `src/CMakeLists.txt`) |
| `02-barriendo.png` | Tras **Run** por el control remoto (`{"cmd":"btn","val":0}`): estado **`Scanning pass 1/1  p 1/51`**, el botón ya dice **`Stop`**, y la radio quedó en el modo del barrido (`mode LSB`) con **`tx: true`** |

`estados.txt` guarda las respuestas crudas del control remoto y las mediciones que
acompañan a lo anterior.

## Lo que el banco NO puede probar (requires-radio)

El banco no tiene radio, y eso acota lo que estas capturas significan:

1. **No hay paquetes del flow**: `dialog_swrscan_update()` nunca corre, así que la
   sesión se queda en `p 1/51` y no hay curva que dibujar con datos reales. La
   forma de la curva con muestras reales está en el test nativo y en los fixtures
   del arnés de UI (`swrscan-3pass`, `-p1/-p2/-p3`, `-5pass-*`).
2. **El watchdog de estancamiento no se dispara acá, y está medido por qué**:
   `grep -c "Flow reset" $CHROOT/out-banco.txt` → **0**. Esa marca es del único
   camino de `radio_tick()` que corre **sin** paquetes (el `else` de
   `x6200_flow_read()`); con 0 ocurrencias, ese camino no corrió nunca en el
   banco: sin equipo, `x6200_flow_read()` no devuelve `false`, el hilo de la radio
   no llega al `else` y `dialog_swrscan_poll()` no corre. En la radio real, cuando
   el flow deja de mandar paquetes, ese `else` **sí** corre. La lógica del timeout
   (aborto + TX off + cierre idempotente) está cubierta por el test nativo sobre el
   reloj virtual (`tests/native/test_swrscan_multipass.c`).
3. **El Stop por el control remoto con el barrido corriendo se DESCARTA**: es el
   hallazgo ya documentado del #119 (ver
   `docs/evidencia/2026-10-16-roe119-banco/README.md`): `apply_cmd()`
   (`src/remote_server.cpp`) sólo deja pasar los comandos de parada mientras la
   radio transmite, y `btn` no es uno de ellos. Por eso el camino Stop/cierre no se
   puede ejercitar por el control remoto: queda cubierto por el test nativo (stop
   parcial, `Stopped (parcial)`) y por el fixture `swrscan-3pass-stop`.
4. **La potencia real de TX** del barrido (0.1 W) y el asentamiento real del VFO
   (`SWRSCAN_SETTLE_MS`): el banco no tiene equipo.

No hubo **crash** en ninguna de las dos capturas (`$CHROOT/crash.txt` no existe) y
la GUI siguió contestando el handshake.

## Convivencia con otra corrida

El banco es un recurso compartido. Una sesión concurrente del mismo arnés
(worktree `cw-tune-122`, issue #122) volvió a levantar el contenedor `banco-apps`
en medio de esta corrida y lo siguió usando para su propia evidencia. Por eso:
las capturas 01 y 02 se tomaron con el contenedor propio (binario `a931216d9592`),
y **no se borró** `banco-apps` al terminar: habría cortado la corrida ajena.
