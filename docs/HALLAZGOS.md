# Hallazgos — registro de lo investigado en el X6200 con esta ROM

Este archivo es el registro de lo investigado. **Regla del proyecto: todo hallazgo va con su
evidencia en el mismo lugar** —el comando con su salida pegada, la línea del log, o la medición con
su origen— y **lo que no se puede fundar no se escribe como hallazgo**: va al
[anexo de hipótesis](#anexo-a-hipótesis-sin-fundar-no-son-hallazgos) con lo que haría falta para
fundarlo, o se saca. Cuando algo se midió en su momento y no quedó la captura, se dice; cuando una
comprobación no se pudo repetir, también.

Convención usada abajo:

- **medido** — salida real de un comando en el equipo, pegada abajo.
- **código** — lo que dice un archivo del repo (verificable leyéndolo, con el archivo citado).
- **sin fundar** — no es hallazgo: está en el anexo A.

El chequeo automático (índice contra secciones, estimaciones fuera del anexo, secciones con números
sin origen) y la convención completa viven en el skill **`hallazgos-fundados`**
(`.agents/hallazgos-fundados/`, con `scripts/audita-hallazgos.sh`).

Índice:

1. [La GUI no tenía audio: `pulse` no estaba en el grupo `audio`](#1-la-gui-no-tenía-audio-pulse-no-estaba-en-el-grupo-audio)
2. [La voz (TTS): qué se midió y qué no](#2-la-voz-tts-qué-se-midió-y-qué-no)
3. [Tiempos de arranque medidos](#3-tiempos-de-arranque-medidos)
4. [La actualización en el lugar no es confiable](#4-la-actualización-en-el-lugar-no-es-confiable)
5. [El desplegable de temas](#5-el-desplegable-de-temas)
6. [Reinicios suaves vs arranques reales](#6-reinicios-suaves-vs-arranques-reales)
7. [La copia de assets a /dev/shm](#7-la-copia-de-assets-a-devshm)
8. [Herramientas y cómo se mide](#8-herramientas-y-cómo-se-mide)
9. [Pendientes ordenados por valor](#9-pendientes-ordenados-por-valor)
10. [Los 2 segundos del arranque: no eran SQLite](#10-los-2-segundos-del-arranque-no-eran-sqlite)
11. [El replay del journal ext4 del rootfs](#11-el-replay-del-journal-ext4-del-rootfs)
12. [Lo que queda del arranque, medido](#12-lo-que-queda-del-arranque-medido)
13. [Actualizar la radio por WiFi](#13-actualizar-la-radio-por-wifi)
14. [La GUI se colgaba al mandar audio](#14-la-gui-se-colgaba-al-mandar-audio)
15. [Anexo A: hipótesis sin fundar (no son hallazgos)](#anexo-a-hipótesis-sin-fundar-no-son-hallazgos)
16. [Inventario de lo que falta portar, con evidencia](#16-inventario-de-lo-que-falta-portar-con-evidencia)
17. [La app de Bluetooth no esta en el menu](#17-la-app-de-bluetooth-no-esta-en-el-menu)
18. [Ajustes: widgets que no se podian ni alcanzar](#18-ajustes-widgets-que-no-se-podian-ni-alcanzar)
19. [Icono de Bluetooth en la barra de estado](#19-icono-de-bluetooth-en-la-barra-de-estado)
20. [La app de Bluetooth: como se llega y el scan](#20-la-app-de-bluetooth-como-se-llega-y-el-scan)
21. [Las constantes viejas del dialogo: texto tapado por los botones](#21-las-constantes-viejas-del-dialogo-texto-tapado-por-los-botones)

---

## 1. La GUI no tenía audio: `pulse` no estaba en el grupo `audio`

**Síntoma**: lo que genera la GUI no sonaba. (Que el receptor se escuche lo dice el usuario; no lo
medimos nosotros.)

**Causa, medida** (registro del 12/10, en la radio):

```
$ id pulse
uid=1000(pulse) gid=1000(pulse) groups=1000(pulse)          ← sin el grupo audio
$ grep ^audio: /etc/group
audio:x:29:                                                  ← el grupo estaba vacío
$ ls -l /dev/snd/pcmC0D0p
crw-rw---- 1 root audio 116, 16 /dev/snd/pcmC0D0p            ← root:audio, 660
$ pactl list short sinks
0  auto_null  module-null-sink.c                             ← el sink real NO existía
```

**Experimento del 13/10 para fundar la cadena** (rompí el grupo a propósito y después lo restauré):

```
$ sed -i "s/^audio:x:29:.*/audio:x:29:/" /etc/group; /etc/init.d/S50pulseaudio restart
$ id pulse
uid=1000(pulse) gid=1000(pulse) groups=1000(pulse)          ← sin audio, otra vez
$ pactl list short sinks
0  auto_null  module-null-sink.c
$ killall x6200_gui; sleep 18; pactl list short sink-inputs
                                                             ← vacío: el stream NO se conectó
$ X6200_VOICE_TEST="prueba con el audio roto" /usr/sbin/x6200_gui
$ cat /mnt/app_logs/voice.log
sintetizado: 82736 muestras de 22050 Hz (reloj de la GUI: 44100 Hz); al audio 0 de 82736 - NO SE ESCRIBIO NADA AL AUDIO
```

**Corrección de lo que decía antes este registro** ✗: decía que "PulseAudio mandaba el stream al
sink nulo y se reproducía en el vacío, sin ningún error". **El experimento lo contradice**: sin el
sink real, el stream **no llegaba a conectarse** (no aparece en `sink-inputs`) y **no se escribía
nada**. Lo que el log viejo mostraba (`- NO SE ESCRIBIO NADA AL AUDIO`) sólo probaba que no había
muestras para mandar, no que el envío hubiera andado (ver §2).

**Arreglo**: agregar `pulse` al grupo `audio` en el `post-build.sh` de la imagen (no sólo a mano en
la tarjeta; ver §13, que además encontró que la tarjeta grabada lo tenía sin aplicar).

**Verificación después del arreglo, medida (13/10)**:

```
$ id pulse
uid=1000(pulse) gid=1000(pulse) groups=1000(pulse),29(audio)
$ pactl list short sinks
0  alsa_output.platform-sound.stereo-fallback  module-alsa-card.c
$ pactl list short sink-inputs
0  0  3                                                      ← el stream de la GUI existe
$ cat /mnt/app_logs/voice.log
sintetizado: 135484 muestras de 22050 Hz (reloj de la GUI: 44100 Hz); al audio 135484 de 135484
```

**La cadena de audio del SoC está configurada y no sale sonido** (medido en la radio; los
mezcladores y `hw_params` se volvieron a medir el 13/10):

| Comprobación | Salida medida |
|---|---|
| `amixer sget Headphone` | `Playback 58 [92%] [-5.00dB] [on]` |
| `amixer sget "AIF1 DA0"` | `Playback 160 [83%] [0.00dB]` |
| `amixer sget "Headphone Source"` | `Item1: 'DAC'` |
| `amixer sget "AIF1 Slot 0 Digital DAC"` | `Playback [on]` |
| `cat /proc/asound/card0/pcm0p/sub0/hw_params` | `access: MMAP_INTERLEAVED` / `format: S16_LE` |
| `dmesg \| grep sun8i-a33-audio` | `#0: sun8i-a33-audio` |
| `pactl list short sinks` | el sink real, `RUNNING` mientras la GUI reproduce |
| `speaker-test -t sine -f 440 -l 1` (13/10) | **no se pudo repetir**: `Playback open error: -16, Device or resource busy` (PulseAudio tiene tomada la placa) |

La prueba de `speaker-test` sin sonido se hizo el 12/10 con la GUI parada; **no quedó conservada la
captura** y hoy no se puede repetir con la GUI andando. Lo que sí está medido hoy es todo lo de la
tabla, incluido que el sink real existe y que las muestras entran al stream.

**Sin fundar** (anexo A): *por qué* no sale sonido por el parlante; que el audio de recepción venga
"del DSP propio"; que sea "limitación de plataforma"; que Bluetooth A2DP sea el único camino
audible (no se probó: los auriculares estaban descargados).

---

## 2. La voz (TTS): qué se midió y qué no

Se implementó TTS real (espeak, español, en un hilo con cola, sin hablar durante TX) sobre las
funciones de voz que la base traía como no-ops (`voice_stub.h`).

**Tamaños, medidos** (13/10):

```
$ ls -la /usr/bin/espeak → 18216 bytes
$ du -sh /usr/share/espeak-data (en la imagen) → 1.2M
$ du -sh .../build/espeak-1.48.04/espeak-data (original) → 2.8M
```

**Corrección importante** ✗: el registro decía que como el aviso `- NO SE ESCRIBIO NADA AL AUDIO`
nunca apareció, "el audio sí se escribió al stream". **Eso no está fundado**: ese mensaje dependía de
que hubiera 0 muestras *sintetizadas* (`escritas == 0`), no del resultado del envío. El 13/10 se
cambió el log para que informe lo que **de verdad entró al stream**, y recién entonces se pudo medir:

```
$ X6200_VOICE_TEST="prueba de voz del x6200" /usr/sbin/x6200_gui; echo "exit=$?"
exit=0
sintetizado: 135484 muestras de 22050 Hz (reloj de la GUI: 44100 Hz); al audio 135484 de 135484
```

Con el audio roto a propósito (§1), el mismo log dice la verdad al revés: `al audio 0 de 82736`.

**Carrera que apareció al medir el binario de la imagen (13/10)** ✗: una corrida del diagnóstico
reportó

```
sintetizado: 46178 muestras de 22050 Hz ...; al audio 0 de 46178 - NO SE ESCRIBIO NADA AL AUDIO
```

La causa: `audio_init()` conecta PulseAudio **en un hilo** (para no bloquear el primer dibujo, §3) y
el diagnóstico hablaba **antes** de que el stream existiera, así que `audio_play()` salía por el
`!audio_up`. Antes de este cambio de log eso se reportaba como "se mandó": era el mismo defecto de
§2 visto desde el otro lado. **Arreglado**: `audio_wait_ready(5000)` (`src/audio.c`, declarada en
`src/audio.h`) y, si el stream no llega en 5 s, el log lo dice (`prueba de voz: el stream de audio
no estuvo listo en 5 s`).

**Medido después del arreglo**, cuatro corridas seguidas del diagnóstico:

```
corrida 1 -> exit=0  al audio 45284 de 45284
corrida 2 -> exit=0  al audio 49316 de 49316
corrida 3 -> exit=0  al audio 48742 de 48742
corrida 4 -> exit=0  al audio 54132 de 54132
```

**Segunda vuelta de la misma carrera** (13/10, más tarde): otra corrida volvió a dar
`al audio 0 de 73204` con el stream ya "arriba". La causa: `audio_wait_ready()` esperaba a que el
**contexto** estuviera listo (`audio_up`), no a que el **stream** estuviera `READY`; en el medio,
`audio_play()` aguanta 2 s y descarta el bloque. Arreglado: ahora espera a
`pa_stream_get_state(play_stm) == PA_STREAM_READY` (`src/audio.c`). **Medido después: 3 de 3**
corridas con todas las muestras enviadas (45284, 49316 y 48742).

**Diagnóstico de por qué habla o no** (log con `descartado`/`encolado`/`sintetizado`, en
`/mnt/app_logs/voice.log`), ejemplo medido:

```
descartado: modo=1 lcd=1 despierta=1 tx=0 [Application|page 2]
encolado:   modo=2 lcd=1 [FFT width: 48]
sintetizado: 58026 muestras de 22050 Hz (reloj de la GUI: 44100 Hz)
```

**Sin fundar** (anexo A): "CPU barata" y que el hilo duerme (no se midió CPU).

---

## 3. Tiempos de arranque medidos

Instrumentación puesta a propósito: `/mnt/app_logs/boot.log` (acumulativo).

| Quién escribe | Qué mide |
|---|---|
| `S01boot_time` (`rootfs/etc/init.d/S01boot_time`) | fin del kernel (`uptime`) |
| `S20gui` (`rootfs/etc/init.d/S20gui`) | cuándo se lanza la GUI |
| la GUI, `boot_mark()` (`src/main.c`, declarada en `src/util.h`) | cada etapa, con `uptime` y duración |
| la GUI | primer frame dibujado (`GUI lista`) |

**Arranque de referencia, medido antes de optimizar** (líneas reales del `boot.log`, leídas de la
tarjeta el 13/10):

```
kernel listo:        3,13 s
GUI arrancando:      7,39 s     → scripts: 4,26 s
GUI lista:          12,4 s      → la GUI: 5,0 s
```

**Arranque medido después del primer arreglo** (mismas condiciones, GUI ya movida a `S20`):

```
kernel listo:        3,15 s
GUI arrancando:      6,96 s     → scripts: 3,81 s
  audio_init         8,1 s
  params_init       10,1 s      → 2,0 s (ver §10: no era SQLite)
  fuentes y estilos 10,2 s
  pantalla principal 10,2 s
  control remoto    10,7 s
GUI lista:          10,8 s      → la GUI: 3,85 s
```

**Hallazgo — la GUI arrancaba en `S95gui`**, después de la red y del NTP: arreglado con
`git mv S95gui S20gui` (el commit está en el repo; `rootfs/etc/init.d/S20gui`).

**Hallazgo — `audio_init` bloqueaba el primer dibujo**: medido entonces en 1,7 s; el registro que
quedó de esa medición es el comentario en `src/audio.c` ("tardaba 1,7 s medidos en la radio"), **no
se conservó la captura**. Arreglado: la conexión con PulseAudio corre en un hilo
(`audio_init` → `audio_init_sync`) y hoy el tramo `audio_init` mide `+0,00 s`.

**Sin fundar** (anexo A): el "6 esperas" de `S48sntp`; que la GUI "sólo necesita DATA, udev, el
daemon y /dev/shm, todo listo en S11". (La atribución de los scripts por fin está medida: §12.)

---

## 4. La actualización en el lugar no es confiable

El `S02update` busca `x6200_r1cbu_*.tgz` en `DATA`, respalda y aplica.

**Lo medido**: en el intento fallido, el `boot.log` quedó en `kernel listo` y **nada más** (no llegó
a `S20gui`), el testigo `x6200_r1cbu_*.tgz` quedó **en `DATA`** (ni aplicado ni borrado) y apareció
un `backup_*.tgz` de **29 MB**. Medido el 13/10, un respaldo completo de `DATA` ocupa **124 MB**:

```
$ du -sh respaldo-DATA-20261013_145813
124M
```

29 MB contra 124 MB ⇒ murió durante el respaldo (inferencia directa de los dos tamaños, no de un log
del proceso). **Corrección**: antes este registro decía "el completo son ~90 MB": era una
estimación; el valor medido es 124 MB.

**Conclusión operativa**: usar la **imagen completa** para actualizar. Si se usa el tgz: cargador
puesto y esperar sin apagar (**el "~10 minutos" no está medido**: no se cronometró una
actualización).

**Sin fundar** (anexo A): que el primer intento se cortara "por un corte de energía" (es el relato
del usuario, no hay log); que la raíz quedara "idéntica, verificada" (se dice verificado pero no se
conservó el md5/diff); que el equipo volviera a arrancar "sacando el tgz de DATA".

---

## 5. El desplegable de temas

**Síntoma**: en la radio se veían 2 temas en el desplegable de Ajustes.

**Causa** (código): la fila de Ajustes estaba escrita a mano con dos opciones del sistema viejo
(`" Simple \n Legacy"`, `params.theme`), mientras que los temas del X6100 (`params.theme_name`) se
listaban con `theme_list()` pero sólo los usaba la acción `ACTION_THEME`. **Arreglado**: el
desplegable arma sus opciones con `theme_list()` y guarda `theme_name` + reinicia.

**Regresión propia**: al reescribirlo se creó un `lv_dropdown_create` crudo sin
`dialog_item(&dialog, obj)` —que es lo que registra el widget en el grupo de foco— y el MFK no
llegaba al desplegable. Arreglado (commit en el repo).

**Cuántos temas hay, medido** (13/10):

```
$ ls .x6100_ref/themes/ → Holo.json  Material 3.json  RS/         (3 archivos)
$ ls rootfs/usr/share/x6200/themes/ → Holo.json  Material 3.json  RS/   (los mismos 3)
código: src/theme.c → enum { ... B_COUNT }; theme_list() recorre los B_COUNT menos B_LEGACY (5)
        y después suma los archivos de los dos directorios
⇒ 5 incorporados ofrecidos + 3 archivos = 8
```

**Chequeo automático, corrido el 13/10**:

```
$ python3 tests/check_actions.py
  acciones ofrecidas: 39 | con manejador: 39
  todas las acciones ofrecidas tienen manejador
```

Ese chequeo es el que encontró `ACTION_WF_MODE` sin manejador (esa feature no está portada: ni este
port ni la base tienen `wf_mode`).

---

## 6. Reinicios suaves vs arranques reales

**Medido** (líneas reales del `boot.log`, leídas de la tarjeta el 13/10):

```
16:13:30  kernel listo: 376.47 s de uptime
18:05:05  kernel listo: 144.42 s de uptime
GUI lista: 3298.1 s
```

**Conclusión operativa**: esos `uptime` grandes **no son arranques fríos**, son reinicios del
userspace con el kernel ya arriba. Los arranques reales son los que empiezan en ~3 s. Al medir,
ignorar las líneas con uptime grande.

**Sin fundar** (anexo A): *por qué* pasa (que el botón de power apague/reinicie por software): no se
verificó en el código/driver del power.

---

## 7. La copia de assets a /dev/shm

El daemon hacía `cp -R /usr/share/x6200/images /dev/shm/` en cada arranque; la guarda
`[ -d /dev/shm/images ] || cp ...` no alcanzaba porque `/dev/shm` es tmpfs y se vacía en cada
arranque (`grep " /dev/shm " /proc/mounts` → `tmpfs /dev/shm tmpfs`).

**Medido en el equipo** (13/10):

```
$ du -sh /usr/share/x6200/images        → 4.4M
$ find /usr/share/x6200/images -type f | wc -l → 16
$ time cp -R /usr/share/x6200/images /tmp/prueba-img
real 0m0.237s
$ df -k /dev/shm  (con la copia hecha)  → 4512 kB usados
```

**Implementado**: bind mount (`rootfs/usr/sbin/x6200_daemon`), con caída a la copia si falla.

**A/B en frío, misma radio, un arranque por estrategia** (el tramo `GUI arrancando → GUI inicio`):

| | tramo medido | `/dev/shm` usado | `GUI lista` |
|---|---|---|---|
| bind mount | **0,76 s** | **0 kB** | **6,4 s** |
| copia a tmpfs | 1,01 s | 4512 kB | 6,8 s |

Diferencia medida: ~0,25 s menos y 4512 kB de RAM menos. **Ojo con una sola muestra**: en un
arranque aislado `pantalla principal` había dado **+0,55 s** y parecía una regresión; con el A/B se
vio que era ruido (en el A/B: +0,07 y +0,09).

---

## 8. Herramientas y cómo se mide

- **`/mnt/app_logs/boot_scripts.log`** + **`rootfs/etc/init.d/rcS` instrumentado**: el `rcS` (que
  corre los scripts en orden) deja `inicio`/`fin` con el uptime de cada script, así se sabe cuánto
  tarda cada uno (§12). La copia original de buildroot queda en `/etc/init.d/rcS.orig` en la radio.
- **`boot_mark()`** (`src/main.c`, declarada en `src/util.h`): deja en `boot.log` el `uptime` **y
  cuánto duró la etapa**. Etapas cubiertas: `GUI inicio`, `lv_init`, `display init`, `audio_init`,
  `eventos/usb`, `display`, `keyboard`, `entradas`, `db_init`, `cfg:*`, `params_load`,
  `params_init`, `fuentes y estilos`, `pantalla principal`, `cw/rtty`, `radio`, `cat/qso_log`,
  `wfview`, `control remoto`.
- **`./tools/prueba-gui-en-radio.sh`**: copia la GUI compilada del volumen `x6200-build-output` a la
  radio por SSH, la reinicia y muestra las marcas nuevas. Es el banco rápido (una corrida tarda
  ~20 s; **no cronometrado**).
- **`./tools/verifica-radio.sh`**: por SSH comprueba GUI corriendo, arranque del `boot.log`, assets
  por bind mount, los `/dev/input/event*` abiertos, `pulse` en el grupo `audio`, el sink real y que
  no haya señales de crash; termina en `TODO OK` o con el número de fallas. Salida real (13/10):
  `TODO OK`.
- **`./tools/mide-arranque-banco.sh`**: corre la GUI en el chroot (sin radio, con QEMU y pantalla
  falsa). **Medido**: en el banco el tramo `GUI arrancando → params_init` da 0,15 s donde la radio
  da 2,0 s, y `control remoto` se estira 14,67 s esperando a la radio. Sirve para **atribución**, no
  para tiempos.
- **`captura-banco.sh`**: corre la GUI en el chroot y convierte el framebuffer falso a PNG (código:
  `trace/fakefb.c` → `480x800`, `line_length = 480 * 4`, `bits_per_pixel = 32`, y la conversión rota
  la imagen 270°). Es la forma de **ver** un cambio antes de grabarlo en la tarjeta.
- **`{"cmd":"cluster_feed","text":"<línea>"}`** (control remoto): inyecta un spot del clúster sin
  servidor. La línea necesita la hora `HHMMZ` (código: `src/remote_server.cpp`, el parser la
  descarta sin aviso si falta).
- **`X6200_VOICE_TEST="texto"`**: dice esa frase por voz al arrancar y **termina** (ver §14); sirve
  para probar el TTS sin tocar pantalla, entrada, radio ni WiFi.
- **`/mnt/app_logs/voice.log`** y **`/mnt/app_logs/boot.log`**: los diagnósticos de voz y arranque.
- **SSH a la radio**: `sshpass -p 123 ssh -F /dev/null -o StrictHostKeyChecking=no -o
  PreferredAuthentications=password -o PubkeyAuthentication=no root@<ip>`. `-F /dev/null` evita un
  problema de permisos de la config local de ssh. **Ojo**: el `busybox` de la imagen **no trae
  `timeout`** (medido: `timeout: command not found`, salida 127); para acotar algo usar
  `cmd & p=$!; sleep N; kill -0 $p 2>/dev/null && kill $p`.

---

## 9. Pendientes ordenados por valor

Con los números **medidos**; lo que no tiene medición va al anexo A.

1. **Scripts de arranque, ahora desglosados** (marca por script, ver §12): **2,09 s** desde el
   primer script hasta lanzar la GUI (3,13 → 5,22 s), de los cuales **S10udev 0,92 s** y el resto
   migajas de 0,04-0,18 s. Los dos scripts más caros (**S96webserver 3,18 s** y **S50pulseaudio
   2,12 s**) corren *después* de lanzar la GUI, así que no retrasan el primer cuadro.
2. **Replay del journal ext4**: 0,5-1,3 s medidos, sólo cuando el apagado anterior fue sucio (§11).
   La decisión pendiente es el rootfs read-only.
3. **`GUI arrancando → GUI inicio` (daemon + enlace dinámico)**: 0,76-1,06 s medidos, sin desglosar.
   Dentro, medido: copia de assets 0,24 s (ya reemplazada por el bind mount) y lectura del binario
   de 11,5 MB desde la raíz 0,467 s.
4. **Pantalla de arranque** (logo + progreso): no baja el tiempo, mejora lo que se percibe.
5. **Auriculares Bluetooth**: candidato para que se oiga la voz; **no probado** (§1).
6. **Features que faltan de la migración**: inventario **fundado** en §16, con el comando que lo
   reproduce (archivo por archivo entre `.x6100_ref/src` y `x6200_gui/src`).

---

## 10. Los 2 segundos del arranque: no eran SQLite

**Lo que se creía** (y estaba mal ✗): que `params_init` gastaba 2,0 s porque `load_items_from_db()`
hacía una consulta por item a SQLite.

**Medido el 13/10**:

```
$ python3  (sqlite3 sobre /mnt/params.db)
tablas: bands, band_params, params, mode_params, atu, memory, msg_cw, transverter, version, digital_modes
índices: sqlite_autoindex_params_1  →  índice único por nombre
filas en params: 67
10 pasadas × 67 consultas SELECT val FROM params WHERE name=?: 0,006 s  → 0,6 ms por pasada
```

**La causa real, medida con las marcas por etapa**: abrir un `/dev/input/eventN`.

```
$ time (10 aperturas de /dev/input/event0 con el shell)
real 0m0.525s                    → ~52 ms por apertura
marcas de boot_mark, mismo arranque:
  keyboard        +0.60 s        ← 6 aperturas (search_kbd_device probando cada event*)
  entradas        +0.97 s        ← 2 teclados + las búsquedas de perilla + las perillas
  cfg:items       +0.01 s        ← SQLite
  params_init     +0.00 s
```

Código: `keyboard_init()` → `search_kbd_device()` probaba `has_letters()` (open + `EVIOCGBIT`) con
cada `/dev/input/event*` (6 en el equipo: `matrix_keypad`, `rotary0..3`, `axp20x-pek`; los nombres
se leen en `/sys/class/input/event*/device/name`); `keypad_init()` abría 2 más; `input_dev_by_name()`
abría de a uno hasta encontrar el nombre buscado (hasta 1+2+3+4 según el orden de `readdir`) y las
perillas abrían la suya. Los tramos medidos (0,60 s y 0,97 s) son de ese orden.

**El arreglo** (sin cambiar qué dispositivos se abren, sólo cómo se los encuentra):

- `src/input_dev.c`: el nombre se lee de sysfs; si no está, cae al `EVIOCGNAME` de siempre.
- `src/kbd_caps.c` (nuevo): las capacidades se leen de
  `/sys/class/input/eventN/device/capabilities/key` (hexadecimal; la última palabra son los bits
  0..63) y se buscan `Q..P` + espacio. Tampoco abre.
- `src/main.c`: los dos teclados se buscan por nombre (`matrix_keypad`, `rotary3`).

**Validación del parseo**, medida el 13/10 contra las capacidades reales del host (16 dispositivos):
sólo `AT Translated Set 2 keyboard` da "tiene letras"; ningún falso positivo. Además hay test nativo
(`tests/native/test_kbd_caps.c`; la suite completa da `538 comprobaciones, 0 fallos`).

**Verificación en la radio** (A/B, los dos binarios, misma sesión):

| | antes | después |
|---|---|---|
| `audio_init → params_init` | **2,0 s** | **0,2 s** |
| GUI propia (`GUI arrancando → GUI lista`) | 3,69 s | **2,11 s** |
| `GUI lista` | 9,2 s | **7,9 s** |

Dispositivos abiertos después del arreglo (`ls -l /proc/<pid>/fd`), medido: `event0` ×1, `event1` ×1,
`event2` ×1, `event3` ×1, `event4` ×2 (teclado del MFK + perilla interna).

---

## 11. El replay del journal ext4 del rootfs

**Qué es**: la raíz (`/dev/mmcblk0p2`, ext4) lleva journal; si queda sucia al apagarse, el kernel la
replaya antes de montarla. Medido en la radio:

```
[    2.645998] EXT4-fs (mmcblk0p2): INFO: recovery required on readonly filesystem
[    2.653387] EXT4-fs (mmcblk0p2): write access will be enabled during recovery
[    3.175398] EXT4-fs (mmcblk0p2): recovery complete                      ← 0,53 s
```

**Cuándo pasa, medido (13/10)**:

- **Reinicio por software** (5 reinicios seguidos, sin replay ninguno):
  ```
  [    2.674679] VFS: Mounted root (ext4 filesystem) readonly on device 179:2.
  [    2.902072] EXT4-fs (mmcblk0p2): re-mounted. Quota mode: disabled.
  ```
- **Apagado sucio** (forzado con `reboot -f`, que saltea el desmontaje, y también tras el apagado
  real del equipo): sí hay replay. **0,53 s** en el caso forzado con poco acumulado; **1,34 s**
  (`2.659 → 4.001`) después del apagado real, con más cambios acumulados; y **0,26 s**
  (`2.645432 → 2.906826`, medido el 13/10) cuando el equipo se quedó **sin batería** — un corte de
  energía de verdad, sin apagado ordenado. Moraleja del rango: el replay se paga, y cuánto tarda
  depende de lo que se haya escrito desde el último commit del journal: **0,26 a 1,34 s**.

**Corrección de lo que decía antes este registro** ✗: decía "en todos los arranques". No: con
apagado ordenado no hay replay; el costo se paga en los arranques reales (el equipo se apaga con el
interruptor), y su tamaño depende de cuánto se haya escrito desde el último commit del journal.

**Qué no toca**: es la partición de la ROM. Medido: `/dev/mmcblk0p3 /mnt vfat rw` — los datos del
usuario (`params.db`, log de QSO, grabaciones, temas) están ahí y vfat no tiene journal.

**Propuestas** (no medidas: son opciones, no hallazgos): montar el rootfs read-only con tmpfs/bind
para lo que se escribe; o sacar el journal (esto último deja la partición sin recuperación
automática tras un corte: **no** hacerlo sin decisión).

---

## 12. Lo que queda del arranque, medido

Arranque en frío final (binario de la imagen, `GUI lista` = primer cuadro):

```
00:00:03  kernel listo: 3.64 s de uptime
00:00:05  GUI arrancando: 5.02 s de uptime   ← scripts: 1,38 s
  GUI inicio     5.8 s                       ← de acá a `GUI lista`: 1,4 s
  audio_init     5.8 s  (+0.00 s)
GUI lista: 7.2 s
```

Reparto, con lo medido:

- **Kernel: ~3,1-3,6 s** hasta los scripts (`S01boot_time`), y de ahí ~1,4-2 s más hasta `udevd`.
  Medido en `dmesg`: la microSD aparece en `2.618570` (`mmcblk0: ... 3.69 GiB`), el panel
  `Jinglitai JLT4013A` va de `1.555968` (power supply) a ~`2.40` (inicializado), y `udevd` figura en
  `5.02`. **Corrección**: antes decía "kernel ~2,7 s hasta `udevd`" y "el panel se lleva ~0,4 s":
  eran estimaciones; los valores medidos son los de arriba.
- **Replay del journal: 0,5-1,3 s** si el apagado anterior fue sucio (§11).
- **Scripts: 2,09 s** desde el primer script hasta lanzar la GUI, **desglosado script por script**
  (ver abajo). Antes decía "1,4-3,8 s sin desglosar": el rango sigue siendo el que se ve entre
  arranques, pero ahora se sabe en qué se va.
- **`GUI arrancando → GUI inicio`: 0,76-1,06 s** medidos, **sin desglosar** entre el daemon y el
  enlace dinámico. Medido aparte: leer el binario (11.538.740 bytes, **no** stripped —`file`— y
  **28** bibliotecas `NEEDED` —`readelf -d`—) desde la raíz cuesta 0,467 s con la caché vacía, y la
  copia de assets 0,24 s (§7).
- **GUI: 1,0-1,4 s** (ya con el arreglo de §10). `main()` hasta `audio_init` es 0,02 s. Tramos
  medidos: `cw/rtty` 0,16-0,21 s, `radio` 0,21 s, `cat/qso_log` 0,07 s, `db_init` 0,15-0,19 s,
  `fuentes y estilos` 0,04-0,09 s, `pantalla principal` 0,03-0,09 s (una vez 0,55 s: ruido, ver §7).
  Ojo: los tramos marcados suman ~0,7 s, así que queda **~0,8-1,0 s sin marca** entre el último
  (`control remoto`) y el primer cuadro: es el dibujo de la pantalla.

**Los scripts, script por script** (medido el 13/10 con `rcS` instrumentado; `boot_scripts.log`,
**dos arranques**: los números entre paréntesis son del segundo):

```
 3.23  +0.08  S01boot_time              5.51  +0.26  S30dbus
 3.43  +0.18  S01create_data           5.77  +0.23  S39btpairings
 3.57  +0.11  S01seedrng               5.89  +0.09  S40bluetoothd
 3.65  +0.06  S01syslogd               6.16  +0.24  S40network
 3.73  +0.05  S02klogd                 6.47  +0.27  S45network-manager
 3.83  +0.07  S02sysctl                6.66  +0.16  S48sntp
 3.90  +0.04  S02update                6.86  +0.16  S49ntp
 3.99  +0.06  S03tune_cache            7.00  +0.11  S50alsa
 4.08  +0.07  S04restore_backup_config  9.15  +2.12  S50pulseaudio
 5.03  +0.92  S10udev                  9.43  +0.25  S50sshd
 5.10  +0.05  S11radio_daemon          9.80  +0.34  S94btcat
 5.22  +0.09  S20gui                  13.01  +3.18  S96webserver
```

Se lee así: **antes de la GUI** (3,13 → 5,22 s; el segundo arranque 3,20 → 5,14) el 44-46% es
`S10udev` (0,92 s / 0,89 s) y el resto son doce scripts de 0,04-0,18 s; **mientras la GUI dibuja su
primer cuadro** (5,2 → 6,9) corren en paralelo dbus, Bluetooth, red y NTP (~1,4 s de trabajo,
medido); y los dos más caros (`S96webserver` 3,18 s / 3,09 s y `S50pulseaudio` 2,12 s / 2,27 s)
vienen después.

**Lo que se probó y NO se sostuvo**: bajar la prioridad (`nice -n 19`) de los scripts posteriores a
la GUI para no pelearle CPU mientras dibuja. Medido (una muestra por lado): `GUI lista` 6,9 → 6,7 s
(dentro del rango 6,6-7,2 s que se ve entre arranques) y los scripts secundarios terminando ~1 s más
tarde. No alcanza: se dejó como estaba, y el comentario en `rcS` lo deja anotado.

---

## 13. Actualizar la radio por WiFi

El rootfs de la radio es ext4 con escritura y hay root por SSH, así que un cambio se puede probar
**sin sacar la tarjeta**: se copian los archivos que cambiaron, se reinicia y se verifica. Ciclo
usado el 13/10 (con `§8` para las herramientas):

```sh
# 1) sacar los archivos nuevos de la imagen que se va a entregar
docker run --rm --entrypoint "" -v x6200-build-output:/b x6200-buildroot \
    cat /b/target/usr/sbin/x6200_daemon > x6200_daemon

# 2) subirlos y dejarlos en su lugar
sshpass -p 123 scp <opciones-ssh> x6200_daemon root@192.168.0.236:/tmp/
ssh root@192.168.0.236 'cp /tmp/x6200_daemon /usr/sbin/ && chmod 755 /usr/sbin/x6200_daemon && sync'

# 3) reiniciar y verificar
reboot; ./tools/verifica-radio.sh
```

**Hallazgo, medido**: la tarjeta grabada tenía `audio:x:29:` **sin `pulse`** (la cadena de §1 sin
aplicar), mientras la imagen nueva sí lo trae. Comprobación de la **imagen sin grabarla**:

```
$ docker run --rm --entrypoint "" -v x6200-build-output:/b x6200-buildroot \
      debugfs -R "cat /etc/group" /b/images/rootfs.ext4 | grep ^audio:
audio:x:29:pulse
```

**Alineación radio ↔ imagen, medido** (13/10, después de copiar los archivos y reiniciar):

```
radio:  md5 x6200_gui 888d1ba924e8cc095d9bc6506b869b5e
        md5 x6200_daemon 871ac41be71825e2388c8925bad0dc98
        md5 /etc/init.d/S20gui 160013835d8f4257d6884feac39cee8f
imagen: los mismos tres md5 (de /b/target, que es lo que empaqueta genimage)
```

Y la comparación de hashes de la radio contra la imagen en `/usr/sbin`, `/etc/init.d`,
`/usr/share/x6200` y `/lib` dio **119 archivos sin diferencias**.

---

## 14. La GUI se colgaba al mandar audio

Apareció al probar la voz de punta a punta **después** del arreglo del grupo `audio` (§1).

**Medido** (proceso colgado): el hilo principal en `futex_wait_queue` y el hilo `threaded-ml` (el
mainloop de PulseAudio) **vivo**, en `do_sys_poll`:

```
$ for t in /proc/604/task/*; do echo "$(cat $t/comm) wchan=$(cat $t/wchan)"; done
x6200_gui wchan=futex_wait_queue
threaded-ml wchan=do_sys_poll
```

**Causa** (código + medición): `audio_play()` y `audio_play_wait()` tenían un tope de 2 s **pero
esperaban con `pa_threaded_mainloop_wait()`, que no tiene timeout**: si el mainloop no avisa, el tope
nunca se evalúa. Y no avisa porque el sink real **acepta las muestras en su buffer pero no las
consume**: medido que entran las 135484 (log de §2) y que `pa_stream_drain()` no termina nunca.

**Corrección de lo que decía antes este registro** ✗: decía que "antes del arreglo del grupo el sink
era `auto_null`, que consume al instante, así que el bug quedaba tapado". El experimento de §1
muestra otra cosa: sin el grupo, **el stream no se conectaba** y `audio_play()` salía por el
`!audio_up` sin esperar nada (y sin escribir nada), así que no había forma de colgarse.

**Arreglo**:

- Las dos esperas duermen 1 ms y vuelven a preguntar, con el tope de 2 s de verdad.
- `audio_play()` devuelve **-1** cuando descarta un bloque (y también cuando todavía no hay stream),
  y anota `play_sin_lugar_hasta` por 5 s: sin eso, cada bloque de una frase o de una transmisión de
  FT8/JS8 pagaría sus 2 s.
- La voz informa lo que **de verdad entró** al stream.

**Medido después del arreglo**:

```
$ X6200_VOICE_TEST="prueba de voz del x6200" /usr/sbin/x6200_gui; echo "exit=$?"
exit=0                                    ← termina sola (~3 s; no cronometrado con precisión)
$ cat /mnt/app_logs/voice.log
sintetizado: 135484 muestras de 22050 Hz (reloj de la GUI: 44100 Hz); al audio 135484 de 135484
```

El diagnóstico de voz ahora corre **antes** de la pantalla, la entrada, la radio y el WiFi, y
devuelve `true` para que `main()` termine (código: `src/voice.c`, `src/main.c`): se puede correr con
la GUI andando y no queda una segunda GUI viva.

**Sin fundar** (anexo A): *por qué* el sink no consume (la explicación "el códec del SoC no está
cableado" es del anexo A, no un hallazgo).

---

## Anexo A: hipótesis sin fundar (no son hallazgos)

Ninguna de estas cosas se puede afirmar con la evidencia que tenemos. Se listan para no perderlas,
con lo que haría falta para fundarlas.

| Hipótesis | Por qué no está fundada | Qué haría falta |
|---|---|---|
| "El audio de recepción sale por el DSP propio del equipo, no por el camino del SoC" | No hay medición ni esquema: sólo se sabe que la recepción se escucha y que la GUI no | Esquema del equipo o medir la señal en el DSP |
| "No suena por el parlante porque el códec del SoC no está cableado a la salida" | Se midió que la cadena está configurada y que no suena; **el porqué** no | Documentación del códec/placa o medir la salida del códec |
| "Es una limitación de plataforma, no del código" | Conclusión causal; podría haber un control de mezcla o una ruta no encontrada | Probar la cadena con un sink que consuma (A2DP) y con la GUI parada |
| "Bluetooth A2DP es el único camino audible" | No se probó: **medido** que el adaptador `hci0` está `UP` y `bluealsa` corre, pero `bluetoothctl devices` no lista **ningún** dispositivo emparejado, así que no hay sink A2DP que probar | Emparejar auriculares y medir |
| El primer intento de `S02update` se cortó por un corte de energía | Es relato del usuario; no hay log | Log del intento / estado del equipo |
| "La raíz quedó idéntica, verificada" tras el intento fallido | Se dice verificado pero no quedó el md5/diff | Guardar los md5 de la raíz antes/después |
| El "~10 minutos" de espera del tgz | No se cronometró | Cronometrar una actualización por tgz |
| El botón de power "apaga por software y vuelve a correr el userspace" | No se verificó en el driver/script | Leer el driver/script del power |
| El "6 esperas" de `S48sntp` y que "todo está listo en S11" | No se leyó el script ni se listó el arranque de scripts | Leer `S48sntp` y medir una marca por script |
| ~~"Los 3,81 s de scripts son udev, `create_data`, `tune_cache` y el daemon"~~ | **Fundado el 13/10**: están en §12, script por script | — |
| ~~El inventario de features que faltan de la migración~~ | **Fundado el 13/10**: está en §16, archivo por archivo y con el comando | — |
| "TX Cal / CESSB / filtro TX son imposibles: el baseband no los expone" | Afirmación de hardware sin fuente | Documentación del baseband/SoC |
| "La app Android X6100 Link nunca se publicó" | Sin fuente | Buscar la publicación / confirmar con el autor |
| "El banco da 0,15 s por los `open()` del sistema de archivos del host" | La comparación es medida; **la explicación** no | Perfilar el banco (QEMU) |
| Que `addgroup` de busybox cree grupos en vez de agregar usuarios | Se observó en su momento, sin captura conservada | Repetirlo y pegar la salida |
| `speaker-test` "corre sin error y sin sonido" (12/10) | No quedó la captura y hoy no se puede repetir con la GUI andando | Parar la GUI, correrlo y pegar la salida |

**Estimaciones explícitas** (dichas como tales, no son hallazgos): el "~20 s" del banco rápido y el
"~3 s" del diagnóstico de voz no se cronometraron con precisión; y el "~1 s" de daemon + enlace
dinámico es en realidad el tramo medido 0,76-1,06 s sin desglosar.

---

## 16. Inventario de lo que falta portar, con evidencia

Comparación entre la referencia del X6100 (`.x6100_ref/src`) y este port (`x6200_gui/src`), hecha el
13/10 **archivo por archivo y símbolo por símbolo** (la hizo un subagente; el que escribe
re-verificó a mano los casos límite que se marcan abajo). Reproducible con:

```sh
cd /home/pablo/desa/x6200 && REF=.x6100_ref/src && PORT=x6200_gui/src
echo "== archivos de la ref ausentes en el port =="
comm -23 <(ls $REF | sort) <(ls $PORT | sort)
echo "== ACTION_* en la ref y no en el port =="
comm -23 \
  <(awk '/typedef enum \{/{f=1} f{print} /press_action_t;/{f=0}' $REF/params/params.h | grep -oE 'ACTION_[A-Z_0-9]+' | sort -u) \
  <(awk '/typedef enum \{/{f=1} f{print} /press_action_t;/{f=0}' $PORT/params/params.h | grep -oE 'ACTION_[A-Z_0-9]+' | sort -u)
echo "== features de la ref buscadas en el port (archivos que las mencionan) =="
for s in broadcast_db freq_scale sstv_resources dialog_snake dialog_modes dialog_channels \
         cw_receiver morse_decoder wf_mode accel_onset cessb codec_gain smeter_cal \
         spot_snap encoder_bind update_alert knob_info; do
  printf "  %-18s %s\n" "$s" "$(grep -rlw "$s" --include='*.c' --include='*.cpp' --include='*.h' $PORT | wc -l)"
done
```

Da: 22 archivos de la referencia ausentes en el port; `ACTION_APP_MODES` y `ACTION_APP_SNAKE`
solamente; y **0** para los 18 símbolos de la lista.

**Lo que falta de verdad** (sacando los renombres de lenguaje y los directorios: el port tiene en
**C** lo que la referencia tenía en **C++** —`band_info`, `dialog_settings`, `knobs`, `vol`— y
`cw_decoder/` y `common/` son directorios):

| Feature | En la referencia | En el port | Evidencia |
|---|---|---|---|
| **App Modes** (selector de modos digitales) | `dialog_modes.c:34`, `params.h:64 ACTION_APP_MODES` | **no** | `grep -rn "dialog_modes\|APP_MODES" x6200_gui/src` → 0 |
| **App Channels/memorias** (100 canales, alta/baja/renombrar/mover) | `channels.c:19` (`channel_t channels[100]`), `dialog_channels.c` | **no** | `grep -rn "channels_get\|channel_t" x6200_gui/src` → 0; y el port lo admite: `main_screen.h:35` "The channels dialog is not ported" |
| **Snake** (huevo de pascua) | `dialog_snake.c`, `buttons.cpp:352-357`, `ACTION_APP_SNAKE` | **no** | `grep -rni snake x6200_gui/src` → 1 hit, y es un comentario |
| **broadcast_db** (base EIBC + emisora bajo WFM) | `broadcast_db.c/.h`, `rootfs/usr/share/x6100/broadcast.csv` | **no** | `grep -rlw broadcast_db x6200_gui/src` → 0; `ls x6200_gui/rootfs/usr/share/x6200/broadcast.csv` → no existe (**sí** está el de la referencia; el `comm` de rootfs lo esconde porque su filtro saca las rutas con `x6100`) |
| **freq_scale** (regla de frecuencias sobre el waterfall) | `freq_scale.c/.h`, `main_screen.c:1808,1952` | **no** | `grep -rlw freq_scale x6200_gui/src` → 0; el port dibuja 3 etiquetas (`main_screen.c:1421-1424`) |
| **sstv_resources** (siembra imágenes/textos SSTV en `/mnt/sstv`) | `sstv_resources.c/.h`, `main.c:174` | **no** (los archivos están, **nada los copia**) | `grep -rlw sstv_resources x6200_gui/src` → 0; `dialog_sstv.c:61` espera `/mnt/sstv/tx_pics` |
| **WF mode** (waterfall fijo/centrado) | `cfg_wf_mode`, `waterfall_view_shift()`, `case ACTION_WF_MODE` | **no** (acción declarada sin handler) | `grep -rn ACTION_WF_MODE x6200_gui/src` → sólo `params.h:72`; `grep -rlw wf_mode x6200_gui/src` → 0 |
| **HMic F1/F2 y long-press**: las opciones Screen rec, ATT, PRE, ATU, WF mode | `dialog_settings.cpp:950-963`, `:808` | **no** esas opciones | no están en `hmic_action_items[]`/`long_action_items[]` del port (`dialog_settings.c`) |
| **IF shift** (desplaza el centro del espectro/waterfall) | 66 referencias en **14** archivos, control `CTRL_IF_SHIFT` | **sólo plumbing**: se guarda y se lee, no se aplica | `grep -rn if_shift x6200_gui/src` → **4** líneas (`cfg/settings_manager.h:94,112`, `settings_manager.cpp:71`, `remote_server.cpp:960`); en `main_screen.c`, `dsp.cpp`, `waterfall.c`, `spectrum.c`, `buttons.cpp` → **0** |
| **Decodificador CW nuevo** (coherent tone tracker, time classifier, resampler) + Dec SNR + CW peak/Q | `src/cw_decoder/*.cpp` (10 archivos), `CTRL_CW_PEAK_ON/Q` | **no** (usa el decodificador K4ICY de siempre) | `grep -rln "cw_receiver\|morse_decoder\|coherent_tone" x6200_gui/src` → 0; el port tiene `cw_decoder.c` y `MFK_CW_DECODER_PEAK_BETA` (`buttons.cpp:281`) |
| **Waterfall ref** (±dB, `CTRL_WF_REF`) | 16 refs, `settings_manager.h:140 p_waterfall_ref` | **no** | `grep -rlw waterfall_ref x6200_gui/src` → 0 |
| **Waterfall: min/max manuales** | `dialog_settings.cpp:1330` | **no** (sólo automáticos) | `grep -rn "Manual min" x6200_gui/src` → 0; el port tiene `waterfall_auto_min/max` |
| **Spot snap** (hold/speed) del clúster | 14 refs | **no** | `grep -rlw spot_snap x6200_gui/src` → 0 |
| **TX Cal** (offsets IQ, ganancia por banda, codec) + **CESSB** | `dialog_settings.cpp:3750`, `cessb` 17 refs | **no** | `grep -rn "txcal\|cessb\|codec_gain" x6200_gui/src` → 0/0/0 |
| **FM pre/de-énfasis** | `dialog_settings.cpp:1749`, `cfg_fm_emphasis` | **no** | `grep -rn emphasis x6200_gui/src` → 0 |
| **Filtro TX en Ajustes** | `dialog_settings.cpp:1782` | **no UI** (el parámetro existe) | los 10 hits de `tx_filter` están en `cfg/*` y `dialog_js8.c`; 0 en `dialog_settings.c` |
| **S-meter cal, accel onset, Anti-VOX, knob info, update alert, encoder bindings** (elegir qué control toma cada perilla) | `dialog_settings.cpp` (`:3349`, `:3352`, `:1476`, `:1453-1465`), `cfg/encoder_bind_types.h` (60+ controles) | **no** ninguno | `grep -rlw` de `smeter_cal`, `accel_onset`, `knob_info`, `encoder_bind` y `grep -rn update_alert` → **0** en todos |
| **Presets JS8/GhostNet** en la tabla `digital_modes` | migraciones `_4`/`_5` (`migrations.c:103,139`), 13 filas tipo 2/3 en `sql/digital_modes.csv` | **no** | `grep -c "JS8\|GHOSTNET" x6200_gui/src/params/migrations.c` → 0 (la tabla termina en `_2`); `awk -F, '$5==2||$5==3' sql/digital_modes.csv` → 0 vs 13. **Impacto**: los dos `dialog_js8.c` llaman igual a `cfg_digital_load(..., CFG_DIG_TYPE_JS8)` (port `:1844`, ref `:1843`), así que en el port el JS8 no encuentra presets |
| **`pending_writes` / `storage_policy`** (escrituras diferidas a la base) | `cfg/pending_writes.*` (123 refs), `cfg/storage_policy.*` (17) | **no** | `grep -rn "pending_writes\|storage_policy" x6200_gui/src` → 0 |
| **`computed_api` / `parameter_api` / `subject_api`** | `cfg/*_api.*` | **no** (el port tiene `subjects.cpp`) | `grep -rn "computed_api\|parameter_api\|subject_api" x6200_gui/src` → 0 |
| **Tests y harness del framework**: `tests/cfg/` (13) y `tests/cw_decoder/` (10), `ft8_loopback`, `js8_wavgen` | `.x6100_ref/tests/`, `tools/` | **no** | el port tiene 38 entradas en `tests/native/`, ninguna del framework `cfg` ni del decodificador nuevo |
| **Generadores de `tools/`** (boot_logo, flags_gen, font_gen, world_map_gen) | 12 entradas en `tools/` | **no** los scripts (los artefactos sí) | `ls x6200_gui/tools` → 3 scripts + `release` + `ui_harness`; el port ya tiene `flags.c`, `world_map.h`, `fonts/` |

**Lo que NO es un gap** (verificado, para no volver a confundirse):

- **Temas**: la referencia tiene 6 incorporados + 3 JSON; el port tiene **los mismos 6** (`theme.c`,
  mismo enum `B_IU1WXY..B_VISION`) + los mismos 3 JSON. El enum `THEME_SIMPLE/LEGACY` no es la
  lista real (`theme_list()` omite `B_LEGACY`). Port ≥ referencia.
- **Band plan IARU**: portado (`band_plan.c` + `band_plan_data.c`).
- **ATU cache**: la referencia lo tiene en `cfg/atu_cache.*`; el port tiene la lógica equivalente en
  `cfg/atu.c` (`find_atu_for_freq`, usado desde `radio.c:166`).
- **FT8**: `ft8/audio_worker.*`, `cq_scheduler.*`, `table_view.*` y `tx_worker.*` **no existen como
  archivos** en el port (medido con el `comm` de arriba), pero es **diferencia estructural, no de
  función**: el port tiene `ftx_worker_*` en `src/ft8/worker.c` y el resto inlineado en
  `dialog_ft8.c` (el `tx_worker()`, el hilo de decode, `make_cq_msg`, y usa `lv_table` directo).
  **No** es una feature que falte.
- **`config/x6100` vs `x6200`**: `band_info.cpp`, `dialog_settings.cpp`, `knobs.cpp`, `vol.cpp`
  aparecen como "sólo en la referencia" por el **nombre**; el port los tiene como `.c`.
- **msg_cw / msg_voice**: mismas funciones (nuevo/editar/borrar/baliza; rec/play/send/rename/delete).
- **Configurador web**: mismas 15 vistas; vive en `.x6200_webserver/`, **fuera** de `x6200_gui`.
- **btcat**, `S39btpairings`, `S94btcat`: idénticos salvo el comentario de atribución.
- **`sql/`**: `bands_*`, `memory`, `params` idénticos; el port **agrega** Broadcast FM y Air.

**Sólo en el port** (el X6200 tiene y el X6100 no): `ACTION_APP_EQ`/`dialog_eq`, `dialog_about`,
`dialog_status`, `dialog_bt` + `bt_devices`, `dialog_callsign`, `adaptive_tuning`,
`audio_enhancement`, `voice`/`voice_text` (TTS), `gpio_safe`, `input_dev`, `kbd_caps`, `flags`,
`band_plan_data`, `goertzel`, `pannel`, bandas WFM/Air.

**No determinado** (ni afirmado ni descartado; queda pendiente para no inventar):

1. Si `/mnt/sstv` se siembra desde **fuera** de este checkout (no hay copiador en el repositorio; el
   submódulo del buildroot está vacío, así que no se puede descartar un paquete de la imagen).
2. Si `.x6200_webserver` es la copia que se **instala** (no se referencia desde el `CMakeLists.txt`
   del port); se verificó paridad de vistas, no de despliegue.
3. Si un `/mnt/params.db` ya migrado pudiera tener las filas JS8 aunque el repositorio no las siembre.
4. Si las salidas de los generadores de `tools/` se regeneran en el build o están congeladas a mano.
5. El comportamiento **en la radio** de lo no portado (IF shift, WF mode, decodificador CW nuevo).
6. Si `pending_writes`/`storage_policy` tendrían impacto observable (desgaste, consumo).

---

## 17. La app de Bluetooth no esta en el menu (y donde esta cada cosa)

**Sintoma**: no se encuentra donde conectar un dispositivo Bluetooth.

**Que hay y donde** (evidencia por codigo, 13/10):

| Que | Donde | Evidencia |
|---|---|---|
| Interruptor de Bluetooth y "Discoverable (CAT)" | En **Ajustes**, seccion "Bluetooth" (junto a "Charger") | `src/dialog_settings.c:1776` `make_bluetooth()`, que lee y escribe con `btcat-ctl bt on\|off` y `btcat-ctl discoverable on\|off` |
| App de dispositivos: tabla, boton **Scan** (~15 s), pareo y conexion | `src/dialog_bt.c`, se abre con `ACTION_APP_BT` | `src/main_screen.c:213` → `dialog_construct(dialog_bt, obj)` |
| Esa accion, en la UI, **solo** esta en la lista de long press | `src/dialog_settings.c:909` `{ " APP Bluetooth", ACTION_APP_BT }` dentro de `long_action_items[]` (`:872`) | `grep -rn 'ACTION_APP_BT' src/` da 4 lineas: `params.h:53` (el enum), `main_screen.c:213` y `:376` (los `case`), y `dialog_settings.c:909` (la lista) |
| La app **no** esta en el menu de apps de la pantalla principal | `app_names[]` no la lista | `src/remote_server.cpp:123`: `off, FT8, FT4, JS8, WSPR, SSTV, WeFax, NavTex, RTTY, CW, Field, GPS, Bands, Settings` |

O sea: para abrirla **desde la radio** hay que asignarla primero: Ajustes → **"APP long press"** →
**" APP Bluetooth "** → y despues **mantener apretada la tecla APP** del panel. Las seis filas de
asignacion son `"GEN/APP/KEY/MSG/DFN/DFL long press"` (`src/dialog_settings.c:955`, `make_long_action()`)
y la tecla APP del panel es `KEYPAD_APP` (`src/keypad.c:75`).

**Las dos vias para conectar**:

1. **En la radio**: Ajustes → "Bluetooth" en On → "APP long press" → " APP Bluetooth " → mantener
   apretado APP → **Scan** → elegir el dispositivo. El pareo muestra un codigo y pide escribirlo con
   el teclado (`src/dialog_bt.c`, "Type %s and Enter on the keyboard").
2. **Por consola** (es el camino que el propio firmware documenta): `btaudio-ctl`. Comandos, de su
   encabezado: `list` (pareados y estado), `scan` (15 s), `pair MAC` (parear + confiar + conectar),
   `connect|disconnect|remove MAC`, `autoconnect MAC on|off`, **`rx on|off`** (manda el audio de
   recepcion a los auriculares por un loopback), `volume N`, `mute on|off`. Medido el 13/10 sin
   auriculares: `btaudio-ctl list` → `HP 0 -1` / `RX yes` / `MUTE yes` (ninguno pareado) y
   `/mnt/btaudio.conf` no existe todavia (van los valores por defecto).

**El audio por Bluetooth ya existe y esta documentado en el codigo** (esto corrige lo que yo habia
propuesto antes en esta seccion):

- `rootfs/etc/pulse/system.pa.d/btaudio.pa` **desconecta a proposito** `module-switch-on-connect`:
  *"Keep the default sink/source on the radio codec when a device connects"*, porque un celular que
  se conecta para CAT entra tambien como fuente A2DP y desviaria el sink. Y deja
  `module-bluetooth-policy auto_switch=0 a2dp_source=false`, y `module-bluetooth-discover
  headset=ofono` (sin HFP/HSP: en esta familia SCO no da audio y tira el WiFi).
- Por eso **no** hay que cargar `module-switch-on-connect` (lo intente y PA lo rechaza por duplicado:
  *"should be loaded once at most"*, y termina descargandolo). El drop-in que habia agregado se
  saco.
- El audio de la GUI **no** puede seguir al sink por defecto: ese stream (`audio.c`, `play_stm`) es
  el **mismo** que manda el audio de **TX del FT8/JS8**; si el default fuera los auriculares, la
  transmision digital saldria por los auriculares en vez de por la radio. Se dejo el nombre fijo de
  la placa, que es lo correcto.
- Lo que si manda audio a los auriculares es el **loopback de `btaudio-ctl rx on`**, que toma el
  audio de **recepcion** (`SOURCE=alsa_input.platform-sound.stereo-fallback`).

**Propuestas** (no medidas, no son hallazgos): poner la app de BT tambien en el menu de apps (hoy
solo se llega por un long press asignado); y, para que la **voz** (TTS) se oiga en los auriculares
A2DP, darle a la voz un **stream propio** que apunte al sink A2DP cuando hay auriculares — no mover
el stream compartido, por lo del audio de TX. **Sin fundar** hasta probarlo con auriculares: al
anexo A.

---

## 18. Ajustes: widgets que no se podian ni alcanzar, y la grilla que no entraba

**Sintoma** (reportado por el usuario): "asegurate que todas las opciones de Settings sean
navegables. Por ejemplo bluetooth no lo es y no puedo cambiarlo". Las etiquetas de Ajustes, ademas,
aparecian cortadas por la izquierda.

**Hallazgo 1 — tres widgets nunca entraban al grupo de foco.** `dialog_item(&dialog, obj)`
(`src/dialog.c`) es lo unico que mete un widget en `keyboard_group`: sin esa llamada no se puede ni
llegar con las teclas ni cambiarlo. Faltaba en:

| Widget | Donde | Consecuencia |
|---|---|---|
| los **dos switches del Bluetooth** ("Bluetooth" y "Discoverable (CAT)") | `bt_switch_cell()`, `dialog_settings.c:1754` | no se podia encender el Bluetooth |
| el desplegable **"Charger"** | `make_bluetooth()`, `dialog_settings.c:1813` | no se podia elegir |
| el switch de la **columna del cluster** | `make_cluster_column()`, `dialog_settings.c:1852` | no se podia cambiar |

Arreglado: `dialog_item()` en los tres. Es la misma clase de defecto que el desplegable de temas
(§5): **un widget que no se registra no existe para el usuario**.

**Hallazgo 2 — la grilla no entraba en el dialog.** `col_dsc[]` y `lv_obj_set_size(grid, 780, 330)`
venian con las medidas de la pantalla del X6100. El dialog **scrolleaba en horizontal** y, con
`LV_OBJ_FLAG_SCROLL_ON_FOCUS` que agrega `dialog_item()`, al enfocar un widget de la derecha las
etiquetas se corrian fuera de la pantalla: en la captura se leia `e (CAT)` en lugar de
`Discoverable (CAT)`. Arreglado como **una tabla de HTML al 100%**:

- `lv_obj_set_size(grid, LV_PCT(100), 330)` (ocupa el ancho del dialog),
- la columna 0 pasa a `LV_GRID_FR(1)`: se lleva el espacio que sobra,
- **scroll solo vertical** (`lv_obj_set_scroll_dir(grid, LV_DIR_VER)` y lo mismo en el dialog):
  no puede aparecer una barra horizontal, pase lo que pase.

**Hallazgo 3 — la barra de scroll tapaba el ultimo campo.** Con el ancho al 100%, el spinbox del ano
se leia `202` en vez de `2026` (medido ampliando la captura): la barra se dibuja **encima** del
contenido. Arreglado reservando 14 px a la derecha (`pad_right`) y dejando la barra en 7 px. Ademas
la unidad de columna subio de 34 a **45**: el spinbox de 4 digitos necesita 2 columnas de ese ancho
(el numero sale de medir la captura, no de estimarlo).

**Chequeo automatico agregado**: `tests/check_settings_focus.py` (enganchado en
`tests/run_all_tests.sh`) recorre `dialog_settings.c` y, por cada widget de foco creado
(`dropdown`, `switch`, `slider` y **`spinbox`**), exige un `dialog_item(&dialog, ...)` en la misma
funcion. Hoy: **28 creados, 28 registrados**. Los `spinbox` no estaban en la primera version del
chequeo y son 14 de esos 28: el chequeo tambien se corrigio.

**Como se verifico** (medido, no razonado):

- Para poder mirar la pantalla sin tocar la radio se agrego **`tools/remote/x6200-remote`**: un
  cliente del socket de control (`/tmp/x6200_remote.sock`) que manda una linea JSON. Medido:
  `x6200-remote '{"cmd":"app","text":"Settings"}'` → `{"t":"ok"}` y Ajustes queda abierto.
  (`tools/remote/compila.sh` lo compila con el toolchain de buildroot.)
- Captura del framebuffer por SSH (`dd if=/dev/fb0 bs=1920 count=800`, 480x800x32, rotada 270° como
  en el banco). Antes: etiquetas cortadas y contenido scrolleado (guardado como
  `evidencia-radio/captura-radio-ultima.png`). Despues: etiquetas completas, `Day, Month, Year` con
  **2026** entero y sin barra horizontal (`evidencia-radio/ajustes-100pct.png`).

---

## 19. Icono de Bluetooth en la barra de estado

**Pedido**: "no deberia ver algun icono en la home que el BT esta prendido, como veo con wifi?"

**Hecho**: la barra (`src/info.c`) tiene ahora la runa de Bluetooth al lado del icono de WiFi, con
los **mismos tres estados y el mismo criterio de color** que usa el WiFi: apagado = negro,
prendido sin dispositivo = gris (`0x909090`) y con auriculares A2DP conectados = blanco.

**De donde sale el estado** (`src/bt_state.c`, nuevo modulo): `BTCAT_BT` de `/etc/default/btcat` y
`/mnt/btcat.conf` (los mismos archivos que escribe `btcat-ctl`, que es el que corre el switch de
Ajustes), y la existencia de `/tmp/btaudio-hp`: `btaudio-ctl` lo escribe con los sinks A2DP cuando
hay auriculares conectados y lo borra cuando no (`/usr/sbin/btaudio-ctl:78`). Ajustes paso a usar
ese modulo compartido: antes tenia su propia copia del lector de esos dos archivos.

**El glifo no estaba en la fuente.** `LV_SYMBOL_BLUETOOTH` (U+F293) **no** existe en `sony_20`: las
fuentes del equipo se generaron con los simbolos que se usan y esa trae solo `f1e6`, `f1eb` (wifi) y
`f240..f244` (bateria) — contado sobre `src/fonts/sony_20.c`. Por eso el simbolo salia como un
**rectangulo vacio** (lo reporto el usuario). En vez de regenerar la fuente, la runa se **dibuja con
`lv_line`**: un trazo de 7 puntos en 14x20 (`LV_USE_LINE` ya estaba habilitado), sin depender de
ninguna fuente y tomando el color del estado.

**Refresco**: un `lv_timer` de 2 s. No se avisa desde el hilo porque `btcat-ctl` corre en un hilo
aparte y LVGL no es thread-safe; es el mismo patron que ya usa `dialog_bt.c` (700 ms). Cuesta dos
lecturas de archivos chicos y un `access()`, y cubre tambien los cambios que hace `btaudio-ctl` por
fuera de la GUI.

**Verificado** con capturas del framebuffer, los tres estados en la misma imagen
(`evidencia-radio/bt-estados.png`):

- `btcat-ctl bt off` → runa en **negro** (no se ve, igual que el WiFi apagado),
- `btcat-ctl bt on` → runa **gris**,
- con `/tmp/btaudio-hp` creado a mano → runa **blanca** (el estado "con auriculares" se verifico
  asi, creando el archivo que escribe `btaudio-ctl`; con auriculares de verdad queda por probar).

---

## 20. La app de Bluetooth: como se llega y el scan

**Pedido**: "no deberia poder tener una UX dedicada a BT para poder seleccionar a que dispositivo me
emparejo y a cual no? ... la UX deberia estar al lado de los settings de wifi", y despues "el boton
de scan en BT hace algo? lo aprete varias veces y no escanea nada".

**La app ya existia, estaba encerrada.** `src/dialog_bt.c` es la UX completa: escanea, lista los
pareados y los encontrados (con `headphones` / `keyboard` / `new`), pareа, conecta y desconecta,
muestra el passkey y tiene los botones de **RX to HP** y **Speaker**. Pero `ACTION_APP_BT` solo se
podia alcanzar asignandosela a un long press (`dialog_settings.c:916`, " APP Bluetooth").

**Donde se llega ahora** (las tres, a la misma app):

| Entrada | Donde |
|---|---|
| **Boton "BT" al lado de "WiFi"** | pantalla principal, pagina **APP 3:3** (`buttons.cpp`: `make_app_btn("BT", ACTION_APP_BT)` en `page_app_3`) |
| Fila **"Devices"** | Ajustes → Bluetooth → "Devices / Pair / connect..." |
| Lista de apps del control remoto | `remote_server.cpp`: `{"cmd":"app","text":"BT"}` (tambien en la pagina /remote del webserver) |

**El scan: funciona, y se comprobo.** Con las pulsaciones del usuario quedaron los archivos:
`/tmp/bt_ui_scan.txt` → `NEW 2C:1B:3A:0A:64:BE Living Room TV hp`, y `btaudio-ctl scan` a mano da
**el mismo** dispositivo. O sea que el script corre y su resultado se muestra en la lista (captura
`evidencia-radio/bt-app.png`: "Living Room TV" con `headphones` y `new`). No aparecian los
auriculares porque **no estaban en modo emparejamiento**: un scan solo ve lo que se anuncia.

**Dos defectos de UX que si eran reales** (`dialog_bt.c`):

1. **Scans superpuestos**: cada pulsacion lanzaba otro `btaudio-ctl scan` de 15 s y todos escribian
   el mismo archivo con `>`, asi que apretar varias veces dejaba **menos** resultados que apretar
   una. Ahora, mientras corre uno, otra pulsacion dice "Scan already running..." y no lanza nada.
2. **Sin respuesta visible**: quedaba el cartel "Scanning (15 s)..." fijo. Ahora hay cuenta
   regresiva y al terminar dice **"Scan done: N device(s)"** o **"Scan done: nothing found"**.

**Affordance de "esta escaneando"** (pedido el 13/10): ademas del texto, el **propio boton** es el
indicador. Mientras el scan corre, el boton Scan:

- cambia su etiqueta a la cuenta regresiva (**`Scan 13s`**, via `BTN_TEXT_FN` + `scan_label()`),
- queda **gris y bloqueado** (`buttons_mark` + `buttons_disabled`; `buttons_press` ignora el press si
  el boton esta `DISABLED`, `buttons.cpp:763`), asi que un segundo toque no lanza otro scan,
- y al terminar vuelve solo a `Scan` y la nota pasa a mostrar el resultado.

Se probo sin manos con el comando **`{"cmd":"btn","val":N}`** que se agrego al control remoto
(aprieta el boton N de la fila de abajo por el mismo camino que las teclas F). Capturas de los tres
momentos en `evidencia-radio/scan-affordance.png`: reposo (`Scan`) → en curso (`Scan 13s` gris) →
terminado (`Scan` + "Scan done: nothing found").

**Pendiente de probar** (necesita manos y auriculares): pulsar Scan con los auriculares en modo
emparejamiento, elegirlos de la lista, parear (con el passkey si lo pide) y encender **RX to HP**
para oir la recepcion en ellos.

**Tipografia** (preguntado el 13/10: "la tipografia de la UX de BT es igual el resto?"). Si, y esta
medido: la app usa la convencion de los dialogos de este port, `VF(22)` —`sony_22`, un paso menos
solo en el tema VISION, la misma macro que `dialog_cluster.c:35`— para cabecera, tabla y lineas de
estado; los botones de abajo salen del estilo global (`btn_style`, `sony_38`, el mismo de todos los
dialogos); y las apps portadas usan el mismo tamano (SSTV/WeFax: `sony_22`/`sony_24`). Sobre las
capturas, la tinta del titulo mide **14 px en BT y 13 px en WeFax/SSTV**, y la linea de estado **23 px
contra 22**: la misma tipografia. El tema activo **no** es VISION (el S-meter esta en la barra de
arriba y VISION lo pone en su propia fila, `theme.h:193`), que es el unico caso en el que BT bajaria
un paso respecto de las apps portadas.

---

## 21. Las constantes viejas del dialogo: texto tapado por los botones

**Sintoma** (reportado por el usuario mirando una captura del registro): "me parece que hay texto
tapado por los botones de abajo" — la linea de estado de la app de BT ("no headphones, RX off" y
"Press a row to pair, connect or disconnect") quedaba media tapada por la fila de botones.

**Medido.** La pantalla tiene 480 de alto, la fila de botones empieza en `480 - 62 = 418`
(`buttons.cpp`: `btn_height = 62`) y `dialog_style` pone la caja del dialogo en y=66 con **348** de
alto (`styles.c`) → 66 + 348 = **414**, 4 px arriba de los botones. Pero las constantes con las que
los dialogos calculan "lo de abajo" eran otras dos:

| Constante | Valor | Deberia ser | Consecuencia |
|---|---|---|---|
| `DIALOG_H` | 372 | 348 | lo apoyado en `DIALOG_H - X` cae **24 px** abajo |
| `DIALOG_Y` | 108 | 66 | lo alineado con `DIALOG_Y` cae **42 px** abajo |

`372` viene del comentario *"480 - top bar - buttons, as the ported apps expect"*: calculado cuando
los botones median 42 px, no 62. Y `DIALOG_Y` decia "below the top bar" cuando la barra terminaba
en 108; ahora termina en 66.

**A quien le pegaba** (cada uno con su sintoma):

- `dialog_bt.c` (el reportado): la tabla y las dos lineas de estado con `DIALOG_H - STATUS_H - 4` →
  tapadas por los botones.
- `dialog_wefax.c`: se dimensiona con `DIALOG_H` **y** se alinea con `DIALOG_Y` → 108 + 372 = 480:
  la caja se comia la fila entera (en la captura se leian "Clear" y "Close" **por detras** del
  previsualizador).
- `dialog_sstv.c`: igual, `lv_obj_set_size(dialog.obj, 795, DIALOG_H)`.
- `dialog_cluster.c` y `dialog_cw_kbd.c`: usan `DIALOG_H` para lo de abajo; medidos con captura sus
  textos **se ven**, pero por pocos pixeles (al anexo A).

**La trampa ya se habia pisado**: `freq_list.c:159` tiene un workaround con este comentario, en vez
de arreglar la constante: *"inside an app's dialog (SSTV, NAVTEX), already at DIALOG_Y, it went down
twice and its bottom under the buttons"*.

**Arreglado** con la geometria real en un solo lugar:

- `DIALOG_BOX_H` = **348** (nueva, el alto real de la caja), usada por `styles.c` en `dialog_style`
  (asi el estilo y la constante no pueden divergir) y por los dialogos que se apoyan abajo:
  `dialog_bt.c`, `dialog_sstv.c` y `dialog_wefax.c`.
- `DIALOG_Y` = **66** (era 108).
- `DIALOG_H` (372) se deja como estaba para el **marco de apps**, que tiene su propia geometria y
  medido **no** tiene el problema: GPS, SSTV y CW muestran su linea de abajo completa.
- `DIALOG_H_MAX` (372) queda como maximo para los buffers estaticos de SSTV/WeFax (sobra, no molesta).

**Verificado con capturas del framebuffer** (`evidencia-radio/`): el estado del BT completo arriba
de los botones (`bt-app.png`), y WeFax (`_wefax-final.png`) y SSTV (`_sstv-final.png`) con su linea
de abajo visible y la fila de botones libre. Antes/despues de la misma franja en `verifica-abajo.png`.
