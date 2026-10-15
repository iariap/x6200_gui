---
name: pruebas-sin-radio
description: Correr, capturar y COMANDAR el x6200_gui sin la radio — tests ARM con qemu-arm-static, el banco con framebuffer falso y el control remoto por socket. Use when the user says "sin radio", "banco", "qemu", "captura de pantalla de la GUI", "probar la UI sin equipo" or asks to drive the app from outside (abrir apps, apretar botones, cambiar frecuencia).
---

# Pruebas sin radio (QEMU + banco + control remoto)

Permite ver y comandar el firmware **real** (el binario ARM de la imagen) en la PC, sin la radio.

## Cuando usarlo

- Ver un cambio de UI sin depender de que la radio este libre.
- Reproducir una pantalla o un crash en segundos, y dejar la captura commiteada.
- Manejar la app por script: abrir apps, apretar botones de la barra, cambiar de pagina, poner una
  frecuencia, leer el estado.

## Requisitos

- `docker`, con la imagen `x6200-buildroot` y el volumen `x6200-build-output` (los del build comun).
- Un **chroot del rootfs** ya extraido (`CHROOT`, por defecto `../.vfix3`). Si no existe, se arma
  extrayendo el rootfs de la imagen (ver `docs/TESTING.md`, nivel 4).
- `qemu-arm-static` utilizable por el contenedor (binfmt_misc).
- Las libs del banco: `tools/bench/` (ya versionadas; `tools/bench/compila.sh` las recompila).

## Puesta a punto: primero el chequeo

```bash
cd x6200_gui
CHROOT=../.vfix3 tools/captura-banco.sh --check
```

No toca nada: verifica docker, la imagen y el volumen del build, el chroot (que sea un rootfs de
verdad), `qemu-arm-static` ejecutando ARM, el cliente de control, las libs del banco (y las compila si
faltan), `python3` + Pillow, el buildroot y si hay cambios sin commitear. Si algo falta lo dice con el
nombre y el comando para arreglarlo, y no sigue. **Es el primer comando a correr cuando algo no anda.**

## Lo mas rapido: una captura

```bash
cd x6200_gui
CHROOT=../.vfix3 tools/captura-banco.sh /tmp/home.png
CHROOT=../.vfix3 tools/captura-banco.sh /tmp/status.png '{"cmd":"app","text":"Status"}'
CHROOT=../.vfix3 tools/captura-banco.sh /tmp/14074.png '{"cmd":"freq","val":14074000}'
docker rm -f banco-apps        # cuando terminaste (deja la GUI viva entre llamadas)
```

El script deja la GUI corriendo entre llamadas para poder encadenar capturas. Sale con error si el
banco no responde.

## El audio: que la GUI escriba y capturarlo

```bash
CHROOT=../.vfix3 CAP_AUDIO=/tmp/audio.wav tools/captura-banco.sh               # prueba de voz + RMS
CHROOT=../.vfix3 CAP_AUDIO=/tmp/vacio.wav CAP_VOZ= tools/captura-banco.sh     # control: silencio
docker rm -f banco-audio                                                      # el modo audio lo deja borrado solo
```

Corre PulseAudio adentro del chroot con un sink nulo que lleva el nombre que la GUI busca por nombre
fijo y graba su monitor: es lo que la GUI manda al sink, con el RMS/pico medidos en la salida (y el
control sin voz tiene que dar silencio digital, si no la captura no sirve). La señal la pone la prueba
de voz de la GUI (`X6200_VOICE_TEST`, espeak, que el script copia del volumen si falta). El volumen
del mixer **no** se puede medir así (sin `/dev/snd` no hay tarjeta): eso es la Parte B, con runbook y
guard en `CAP_LOOPBACK=si`. Todo el detalle, los números medidos y lo que queda pendiente:
[`docs/banco-audio.md`](../../docs/banco-audio.md).

## Lo que el script garantiza (y como verificarlo a mano)

1. **Se prueba la ultima version.** Si **cualquiera** de estas se cumple, **limpia y despues
   reconstruye antes de probar**: el **hash de los fuentes** (`src/` + `lvgl/src`) cambio respecto de
   la ultima corrida, hay cambios **sin commitear**, o algun fuente es mas nuevo que el binario. No
   depende solo de timestamps. (`RECONSTRUIR=auto` por defecto, `si` para forzar, `no` para probar el
   binario que ya esta.)
   La limpieza es `make x6200-gui-dirclean` **antes** del `x6200-gui-rebuild`: el rebuild compila en
   una **copia propia del repo** y su mtime no es confiable (el `check-matrix` daba ok con un binario
   viejo, `docs/HALLAZGOS.md` 65), asi que ese camino no reusa ni la copia ni los `.o`. Cuesta un
   rebuild completo, a proposito.
   Al final imprime **commit, md5 de la fuente y md5 del binario** que corrio: si el `fuente:` no
   coincide con el arbol que querias probar, la corrida no vale. El md5 del binario se compara con el
   de la radio o con el de la imagen. En la pantalla, **Status → "CUI vX.Y.Z"** muestra la version del
   firmware.
2. **Se usa el socket correcto.** Al arrancar borra `tmp/fakefb0`, `crash.txt` y el
   `tmp/x6200_remote.sock` viejos (un socket viejo hace que cualquier espera pase de largo y que la
   captura salga en blanco), espera a que exista el socket y hace un **handshake `{"cmd":"get"}`**:
   el socket correcto es el que **contesta**, y si no contesta reinicia el banco una vez. Si aun asi
   no contesta, falla mostrando la cola de `out-banco.txt`.
3. **No deja evidencia falsa.** Si el PNG sale mas chico que `MINIMO_PNG` (4000 bytes) o el
   framebuffer es uniforme, **falla** en vez de guardar una captura vacia: una captura en blanco
   commiteada como evidencia es peor que un error. La captura normal del banco tiene ~300 colores
   distintos y 15-30 KB.
3. **QEMU con el CPU correcto.** Corre con `QEMU_CPU=cortex-a7` (el core de la R16). Con el modelo
   por defecto el binario muere a los pocos segundos con
   `qemu-arm-static: QEMU internal SIGBUS {code=ADERR}` y la captura sale en blanco.

Variables utiles: `CHROOT`, `VOLUMEN`, `IMAGEN`, `BUILDROOT`, `ESPERA` (segundos tras la orden),
`RECONSTRUIR`, `QEMU_CPU`.

## Comandar la app desde afuera

La GUI escucha en un **socket UNIX** (`/tmp/x6200_remote.sock` dentro de la radio o del chroot) y
recibe **un objeto JSON por linea**. El cliente es `tools/remote/x6200-remote.c`
(`tools/remote/compila.sh` lo compila para ARM).

Dos detalles que hacen tropezar:

- El socket queda del usuario `nobody` y **modo 600**, asi que desde el host no se puede conectar:
  hay que usar el cliente **dentro del chroot** (corre como root), que es lo que hace el script.
- Las ordenes no se aplican directo: van al **hilo de LVGL** por el scheduler. La respuesta a lo que
  se manda es `{"t":"ok"}` o `{"t":"err","err":"..."}`.

```bash
# a mano, con el banco andando
docker run --rm --entrypoint "" -v "$PWD/../.vfix3:/t" x6200-buildroot \
    sh -c 'chroot /t /tmp/x6200-remote "{\"cmd\":\"app\",\"text\":\"Status\"}"'
```

### Comandos (definidos en `src/remote_server.cpp`)

| Comando | Que hace |
|---|---|
| `{"cmd":"get"}` | una linea de estado (sirve de handshake) |
| `{"cmd":"sub"}` | estado 5/s, espectro 10/s, eventos y texto decodificado hasta que el cliente cierra |
| `{"cmd":"freq","val":14074000}` | frecuencia (Hz) |
| `{"cmd":"tune","val":-100}` | paso de sintonia |
| `{"cmd":"mode","val":"USB"}` | modo |
| `{"cmd":"band","val":40}` | banda |
| `{"cmd":"pre\|att\|nb\|nr","val":0\|1}` | preamplificador, atenuador, NB, NR |
| `{"cmd":"agc"}` | siguiente AGC |
| `{"cmd":"rfgain","val":0..100}` | ganancia de RF |
| `{"cmd":"filter","val":Hz}` | ancho de filtro |
| `{"cmd":"nb_level\|nb_width","val":0..100}` | parametros del NB |
| `{"cmd":"nr_level","val":0..60}` | nivel de NR |
| `{"cmd":"agc_mode","val":0..3}` | OFF / SLOW / FAST / AUTO |
| `{"cmd":"pwr","val":1..100}` | potencia de TX (decimas de watt) |
| `{"cmd":"vol","val":..}` | volumen |
| `{"cmd":"location","text":"45.0,7.7"}` | posicion del telefono (Field Ops) |
| `{"cmd":"qso_log",...}` | registro QSO |
| `{"cmd":"app","text":"FT8"}` | abre/cierra una app |
| `{"cmd":"app","text":"<app>","val":..,"text":".."}` | comando **dentro** de la app abierta (`app_cmds[]`) |
| `{"cmd":"btn","val":N}` | aprieta el boton N de la barra de abajo |

Apps por nombre: `off`, `FT8`, `FT4`, `JS8`, `WSPR`, `SSTV`, `WeFax`, `NavTex`, `RTTY`, `CW`,
`Field`, `GPS`, `Bands`, `Settings`, `BT`, `Cluster`, `CwTrainer`, `Eq`, `Status`, `QTH`,
`Callsign`, `Recorder`, `WiFi`. Los comandos especificos de cada app estan en `app_cmds[]`
(`src/remote_server.cpp`): tienen `min`/`max` para `val` y/o texto.

### Invariantes

- **No hay PTT por el control remoto**: solo las apps transmiten.
- Mientras la radio transmite, solo se aceptan los comandos de stop/close.
- Todo se aplica en el hilo de LVGL: para ver el efecto, esperar y volver a capturar (`ESPERA`).

## Barrido de las apps en una sola corrida

`tools/barrido-apps-banco.sh` levanta el banco UNA vez (con `captura-banco.sh`: build fresco, socket
correcto, handshake) y contra esa misma GUI abre las 22 apps por el socket -dwell configurable,
verificacion de que sigue viva y captura por app-, y escribe la evidencia a JSON.

```bash
CHROOT=../.vfix3 tools/barrido-apps-banco.sh           # las 22 apps, 20 s de permanencia
DWELL=10 REINTENTAR=si tools/barrido-apps-banco.sh     # re-corre una vez lo que falle
SALIDA=/tmp/barrido tools/barrido-apps-banco.sh        # donde dejar la evidencia
```

Cierra el pendiente "captura-banco.sh reinicia la GUI por cada llamada": las ordenes van por
`docker exec` al contenedor que ya corre. Deja `barrido.json` (app, resultado, `app_estado`, colores,
tiempo, captura y log de cada app) mas `capturas/` y `logs/` en el directorio de `SALIDA`.

Una app esta **viva** solo si el contenedor sigue arriba, el socket contesta el handshake, el
framebuffer dibujo algo **no uniforme** y el estado dice que la app abierta es la que se pidio
(`"app"` del `{"cmd":"get"}`). Si no: `muerta` (el proceso se cayo), `sin-dibujo` (pantalla
uniforme) o `no-abrio` (la app pedida no quedo abierta); las tres fallan. El handshake se reintenta
3 veces y el crash se corrobora por los dos lados (contenedor/socket y el `CRASHLOG` del log de la
GUI).

## Otros caminos sin radio

1. **Tests ARM nativos** (logica, sin UI), con `qemu-arm-static`:
   `qemu-arm-static tests/native/build/arm_tests` -> hoy **111 comprobaciones, 0 fallos**.
2. **Dependencias del binario de la imagen**: extraer el rootfs y correr
   `LD_TRACE_LOADED_OBJECTS=1 qemu-arm-static -L /tmp/root /tmp/root/usr/sbin/x6200_gui`
   (`docs/TESTING.md`, nivel 4).
3. **Harness de UI en el host** (`tools/ui_harness/`): renderiza componentes reales del GUI sin
   QEMU ni radio; es la base del pipeline de A/B (`docs/AB_TESTING.md`).

## Problemas conocidos

| Sintoma | Causa / solucion |
|---|---|
| `QEMU internal SIGBUS {code=ADERR}` | Falta `QEMU_CPU=cortex-a7` (el script ya lo pone) |
| `connect ... Connection refused` | El socket es de `nobody` modo 600: usar el cliente dentro del chroot |
| Captura en blanco (PNG de ~1 KB) | Socket o framebuffer viejos; el script los borra. El fb es `480x800x32 BGRA` y hay que rotarlo 270 |
| El banco muere solo despues de un rato | Artefacto del rotativo bajo QEMU; el script reinicia el contenedor |
| `md5` del binario no cambia | `RECONSTRUIR=si` (el hash de fuentes o los cambios sin commitear tambien lo fuerzan; ese camino fuerza `dirclean` antes del rebuild) |
| `fuente:` distinto del arbol que probas | El hash es de `src/` + `lvgl/src`: commitear o sacar los cambios sin commitear, y volver a correr |
| Avisos `avisa_subject_nulo: subject_get_int con sujeto NULL` | Esperables en el banco: no hay hardware, los sujetos arrancan nulos. No impiden la captura |
| `ERROR: la captura parece vacia` | La GUI no dibujo: subir `ESPERA`, o correr `--check` para ver si falta algo |

En la radio el mismo control remoto funciona por SSH (el cliente vive en `/tmp/x6200-remote`); para
eso estan `tools/prueba-gui-en-radio.sh`, `tools/captura-radio.sh` y `tools/prueba-apps-radio.sh`.
