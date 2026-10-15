# Registro de pruebas del X6200

Que una versión **funcione** no se sostiene con "debería andar": se sostiene con corridas de prueba
medidas. Este registro es el hermano de [`docs/HALLAZGOS.md`](HALLAZGOS.md) —ahí van los hallazgos,
acá van las **corridas**— y usa su misma regla, que es del dueño del proyecto:

> todos los hallazgos deben estar fundados, si no se pueden fundar no debe ser un hallazgo

## Cómo se registra una corrida

Cada vez que se compila y se despliega una versión nueva en la radio se agrega **una fila** a la
tabla de abajo y **una sección** al final. Cada afirmación lleva, en el mismo lugar, una de estas
tres cosas:

| Estado | Qué se escribe | Ejemplo de este registro |
|---|---|---|
| **medido** | el comando y su salida pegada, o el número con su origen y fecha | `md5sum /usr/sbin/x6200_gui` → `d0ff2dba9c85` |
| **código** | el archivo (y la línea) que lo dice | `codigo: src/spot_marks.c:181` |
| **no medido** | se dice así, y va al **anexo B** | "no se probó transmisión" |

Un número redondeado está bien **si está dicho como redondeo**. Lo que no puede aparecer es un
número inventado presentado como medición. Si algo se midió en su momento y no quedó la captura, se
dice; si una comprobación no se pudo repetir, también.

**Evidencia cruda:** las capturas, logs y pilas van a `docs/evidencia/<fecha>-<tema>/` y cada
sección los enlaza. La auditoría `tests/audita-pruebas.sh` verifica que la estructura cierre y que
**cada archivo citado exista**.

## Resumen

| Fecha (2026) | Build (md5) | Commit | verifica-radio | Barrido de apps | Caídas | Veredicto |
|---|---|---|---|---|---|---|
| 15/10 ~16:10 | `d0ff2dba9c85` (sin cambios) | `f6d1cb1` | TODO OK | no se corrió | 0 en 90 s | **Sin desplegar**: `4e9b5f4` (#77) queda pendiente, el volumen del build lo tiene el A/B del otro agente |
| 15/10 ~15:20 | `d0ff2dba9c85` | `337036c` (incluye `457a9a3`) | TODO OK | 22 ok | 0 en el barrido y 0 en 180 s de vigilancia | **Funciona** |
| 15/10 ~14:05 | `5a6e561e502f` | `e5a869b` | TODO OK | (no se corrió) | no medido | **Funciona** para #19/#26/#30/#47 |

---

## 15/10 ~16:10 — corrida sin despliegue (la radio sigue en `d0ff2dba9c85`)

**Qué se hizo:** el ciclo de monitoreo. `git fetch fork` y `HEAD..fork/lu2dsx/x6200-rom` dieron **0
commits**, pero **la radio no tiene el build del tip**: el commit de fuentes `4e9b5f4` (#77,
tipografías) está en la rama y **no desplegado**.

**Por qué no se desplegó:** el volumen del build (`x6200-build-output`) lo está usando el otro agente
para su A/B de fuentes:

```
$ docker run --rm --entrypoint "" -v x6200-build-output:/b x6200-buildroot sh -c 'ls -l /b/target/usr/sbin/x6200_gui; md5sum ...; cat /b/.fuente.md5'
  -rwxr-xr-x ... Oct 15 19:04 /b/target/usr/sbin/x6200_gui     (compilado 5 minutos antes)
  md5 237f1f6bc1da
  sello .fuente.md5 = 8b10fe903942
$ (hash de mis fuentes)          = b3334bef0b7d               (distinto: el volumen es suyo)
```

Un build limpio le pisaría el binario del A/B, así que el despliegue queda **pendiente** (regla del
skill `qa-radio`).

**Salud de la radio, medido:** `tools/verifica-radio.sh` → `TODO OK`; los dos últimos logs con
`CRASHLOG=0` y `buffer overflow=0`; vigilancia de 90 s con el PID estable (`309 → 309`) y **0 caídas**.
El equipo reinició (uptime `712.97` s) y el build desplegado persistió.

**Ajustes del dueño, sin tocar:** `theme_name=IU1WXY`, `cw_tune=cw_decoder=key_train=0`,
`swrscan_span=50000`.

**Hueco del proceso que salió de esta corrida:** la idempotencia se medía contra git (`HEAD..remote`), y
eso no dice si la radio tiene el build del tip. Corregido en el skill `qa-radio`: ahora se compara el md5
**desplegado** y, antes de compilar, se mira el sello del volumen compartido.

## 15/10 ~15:20 — `d0ff2dba9c85` (commit `337036c`)

**Qué se probó:** el build que trae el arreglo del #53 (iconos de Field Ops), el del #63 (crash del
WSPR) y los cambios de fuentes/ECO.

**Compilación y despliegue, medido:**

```
$ tools/compila-gui.sh
  binario: md5 d0ff2dba9c85   (commit 337036c - evidencia: barrido de las 23 apps ...)
$ tools/prueba-gui-en-radio.sh
  GUI lista: 7643.1 s
$ md5sum /usr/sbin/x6200_gui     # en la radio (192.168.0.236, encontrada sola por tools/lib-radio.sh)
  d0ff2dba9c85
```

**Chequeo general:**

```
$ tools/verifica-radio.sh
  TODO OK        (assets, 6 dispositivos de entrada, sink de PulseAudio, sin crashes)
```

**Barrido de apps, medido** (`PERMANENCIA=14`, evidencia:
[`barrido-23-apps-0-caidas.log`](evidencia/2026-10-15-validacion-arreglos/barrido-23-apps-0-caidas.log)):

```
22 apps con "ok"      (abre: {"t":"ok"}, cierra: {"t":"ok"})
0 menciones de caída  ($ grep -ci "cayo" → 0)
```

Es el mejor barrido registrado: en versiones anteriores caían 5 a 8 apps por barrido (issue #19).

**Vigilancia de 3 minutos** (PID y cantidad de logs):

```
  t=0    pid=2956  logs=185
  t=180  pid=2956  logs=185   → caídas: 0
  params CW: cw_tune=0 cw_decoder=0 key_train=0
```

**Lo que se probó puntualmente, y salió bien:**

- **WSPR (#63)**, que antes reiniciaba la GUI: `{"cmd":"app","text":"WSPR"}` → `{"t":"ok"}`, el
  proceso siguió vivo (`pid=4161`) y el último log quedó con `0` CRASHLOG. El arreglo `457a9a3` sí
  está en este build: `git merge-base --is-ancestor 457a9a3 337036c` → **sí** (código/medido).
  Evidencia: [`07-wspr.png`](evidencia/2026-10-15-validacion-arreglos/07-wspr.png).
- **Field Ops** abre, pero sigue con **3 errores de archivo** (rutas `/usr/share/x6100/field/*_96.png`):
  es un defecto **abierto y reportado** en el issue
  [#64](https://github.com/iariap/x6200_gui/issues/64), no una falla de esta corrida. Evidencia:
  [`errores-field-ops.txt`](evidencia/2026-10-15-validacion-arreglos/errores-field-ops.txt) y
  [`06-field-ops.png`](evidencia/2026-10-15-validacion-arreglos/06-field-ops.png).

**Transmisión: intento, y no se pudo medir.** Con la antena de 40 m ya conectada y autorización del
dueño (QRP, 40 m) se intentó disparar el **barrido de ROE**, que es el único camino de transmisión de
las apps (`Run` → `radio_start_swrscan()` → 3 W en LSB; `codigo: src/radio.c:667-690`). **No se
transmitió**, porque la app no se puede abrir por el control remoto:

```
$ for n in SWRSCAN Swrscan swrscan "SWR Scan" SWR; do
    ssh root@radio "/tmp/x6200-remote '{\"cmd\":\"app\",\"text\":\"$n\"}'"; done
  'SWRSCAN'  -> {"t":"err","err":"Unknown app"}
  'Swrscan'  -> {"t":"err","err":"Unknown app"}
  'swrscan'  -> {"t":"err","err":"Unknown app"}
  'SWR Scan' -> {"t":"err","err":"Unknown app"}
  'SWR'      -> {"t":"err","err":"Unknown app"}
```

Que **no hubo emisión** está medido, no supuesto: el log de la radio no registra TX y el estado quedó
igual antes y después (`7.1001 MHz LSB 40m SSB pwr=50`). El defecto quedó reportado (issue
[#78](https://github.com/iariap/x6200_gui/issues/78), previa búsqueda de duplicados).

Antes de intentarlo se acotó el barrido: `swrscan_span` pasó de **200000** a **50000** Hz (el log de
arranque lo confirma: `Loaded swrscan_span=50000`), así que con el dial en 7.100 el barrido quedaría
en 7.05–7.15, dentro de 40 m. Evidencia:
[`08-swrscan-tx.png`](evidencia/2026-10-15-validacion-arreglos/08-swrscan-tx.png).

## 15/10 ~14:05 — `5a6e561e502f` (commit `e5a869b`)

**Qué se probó:** la primera compilación **limpia** (el build incremental dejaba un binario viejo:
dio `30bef0dbec2a` contra `5a6e561e502f` del limpio) y los arreglos de audio/temas.

**Medido, uno por uno:**

| Issue | Antes (medido) | Después (medido) | Evidencia |
|---|---|---|---|
| **#30** volumen | GUI en `0` con la base en `8`; el comando remoto contestaba `ok` pero no cambiaba nada | GUI `4` con la base `4`; `{"cmd":"vol","val":30}` → `{"t":"ok"}` y GUI `30` | [`01-vol-30.png`](evidencia/2026-10-15-validacion-arreglos/01-vol-30.png) (el pie muestra `Volume: 30`) |
| **#44** medidor de RS | `#ff4040`, contraste **2.72:1** | `#f08080`, contraste **3.64:1** (351 píxeles del color puro) | [`04-rs-despues.png`](evidencia/2026-10-15-validacion-arreglos/04-rs-despues.png) |
| **#51/#19** CW | `sig=6`, `buffer overflow detected` de glibc, `key_train` quedaba en `1` | ese `sig=6` no volvió a aparecer y `key_train` quedó en `0` | [`crashes-sig11.txt`](evidencia/2026-10-15-validacion-arreglos/crashes-sig11.txt) |

**Y una causa que apareció midiendo (#44):** el color del tema no llegaba a la radio porque la GUI
lee los temas de `/mnt/themes` y **sólo los copia si no existen** (`codigo: src/main.c:300-301`,
`src/theme.c:21`), así que la copia vieja en `DATA` gana. Con `/mnt/themes/RS/theme.json` corregido
a mano, el medidor pasó a `#f08080` y **3.64:1**. Quedó documentado en el issue
[#44](https://github.com/iariap/x6200_gui/issues/44).

---

## Anexo B: lo que **no** está medido

| Pendiente | Por qué no está fundado | Qué haría falta |
|---|---|---|
| **Transmisión (TX) y RF** | Intentado el 15/10 y **no medido**: la app del barrido de ROE no se puede abrir por el control remoto (issue #78), así que `radio_start_swrscan()` nunca corrió. La antena es de 40 m. | Que el remoto pueda abrir esa app; después, un barrido de ROE con la potencia al mínimo |
| **Estabilidad de horas** | La vigilancia más larga fue de 180 s. | Dejar la radio corriendo y contar caídas por hora |
| **El #63 con spots reales decodificados** | Se abrió WSPR y quedó viva, pero no se esperó un ciclo completo con decodificación. | Dejarla en WSPR varios ciclos y mirar el log |

**Cerrado de este anexo:** *"el barrido dice 22 y el menú tiene 23"* — medido: el que falta es el
**SWR Scan**, que no está en la lista de apps del control remoto (`src/remote_server.cpp:285-292`,
12 entradas; probadas 5 variantes del nombre, todas `Unknown app`). Ver issue
[#78](https://github.com/iariap/x6200_gui/issues/78).
