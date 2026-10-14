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
22. [Aceleracion grafica: la GPU esta, no se usa](#22-aceleracion-grafica-la-gpu-esta-no-se-usa)
23. [Bluetooth completo: desemparejar, auto-connect, volumen y mute](#23-bluetooth-completo-desemparejar-auto-connect-volumen-y-mute)
24. [WCAG 2.1 en todos los temas, con chequeo automatico](#24-wcag-21-en-todos-los-temas-con-chequeo-automatico)
25. [BT de punta a punta, con auriculares de verdad](#25-bt-de-punta-a-punta-con-auriculares-de-verdad)
26. [Contraste en la lista de BT: el rojo que pinta LVGL](#26-contraste-en-la-lista-de-bt-el-rojo-que-pinta-lvgl)
27. [Barrido WCAG de toda la UI: colores que pinta LVGL y colores a mano](#27-barrido-wcag-de-toda-la-ui-colores-que-pinta-lvgl-y-colores-a-mano)
28. [WCAG: barra de estado, switch, slider y about](#28-wcag-barra-de-estado-switch-slider-y-about)
29. [Todas las apps en el menu de apps](#29-todas-las-apps-en-el-menu-de-apps)
30. [Verificacion en el banco (QEMU): paginas APP y WCAG](#30-verificacion-en-el-banco-qemu-paginas-app-y-wcag)
31. [Ajustes con tabs: los botones de abajo ahora llevan a cada seccion](#31-ajustes-con-tabs-los-botones-de-abajo-ahora-llevan-a-cada-seccion)
32. [Algunas apps matan la GUI (y el equipo parece reiniciarse)](#32-algunas-apps-matan-la-gui-y-el-equipo-parece-reiniciarse)
33. [Todos los botones de las apps tienen handler](#33-todos-los-botones-de-las-apps-tienen-handler)

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
  `evidencia/captura-radio-ultima.png`). Despues: etiquetas completas, `Day, Month, Year` con
  **2026** entero y sin barra horizontal (`evidencia/ajustes-100pct.png`).

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
(`evidencia/bt-estados.png`):

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
`evidencia/bt-app.png`: "Living Room TV" con `headphones` y `new`). No aparecian los
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
momentos en `evidencia/scan-affordance.png`: reposo (`Scan`) → en curso (`Scan 13s` gris) →
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

**Verificado con capturas del framebuffer** (`evidencia/`): el estado del BT completo arriba
de los botones (`bt-app.png`), y WeFax (`evidencia/wefax-final.png`) y SSTV (`evidencia/sstv-final.png`) con su linea
de abajo visible y la fila de botones libre. Antes/despues de la misma franja en `verifica-abajo.png`.

---

## 22. Aceleracion grafica: la GPU esta, no se usa

**Preguntado el 13/10**: "tenemos aceleracion grafica activa en la radio?"

**Respuesta: no. La GPU existe y el kernel la tiene andando, pero nadie la usa: la GUI dibuja por
software.** Todo medido en la radio:

| Que | Como se ve |
|---|---|
| GPU presente | `lima 1c40000.gpu: gp - mali400 version major 1 minor 1` (kernel, driver **lima**) |
| Nodos DRM | `/dev/dri/card0`, `card1`, `renderD128`; `platform-1c40000.gpu-card` y `gpu-render` |
| Nadie la usa | `fuser /dev/dri/card0` y `/dev/dri/renderD128` → **vacio**; el que tiene `/dev/fb0` es la GUI |
| Sin userspace | `ls /usr/lib \| grep -iE "egl\|gles\|gbm\|mali"` → nada; el proceso de la GUI no mapea libEGL/libGLES/libmali (0 coincidencias en `/proc/<pid>/maps`) |
| LVGL por software | `lv_conf.h`: `LV_USE_GPU_ARM2D 0`, `LV_USE_GPU_STM32_DMA2D 0`, `LV_USE_GPU_NXP_PXP 0`, …; y `LV_COLOR_DEPTH 32` |
| Camino de dibujo | `screen_rec_flush()` copia con `memcpy` el buffer de LVGL y `screen_rec_out()` lo lleva al framebuffer |

Lo unico que corre en hardware es el **controlador de display**: `/proc/fb` = `sun4i-drmdrmfb`
(`sun4i-drm` + display engine), o sea el *scanout* (mostrar el cuadro ya dibujado) no cuesta CPU,
pero **dibujarlo** si:

```
PID     USER  STAT  VSZ    %CPU  COMMAND
5993    root  R     246m   20-25%  /usr/sbin/x6200_gui      (3 muestras, waterfall corriendo)
```

Un 20-25% de **un** core (la radio tiene 4, `grep -c processor /proc/cpuinfo` = 4), con el waterfall
en pantalla. No esta medido cuanto de eso es el waterfall y cuanto el resto de la GUI: al anexo A.

**Para tener aceleracion de verdad** haria falta, ademas de la GPU que ya esta: Mesa con el driver
**lima** + EGL/GBM en el rootfs, y un renderer que use GLES. LVGL 8 -el que usa este port- **no**
tiene backend GLES (LVGL 9 tiene `lv_opengles`), asi que no es una opcion de compilacion: es un
cambio de arquitectura de dibujo. Sin fundar: al anexo A.

---

## 23. Bluetooth completo: desemparejar, auto-connect, volumen y mute

**Pedido**: "implementalo y termina completamente todo lo relacionado a BT".

La app tenia scan, list, pair, connect, disconnect, RX to HP y Speaker. Faltaba lo que el script
(`/usr/sbin/btaudio-ctl`) ya sabia hacer y nadie llamaba. Los cuatro huecos, y que se hizo:

| Hueco | Comando del script que no se usaba | Hecho |
|---|---|---|
| **Desemparejar** (elegir "a cual no") | `remove MAC` | boton **Auto** con **hold** = unpair (lo destructivo detras de un long press) |
| **Auto-connect** por dispositivo | `autoconnect MAC on\|off` | boton **Auto**: pulsacion prende/apaga; el estado ya se parseaba (`DEV <mac> <connected> <role> <autoconnect> <name>`) |
| **Volumen de los auriculares** | `volume N` | la perilla de volumen ahora tambien mueve el sink A2DP cuando hay auriculares |
| **Silenciar el altavoz** | `mute on\|off` | se escribia el flag y **no pasaba nada**: ahora llama al mute del equipo (`radio_change_mute`) |

**Y un bug propio, encontrado al verificar**: abrir la app **se llevaba la GUI puesta**. La causa:
`auto_label()` (la etiqueta del boton nuevo) llama a `selected_device()` y el marco de botones pide
esa etiqueta **antes** de que exista la tabla → `lv_table_get_selected_cell(NULL, ...)`. Se veia
como "la app no abre y vuelve el home" (medido: el PID de `x6200_gui` cambiaba y `dmesg` con
segfault). Arreglado con un `if (table == NULL) return NULL;` y verificado: PID igual antes y
despues de abrir, `dmesg` con 0 segfaults.

**Como se verifico** (sin auriculares de verdad, con los archivos de estado que escribe el script):

- **Volumen, A/B**: sin `/tmp/btaudio-hp` (sin auriculares) cambiar el volumen **no** llama al
  script; con el archivo presente, el log muestra `btaudio-ctl volume 55`. Es exactamente el A/B.
- **Mute**: apretar el boton deja en `/mnt/app_logs/bt.log`
  `ui: btaudio-ctl mute on` + `altavoz apagado (mute del equipo)`.
- **Auto sin dispositivo elegido**: no manda nada y avisa "Pick a paired device first".
- Capturas de los botones y la nota: `evidencia/bt-botones.png`.

**Para poder medir todo esto** se agrego `/mnt/app_logs/bt.log` (una linea por accion, como
`boot.log` y `voice.log`) y dos comandos al control remoto: `{"cmd":"btn","val":N}` (aprieta el
boton N de la fila de abajo, por el mismo camino que las teclas F) y `{"cmd":"vol","val":N}` (pasa
por `radio_change_vol`, o sea por el mismo camino que la perilla).

**Queda por probar con auriculares de verdad** (necesita hardware): parear, conectar, el passkey,
el auto-connect y que el volumen suene. Lo que se puede fundar sin ellos esta fundado.

---

## 24. WCAG 2.1 en todos los temas, con chequeo automatico

**Pedido**: "todos los colores deben ser WCAG 2.x" (y AGENTS.md ya decia "Use WCAG coloring", sin
que nadie lo verificara).

**Chequeo nuevo**: `tests/check_wcag.py` (enganchado en `tests/run_all_tests.sh`), Python 3 solo
stdlib, con la formula oficial WCAG 2.1 (luminancia relativa sRGB y `(L1+0.05)/(L2+0.05)`). Lee las
**4 paletas built-in** de `src/theme.h` (y valida que cada rol tenga sus 4 hex), los **temas JSON**
de la imagen y reconstruye VISION y Outdoor-Negative como los arma `theme.c`. **115 pares
fg-sobre-bg** (hoy 117), cada uno con el `archivo:linea` de donde sale; umbrales **4.5** (texto
normal), **3.0** (texto grande y no-texto, WCAG 1.4.11) y las exenciones de 1.4.3 (deshabilitados)
informadas pero no fatales.

**Lo que encontro**: **65 fallas en 9 temas**. Las peores: `title_text` sobre `list` **1.51:1**
(IU1WXY, Legacy), `card` sobre `bg` **1.45:1** (Legacy), `btn_border` sobre `btn` **1.16:1** (Holo),
`btn_active_border` sobre `btn_active` **1.00:1** (VISION), `bar_border` sobre `bar` **2.25:1**
(Classic). En el tema que usa la radio (**Material 3**) habia 1: `title_text`/`list` 1.59.

**Arreglado** ajustando **solo la luminosidad** (mismo tono, HLS) y con margen chico sobre el
umbral: 22 colores en `src/theme.h`, 5 en los overrides de `src/theme.c` (VISION y
Outdoor-Negative), 6 en `Holo.json` y 9 en `RS/theme.json`. Los ratios vinculantes quedaron entre
3.18 y 4.82 (los cambios estan listados con el hex viejo y el nuevo en el commit). `Material 3` no
necesito cambios propios: **hereda** el `title_text` arreglado de IU1WXY.

**Un caso que el chequeo no podia ver y se arreglo a mano**: `dialog_field.c` pintaba la fila
seleccionada de los listados con **negro fijo** sobre `TC(ok)`/`TC(warn)` → 3.45:1 y 3.84:1 en
Outdoor. Ahora usa `TC(panel)` (el color de fondo del tema: negro en los oscuros, blanco en el
claro) y se agregaron los dos pares al chequeo (paso de 115 a 117 pares).

**Verificado**: `tests/check_wcag.py` → **0 fallas** (9 temas, 117 pares, 808 OK, 144 exentos);
`check_actions.py` y `check_settings_focus.py` siguen OK; los 3 JSON parsean. Y en la radio: el
tema activo es **"Material 3"**, que es un **JSON en `/mnt/themes/`** (la particion de datos
sobrevive a reflashear), asi que ademas de reconstruir la imagen se copiaron los JSON arreglados a
`/mnt/themes/`. Antes/despues de las pantallas en `evidencia/comparacion-antes.png` y
`evidencia/comparacion-despues.png`: el tema queda igual, con los bordes y los textos secundarios mas claros.

**Exenciones** (WCAG 1.4.3, deshabilitados; se miden y se informan): `track` 1.00, `icon_off` 1.04,
`off` 1.09, `btn_off_text` 1.20, `text_mute` 1.73, `text_faint` 1.84, `switch_off` 2.81.

---

## 25. BT de punta a punta, con auriculares de verdad

**Probado el 13/10 con los auriculares del usuario** (Pixel Buds Pro 2, `B8:7B:D4:17:C7:F3`),
monitoreando la radio cada 6 s mientras el usuario probaba. La secuencia y lo que salio de cada
paso:

| Hora | Que paso | Evidencia |
|---|---|---|
| 22:34:37 | Scan: aparecen el TV y **los Buds** | `/tmp/bt_ui_scan.txt` con las dos lineas |
| 22:35:06 | Toca la fila de los Buds (pair) | `/mnt/app_logs/bt.log`: `ui: btaudio-ctl pair B8:...` |
| 22:35-22:36 | **El pareo falla** | `/tmp/bt_pair.log`: `Device ... not available` |
| 22:41:58 | **Conectados** | sink `bluez_sink.B8_7B_D4_17_C7_F3.a2dp_sink`, `loop=1` |
| 22:42:17-29 | El usuario mueve la perilla | `btaudio-ctl volume 50`, `43`, `2` en el log |

**Tres defectos que solo se ven probando de verdad** (los tres arreglados):

1. **El pareo fallaba por una espera mal escrita**: `pair()` esperaba "la MAC en el log", y bluez
   tambien escribe la MAC cuando **borra** el device del cache (`[DEL] Device <mac>`), asi que
   mandaba el `pair` antes de tiempo y contestaba `not available`. Ahora espera una aparicion
   **positiva** (`NEW`/`CHG` con esa MAC) en **ese** scan y, mientras el equipo siga apareciendo,
   **reintenta** el pareo hasta 3 veces. Si nunca aparece, falla limpio: medido, **84 s → 51 s**.
2. **La app no decia nada si el pareo fallaba**: quedaba "Pairing..." para siempre. Ahora dice
   `Pairing failed: put it in pairing mode and retry` o `Paired: use RX to HP to hear it`.
3. **El rol estaba mal**: los auriculares con controles tactiles anuncian **HID (HOGP) ademas de
   A2DP**, y `role()` miraba HID primero, asi que los Buds salian como `kbd` (la app los mostraba
   como "keyboard" y `list` decia `HP 0`). Ahora el audio se mira antes que HID: `DEV <mac> 1 hp 0
   Pablo's Pixel Buds Pro 2`.

**Lo que quedo verificado con hardware real**: A2DP conectado, **loopback de RX a los auriculares
cargado** (`loop=1`), y **la perilla de volumen moviendo el sink de los Buds** (los `volume 50/43/2`
del log son del usuario girando la perilla, no de una prueba).

**El mute del altavoz con auriculares** (lo que pide `BTAUDIO_MUTE="yes"`) se arreglo y se verifico
en A/B con el archivo de estado que escribe el propio script (`/tmp/btaudio-hp`):

```
22-45-26 auriculares conectados: altavoz a 0 (BTAUDIO_MUTE=yes)
22-45-30 auriculares desconectados: altavoz de vuelta
```

Y solo desmutea si el mute lo puso esto (no pisa un mute puesto a mano).

4. **Dos loopbacks al mismo sink** (el audio de recepcion salia **al doble**): `sync_audio` se llama
   desde el manejador de `pactl subscribe` **y** desde el monitor del script, y si los dos corren a
   la vez los dos ven "no hay loopback" y los dos lo cargan. Medido: modulos **21 y 22 identicos**
   (`source=alsa_input... sink=bluez_sink.B8_7B...`). Arreglado con un lock de `mkdir` (atomico) en
   `sync_audio` y una red de seguridad en `apply_rx` que deja **un solo loopback por sink**.
   Verificado: `pactl list short modules | grep -c module-loopback` = **1**.

**Estado con los auriculares del usuario conectados** (`btaudio-ctl list`):

```
DEV  B8:7B:D4:17:C7:F3  1  hp  0  Pablo's Pixel Buds Pro 2     <- conectado, rol corregido
HP   1  50                                                     <- 1 auricular, volumen 50%
RX   yes                                                       <- recepcion a los auriculares
MUTE yes                                                       <- altavoz en 0
```

Ojo con el volumen: quedaba en **2%** (−97 dB, inaudible) de las pruebas con la perilla; se dejo en
50% para poder escuchar.

**Nota de proceso** (me paso y se ve en los md5): un `make` de imagen **sin** `make
x6200-gui-rebuild` antes puede reusar el paquete ya instalado y dejar en la imagen el archivo
**viejo** de `x6200_gui/rootfs/` (paso con `btaudio-ctl`: la imagen quedo con la version sin el
lock y la radio con la nueva). El chequeo que lo detecta es comparar el contenido:
`md5sum /b/target/<archivo>` contra el del repo, no solo el `x6200_gui`. Ahora la imagen
`x6200_20261014_0400.img` tiene `btaudio-ctl faf89d7762e6` = el del repo, y la radio lo mismo.

**Queda para el oido del usuario**: que el audio de recepcion suene en los Buds (el camino esta
armado y medido: un loopback, el sink de ellos, 50%) y que el altavoz quede en silencio.

---

## 26. Contraste en la lista de BT: el rojo que pinta LVGL

**Reportado por el usuario**: "la UI de BT tiene poco contraste entre los elementos de la lista y el
texto (gris claro sobre blanco) eso no es WCAG".

**Medido en la radio** (pixel a pixel de una captura del framebuffer, `evidencia/bt-contraste.png`): en la lista de dispositivos,
la fila normal daba **14.41:1** ✓ (texto `#e2e2e9` sobre el fondo del tema `#111318`), pero la
**fila elegida** daba **3.68:1** ✗: fondo **`#F44336`** con el texto casi blanco.

**De donde salia ese rojo**: `#F44336` **no existe en el repo** (se busco: no esta en `src/` ni en
los temas JSON). Lo pinta el **tema por defecto de LVGL**: `lv_table` dibuja la celda elegida con
`LV_STATE_FOCUSED` **falsificado solo para esa celda**
(`lvgl/src/widgets/lv_table.c:789`: `if(row == table->row_act && col == table->col_act) { ... if(obj->state & LV_STATE_FOCUSED) cell_state |= LV_STATE_FOCUSED; }`
y despues `obj->state = cell_state` para pedir el descriptor de dibujo), y ese estado se pinta con
el color de foco de LVGL, ignorando el tema de la aplicacion.

**Arreglado** pintando esa celda con el par del tema en los **cuatro** estados que puede tomar
(`LV_PART_ITEMS` con `FOCUSED`, `PRESSED`, `FOCUS_KEY` y `EDITED`) → `TC(sel_bg)` / `TC(sel_text)`.
Medido despues: **7.22:1** ✓ (fondo `#284777`, texto `#d6e3ff`), y la fila normal sigue en 14.41:1 ✓.
Ademas las celdas normales pasaron a fondo transparente (`LV_OPA_TRANSP`), asi que se ve el fondo de
la tabla (el problema original: LVGL pintaba las celdas con su fondo claro y el texto del tema
quedaba casi blanco sobre blanco).

**La leccion general** (y por que el chequeo no lo veia): `tests/check_wcag.py` validaba pares
**rol-sobre-rol del tema**, y este caso no es eso: el fondo lo pone LVGL por defecto y el texto el
tema. Es la misma clase que el negro fijo de `dialog_field.c` (§24). Queda pedido y en curso un
barrido de toda la UI para esa clase (colores de LVGL por defecto + colores a mano en el codigo).

---

## 27. Barrido WCAG de toda la UI: colores que pinta LVGL y colores a mano

**Pedido**: "revisa toda la UI de la radio y ajusta lo que haga falta para que sea WCAG con
contraste". Es la continuacion de §26: el chequeo validaba pares **rol-sobre-rol del tema**, y hay
dos clases que no ve.

**Clase 1 - widgets que LVGL pinta con SUS colores.** Mecanismo confirmado en
`lvgl/src/widgets/lv_table.c:786-793` y `lvgl/src/widgets/lv_btnmatrix.c:749-780`: el estado se
**falsifica solo para la celda/tecla activa** y el descriptor de dibujo se reconstruye desde el tema
por defecto (colores compilados en `lv_hal_disp.c:216`: `lv_palette_main(BLUE/RED)`), pisando los
`TC()` del proyecto. Pares que fallaban (medidos):

| archivo | fg | bg real | ratio |
|---|---|---|---|
| `dialog_msg_cw.c:173` | `#ffffff` | `#ffffff` (items de LVGL) | **1.00** |
| `dialog_msg_voice.c:474` | `#ffffff` | `#ffffff` | **1.00** |
| `dialog_recorder.c:282` | `#ffffff` | `#ffffff` | **1.00** |
| celda FOCUSED de `lv_table` | `TC(text)` `#e2e2e9` | `#F44336` compilado | **2.86** |
| celda FOCUS_KEY | `#ffffff` | `#2196F3` compilado | **2.42-3.12** |
| `dialog_ft8.c:816` | `#C0C0C0` | celdas al 50% | **2.33** |
| `textarea_window.c:156` | `#2196F3` | `#2196F3` al 20% | **3.50** |
| `dialog_settings.c:256,1908` | `#ffffff` | `#ffffff` (Outdoor) | **1.00** |

**Clase 2 - colores a mano en el codigo**: `dialog_field.c:1098` (`lv_color_black()` sobre
`TC(bg)`, **1.00** en Classic), `band_plan.c:93` (blanco sobre franjas literales `#33AA55`/`#CC8800`,
**2.96**), `cw_tune_ui.c:45` (bloque activo `#FF8080` sobre el mismo brillo, **1.65**).

**Arreglado** (usando siempre roles del tema, para que valga en los 9 temas):

| Que | Antes | Despues |
|---|---|---|
| Las 3 tablas blancas (msg_cw, msg_voice, recorder) | 1.00 | **16.20-21.00** |
| Las 11 tablas: celda elegida (FOCUS/FOCUS_KEY/EDITED) | 2.42-3.68 | **7.22** |
| FT8 (texto y fondo de celdas) | 2.33-3.57 | **6.50** |
| Teclado (textarea con foco) | 3.50 | **21.00** |
| Chip VOL/MFK (`dialog_field.c`) | 1.00 | **21.00** |
| Band plan (etiqueta elegida por luminancia WCAG) | 2.96 | **7.02-7.09** |
| CW tune (borde por luminancia sobre el relleno) | 1.65 | **8.65** |
| Ajustes (`lv_color_white()` → `TC(text)`) | 1.00 | **21.00** |

**Verificado**: compila sin errores ni warnings; `check_wcag.py` → **0 fallas** (9 temas, **882
pares OK**, 144 exentos, 72 informativos); `check_actions.py`, `check_settings_focus.py` y los tests
nativos (538) siguen OK. Y en la radio: las **12 pantallas** (home, Ajustes, BT, GPS, SSTV, WeFax,
CW, FT8, JS8, Recorder, WiFi, Bands) siguen dibujando bien, sin widgets rotos (`evidencia/`).

**Lo que quedo SIN arreglar, con numeros** (hace falta una decision de diseño, no un `TC()`):

- **Switch de Ajustes**: el riel lo pinta LVGL (`#E0E0E0`) y sobre el panel da **2.81** en Outdoor y
  **1.31** en Holo; el rol `switch_off` tampoco cierra (2.81). El knob blanco sobre el indicador
  azul: **1.72** en Material 3. Ningun rol existente cierra en los 9 temas (VISION 20.00 vs Holo
  1.31), asi que hace falta un rol nuevo para el riel/knob.
- **Slider (knob)**: LVGL lo pinta con el mismo `color_primary` que el indicador → **1.03** en
  Material 3, 1.00 en IU1WXY: el knob no se distingue del relleno.
- **Fondo de la barra de estado y de `dialog_about`**: es un **bitmap opaco**
  (`images/top_short*.bin`), y los `.bin` no cierran con ninguna geometria/CF de LVGL, asi que los
  `#909090`/`#AAAAAA` de `info.c` y `dialog_about.c` **no tienen ratio fundado** (a la espera de
  decodificar el bitmap, o de medirlo por muestreo de la captura).
- `lv_label` en dialogos sin `remove_style_all`: el tema pinta `card` blanco con texto `#212121`;
  el proyecto casi siempre borra el estilo, pero no se verifico en runtime.

---

## 28. WCAG: barra de estado, switch, slider y about

Cierra lo que §27 dejo pendiente por necesitar una decision de diseño. Todo medido, y las tres cosas
que quedaban sin arreglar ahora cumplen.

**Lo que se midio primero, en capturas de la radio** (pixel a pixel):

| Elemento | Medido | |
|---|---|---|
| Info box: gris `#99A0A7` sobre las partes **blancas** del bitmap | **2.3:1** | ✗ |
| Info box: rojo `#FF8080` sobre las partes blancas | **2.0:1** | ✗ |
| Switch: riel de LVGL (`#E0E0E0`) contra el dialogo | **2.78:1** | ✗ |
| Switch: perilla contra el riel | 3.12:1 | ✓ |
| Slider: perilla contra el fondo | 7.47:1 | ✓ |

El problema de fondo de la barra de estado: su fondo es un **bitmap** (`images/top_short*.bin`) con
zonas claras y oscuras, asi que **ningun color de texto fijo** puede cumplir contra las dos.

**Arreglado**:

1. **Barra de estado** (`info.c`): cada item pinta ahora su celda con `TC(bar)` (el rol que el tema
   define justamente para "top bar boxes: info, S-meter, clock") y usa roles para el texto
   (`TC(text)`, `TC(text_dim)` para WiFi/BT y ATU apagado, `TC(text_mute)` para el modo bloqueado,
   `TC(mode_dig)` para los modos digitales). El bitmap deja de importar: los pares quedan validados
   por el chequeo en los 9 temas.
2. **Switch y slider** (`styles.c`: `styles_switch_init()` / `styles_slider_init()`, aplicados en los
   7 lugares de `dialog_settings.c`): la **pista** usa `TC(control_border)`, la **perilla**
   `TC(panel)`, y **no hay relleno**: el estado lo indica la **posicion de la perilla**, que es como
   WCAG evalua un switch (1.4.11 pide identificar el componente y su estado; la posicion es
   informacion visual con contraste ≥3 contra la pista). Antes las tres partes las pintaba el tema
   de LVGL.
   Los valores de `control_border` se **calcularon** por tema (el gris mas discreto que da ≥3.1
   contra el dialogo y el panel): IU1WXY `#b5b5b5`, Legacy `#aeaeae`, Outdoor `#929292`, Classic
   `#626262`, Holo `#626262`, y RS `#6e6e6e` (**clave nueva** en su JSON: no la definia y heredaba
   una que no cerraba contra su dialogo, 2.58:1).
   Se probo primero un diseño con relleno (`text` sobre la pista) y **ningun gris lo cerraba** en
   IU1WXY ni Legacy: el relleno blanco obliga a una pista clara y la pista clara no llega a 3:1
   contra el blanco. Sacar el relleno es lo que hace que haya solucion en los 9 temas.
3. **`dialog_about`**: `lv_color_white()` → `TC(text)`, `#AAAAAA` → `TC(text_dim)`, y el
   "73 de LU2DSX" (que es **texto**, no un icono: 4.5) → `TC(text_head)`, que el chequeo ya validaba
   contra `dialog`. El corazon `#FF4444` daba 2.15:1 en IU1WXY.

**Verificado**: `check_wcag.py` → **0 fallas** (9 temas, **963 pares OK**, 153 exentos, 72
informativos); compila sin errores ni warnings; `check_actions.py`, `check_settings_focus.py` y los
tests nativos (538) siguen OK.

**Pendiente de ver en pantalla**: la radio quedo en **otra subred** (la laptop esta en
192.168.18.0/24 y la radio en 192.168.0.x: no responde al barrido ni por MAC), asi que las capturas
de antes/despues de estos cuatro cambios y el despliegue de la imagen quedan para cuando vuelva a la
red.

---

## 29. Todas las apps en el menu de apps

**Reportado**: "revisa que todas las app esten en el menu de app, por ejemplo la de Field Ops no la
veo".

**Lo que habia**: el port tiene **23 apps** (`case ACTION_APP_*` en `main_screen_start_app`) y solo
**9** tenian boton en las paginas APP (`page_app_1` = RTTY, FT8, SWR Scan; `page_app_2` = Recorder,
QTH, Callsign, Settings; `page_app_3` = WiFi, BT). Las otras **14** —Field Ops, GPS, JS8, WSPR, SSTV,
WeFax, NavTex, Cluster, CW Keyboard, CW Trainer, Status, Bands, Eq y **About**— solo se alcanzaban
asignandolas a un **long press** desde Ajustes ("APP long press"), que es un camino escondido: de
ahi la sensacion de que la app no existe. (About ni siquiera estaba en esa lista: 22 de 23.)

**Arreglado**: las paginas APP pasaron de 3 a **6**, agrupadas por uso, y **las 23 apps** tienen
boton. El boton de pagina ocupa un lugar de los 5 (`#define BUTTONS 5`), asi que entran 4 apps por
pagina:

| Pagina | Apps |
|---|---|
| **APP 1:6** | FT8, JS8, WSPR, RTTY (modos digitales) |
| **APP 2:6** | SSTV, WeFax, NavTex, Cluster (imagen y avisos) |
| **APP 3:6** | Recorder, **Field Ops**, GPS, Bands (operacion de campo) |
| **APP 4:6** | CW Kbd, CW Trainer, Eq, Status (herramientas) |
| **APP 5:6** | Settings, Callsign, QTH, About (configuracion) |
| **APP 6:6** | WiFi, BT, SWR Scan (radio y red) |

El rotulo del boton de pagina paso a `(APP n:6)` y el grupo `buttons_group_app` tiene las 6 (el
ciclo lo hace solo: `(j + 1) % groups[i].size`). La lista del long press queda como estaba: es un
camino extra, no el unico.

**Chequeo nuevo**: `tests/check_apps_menu.py` (en `tests/run_all_tests.sh`) lee los `case
ACTION_APP_*` de `main_screen.c`, lee los cuerpos de las `page_app_N` de `buttons.cpp`, resuelve cada
`&btn_x` a la accion de su `make_app_btn`/`make_action_btn` y **falla si alguna app no esta en
ninguna pagina** (tambien avisa si una pagina se pasa de `BUTTONS` y si una app aparece dos veces).
Hoy: **23 apps, 23 en el menu, 6 paginas**.

---

## 30. Verificacion en el banco (QEMU): paginas APP y WCAG

Con la radio en otra subred (§28), el trabajo se verifico en el **banco**: chroot de la imagen +
**QEMU user** + framebuffer falso (`libfakefb` deja cada cuadro en `/tmp/fakefb0`, 480x800x32 BGRA,
rotado 270° como en la radio). Herramienta nueva: **`tools/captura-banco.sh`** (arranca la GUI en el
banco, la maneja por su socket de control y saca el PNG; deja la GUI corriendo entre llamadas para
encadenar capturas).

Para manejarla sin teclas se agrego al control remoto el comando **`{"cmd":"page","text":"app"}`**
(gen|app|key|dfn|dfl): llama al mismo `buttons_load_page_group()` que la tecla APP (`main_screen.c:700`).

**Verificado**:

- **Las 6 paginas APP**, con las 23 apps y los rotulos entrando bien:
  `APP 1:6` FT8/JS8/WSPR/RTTY · `APP 2:6` SSTV/WeFax/NavTex/Cluster · `APP 3:6` Recorder/**Field
  Ops**/GPS/Bands · `APP 4:6` CW Kbd/CW Trainer/Eq/Status · `APP 5:6` Settings/Callsign/QTH/**About**
  · `APP 6:6` WiFi/BT/SWR Scan (capturas en `evidencia-banco/paginas-app.png`).
- **El look WCAG**: celdas de la barra de estado con `TC(bar)` y texto de roles, el "PRE" resaltado
  con el par `sel_bg`/`sel_text`, el "ATU1" sin cargar atenuado (`text_dim`), y el switch/slider con
  pista `control_border` + perilla `panel` y sin relleno (`evidencia-banco/final-barra.png`,
  `wcag-settings.png`).

**Dos cosas que solo se vieron en el banco** (y quedaron arregladas):

1. `att_pre_label_update` pintaba **negro sobre blanco al 50%** y `atu_label_update` dejaba el fondo
   en **blanco al 50%** con texto del tema: sobre el fondo del tema eso es texto claro sobre celda
   clara. La auditoria no lo podia ver porque el fondo era el bitmap. Ahora usan pares del tema
   (`sel_bg`/`sel_text` para el estado activo, `bar`+`text`/`text_dim` para el resto) y el chequeo
   los valida: **990 pares OK, 0 fallas**.
2. La barra de estado tenia todavia tres colores a mano (`#909090` de WiFi conectando, `#FF0000` del
   ATU y el blanco de WiFi conectado): ahora ninguno (`grep -c "lv_color_hex\|lv_color_white\|
   lv_color_black" src/info.c` = **0**).

**Hallazgos del banco que quedan anotados** (no bloquean, son del banco o del control remoto):

- La GUI en el banco **muere a los ~1-3 minutos** con **Bus error en `main_knob_thread`**
  (`rotary.c`, LR resuelto con `addr2line`: 0x27e58). Le pasa tambien al build anterior y no le pasa
  a la radio, que lleva horas andando: es del dispositivo de perilla falso del banco (acceso
  desalineado bajo QEMU). Para capturar alcanza con hacerlo dentro de esa ventana.
- La lista de apps del **control remoto** (`app_names[]`) no tiene About, Cluster, CW Trainer, Eq,
  Status, QTH, Callsign, Recorder ni WiFi: abrirlas por el socket responde "Unknown app". Es del
  protocolo remoto (la pagina /remote del webserver usa la misma lista), no de la GUI.

---

## 31. Ajustes con tabs: los botones de abajo ahora llevan a cada seccion

**Pedido**: "en la pantalla de settings los botones no son usados, quiero que agrupes los settings
por temas y que cada boton sea como un tab que se puede cambiar para ir mas agil a cada seccion".

**Lo que habia**: 25 filas (date/time, backlight, mag, s-meter, band plan, wfview, clock, las dos
acciones, audio gain/enh, voice, auto, waterfall x3, sp mode, freq accel, theme, bluetooth, cluster
column, font, transverter xN) en una lista que se recorria **solo con la perilla**: `dialog_settings`
no tenia `.btn_page`, asi que el marco de dialogos dejaba la fila de botones vacia.

**Arreglado**: 8 grupos por tema, cada uno **contiguo** (se reordenaron las llamadas para que lo
sean), con su tab:

| Tab | Que entra |
|---|---|
| **Reloj** | Day/Month/Year, Hour/Min/Sec, Clock view, Timeout Clock/Power/TX |
| **Pantalla** | Timeout/Brightness, Buttons brightness, S-meter needle, Band plan |
| **Info** | Mag Freq, Info, ALC |
| **Audio** | Line-in/out, audio gain, audio enhance, Speaker mode, Voice |
| **Espectro** | Spectrum auto min/max, Waterfall auto min/max, line/zoom, smooth scroll, battery |
| **Operacion** | Freq acceleration, Bluetooth, Discoverable, Cluster column, wfview, transverter |
| **Apariencia** | Theme, Font |
| **Acciones** | APP long press, MIC long press |

Son 8, y la fila admite 5 (`#define BUTTONS 5`), asi que van en **dos paginas de 4** con el boton de
pagina: `Reloj Pantalla Info Audio | Mas >` y `Espectro Operacion Apariencia Acciones | < Mas`
(`tab_mas_cb`/`tab_menos_cb` llaman a `buttons_load_page()`, que es publica).

Cada tab (`settings_goto(g)`) hace tres cosas:

1. Lleva el grupo **arriba** de la vista con `lv_obj_scroll_to_y(grid, lv_obj_get_y(primero))`:
   `lv_obj_scroll_to_view()` solo asegura que se vea, y con el grupo ya parcialmente visible el
   salto quedaba a medias (visto en la radio: la seccion empezaba a media pantalla).
2. Le da el foco a su **primer widget navegable**: los ajustes viven dentro de contenedores (la
   etiqueta y el campo), y al grupo de foco entra el campo, no el contenedor, asi que hay que
   bajar por el arbol (`primer_navegable()`). Sin esto la vista saltaba pero la perilla volvia al
   widget de antes.
3. Marca el tab activo con `buttons_mark()` (LV_STATE_CHECKED); los del otro lado quedan sin
   `label_obj` y la funcion los ignora.

**Dos cosas que aparecieron al verificar en la radio** (no en el banco, porque ahi no hay tema
Material 3):

1. El switch **prendido** salia con el azul `#2196f3` de LVGL, no con el color del tema: el tema
   de LVGL trae su propio estilo para `LV_STATE_CHECKED` del switch y, al ser **mas especifico**
   que un estilo local sin estado, ganaba. Ahora `styles_switch_init()` pisa los cuatro selectores
   (0, CHECKED, PRESSED, CHECKED|PRESSED) y el switch prendido mide `#8e9099` = `control_border`
   con la perilla `#111318` = `panel`, igual que el slider (medido sobre el framebuffer).
2. Dos de los cinco `lv_switch_create` de `dialog_settings.c` no llamaban a
   `styles_switch_init()` (los de S-meter needle y Band plan): quedaban con el estilo de LVGL. Ya
   estan los cinco.

**Verificado en la radio** (capturas en `evidencia-radio/tabs/`): los 8 tabs con su rotulo, el
marcado del activo, el salto de cada uno a su seccion arriba de todo y el foco en su primer campo;
`check_wcag.py` 990 pares / 0 fallas, `check_settings_focus.py`, `check_actions.py`,
`check_apps_menu.py` y los 538 tests nativos, todos OK.

---

## 32. Algunas apps matan la GUI (y el equipo parece reiniciarse)

**Reportado**: "hay algo con las apps, algunas al querer accederlas hacen reiniciar el equipo".

**Lo que pasa, medido**: abrir **CW Trainer** o **Status** mata el proceso de la GUI con
**SIGSEGV** (`rc=139`, medido instrumentando el supervisor para que registre el codigo de salida del
proceso). La GUI **no** reinicia el equipo: el uptime sigue subiendo. Lo que pasa es que la GUI la
supervisa `/usr/sbin/x6200_daemon`, que es un `while true` que la vuelve a lanzar en cuanto muere:

```sh
while true; do
    /usr/sbin/x6200_gui > /tmp/x6200_log.txt 2>&1 || sync
    mv /tmp/x6200_log.txt /mnt/app_logs/x6200_log_$(date ...).txt
done
```

Asi que la pantalla vuelve al logo en un segundo y **parece** un reinicio del equipo. Reproducido
7 de 7 veces con CW Trainer y 2 de 2 con Status; las otras 21 apps (incluidas las 9 que antes ni se
podian abrir) abren y cierran bien.

**Como se acoto**:

1. Barrido de las 23 apps por control remoto, con deteccion de reinicio por **PID de la GUI** y
   cantidad de logs (no alcanza con el uptime, que no baja): todas "ok" con 3 s de espera. Repitiendo
   con **20 s** de permanencia aparecen CW Trainer y Status.
2. El log del proceso muerto (el supervisor lo guarda en `/mnt/app_logs/`): el cambio de modo a CW y
   los filtros terminan bien (marcas propias A..F en `cfg/mode.c`, todas impresas) y **no hay
   ninguna linea mas**: el SIGSEGV es silencioso y ocurre despues, en la construccion de la UI de
   la app.
3. `dmesg` no deja mensaje y no habia core: el `core_pattern` era `core` (relativo) y el proceso
   corre con el directorio en `/`, pero al apuntarlo a `/mnt` el kernel aborta el volcado con
   "cannot preserve file permissions" (vfat). Apuntandolo a `/tmp` (tmpfs) **si** se obtuvo el core
   (210 MB): queda pendiente analizarlo con un gdb multiarquitectura (el toolchain de buildroot no
   trae gdb y el core es ARM).
4. Antes se descartaron, con datos: memoria (`oom_kill 0`, GUI de 20 MB), el handler de asserts de
   LVGL (`while(1)`, colgaria en vez de salir), los `exit(0)` del arbol (marcados con `fprintf` sin
   buffer: ninguna marca aparecio) y las librerias de instrumentacion del banco (`ld.so.preload`
   vacio y no estan en la radio).

**Herramienta nueva**: `tools/prueba-apps-radio.sh` abre las 23 apps o aprieta los 24 botones de las
paginas APP y detecta si la GUI se cayo (PID o log nuevo) o si el equipo reinicio (uptime). Es la que
encontro estas dos. Tambien se completo el control remoto: ahora expone las **23 apps** (antes 15),
asi que la pagina /remote del webserver tambien las puede abrir.

---

## 33. Todos los botones de las apps tienen handler

**Pedido**: "asegurate que todos los botones de las apps tengan su handler para que no se reinicie".

**La pista era real**: en el log del crash de CW Trainer (§32) aparecia, cada ~26 s,
`buttons_press: Button 0 press action is NULL`: una app apretando un boton **sin handler**.

**Auditoria** (268 botones en 61 paginas, `tests/check_app_handlers.py`): **4 sin handler**.

| Archivo | Boton | Que era |
|---|---|---|
| `dialog_field.c` | `b_home` ('FIELD OPS') | titulo de pagina, sin `.press` |
| `dialog_field.c` | `b_bands` ('BANDS') | idem |
| `dialog_field.c` | `b_logpg` ('LOGBOOK') | idem |
| `dialog_bands.c` | `btn_page` ('BANDS 1:1') | idem |

Los cuatro son los **titulos de pagina** de esas apps: ahora tienen handler y reaplican su pantalla
(`titulo_cb` / `bands_title_cb`), asi que no queda ningun boton muerto.

**Ademas, `buttons_press()` tenia un SIGSEGV esperando**: hacia

```c
lv_obj_t *btn_obj = lv_obj_get_parent(item->label_obj);   /* label_obj puede ser NULL */
```

**antes** de mirar si el item tenia handler. Un item puede estar en la fila de botones y a la vez
tener `label_obj == NULL` (por ejemplo si el **mismo item se usa en dos paginas** y se descarga una):
ahi `lv_obj_get_parent(NULL)` es un segfault, no un aviso. La auditoria encontro items compartidos
entre paginas en `dialog_field.c` (6: `b_act`, `b_spot`, `b_back`, `b_reccq`, `b_log`) y en
`dialog_cluster.c` (`btn_close`). Ahora se avisa y se sale, igual que con el item NULL.

**Chequeo nuevo**: `tests/check_app_handlers.py` recorre todas las `buttons_page_t`, resuelve cada
item a su definicion (entendiendo los helpers `make_*btn*`), **falla si algun boton no tiene
`.press`** y **avisa** si un item se usa en dos paginas del mismo archivo. Enganchado a
`tests/run_all_tests.sh`. Hoy: **268 botones, 0 sin handler**.

**Lo que NO arreglo** (medido despues de estos cambios, con 20 s de permanencia por app):
**CW Trainer y Status siguen muriendo** (la GUI se cae y el supervisor la relanza). O sea: los
handlers faltantes **no eran la causa del SIGSEGV** de §32; siguen Bands, Field y About OK. La
investigacion de esas dos sigue abierta con el core ya capturado.
