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
| 15/10 ~15:20 | `d0ff2dba9c85` | `337036c` (incluye `457a9a3`) | TODO OK | 22 ok | 0 en el barrido y 0 en 180 s de vigilancia | **Funciona** |
| 15/10 ~14:05 | `5a6e561e502f` | `e5a869b` | TODO OK | (no se corrió) | no medido | **Funciona** para #19/#26/#30/#47 |

---

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
| **Transmisión (TX) y RF** | Todas las corridas fueron de recepción y UI. La antena se conectó el 15/10 cerca de las 15:40, según el dueño. | Autorización explícita del dueño y una prueba con potencia mínima (barrido de ROE, un ciclo de WSPR) |
| **Estabilidad de horas** | La vigilancia más larga fue de 180 s. | Dejar la radio corriendo y contar caídas por hora |
| **El #63 con spots reales decodificados** | Se abrió WSPR y quedó viva, pero no se esperó un ciclo completo con decodificación. | Dejarla en WSPR varios ciclos y mirar el log |
| **El barrido dice 22 y el menú tiene 23 botones de app** | El log del barrido informa 22 `ok`; no se investigó la diferencia. | Revisar la lista del barrido contra el menú de apps |
