# Banco QEMU — SWR Scan con el barrido automático y TX al piso del equipo (#113)

**Fecha**: 2026-10-16 · Rama `factory/roe-parabola-113` · HEAD `e1878ac4`
**Binario probado**: `md5 69c0f6e6f7fb` (13.743.188 B, "es el firmware del port", 3/3 marcas)
**Fuente**: `src/ + lvgl/src` → `md5 fe5839dd9dea` · QEMU_CPU `cortex-a7`

## Método

```bash
cd <worktree>
export CHROOT=/home/pablo/desa/x6200/.vfix3 \
       BUILDROOT=/home/pablo/desa/x6200/buildroot_new/AetherX6200Buildroot
tools/remote/compila.sh                      # cliente del socket de control
tools/captura-banco.sh --check               # preflight: docker+imagen+volumen+chroot+qemu+PIL ok
tools/captura-banco.sh docs/evidencia/2026-10-16-roe113-banco/01-swrscan-abierto.png \
    '{"cmd":"app","text":"SWRScan"}'
# el contenedor banco-apps queda corriendo: se encadena por docker exec (un SEGUNDO
# captura-banco.sh reinicia la GUI y se pierde la app abierta)
docker exec banco-apps sh -c 'chroot /t /tmp/x6200-remote "{\"cmd\":\"btn\",\"val\":0}"'   # Run
docker exec banco-apps sh -c 'chroot /t /tmp/x6200-remote "{\"cmd\":\"get\"}"'             # estado
# y la captura del framebuffer con el mismo paso PIL del script (rotado 270, 480x800x32 BGRA)
```

El `captura-banco.sh` hace el rebuild cuando el hash de los fuentes cambió: compila el binario ARM
desde este worktree y lo pasa por el gate `verifica-firmware-port.py` antes de correrlo.

## Resultado

```
OK: .../usr/sbin/x6200_gui es el firmware del port (ELF32 ARM, 13743188 B, 3/3 marcas)
  respuesta: {"t":"ok"}
  captura: 01-swrscan-abierto.png (289 colores distintos)
  captura: 02-swrscan-corriendo.png (289 colores distintos)
  version: e1878ac4 tests: comentario del test del barrido sin la parte
  fuente: md5 fe5839dd9dea (src/ + lvgl/src)
  binario: md5 69c0f6e6f7fb   qemu-cpu: cortex-a7
```

Estado después de apretar **Run** (`btn 0`):

```
{"t":"state","vfo":"A","freq":21250000,"mode":"LSB",...,"pwr":50,"db":-121,"tx":true,"app":"SWRScan",...}
```

- `01-swrscan-abierto.png`: el diálogo abre y dibuja el chart (ejes 1,5..4,5 y 21.226 / 21.250.000 /
  21.274), botón `Run`, `Span: 50 kHz` (= **41 puntos** del modo automático, paso 1250 Hz, 1435 ms
  de barrido). La línea blanca del borde inferior es la curva con los datos iniciales
  (`data[] = 1.0`): el camino de la parábola corre en el binario ARM y queda **dentro** del área.
- `02-swrscan-corriendo.png`: con el barrido andando (botón `Stop`, modo `LSB`, `tx:true`,
  `app:"SWRScan"`), la misma línea: el barrido arranca y el diálogo sigue vivo. **`crash.txt` no
  existe** (no hubo crash).
- `"pwr":50` en el estado es **cfg.pwr** (5,0 W, la potencia del operador): el barrido no la toca
  (la potencia de TX del barrido se escribe en el registro del equipo, no en la config de la GUI), y
  al pararlo `radio_stop_swrscan()` la restaura (patrón #84/#102/#94). El modo que muestra `01` es el
  estado de arranque del banco (en la corrida vieja era CW), no algo que cambie este diff.

### Cambio visible medido (geometría borde a borde)

La captura vieja (`ee66faaf`, 50 puntos fijos, paso `span/50`) y la nueva (`e1878ac4`, 41 puntos
automáticos, paso `span/40`) con el mismo span (50 kHz) y la misma curva plana (`data[] = 1.0`).
Medido con PIL sobre la corrida blanca más larga de la fila de la curva (y=398):

| Captura | Corrida blanca | Borde derecho del chart (x=789) |
|---|---|---|
| vieja (`ee66faaf`) | x 10..**773** = 764 px (49/50 del ancho) | termina **16 px antes** |
| nueva (`e1878ac4`) | x 10..**789** = 780 px (ancho completo) | **llega al borde** |

Es el cambio esperado de la geometría nueva: antes la última muestra quedaba un paso abajo del borde
alto del span (49 intervalos de 50) y ahora el barrido va de borde a borde
(`paso = span / (puntos - 1)`, index 0 = borde bajo, index `puntos-1` = borde alto).

## requires-radio (lo que el banco NO puede probar)

El banco no tiene radio: **no hay paquetes del flow**, así que `dialog_swrscan_update()` nunca corre
y `data[]` queda en `1.0` → la curva es una recta plana en el borde inferior del chart. Por eso
queda **requires-radio**:

- la **forma de la parábola** (un valle limpio, el vértice, el clamp con datos reales);
- la **potencia real de TX** del barrido: el banco no tiene equipo, así que la escritura del registro
  (`x6200_control_txpwr_set(SWRSCAN_TX_POWER_W)`, 0.1 W) no es observable acá. Lo que sí queda
  verificado es el camino completo en el binario ARM (el barrido arranca y no crashea) y, en código,
  el valor (`SWRSCAN_TX_POWER_DW = 1` = el piso del control del equipo) con `_Static_assert` + test;
- que el **medidor de ROE siga midiendo con la misma calidad a 0.1 W** (el firmware reporta `vswr`
  en el flow; la GUI no tiene umbral de potencia, así que no hay evidencia de código de un umbral).

Lo que sí se verificó acá: el binario ARM del port compila desde este worktree, pasa el gate #86,
abre el diálogo, arranca el barrido (TX, modo LSB), dibuja la curva por el camino nuevo, la geometría
borde a borde se ve en la captura y **no crashea**.
