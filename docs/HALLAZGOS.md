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
daemon y /dev/shm, todo listo en S11"; que los 3,81 s de scripts sean "udev, `create_data`,
`tune_cache` y el daemon de radio" (no se midió script por script).

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

1. **Scripts de arranque**: 1,4-3,8 s entre `kernel listo` y `GUI arrancando` (medido en varios
   arranques). No está desglosado por script: falta una marca por script.
2. **Replay del journal ext4**: 0,5-1,3 s medidos, sólo cuando el apagado anterior fue sucio (§11).
   La decisión pendiente es el rootfs read-only.
3. **`GUI arrancando → GUI inicio` (daemon + enlace dinámico)**: 0,76-1,06 s medidos, sin desglosar.
   Dentro, medido: copia de assets 0,24 s (ya reemplazada por el bind mount) y lectura del binario
   de 11,5 MB desde la raíz 0,467 s.
4. **Pantalla de arranque** (logo + progreso): no baja el tiempo, mejora lo que se percibe.
5. **Auriculares Bluetooth**: candidato para que se oiga la voz; **no probado** (§1).
6. **Features que faltan de la migración**: app de canales/memorias, app de modos, `broadcast_db`,
   `freq_scale`, `sstv_resources`, waterfall fijo/centrado y Snake. El inventario del que sale esta
   lista no está respaldado por un diff contra la base: al anexo A.

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
  (`2.659 → 4.001`) después del apagado real, con más cambios acumulados.

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
- **Scripts: 1,4-3,8 s** entre `kernel listo` y `GUI arrancando` (medido en varios arranques).
- **`GUI arrancando → GUI inicio`: 0,76-1,06 s** medidos, **sin desglosar** entre el daemon y el
  enlace dinámico. Medido aparte: leer el binario (11.538.740 bytes, **no** stripped —`file`— y
  **28** bibliotecas `NEEDED` —`readelf -d`—) desde la raíz cuesta 0,467 s con la caché vacía, y la
  copia de assets 0,24 s (§7).
- **GUI: 1,0-1,4 s** (ya con el arreglo de §10). `main()` hasta `audio_init` es 0,02 s. Tramos
  medidos: `cw/rtty` 0,16-0,21 s, `radio` 0,21 s, `cat/qso_log` 0,07 s, `db_init` 0,15-0,19 s,
  `fuentes y estilos` 0,04-0,09 s, `pantalla principal` 0,03-0,09 s (una vez 0,55 s: ruido, ver §7).

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
| "Bluetooth A2DP es el único camino audible" | No se probó (los auriculares estaban descargados) | Conectar auriculares BT y medir |
| El primer intento de `S02update` se cortó por un corte de energía | Es relato del usuario; no hay log | Log del intento / estado del equipo |
| "La raíz quedó idéntica, verificada" tras el intento fallido | Se dice verificado pero no quedó el md5/diff | Guardar los md5 de la raíz antes/después |
| El "~10 minutos" de espera del tgz | No se cronometró | Cronometrar una actualización por tgz |
| El botón de power "apaga por software y vuelve a correr el userspace" | No se verificó en el driver/script | Leer el driver/script del power |
| El "6 esperas" de `S48sntp` y que "todo está listo en S11" | No se leyó el script ni se listó el arranque de scripts | Leer `S48sntp` y medir una marca por script |
| "Los 3,81 s de scripts son udev, `create_data`, `tune_cache` y el daemon" | Atribución sin medir cada script | Marca de tiempo por script |
| El inventario de features que faltan de la migración | No hay diff contra la base | Diff/`grep` contra `.x6100_ref` |
| "TX Cal / CESSB / filtro TX son imposibles: el baseband no los expone" | Afirmación de hardware sin fuente | Documentación del baseband/SoC |
| "La app Android X6100 Link nunca se publicó" | Sin fuente | Buscar la publicación / confirmar con el autor |
| "El banco da 0,15 s por los `open()` del sistema de archivos del host" | La comparación es medida; **la explicación** no | Perfilar el banco (QEMU) |
| Que `addgroup` de busybox cree grupos en vez de agregar usuarios | Se observó en su momento, sin captura conservada | Repetirlo y pegar la salida |
| `speaker-test` "corre sin error y sin sonido" (12/10) | No quedó la captura y hoy no se puede repetir con la GUI andando | Parar la GUI, correrlo y pegar la salida |

**Estimaciones explícitas** (dichas como tales, no son hallazgos): el "~20 s" del banco rápido y el
"~3 s" del diagnóstico de voz no se cronometraron con precisión; y el "~1 s" de daemon + enlace
dinámico es en realidad el tramo medido 0,76-1,06 s sin desglosar.
