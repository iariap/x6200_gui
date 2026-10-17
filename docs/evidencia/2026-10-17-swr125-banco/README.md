# Banco QEMU — barrido de ROE multipasada (issue #125)

**Fecha**: 2026-10-17 · Rama `factory/swrscan-multipass-125` · base `52a0204e`
**Binario probado (01/02)**: `md5 a931216d9592` (13.753.312 B, "es el firmware del port", 3/3 marcas)
**Fuente**: `src/ + lvgl/src` → `md5 31e3f713ca17` · QEMU_CPU `cortex-a7`
**Chroot**: `/home/pablo/desa/x6200/.vfix3` · `BUILDROOT=/home/pablo/desa/x6200/buildroot_new/AetherX6200Buildroot`

## Seguimiento (orden del owner, 17/10): la medición en la resonancia de la antena

Las capturas 01/02 cerraron el #125 con el centro/span **por defecto del banco**
(el dial quedó en 21.250.000 Hz, banda de 15 m), **fuera de la resonancia de la
antena del owner** (HF-008, 40 m, ~7,01–7,03 MHz): el gráfico salía sin la curva
de ROE. El owner pidió ajustarlo. Este seguimiento re-captura la **medición** con
el dataset **real** del owner anclado a su rango de resonancia.

Rama `factory/swr125-capturas`, base `996b7fa7` (trunk tip, el merge del #125).

### El seed (único cambio de código)

`tools/ui_harness/src/swr_backend.c` (**+84 / −1**): se agregó el dataset real del
owner (`owner_40m_valle`) como **seed embebido** del backend fake, a partir de los
valores extraídos de su pantalla
(`docs/evidencia/2026-10-15-validacion-arreglos/roe-muestras-extraidas.csv`,
commit **b39d6014**): 496 puntos re-muestreados (vecino más próximo) a la grilla
de 51 del barrido. El seed fija el centro y el span del **dataset real**, no el
default del banco. No se tocó nada del producto (`dialog_swrscan.c`,
`swr_session.c`, `roe_curve.c`) ni RF; el CSV compartido de los fixtures
(`tests/fixtures/swrscan/swr_mock_samples.csv`) **tampoco** (los tests nativos
fijan su tamaño exacto: 1.428 muestras). Fixtures nuevos:
`tools/ui_harness/fixtures/swrscan-owner-40m.scn` y `swrscan-owner-40m-sweep.scn`.

### Por qué el arnés de UI y no el ROM del banco

El banco QEMU corre el **firmware de la ROM**, que **no tiene radio** (no hay
paquetes del flow) y **no compila el backend fake** (`SWRSCAN_MULTIPASS_TX_ENABLED = 0`
en `src/CMakeLists.txt`): la ventana de la ROM **nunca** puede dibujar la curva
(por eso 01/02 salían sin ella, más allá del rango). La única superficie que
renderiza la curva real es el **arnés de UI** (`tools/ui_harness`), que compila la
ventana REAL (`src/dialog_swrscan.c` + `swr_session.c` + `roe_curve.c`) sobre el
backend fake con los datos del owner. Por eso 03/04 son capturas del arnés.

### Centro y span (b)

- **Centro: 7.026.825 Hz**, **span: 21.562 kHz** — el **rango exacto declarado en
  el CSV real del owner** (7.016.044 → 7.037.606 Hz), que es la fuente de la
  verdad de este seguimiento. Con el span exacto del dataset los 51 puntos de la
  grilla caen **dentro** de lo medido (sin extrapolar). El span más cercano del
  control del SWR (50 kHz) forzaría ~14 kHz de extrapolación de cada lado, así que
  se usó el span exacto del dataset, que el backend ya soporta (deriva centro/span
  del dataset; no se tocó el producto).
- La grilla del diálogo es `swr_grid_build(7.026.825, 21.562, 51)`; el backend
  corta la corrida si el dial no está en el centro (fail-closed).

### Mínimo visible (c)

- `03-swrscan-valle.png`: **`min 1.0 @ 7.019.925 kHz (n=51/51)`** — estado
  `Completed`, sesión congelada.
- `04-swrscan-valle-sweep.png`: **`min 1.0 @ 7.019.925 kHz (n=25/51)`** — a mitad
  de la única pasada (`Scanning pass 1/1  p 26/51`, botón en `Stop`).
- El mínimo es la ROE **medida** agregada (nunca el vértice del ajuste, #125); el
  tick vertical marca su frecuencia. La curva dibujada es el ajuste cuadrático
  decorativo de los puntos medidos (#125, `roe_curve.c`): con esta antena
  (bien adaptada, ROE ~1,0–1,5) el ajuste es poco profundo; el dato duro del
  mínimo está en el readout y en el tick.

### Rango y escala (d)

- Rango barrido: **7.016.044 – 7.037.606 Hz** (span 21.562 kHz), la zona de
  resonancia de la antena. Etiquetas del eje (centro ± span/4): 7.021.435 /
  7.026.825 / 7.032.215.
- Escala: **Linear** (el default compilado de la ROM, `src/cfg/cfg.c`), igual que
  el resto de los fixtures `swrscan-*` del arnés.
- Una pasada (**Quick**, la única que habilita la ROM); el arnés compila el gate
  multipasada ON, pero el fixture pide 1.

### Cómo se reprodujo

```sh
cd <worktree>   # state/worktrees/x6200/swr125-capturas
docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" \
  -v "$PWD:/src" -w /src/tools/ui_harness x6200-uiharness \
  -lc "mkdir -p build && cd build && cmake .. -DHARNESS_REVISION=$(git rev-parse HEAD) && make -j\$(nproc)"
docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" \
  -v "$PWD:/src" -w /src/tools/ui_harness/build x6200-uiharness -lc "
    ./ui_harness --scenario ../fixtures/swrscan-owner-40m.scn \
        --png out/03-swrscan-valle.png --json out/03-swrscan-valle.json
    ./ui_harness --scenario ../fixtures/swrscan-owner-40m-sweep.scn \
        --png out/04-swrscan-valle-sweep.png --json out/04-swrscan-valle-sweep.json"
```

El binario del arnés de esta corrida: `md5 b66550230b4b` (sin RF linkeada: el
instrumento es `mocks/mock_radio.c`, contadores; `tx_on=1`, `tx_off=1` en el JSON
son del mock). Los JSON crudos quedan junto a los PNG (`03-swrscan-valle.json`,
`04-swrscan-valle-sweep.json`) con el bloque `swr` completo (dataset, puntos,
comandos, estado, congelado).

| PNG | md5 |
|---|---|
| `03-swrscan-valle.png` | `4b7cc35b75f056aa55117a22b4b60537` |
| `04-swrscan-valle-sweep.png` | `1dfd6837459586e70906f2ec25b20c86` |

## Método (01/02: banco QEMU del ROM)

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
| `03-swrscan-valle.png` | **Seguimiento 17/10.** La ventana REAL sobre el backend fake con el **seed real del owner** (antena de 40 m) anclado a su resonancia: estado **`Completed`** (congelado), la **curva** dibujada sobre el rango **7.016.044–7.037.606 Hz** (`Span: 21 kHz`), el **tick del mínimo** y el readout **`min 1.0 @ 7.019.925 kHz (n=51/51)`**; la fila `Run / Scale: Linear / Span: 21 kHz / Passes: 1` |
| `04-swrscan-valle-sweep.png` | **Seguimiento 17/10.** La MISMA sesión a mitad de la pasada: estado **`Scanning pass 1/1  p 26/51`**, el botón en **`Stop`**, la **curva parcial** con el valle visible y el readout **`min 1.0 @ 7.019.925 kHz (n=25/51)`** |

`estados.txt` guarda las respuestas crudas del control remoto y las mediciones que
acompañan a lo anterior.

## Lo que el banco NO puede probar (requires-radio)

El banco no tiene radio, y eso acota lo que estas capturas significan:

1. **No hay paquetes del flow**: `dialog_swrscan_update()` nunca corre, así que la
   sesión se queda en `p 1/51` y no hay curva que dibujar con datos reales. La
   forma de la curva con muestras reales está en el test nativo y en los fixtures
   del arnés de UI (`swrscan-3pass`, `-p1/-p2/-p3`, `-5pass-*`; y ahora
   `swrscan-owner-40m`, con el seed real del owner).
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
la GUI siguió contestando el handshake. En el seguimiento, las capturas 03/04 son
del arnés de UI: no hay radio ni RF (el binario no linkea ninguna librería de RF).

## Convivencia con otra corrida

El banco es un recurso compartido. Una sesión concurrente del mismo arnés
(worktree `cw-tune-122`, issue #122) volvió a levantar el contenedor `banco-apps`
en medio de esta corrida y lo siguió usando para su propia evidencia. Por eso:
las capturas 01 y 02 se tomaron con el contenedor propio (binario `a931216d9592`),
y **no se borró** `banco-apps` al terminar: habría cortado la corrida ajena.
