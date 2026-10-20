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
44. [El barrido de ROE no dibuja un valor fuera de escala](#44-el-barrido-de-roe-no-dibuja-un-valor-fuera-de-escala)
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
34. [El core dice donde muere CW Trainer y Status (y tres punteros colgados)](#34-el-core-dice-donde-muere-cw-trainer-y-status-y-tres-punteros-colgados)
35. [Estado final de las apps que matan la GUI: que quedo arreglado y que falta](#35-estado-final-de-las-apps-que-matan-la-gui-que-quedo-arreglado-y-que-falta)
36. [Causa raiz: la pantalla principal se destruye al abrir la app](#36-causa-raiz-la-pantalla-principal-se-destruye-al-abrir-la-app)
37. [Lo que arreglo las apps que se caian (y lo que no)](#37-lo-que-arreglo-las-apps-que-se-caian-y-lo-que-no)
38. [Status y JS8: los dos crashes que quedaban (y que la migracion estaba bien)](#38-status-y-js8-los-dos-crashes-que-quedaban-y-que-la-migracion-estaba-bien)
39. [QTH y Callsign no abrian por el control remoto (y el barrido las daba por vivas)](#39-qth-y-callsign-no-abrian-por-el-control-remoto-y-el-barrido-las-daba-por-vivas)
40. [El volumen que el control remoto no veia (y la etiqueta de ganancia del Audio)](#40-el-volumen-que-el-control-remoto-no-veia-y-la-etiqueta-de-ganancia-del-audio)
41. [CW Trainer: el desborde del buffer del decodificador CW (issue #51)](#41-cw-trainer-el-desborde-del-buffer-del-decodificador-cw-issue-51)
42. [WSPR como primera app mataba la GUI: la etiqueta NULL del modo digital](#42-wspr-como-primera-app-mataba-la-gui-la-etiqueta-null-del-modo-digital)
43. [Codigo vendoreado de WSPR: escritura fuera del array en pack_call (sin llamadores)](#43-codigo-vendoreado-de-wspr-escritura-fuera-del-array-en-pack_call-sin-llamadores)
45. [La cola de eventos se traba al llenarse (issue #111)](#45-la-cola-de-eventos-se-traba-al-llenarse-issue-111)
46. [La curva de ROE escalonada del #119: el GUI flasheado no tiene el #117](#46-la-curva-de-roe-escalonada-del-119-el-gui-flasheado-no-tiene-el-117)
47. [El fantasma del chart de ROE: el panel del dialogo es semitransparente](#47-el-fantasma-del-chart-de-roe-el-panel-del-dialogo-es-semitransparente)
48. [El crash de CW del #116: la ventana de mensajes con la tabla vacia le pasaba NULL al keyer](#48-el-crash-de-cw-del-116-la-ventana-de-mensajes-con-la-tabla-vacia-le-pasaba-null-al-keyer)
49. [El widget de CW tune en el tema claro: las barras desaparecen sobre su propio panel](#49-el-widget-de-cw-tune-en-el-tema-claro-las-barras-desaparecen-sobre-su-propio-panel)
50. [La parabola de ROE invertida en la radio: el ajuste libre del render, no el camino de datos](#50-la-parabola-de-roe-invertida-en-la-radio-el-ajuste-libre-del-render-no-el-camino-de-datos)
51. [Las caidas del #121: una ultima respiracion persistente (y el aviso de bateria baja)](#51-las-caidas-del-121-una-ultima-respiracion-persistente-y-el-aviso-de-bateria-baja)
52. [El tono de CW se mide con el signo invertido (y el auto-ajuste lo compensa)](#52-el-tono-de-cw-se-mide-con-el-signo-invertido-y-el-auto-ajuste-lo-compensa)
53. [El auto-ajuste de peak beta no puede variar con la WPM: el bloque de FFT es mas largo que medio punto](#53-el-auto-ajuste-de-peak-beta-no-puede-variar-con-la-wpm-el-bloque-de-fft-es-mas-largo-que-medio-punto)
54. [La linea central del waterfall del #140: el defecto de alto 0 no existe en el trunk](#54-la-linea-central-del-waterfall-del-140-el-defecto-de-alto-0-no-existe-en-el-trunk)
55. [La linea central del waterfall del #140: la geometria estaba, el color no (medido por tema)](#55-la-linea-central-del-waterfall-del-140-la-geometria-estaba-el-color-no-medido-por-tema)
56. [El SIGSEGV del #131: el hilo de radio tocaba LVGL con las teclas del panel (hkey_put)](#56-el-sigsegv-del-131-el-hilo-de-radio-tocaba-lvgl-con-las-teclas-del-panel-hkey_put)
57. [El espectro del #149: el dibujo leia los buffers sin lock (carrera confirmada con TSan)](#57-el-espectro-del-149-el-dibujo-leia-los-buffers-sin-lock-carrera-confirmada-con-tsan)
58. [lv_async_call no es thread-safe en LVGL 8: la app de Propagacion usa el scheduler](#58-lv_async_call-no-es-thread-safe-en-lvgl-8-la-app-de-propagacion-usa-el-scheduler)
59. [El SIGSEGV del #131 (reabierto): otros tres caminos tocaban LVGL fuera del hilo de GUI](#59-el-sigsegv-del-131-reabierto-otros-tres-caminos-tocaban-lvgl-fuera-del-hilo-de-gui)
60. [El squelch del #166: la lib del control ya habilita el enable (el comentario de radio.c:611 era redundante)](#60-el-squelch-del-166-la-lib-del-control-ya-habilita-el-enable-el-comentario-de-radioc611-era-redundante)
61. [El md5 del build no distinguia commits: `rsync -au` de Buildroot no reponia la copia (#165)](#61-el-md5-del-build-no-distinguia-commits-rsync--au-de-buildroot-no-reponia-la-copia-165)

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

## 44. El barrido de ROE no dibuja un valor fuera de escala

**Sintoma**: el owner reporto que el grafico del barrido se ve **vacio**. Yo lo reproduje y lo cargue como
issue #101 creyendo que era un defecto.

**Causa, medida** (radio en 192.168.0.236, GUI `6841894a9409`; mismo metodo: pixeles no-fondo del area del
grafico, `docs/evidencia/2026-10-15-validacion-arreglos/`):

```
  $ <barrido de ROE con el dial en cada frecuencia; captura a los 35 s>
  7.167971 MHz (fuera de la resonancia de la antena de 40 m):  10.82%
  7.012000 MHz (rango resonante, el owner puso la radio ahi):   17.83%   <- la curva aparece
```

El eje del chart va de **1,5 a 4,5** (se lee en las capturas `16-roe-con-att.png` y `18-roe-en-7.012.png`).
Una ROE **mayor que el tope** no se dibuja: queda un grafico vacio, indistinguible de uno roto.

**Correccion de lo que decia el issue #101**: yo habia propuesto dos hipotesis (una regresion del #96 y el
estado `pre`/`att`). Las dos quedaron **refutadas por medicion**:

```
  $ <mismo barrido, cambiando pre/att>
  pre=1 att=0 -> 10.82%      att=1 pre=0 -> 10.82%     (identico: no es el estado)
```

y el diff del #96 es de ciclo de vida (`swrscan_stop()`/`swrscan_release()`/`LV_EVENT_DELETE`), no del
trazado. **No es un defecto**: es la ROE fuera de escala.

**Sin fundar** (anexo A): **no medi la ROE real a 7,167971** (no lei el valor, solo observe que no se
dibuja). Falta: leer la ROE a esa frecuencia con otra antena o con un medidor externo.

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
| "La app de SWR Scan se cierra sola al terminar el barrido" (#119, defecto 2) | El cierre no está en el código del fin de barrido (§46) y no quedó el log de la GUI de esa corrida; el candidato (el `app off` del control remoto, que sí se acepta con el barrido transmitiendo) está fundado **en el código**, no en esa corrida | Correr el barrido con el log de la GUI a mano (`/tmp/x6200_log.txt` → `/mnt/app_logs/`), el `pid` de la GUI y el estado `app` del control remoto; y revisar el script de la corrida por si manda `{"cmd":"app","text":"off"}` |
| El bucle de reinicio del CW Trainer (issue #51) se rearma en el arranque "por sí solo" | El banco no tiene camino de audio y el chroot no estaba armado, así que no se pudo medir la cadencia en el equipo | Medir en la radio el `PID` de la GUI por 150 s tras un reset (issue #15), con `key_train` en `params.db` |
| El paso exacto por el que `hkey_put()` fuera de hilo corrompió `event_head` (issue #131), y que el trigger sea sólo el cambio de filtro | Está fundado que `hkey_put()` corría en el hilo de radio y tocaba LVGL (§56), pero **no** la escritura concreta que deja el frame muerto en la lista de eventos: el banco no tiene flow y no ejecuta `radio_tick`, y la corrupción necesita concurrencia real. Los dos crashes tienen el mismo `pc`/`lr` con contextos distintos (`on_vfo_mode_change` y `on_high_filter_change`) | Correr en la radio la receta de §56 (200 ciclos de tecla + filtro + modo + banda) con el fix y mirar `CRASHLOG`; si vuelve, guardar el log vivo completo y `/mnt/lastbreath` del momento |

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
arriba y VISION lo pone en su propia fila, `theme.h:192`), que es el unico caso en el que BT bajaria
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

---

## 34. El core dice donde muere CW Trainer y Status (y tres punteros colgados)

**Metodo que funciono** (para no repetir el camino largo): el core no se guardaba porque el
`core_pattern` era relativo y el proceso corre con el directorio en `/`, y ademas en `/mnt` (vfat) el
kernel aborta el volcado con "cannot preserve file permissions". Con `core_pattern=/tmp/core.%p` y un
arranque de la GUI con `ulimit -c unlimited` (el daemon de la imagen no lo pone) **si** se obtuvo el
core (193 MB). Para leerlo no hacian falta gdb-multiarch: alcanza el **gdb del host** con
`set architecture arm` y `set solib-search-path`, y para las direcciones `eu-stack -l` (da el mapa de
modulos con la base) + `addr2line` del toolchain.

**El backtrace** (CwTrainer, reproducible 7/7):

```
#0 lv_obj_has_flag_any        (liblvgl.so base 0xb6150000, PC 0xb616cc28 = offset 0x1CC28)
#1 lv_obj_is_layout_positioned
#2 lv_obj_add_flag            <- LVGL
#3/#4 codigo de la app        (la pila esta corrupta y gdb no los resuelve)
```

O sea: **la app le pasa a `lv_obj_add_flag()` un objeto invalido** (NULL o liberado). Y las marcas
propias en `construct_cb` de las dos apps **no llegan a imprimirse**: el crash ocurre **antes**, en el
camino de apertura de la app (que incluye el cambio de modo a CW y sus observadores).

**Un problema de despliegue que aparecio en el camino** (y que explica horas de confusion): el script
`tools/prueba-gui-en-radio.sh` copiaba **solo el binario**, no las librerias. La radio tenia la
`liblvgl.so` del arranque anterior (`5ad04ccc…`), asi que un cambio hecho en `lvgl/` **no llegaba** y
no habia forma de verlo desde el log. Arreglado: ahora sincroniza `liblv*.so*` (con `SIN_LIBS=1` se
puede saltear).

**Tres punteros colgados, arreglados** (misma clase: observadores globales del modo/banda que tocan
widgets de una pantalla destruida):

| Archivo | Objeto | Que pasaba |
|---|---|---|
| `cw_tune_ui.c` | `obj` (UI de sintonía CW) | observador de `cfg_cur.mode` sin guarda |
| `pannel.c` | `obj` (panel de decoder) | idem, y es el panel que se oculta al abrir una app |
| `band_plan.c` | `segs[i].box/label` | idem, recreados en cada `band_plan_init` |

Los tres ahora limpian su puntero con un handler de `LV_EVENT_DELETE` (comparando el target, para no
borrar el puntero nuevo cuando el viejo muere despues) y `update_visibility()` sale si es NULL.

**Lo que sigue abierto**: CW Trainer y Status **siguen muriendo** (medido despues de cada arreglo,
20 s por app). El proximo paso es instrumentar la **familia entera** (`lv_obj_add_flag`,
`lv_obj_clear_flag`, `lv_obj_add/clear_state`) con el llamador, porque el parche a
`lv_obj_add_flag` no imprimio **ninguna** linea antes de morir (con `LV_USE_ASSERT_OBJ` en 0 un
objeto invalido no avisa: por eso el crash es mudo), o revisar los otros observadores de
`cfg_cur.mode` (`spectrum.c`, `tx_info.c`, `info.c`, `rtty.c`, `dialog_eq.cpp`).

---

## 35. Estado final de las apps que matan la GUI: que quedo arreglado y que falta

**Lo que se midio con la instrumentacion de LVGL** (registrando objeto y llamador de la familia
`lv_obj_add_flag` / `clear_flag` / `add_state` / `clear_state` y de `lv_obj_is_layout_positioned`,
todo a un archivo propio con `fflush` para que no se pierda): las ultimas llamadas de la app antes
de la muerte salen de **`redraw()` de `spot_marks.c`** y de **`redraw()` de `band_plan.c`**, y el
crash cae adentro de LVGL recorriendo un objeto **liberado** en el layout flexivo (`flex_update` en
el simbolo mas cercano). O sea: **use-after-free de un widget**, no un puntero NULL suelto.

**Arreglado** (la clase entera: pools/objetos estaticos de la pantalla principal usados por
observadores y timers **globales**, que sobreviven a la destruccion de la pantalla):

| Archivo | Que se limpia |
|---|---|
| `spot_marks.c` | pool de marcas de spots (la redibuja un timer de 1 s) |
| `band_plan.c` | los 15 segmentos del plan de banda |
| `cw_tune_ui.c` | la UI de sintonia CW |
| `pannel.c` | el panel del decodificador CW |
| `dialog_field.c` / `dialog_bands.c` | los 4 botones que no tenian handler (§33) |

Todos limpian su puntero con un handler de `LV_EVENT_DELETE` (comparando el target) y su
`redraw`/`update` sale si el objeto ya no esta.

**Descartado con datos, no con opiniones**:

- **Desborde de pila por un ciclo de layout**: en un backtrace de 60 tramas del core solo hay **3**
  tramas de layout. No es recursion.
- **El control remoto** (mi extension de hoy a 23 apps): `{"cmd":"app","text":"NoExiste"}` responde
  "Unknown app" y `{"cmd":"btn","val":9}` responde "Invalid value", y en los dos casos la GUI
  **sigue viva**. Los arrays estan parejos (23 y 23) y el indice se resuelve acotado.
- **Memoria**: `oom_kill 0` y la GUI usa 20 MB.
- **Los `exit(0)` del arbol** y las librerias de instrumentacion del banco (§32).

**Estado medido con 15 a 22 s de permanencia por app**: **21 de 23 apps andan** (todas las que se
probaron, incluidas las 9 que antes ni se podian abrir); **CW Trainer y Status siguen matando la
GUI** ✗. El equipo **no se reinicia**: el supervisor la relanza en ~1 s y el uptime sigue subiendo.

**Proximo paso concreto** (no hace falta repetir el camino largo): activar
`LV_USE_ASSERT_OBJ 1` en `lv_conf.h` con un `LV_ASSERT_HANDLER` que registre
`__builtin_return_address(0)` y salga. Hoy ese assert esta en **0**, y por eso el objeto invalido no
avisa y el crash es mudo. Con eso LVGL mismo dice **que objeto** es invalido y **quien** se lo paso.

---

## 36. Causa raiz: la pantalla principal se destruye al abrir la app

**La herramienta que lo resolvio** (anotada para no volver a dar vueltas): activar
`LV_USE_ASSERT_OBJ 1` en `lv_conf.h` con

```c
#define LV_ASSERT_HANDLER_INCLUDE <stdio.h>
#define LV_ASSERT_HANDLER (fprintf(stderr, "LVGLASSERT %s:%d caller=%p\n", __FILE__, __LINE__, \
                                    __builtin_return_address(0)), fflush(stderr), __builtin_trap());
```

(como **expresion** y con `;` final: LVGL usa `LV_ASSERT_HANDLER` sin punto y coma). Con eso el
crash deja de ser mudo y sale en el log **con el llamador**:

```
LVGLASSERT src/core/lv_obj.c:231 caller=0x4dc1fc
```

**Que dice ese llamador**: `0x4dc1fc` cae dentro de **`band_plan_init`** (los simbolos vecinos son
`scheduler_put` 0x4c860 y `scheduler_put_noargs` 0x4cb34 para el llamador de `main_screen`, y
`redraw`/`band_plan_init` para este). O sea: el codigo que **arma la pantalla principal**
(`main_screen()`) esta trabajando sobre un **padre muerto**: crea objetos bajo un contenedor ya
destruido.

Y una marca al entrar a `main_screen()` muestra que se llama **una sola vez por arranque**, desde
`scheduler_put`:

```
SCREENDBG main_screen obj=(nil) caller=0x4c9b34
```

**Conclusion (fundada)**: el arbol de objetos de la pantalla principal **se destruye cuando se abre
una app**, y lo que sobrevive -el timer de 1 s de `spot_marks`, el plan de banda, los observadores
globales del modo y la banda- sigue tocando esa memoria liberada. Eso es el use-after-free que
aparece adentro de LVGL recorriendo el layout flexivo (`lv_obj_has_flag_any` por `flex_update`), y
es tambien la razon de que el crash llegue **unos segundos despues** de abrir la app: lo dispara el
timer.

**Proximo paso, concreto**: encontrar quien destruye los hijos de la pantalla principal en el camino
de apertura de la app (`dialog_construct` / `apps_disable` / el frame del app) y arreglar ese ciclo
de vida. Con el assert de arriba el aviso es inmediato. **Estado**: 21 de 23 apps andan y el equipo
no se reinicia; CW Trainer y Status siguen matando la GUI.

---

## 37. Lo que arreglo las apps que se caian (y lo que no)

**Arreglado y verificado**: **CW Trainer** ya no mata la GUI ✓. Eran dos cosas: las llamadas a
`lv_obj_add_flag` sobre objetos **globales** que podian no existir (84 guardas de NULL en 20
archivos, §33 y estas) y su **timer de 1 s** siguiendo vivo despues de cerrarse el dialogo
(`show()`/`refresh_ui()` tocaban widgets liberados: ahora hay una bandera `ui_alive` y un handler de
`LV_EVENT_DELETE` en `dialog.obj` que borra el timer por **cualquier** camino en que muera el
dialogo, porque el marco no siempre llama a `destruct_cb`).

**Probado y descartado con datos**: reescribir los diez `system("amixer ...")` de `mixer_setup()`
por la API de ALSA (con las guardas `has_*`) **empeoro** las cosas: pasaron a morir 5 de 10 apps en
vez de 1. Revertido. Queda anotado para no volver a intentarlo sin entender por que.

**Sigue fallando**: **Status** y **JS8** ✗ (con 18-20 s de permanencia; el resto de las probadas
anda). Lo que se sabe de Status: **no tiene ni una llamada** a `lv_obj_add_flag` en su archivo, asi
que su crash no es por esa via; y cuando muere deja al GUI en un arranque que a veces vuelve a
morir. El proximo paso es capturar el **core del proceso recien nacido** (con `core_pattern` en
`/tmp` y `ulimit -c unlimited`) y leerlo con gdb, que para un crash de arranque tiene la pila
intacta.

---

## 38. Status y JS8: los dos crashes que quedaban (y que la migracion estaba bien)

**La herramienta que lo resolvio, y que queda en el build** (antes el crash era mudo: el log se
cortaba y no decia nada):

* Un **handler de crashes** en `main.c` (SIGSEGV/BUS/ABRT/ILL/FPE) que registra la senal, la
  direccion que fallo, **PC/LR/SP**, la **base del binario** (`&main`) y un volcado de 160 palabras
  de pila. Con la base alcanza para resolver: `link = link(main) + (pc - base)` y `addr2line`, sin
  depender del ASLR ni de gdb (el toolchain no lo trae). Cuando el PC cae en una libreria, el
  volcado de pila da la cadena de la app: asi se encontro JS8.
* Desde el **#131**, antes del `CRASHLOG-STACK` va una **tabla de modulos** con la base de carga y el
  md5 de cada `.so`/ejecutable, una linea por modulo:
  `CRASHLOG-MOD <base> <md5> <path>`. La tabla se arma **una sola vez al arrancar**
  (`src/crashlog_modules.c`): lee `/proc/self/maps` (o `dl_iterate_phdr` si el chroot no tiene
  `/proc`), calcula los md5 sin heap y deja el bloque formateado en `.bss`; el handler solo hace
  `write(2)` de ese bloque. Al arrancar queda una linea
  `CRASHLOG-MOD-INIT modulos=<n> exe_base=<base> exe_md5=<md5> exe=<path> (<ms> ms)` para medir el
  costo. Con eso un `pc`/`lr` en `0xb6xxxxxx` se resuelve contra el `.so` exacto sin depender de lo
  que haya quedado en el equipo.
* `LV_USE_ASSERT_OBJ` **queda en 0**: se probo en 1 (con un handler que registra el llamador) y
  sirvio para encontrar un caso (§34), pero convierte en abortes otros que el equipo toleraba: el
  barrido paso de 2 apps caidas a 6. Revertido y anotado.

**Status: dos bugs, los dos con el numero de fallo a la vista**:

1. `buttons_load_page()` hacia `page->items[0]->voice` sin mirar si `items[0]` era NULL, y la pagina
   de Status es `{.items = {NULL, NULL, NULL, NULL, &btn_close}}`: el crash fue `fallo=0x14`
   (exactamente el offset del campo `voice`) dentro de `buttons_load_page`. Guardado (y lo mismo en
   `buttons_load_page_group`, que hacia `cur_page->items[0]->next`).
2. `char *ip = NULL; char *gw = NULL; wifi_get_ipaddr(&ip, &gw)`: esa funcion hace
   `strcpy(*ip_addr, ...)`, o sea que espera punteros a **buffers**, no punteros NULL. El crash fue
   `fallo=(nil)` con el `lr` dentro de `wifi_get_ipaddr` (resuelto con el truco de la base). Status
   ahora pasa buffers de verdad y la funcion valida su contrato.

**JS8: un callback global que sobrevivia al dialogo**. El volcado de pila dio la cadena
`on_audio -> tx_play -> update_status` (las tres en `dialog_js8.c`) y el fallo era NULL dentro de una
libreria: el callback de audio es global y seguia llamando a la UI despues de cerrarse el dialogo.
Se le puso el mismo patron que al CW Trainer: bandera `ui_alive` + handler de `LV_EVENT_DELETE` en
`dialog.obj`, y la guarda tambien en `on_audio` (sin eso, JS8 dejaba rota a la app **siguiente** que
se abria: medido, CW Trainer moria despues de JS8).

**Revision de la migracion desde el X6100** (`.x6100_ref`):

* **JS8 esta portado fielmente**: 5421 lineas contra 5413 de la referencia. Las diferencias son las
  deliberadas (adaptador de audio de banda base compleja, publicacion al control remoto no portada) y
  **una pieza que falta**: el boton global `btn_dig_pwr` ("TX power steps for the digital apps"), que
  el X6100 usa en JS8, FT8 y SSTV. En el port la pagina 7 de JS8 conserva un lugar NULL. Queda
  anotado para restaurarlo.
* **Status no existe en el X6100** (no hay `dialog_status` ni equivalente en la referencia): es una
  app nueva de este port, asi que no "se migro mal": se escribio aca.

---

## 39. QTH y Callsign no abrian por el control remoto (y el barrido las daba por vivas)

**Sintoma** (barrido del issue #19): al abrir las 22 apps por el socket de control, `QTH` y
`Callsign` quedaban "vivas" pero la captura mostraba la pantalla de inicio con la barra de botones
de la app anterior: nunca abrian.

**Causa, medida** (con el binario previo al arreglo, `md5 2875410929b0`, sobre `e93b22f`):

```
$ docker exec banco-apps chroot /t /tmp/x6200-remote '{"cmd":"app","text":"QTH"}'
{"t":"ok"}
$ docker exec banco-apps chroot /t /tmp/x6200-remote '{"cmd":"get"}' | sed -n 's/.*"app":"\([^"]*\)".*/\1/p'
                       # vacio; la captura (reports/BARRIDO-APPS-19/repro/QTH-sin-abrir.png)
                       # muestra la pantalla de inicio: no abrio
$ docker exec banco-apps chroot /t /tmp/x6200-remote '{"cmd":"app","text":"Status"}' ; sleep 2
$ docker exec banco-apps chroot /t /tmp/x6200-remote '{"cmd":"app","text":"off"}' ; sleep 2
                       # Status seguia abierta: "off" no cierra un dialogo
```

**Codigo** (las dos causas): en `src/remote_server.cpp`, `open_app()` manda TODAS las apps a
`main_screen_start_app()`, y esa funcion (en `src/main_screen.c`) tenia `case` para 20 apps pero
**no** para `ACTION_APP_QTH`/`ACTION_APP_CALLSIGN`, que se atendian solo en `main_screen_action()`
(el dispatcher de los botones de accion): al pedir QTH por el socket, `main_screen_start_app()`
corria `apps_disable()` y despues no hacia nada. Y `current_app()` nombraba solo 8 apps, asi que
`{"cmd":"get"}` dejaba `"app":""` para cualquier dialogo (Status incluido) y
`{"cmd":"app","text":"off"}` -que cierra solo cuando `current_app()` no es vacio- no la cerraba.

**Arreglo**:

* `src/main_screen.c`: QTH y Callsign pasan por `main_screen_start_app()` (y `main_screen_action()`
  los despacha ahi), como el resto de las apps: un solo camino.
* `src/remote_server.cpp`: `current_app()` nombra las 22 apps de `app_names[]` (los 12 dialogos que
  faltaban, mas la tecla CW).
* `tests/check_apps_menu.py`: chequeo estatico nuevo: toda `ACTION_APP_*` de `actions[]` de
  `remote_server.cpp` tiene que tener `case` en `main_screen_start_app()`, y los largos de
  `app_names[]`/`actions[]` tienen que coincidir.

**Verificacion, medida** (binario con el arreglo, `md5 a553a015fdfe`):

```
$ docker exec banco-apps chroot /t /tmp/x6200-remote '{"cmd":"app","text":"QTH"}' ; sleep 1.5
$ docker exec banco-apps chroot /t /tmp/x6200-remote '{"cmd":"get"}' | sed -n 's/.*"app":"\([^"]*\)".*/\1/p'
QTH
$ docker exec banco-apps chroot /t /tmp/x6200-remote '{"cmd":"app","text":"off"}' ; sleep 1
$ docker exec banco-apps chroot /t /tmp/x6200-remote '{"cmd":"get"}' | sed -n 's/.*"app":"\([^"]*\)".*/\1/p'
                       # vacio: cerro
```

Y el barrido completo de las 22 apps (dwell 20 s), con el campo `app_estado` igual a la app pedida
en todas: `tools/ab_testing/reports/BARRIDO-APPS-19/barrido.json` (22/22 vivas). El chequeo
estatico falla si se vuelve a sacar el `case`:

```
$ python3 tests/check_apps_menu.py
  control remoto: 23 apps en app_names[] | 23 acciones
  todas las apps del menu llegan al dispatcher que usa su boton
```

**Queda anotado, medido**: `{"cmd":"app","text":"off"}` **no apaga RTTY** (no es un dialogo:
`main_screen_close_app()` es `dialog_destruct()`, que no lo toca). En el barrido no molesta porque
abrir la app siguiente pasa por `apps_disable()`, que lo apaga. Apagarlo bien pide ademas devolver
la barra de botones al inicio (hoy la pagina de RTTY queda cargada despues de cerrar un dialogo).

---

## 40. El volumen que el control remoto no veia (y la etiqueta de ganancia del Audio)

**Sintoma** (reportado por el usuario, issue #30): el volumen de recepcion no se aplica; la radio
suena muy bajo y subirlo por el control remoto no lo cambia. En el equipo, `/mnt/params.db` tenia
`vol = 8` y el `GET` del propio control remoto contestaba `"vol": 0`.

**Causa, medida en el banco** (binario ARM de la imagen con QEMU, `tools/captura-banco.sh`,
commit `5f07ac6`, md5 del binario `b662ca41cde6`):

```
$ sqlite3 .vfix3/mnt/params.db "select name,val from params where name in ('vol','sql','nb','nr');"
nb|1
nr|1
sql|0
vol|10
$ docker run --rm --entrypoint "" -v .vfix3:/t x6200-buildroot sh -c "chroot /t /tmp/x6200-remote '{\"cmd\":\"get\"}'"
{"t":"state","vfo":"A","freq":21250000,...,"rfgain":0,"sql":0,"vol":0,...,"nb":0,"nr":0,
 "nb_level":0,"nb_width":0,"nr_level":0,"pwr":50,...,"did":""}
$ grep -c "sujeto NULL" .vfix3/out-banco.txt
28
```

Los 28 avisos del log son 7 por cada `GET` (4 `GET` en esa corrida: el handshake y tres
consultas): `vol`, `sql`, `nb`, `nb_level`, `nb_width`, `nr`, `nr_level`, exactamente los siete
Param que en `state_line()` (`src/remote_server.cpp:610-614`) leen items de `cfg`. Todos los
avisos dicen `subject_get_int con sujeto NULL` y salen de `avisa_subject_nulo()`
(`src/cfg/subjects.cpp:89`), que devuelve el valor neutro: por eso el `vol` que contesta el control
remoto es 0 y no el de la base.

La causa, en el codigo: `cfg_sm` (`src/cfg/settings_manager.cpp`) es un global C++, asi que su
constructor corre antes de que `cfg_init()` cree los items de `cfg` y sus subjects. Los Param de
esos items quedaban ligados a un `Subject *` NULL para siempre: `init_live()` rehacia el mapeo de
los subjects de estado vivo (`cfg_cur.*`, los del dial) pero no el de los persistidos. `pwr` no
estaba roto porque su `tief()` lee `cfg.pwr.val` en cada llamada, no al construir.

**Arreglo**: `SettingsManager::init_persisted()` vuelve a ligar los Param persistidos, y
`cfg_sm_init_live()` (el ultimo paso de `cfg_init()`, `src/cfg/cfg.c`) lo llama junto con
`init_live()`. `check_live()` ahora recorre los veinte parametros (persistidos y de estado vivo) y
devuelve cuantos quedaron sin subject, para que se pueda comprobar en vez de mirarlo en el log.

**Verificacion, medida** (el mismo banco, commit `9a5a769`, md5 del binario `e8572f2d8355`):

```
$ sqlite3 .vfix3/mnt/params.db "select name,val from params where name in ('vol','sql','nb','nr');"
nb|1
nr|1
sql|0
vol|8
$ docker run --rm --entrypoint "" -v .vfix3:/t x6200-buildroot sh -c "chroot /t /tmp/x6200-remote '{\"cmd\":\"get\"}'"
{"t":"state","vfo":"A","freq":21250000,...,"rfgain":0,"sql":0,"vol":8,...,"nb":1,"nr":1,
 "nb_level":0,"nb_width":0,"nr_level":0,"pwr":50,...,"did":""}
$ docker run --rm --entrypoint "" -v .vfix3:/t x6200-buildroot sh -c "chroot /t /tmp/x6200-remote '{\"cmd\":\"vol\",\"val\":30}'"
{"t":"ok"}
$ ... '{"cmd":"get"}'  ->  ..."vol":30...
$ grep -c "sujeto NULL" .vfix3/out-banco.txt
0
```

Es decir: el estado que ve el control remoto pasa a ser el de la base (`vol 8`, `nb 1`, `nr 1`),
el comando `vol` del control remoto mueve ese estado, y los avisos de sujeto NULL desaparecen del
log. Lo mismo cubre el arnes de UI: `settings-audio` y las otras 24 fixtures lo comprueban en cada
corrida (`mocks/mock_settings_manager.cpp`, con `check_live() == 0`).

**La etiqueta de "Play,Rec gain"** (mismo cambio, issue #26). Medido con el inspector del arnes
(`ui_harness --inspect`) sobre el render de la fixture `settings-audio`: las dos etiquetas de valor
de la fila de ganancia de Ajustes -> Audio salian con el texto `f`:

```
('Play,Rec gain', 22, 154, 197, 28)
('f', 619, 154, 11, 28)          <- etiqueta del slider de Play gain
('f', 763, 154, 11, 28)          <- etiqueta del slider de Rec gain
```

Eran dos causas que se sumaban, las dos de codigo: `slider_with_text()`
(`src/dialog_settings.c`) le pasaba un `int32_t` a un formato `"%.1f"` (tipo equivocado en los
argumentos variables), y el `lv_conf.h` del arnes no habilitaba el `%f` de LVGL, que la ROM si
habilita (`lv_conf.h` de la raiz: `LV_SPRINTF_USE_FLOAT 1`). Con las dos corregidas, la etiqueta de
Play gain dice `3.0` y la de Rec gain `-7.0` (el valor al que la fixture la arrastra).

**Sin fundar** (anexo A): el silencio digital del sink que el issue #30 mide en la radio
(`parecord` del monitor, RMS -240 dBFS). El banco no tiene el stack de audio real: la GUI corre con
un framebuffer falso y sin Pulse ni codec, asi que ahi no hay monitor que grabar. Lo que se midio
en el banco es el camino `params.db -> estado de la GUI -> control remoto`, que es donde estaba el
defecto; que ese estado llegue al codec es del aplicador del SDK (`x6200_control_rxvol_set`) y del
canal fisico (#15).

---

## 41. CW Trainer: el desborde del buffer del decodificador CW (issue #51)

**Sintoma** (reportado por el usuario, issue #51): usar el CW Trainer deja la GUI en un bucle de
reinicio, una caida cada medio minuto, medida en la radio.

**Causa, reproducida en el host** (codigo + medicion): el decodificador CW acumula los puntos y
rayas del caracter en curso en un buffer fijo `elements[128]` (`src/cw_decoder.c:57`) con un
`strcat()` sin limite, uno por cada flanco de bajada. Una racha de eventos de tecla sin espacio
inter-caracter (senal pegada, o el audio corrupto que describe #19) lo desborda y el chequeo
fortify de glibc aborta el proceso dentro de `strcat`. Es el mismo sintoma de #19
(`*** buffer overflow detected ***` terminando en `abort()` dentro de libc) y la pila resuelta de
#19 (`audio.c` -> `cw_put_audio_samples` -> `cw_decoder_signal`) apunta a esta funcion.

Se reprodujo en el host con el mismo `cw_decoder.c`, un sink que junta el texto y una racha de 300
eventos de 20 ms (por debajo del umbral de elemento de 139 ms, sin espacio):

```
$ cd /tmp/opencode/cwtest
$ gcc -std=gnu11 -O2 -D_FORTIFY_SOURCE=2 -I. cw_decoder.c driver.c -o test
$ ./test
*** buffer overflow detected ***: terminated
$ echo $?
134
```

**Correccion de lo que decia antes este registro**: la hipotesis del issue #19 (el contrato de
`cbuffercf_read` en `cw_put_audio_samples`, `src/cw.cpp:228-235`, que devolveria en `n` menos
muestras contiguas que `desired_num`) **no se sostiene con la libreria que se envia**: liquid-dsp
1.4.0 (`AetherX6200Buildroot/buildroot/dl/liquid-dsp/liquid-dsp-1.4.0.tar.gz`; el header del
sysroot dice `LIQUID_VERSION "1.4.0"`) llama a `CBUFFER(_linearize)()` dentro de `CBUFFER(_read)`
cuando la peticion cruza el final del buffer (`liquid-dsp/src/buffer/src/cbuffer.c`), asi que `n`
es siempre `desired_num` y el `release` con `desired_num` es correcto.

**Arreglo** (`src/cw_decoder.c`): la acumulacion pasa por `cw_decoder_element()` (`:172`), que corta
en el borde del buffer (decodifica lo que entro y arranca de nuevo) en vez de hacer `strcat()`.
Ningun Morse valido mide mas de 9 elementos, asi que el corte solo actua sobre basura; el
`_Static_assert(sizeof(elements) >= 16, ...)` (`:65`) deja la invariante en el codigo.

**Verificacion, medida**: el test nativo `tests/native/test_cw_decoder.c` reproduce la racha y
comprueba que la GUI sobrevive y que el decodificador sigue decodificando (un punto aislado es
"E"):

```
$ tests/native/run_native_tests.sh          # con el src sin arreglar
FALLO .../tests/native/test_cw_decoder.c:78: got_calls >= 2
814 comprobaciones, 4 fallos
$ tests/native/run_native_tests.sh          # con el arreglo
814 comprobaciones, 0 fallos
```

**Aparte** (issue #51, mitigacion en la app): el estado de radio que la app arma (key train, sink
del decodificador, modo forzado a CW) se devolvia solo desde `destruct_cb`, y el marco de dialogos
no siempre lo llama (lo que motivo la bandera `ui_alive` de #19, `fb640dc`): por ese camino el
equipo quedaba armado. Ahora `trainer_release()` (`src/dialog_cw_trainer.c:1281`, idempotente)
devuelve ese estado y lo llaman tanto `destruct_cb` como el `LV_EVENT_DELETE` (`ui_deleted_cb`,
`:1275`).

**Sin fundar** (anexo A): que el circuito de reinicio se rearme en el arranque por si solo; en el
banco no se pudo medir (no tiene camino de audio y el chroot no estaba armado). Y el contrato de
`cbuffercf_read` si se cambia de version de liquid.

## 42. WSPR como primera app mataba la GUI: la etiqueta NULL del modo digital

**Sintoma** (reportado por el usuario, issue #63): al abrir la app WSPR la radio "se reinicia". Es el
mismo cuadro de §32: la GUI muere y el equipo da la impresion de reiniciarse.

**Causa, medida en el banco** (`tools/captura-banco.sh` con QEMU, commit `11f8d68`, binario de la
imagen `md5 163f1f5883ae`, WSPR como primera app de una GUI recien arrancada). La base del banco
(`.vfix3/mnt/params.db`) es la de una radio actualizada en el lugar: no tiene filas de WSPR:

```
$ sqlite3 .vfix3/mnt/params.db "select type, count(*) from digital_modes group by type;"
0|11
1|9
$ docker exec banco-apps chroot /t /tmp/x6200-remote '{"cmd":"app","text":"WSPR"}'
{"t":"ok"}
$ grep -n "digital_modes\|CRASHLOG" .vfix3/out-banco.txt | tail -2
312:[Warn]	(271374.047, +48)	 cfg_digital_load: No results for load from digital_modes with dir=0, freq=21250000, type=4 	(in digital_modes.c line #84)
314:CRASHLOG sig=11 fallo=(nil) pc=0x4160d6a8 lr=0x1000 sp=0x40800998 base=0x41e084
```

El PC del volcado cae dentro de `liblvgl.so` (el banco la mapea en el pid 7 del contenedor; la lib
del chroot es la que cargo esa corrida, el banco no la reescribe):

```
$ docker exec banco-apps sh -c 'for p in $(ls /proc | grep -E "^[0-9]+$"); do grep -H liblvgl /proc/$p/maps 2>/dev/null | head -3; done'
/proc/7/maps:815a0000-8162d000 r--p 00000000 00:23 21421257                           /t/usr/lib/liblvgl.so
/proc/7/maps:8162d000-8163d000 ---p 0008d000 00:23 21421257                           /t/usr/lib/liblvgl.so
/proc/7/maps:8163d000-8163e000 r--p 0008d000 00:23 21421257                           /t/usr/lib/liblvgl.so
$ md5sum .vfix3/usr/lib/liblvgl.so
cd30ca6c58ad42f06caad930b65f0ee2  .vfix3/usr/lib/liblvgl.so
```

El lado invitado esta 0x40000000 abajo del host: `nm` da `main` en `0x1e084` y el volcado dice
`base=0x41e084`, o sea que el binario vive en `0x41000000`. Entonces
`pc - base_lib = 0x4160d6a8 - 0x415a0000`, y ese offset, resuelto contra esa lib, da el simbolo:

```
$ python3 -c 'print(hex(0x4160d6a8 - 0x415a0000))'
0x6d6a8
$ nm -n --defined-only liblvgl.so | grep -i vsnprintf
0006cfdc _vsnprintf
0006df20 lv_snprintf
$ addr2line -f -e liblvgl.so 0x6d6a8
_vsnprintf
lv_printf.c:?
```

**Causa, en el codigo** (dos piezas):

1. `cfg_digital_label_get()` (`src/cfg/digital_modes.c:100`) devolvia `label`, que es NULL hasta que
   un `cfg_digital_load()` tenga exito. Con una base sin filas del tipo pedido (WSPR = 4, y tampoco
   JS8 = 2/3) el load falla (el `LV_LOG_WARN` de arriba) y la etiqueta queda NULL.
2. El consumidor que crashea no la mira: `lv_label_set_text_fmt(frame.head_left, "%s   %.4f MHz",
   cfg_digital_label_get(), ...)` en el `timer_cb` de `src/dialog_wspr.c`, y el `%s` de LVGL
   desreferencia el argumento sin guarda (`lvgl/src/misc/lv_printf.c:820`, `case 's'`:
   `_strnlen_s(p, precision ? ... : (size_t) -1)`, que recorre `*s` en `lv_printf.c:146`).

Los otros tres caminos que tambien le pasan la etiqueta a un `%s`
(`dialog_wspr.c:551,637,711`, `dialog_ft8.c:661,1483`, `dialog_js8.c:1866`) estan detras de un
`cfg_digital_load()` exitoso, y por eso el barrido anterior no veia el crash: abrir FT8/FT4/JS8 antes
dejaba la etiqueta seteada (§19).

**Por que la base del banco no tiene el tipo 4** (medido): la copia de la base la hace una sola vez
`S20gui` (`rootfs/etc/init.d/S20gui:23`: `[ ! -f /mnt/params.db ] && cp
/usr/share/x6200/params.default.db /mnt/params.db`), asi que una radio que ya tenia `params.db` de una
ROM anterior conserva una base sin filas de WSPR (mismo problema de fondo que §4). La base que la ROM
*instala* si las tiene (`sqlite3 .vfix3/usr/share/x6200/params.default.db`: `0|11`, `1|9`, `4|11`).

**Arreglo**: `cfg_digital_label_get()` no puede devolver NULL; devuelve `""` mientras no haya
etiqueta (`src/cfg/digital_modes.c`), y el contrato queda escrito en `src/cfg/digital_modes.h`. Cubre
WSPR, JS8 y FT8 por igual. En el mismo archivo va un cast explicito `(const char *)` sobre
`sqlite3_column_text()`, que devuelve `const unsigned char *`: al entrar el modulo al suite nativo
bajo `-Wall -Wextra`, ese warning previo se saca de raiz.

**Verificacion, medida en el banco** (mismo banco, mismas condiciones, `RECONSTRUIR=si` desde el
worktree, binario `md5 d0ff2dba9c85`). Primero, que el arbol de build compilo el fuente del worktree:

```
$ docker run --rm --entrypoint "" -v x6200-build-output:/b x6200-buildroot \
    sh -c 'sed -n "/char \*cfg_digital_label_get/,/^}/p" /b/build/x6200-gui-custom/src/cfg/digital_modes.c'
char *cfg_digital_label_get() {
    /* ... */
    return label != NULL ? label : "";
}
```

Y despues, la misma orden y el mismo `params.db` que mataban a la GUI:

```
$ docker exec banco-apps chroot /t /tmp/x6200-remote '{"cmd":"app","text":"WSPR"}'
{"t":"ok"}
$ docker exec banco-apps chroot /t /tmp/x6200-remote '{"cmd":"get"}'
{"t":"state",...,"app":"WSPR","ax":{"st":"RX","sec":17,"pct":...
$ grep -c CRASHLOG .vfix3/out-banco.txt
0
```

Mas 9 sondeos del socket cada 10 s (90 s) con WSPR abierta, todos contestados, con `banco-apps`
todavia arriba al final. El aviso `No results for load ... type=4` sigue en el log (es el camino
normal de esa base); lo que ya no aparece es el `CRASHLOG`.

**Test nativo** (el que reproduce el crash sin radio): `tests/native/test_digital_modes.c` compila el
modulo real contra stubs de sqlite3/cfg/lvgl (mismo staging que los tests de clock y backlight) y
fija el contrato "nunca NULL" (y que un load exitoso siga publicando la etiqueta). En la revision sin
el arreglo el test muere con el mismo SIGSEGV (la lectura de la etiqueta no lleva guarda, igual que el
consumidor real) despues de decir en stderr por que; con el arreglo pasa (`12 comprobaciones, 0
fallos`). Corre en `tests/native/run_native_tests.sh`.

**Sin fundar** (anexo A): que en la radio ese SIGSEGV termine en un *reinicio* (eso es el relato del
usuario; lo medido es que la GUI muere, §32) y el log de la radio para este issue: causa y arreglo
quedaron medidos en el banco.

---

## 43. Codigo vendoreado de WSPR: escritura fuera del array en pack_call (sin llamadores)

Salio del mismo barrido de los caminos de memoria de WSPR que el §42. Es codigo de K1JT/rtlsdr-wsprd
(`src/wspr/wsprd/`, ver `UPSTREAM.md`), no de este port, y hoy **no tiene llamadores**.

**Medido** (driver minimo que llama a la funcion como la compila la ROM; `-fstack-protector-all`
pone el canario pegado al buffer y la escritura fuera del array se ve):

```
$ gcc -std=gnu11 -O1 -g -w -fstack-protector-all -I src/wspr/wsprd \
    driver.c src/wspr/wsprd/{wsprsim_utils,nhash,tab,fano,wsprd_utils}.c -lm -o driver
$ ./driver
K1ABC    -> 259047992
*** stack smashing detected ***: terminated
Aborted (core dumped)
```

**Codigo** (`src/wspr/wsprd/wsprsim_utils.c:49-67`): el buffer es `char call6[6]` y la rama
"digito en la posicion 1" escribe `for (i = 1; i < call_len + 1; i++) call6[i] = callsign[i - 1];`.
Con un callsign de 6 caracteres que entre por esa rama (`K1ABCD`, `A1BCDE`) el ultimo indice escrito
es 6: un byte fuera del array. Ese byte no se relee (el bucle que empaqueta recorre `0..5`), asi que
el valor que devuelve la funcion no cambia: el defecto es la escritura en si. El import es pristine
salvo los patches que lista `UPSTREAM.md`, y `wsprsim_utils.c` no esta en esa lista.

**Sin llamadores, medido** (`pack_call` no aparece en ningun camino de la app ni del arnes):

```
$ grep -rn "pack_call" src/ tools/ tests/ | grep -v "^src/wspr/wsprd/wsprsim_utils.c"
src/wspr/wsprd/wsprsim_utils.h:6:long unsigned int pack_call(char const *callsign);
```

**Queda sin tocar a proposito**: es codigo vendoreado (parchearlo agrega una entrada nueva a los
"Local patches" de `UPSTREAM.md`) y hoy es inalcanzable desde la app, asi que no entra en el arreglo
del §42: pediria decision del owner. Camino barato para cerrarlo cuando se decida: acotar el bucle
(`i < call_len + 1 && i < sizeof(call6)`), que no cambia lo que devuelve la funcion, con un test
nativo del valor empaquetado.

---

## 45. La cola de eventos se traba al llenarse (issue #111)

**Sintoma** (issue #111, medido en la radio): 25.624 `Overflow (events.c line #90)` de 26.313
lineas en un run, y en la ventana larga de `docs/PRUEBAS.md` "+4.447 lineas en 240 s = **18/s**, el
100% Overflows, con la radio en reposo (load 0,84)". Es sostenido, no rafagas, y no lo dispara la
carga de la UI.

**Causa, codigo** (`src/events.c`): la cola es un anillo de 64 slots con un mutex, con productores
en varios hilos y el consumidor en el lazo de LVGL (`event_obj_check()`). El productor daba por
lleno el anillo mirando el **contenido** del slot siguiente:

```
    uint8_t next = (queue_write + 1) % QUEUE_SIZE;
    if (queue[next]) { ... LV_LOG_ERROR("Overflow"); return; }
```

pero el consumidor usa `queue_read == queue_write` como marca de **vacio**. En el borde, `next`
vale `queue_read`, y ese slot esta en NULL (el consumidor lo limpia al leerlo), asi que el productor
lo daba por libre, escribia ahi y dejaba `queue_write == queue_read`: el consumidor veia la cola
vacia con los 64 eventos adentro. Como despues `queue[1]` ya no esta libre, el productor no vuelve a
encolar nunca: la cola queda trabada **para siempre** y todo evento posterior se descarta. De ahi el
sintoma sostenido al 100% con la radio quieta: una vez que la carga de la UI hizo llenar la cola, el
descarte no vuelve atras.

**Medido** (test nativo, `tests/native/test_events.c`; el commit `8a05f342` es el test sin el
arreglo, el `ab7710b3` el codigo previo):

```
$ git checkout 8a05f342 -- tests/native/test_events.c
$ git checkout ab7710b3 -- src/events.c src/events.h
$ sh tests/native/run_native_tests.sh
FALLO tests/native/test_events.c:46: events_stub_dispatched() == QUEUE_CAPACITY
FALLO tests/native/test_events.c:52: events_stub_dispatched() == QUEUE_CAPACITY + 1
FALLO tests/native/test_events.c:74: lines <= EVENTS_OVERFLOW_LOG_DETAIL
3 comprobaciones, 3 fallos
```

64 envios y un drenaje entregan **0** eventos (los 64 quedan varados y la cola no se recupera), y
100 descartes escriben **99** lineas de log.

**Arreglo**: el productor detecta el lleno por posicion (`next == queue_read`) y el consumidor toma
el mutex para decidir si la cola esta vacia; el descarte libera el `param` (no se encolaba, el
consumidor no lo liberaba: cada descarte filtraba) y el log queda acotado (los primeros 8 descartes
en detalle, despues un resumen cada 10 s con el total y el desglose por tipo de evento). Se agregan
los contadores `event_queue_stats_get()` (ocupacion pico, capacidad, total perdido) y
`event_queue_drops_of()` (perdidos por tipo).

**Verificacion, medida**:

```
$ sh tests/native/run_native_tests.sh
19 comprobaciones, 0 fallos
```

y el mismo modulo compilado para el target (Cortex-A7, gcc 12.3.0 del buildroot):

```
$ docker run --rm --entrypoint "" -v "$PWD":/w:ro -v x6200-build-output:/b x6200-buildroot \
    sh -c '/b/host/bin/arm-buildroot-linux-gnueabihf-gcc -c -O2 -std=gnu11 \
    -I/w -I/w/src -I/w/lvgl -I/b/host/arm-buildroot-linux-gnueabihf/sysroot/usr/include \
    /w/src/events.c -o /tmp/events_arm.o'
arm -c events.c exit=0
```

**Sin fundar** (anexo A): que el flood medido en la radio sea este latch y no un productor que
supera estructuralmente al consumidor. El latch esta probado en el test; cual productor lo dispara
en la radio lo dicen los contadores nuevos en la proxima corrida. Por frecuencia el candidato es
`meter_update()` -> `event_send(obj, LV_EVENT_REFRESH, NULL)` (`src/meter.c:319`), una vez por
cuadro de waterfall (`waterfall_fps_ms = 1000/25`, `src/dsp.cpp:119`), que coincide con el ritmo
medido.

---

## 46. La curva de ROE escalonada del #119: el GUI flasheado no tiene el #117

**Sintoma** (reporta el owner en el #119): con la ROM `c640645d810c` (GUI `e294229dd32d`) la curva
de ROE del barrido sale **escalonada** — mesetas planas con escalones verticales — aunque el codigo
del #117 dice "fitted to a single parabola". El QA lo midio leyendo el framebuffer en bucle
(`docs/evidencia/2026-10-15-validacion-arreglos/frame-*.png`) y anexo la curva extraida (496 puntos,
`roe-muestras-extraidas.csv`, commit `b39d6014`): corridas de hasta **62 columnas con el mismo valor**.

**Causa, medida**: el GUI que quedo en la imagen flasheada **no es el del #117**. Su
`roe_curve_resample` es, byte por byte, el **Catmull-Rom del #104** — el resampler que el #113
reemplazo. Medido compilando los dos `roe_curve.c` con el toolchain y los flags del build de la ROM
(buildroot 2023.02.11, gcc 12.3.0, `-O3 -g0 -D_FORTIFY_SOURCE=1`, el `TARGET_CFLAGS` que la propia
imagen declara) y buscando los bytes en el binario de la radio:

```
$ python3 -   # compara old.text (roe_curve.c del #104) y new.text (el del arbol) contra gui-nuevo
old(-O3) resample bytes: 376
ROM match old(-O3) resample: True
diff bytes: 0
ROM contains new(-O3) resample: False
ROM contains new(-O3) quadratic_fit: False
```

(`gui-nuevo` = `/tmp/gui-nuevo`, md5 `e294229dd32d133d78774e3b971517a3`, 13.257.128 B: el
`x6200_gui` de la imagen, sacado de la radio. `nm` del objeto da
`00000020 T roe_curve_resample`; en el binario cae en `0x4393c` y el desensamblado es el
Catmull-Rom completo, con su clamp a las dos muestras que enmarcan cada tramo.)

**Por que eso dibuja la escalera**: el Catmull-Rom del #104 clampa cada punto al rango de las dos
muestras que lo enmarcan (`src/roe_curve.c` del #104, `lo`/`hi` = min/max de `p1`/`p2`). Con
muestras **cuantizadas** (el `vswr` del flow viene en decimas: `radio.c:246`,
`dialog_swrscan_update(pack->vswr * 0.1f)`) dos muestras iguales dejan el tramo entero **plano**:
de ahi las mesetas; el salto entre mesetas es el escalon vertical. El ancho de las mesetas lo
confirma: son multiplos exactos de un intervalo de muestreo. El binario flasheado es **pre-#113**
(50 puntos fijos, `STEPS 50`), asi que el intervalo es `780/49 = 15.9 px`, y las mesetas medidas en
`frame-2.png` son 16 / 32 / 47 / 62 px = 1 / 2 / 3 / 4 intervalos (medido columna por columna sobre
el framebuffer).

**De que arbol salio ese GUI** (codigo): `state/worktrees/x6200/events-queue-111` (HEAD `dbc5e66c`,
base `ab7710b3` — **anterior** al merge del #117 `fe67dedb`) tiene exactamente las tres marcas del
binario: `src/roe_curve.c` de 2.236 B (el Catmull-Rom del #104), `src/dialog_swrscan.c` de 11.410 B
(el de 50 puntos, pre-#113) y el arreglo de la cola de eventos del #111 (cuya cadena
`Overflow: %u eventos perdidos` esta en el binario). El checkout principal `x6200_gui` (el otro
candidato) esta en `2392624`, tambien pre-#117, pero **no** tiene el arreglo del #111: la cadena no
esta en su `events.c`.

**Mecanismo del build** (codigo): el paquete `x6200-gui` de buildroot usa el override srcdir
(`local.mk` → `X6200_GUI_OVERRIDE_SRCDIR=/workspace_gui`) y copia con `rsync -au` sobre el volumen
persistente `x6200-build-output`; si el paquete ya esta construido, `make` no re-copia ni recompila
nada. La imagen que se flasheo llevaba el GUI que se habia compilado antes desde **otro worktree**,
y la nota del flasheo ("build: HEAD 223b30a; incluye #117") se creyo del `git log` del arbol, no del
binario. El propio skill del QA documenta el mount (`-v .wt-pruebas:/workspace_gui:ro`,
`.agents/qa-radio/SKILL.md`, seccion "Imagen de SD"), que apunta a un arbol fijo.

**El codigo del arbol no es la causa, medido** con el dataset real del QA (496 puntos, commit
`b39d6014`) sobre el camino real del chart (`construct_cb` → Run → `dialog_swrscan_update` por punto
→ `draw_cb`, con la escala Log y la geometria del chart) en el test nuevo
`tests/native/test_swrscan.c::test_swrscan_render`:

```
#119 forma dibujada (dataset real de 496 puntos, 41 muestras del barrido):
  paso maximo entre puntos dibujados: 1 px (la escalera del #104 da 8 px)
  caida maxima a la izquierda/derecha del valle: 0 / 0 px (escalera: decenas)
```

y con el `roe_curve.c` del #104 en su lugar (el que corre la radio), el mismo test:

```
FALLO tests/native/test_swrscan.c:474: worst_step <= 2
FALLO tests/native/test_swrscan.c:497: worst_left <= 1
FALLO tests/native/test_swrscan.c:498: worst_right <= 1
FALLO tests/native/test_swrscan.c:510: inside
#119 forma dibujada (dataset real de 496 puntos, 41 muestras del barrido):
  paso maximo entre puntos dibujados: 9 px
  caida maxima a la izquierda/derecha del valle: 2 / 3 px
125 comprobaciones, 4 fallos
```

**Arreglo**: reconstruir la imagen desde el arbol que tiene el #117 y **verificar el binario** antes
de flashear (que no contenga el resample del #104 / que el chart dibuje el valle del ajuste). El
codigo del arbol no se toca por este defecto: el test nuevo lo deja fijado (forma dibujada con el
dataset real, contra el resample que corra).

**Sin fundar** (anexo A): el cierre solo de la app al terminar el barrido (defecto 2 del #119). El
barrido del dialogo no termina solo —`do_step` envuelve `freq_index` y sigue midiendo— y en el codigo
**no hay ningun camino de fin de pasada que cierre el dialogo**: los unicos cierres son la tecla ESC
(`dialog_swrscan.c` `key_cb` → `dialog_destruct`), el `{"cmd":"app","text":"off"}` del control
remoto (`main_screen_close_app`) y las teclas de la barra de casa (`apps_disable`). El QA no guardo
el log de la GUI de esa corrida (`/mnt/app_logs/x6200_log_*.txt`), asi que el camino no se puede
fijar desde el repo. Lo que **si** quedo medido en el banco (2026-10-16, esta corrida): con el
barrido transmitiendo, el control remoto **descarta** el `btn` (no se puede apretar Stop por el
remoto) pero **acepta** `app off` —`apply_cmd()` deja pasar solo los comandos de parada
(`is_stop()`: `CMD_APP` con `val` 0 y los `app_cmds` con `val < stop`) cuando
`radio_get_state() != RADIO_RX`—, y las herramientas del QA mandan justamente `app off`
(`tools/prueba-apps-radio.sh`, `tools/captura-pantallas.sh`). Es el candidato mas probable y queda
como hipotesis en el anexo A, con lo que falta para fundarlo.

---

## 47. El fantasma del chart de ROE: el panel del dialogo es semitransparente

**Sintoma** (reporta el owner en el #119): al abrir el SWR Scan, dentro del grafico se ve un
rectangulo tenue con etiquetas que no son del chart ("etiquetas viejas"), y el QA lo leyo como
"region sucia del frame anterior" (`25c-roe-instantanea.png`).

**Causa, medida**: no es un repintado: es la **pantalla principal viendose a traves** del panel del
dialogo. El panel es una imagen del tema (`images/dialog.bin`, 9-slice) con **alpha 204/255** (80%
opaco) — medido leyendo el canal alfa del asset:

```
$ python3 -   # images/dialog.bin: header LVGL + BGRA, alfa del centro
images/dialog.bin      center alpha: mean 204.85   (histograma: 204 x 45.412 px)
images/dialog_dark.bin center alpha: mean 243.0    (histograma: 243 x 49.950 px)
```

y el chart era el **unico contenido del dialogo sin fondo propio**
(`lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, ...)`, `src/dialog_swrscan.c`), asi que lo que hay
detras se ve. Con la captura ampliada y con el contraste estirado se lee que el fantasma es
exactamente la **fila de frecuencia de la home** (la caja redondeada con `7.004 / 7.028.544 / 7.052`,
o sea el dial +- el ancho del FFT) y su **tira de bandas** (`40m CW`, `40m SSB`) — los mismos
valores del dial que la captura (7.028.544).

**Correccion de la hipotesis del issue**: "region sucia del frame anterior" / "invalidate
incompleto" queda **refutado**: en `frame-2.png` (el barrido corriendo, con el chart invalidado una
vez por paquete, ~28/s) el fantasma sigue ahi, igual que en `25c` (el chart vacio). Invalidar no
cambia la translucidez del panel.

**Arreglo** (display-only): el chart pinta su propio fondo con el rol del tema, el mismo idioma que
las tablas de contenido de los otros dialogos (`dialog_bt.c`, `dialog_cluster.c`) y el cuerpo de las
apps (`app_frame_body`):

```c
lv_obj_set_style_bg_color(chart, TC(panel), LV_PART_MAIN);
lv_obj_set_style_bg_opa(chart, LV_OPA_COVER, LV_PART_MAIN);
```

El rol `panel` de los seis temas que trae la ROM es casi el mismo color que el `dialog` que tapa
(Aurora `#1B1529` vs `#221A33`, Redshift `#0A0505` vs `#140809`, Sunlit `#FFFFFF` vs `#FFFFFF`,
Trail `#132117` vs `#1A2A1E`), asi que el cambio se ve como el grafico con su panel, no como un
rectangulo distinto.

**Verificacion, medida**: el test nuevo (`tests/native/test_swrscan.c::test_swrscan_render`) fija que
el objeto en el que el dialogo dibuja tiene fondo propio:

```
CHECK(swrscan_stub_chart_bg_opa() == LV_OPA_COVER);
CHECK(swrscan_stub_chart_bg_color_set());
```

con el stub del tema resolviendo `TC(panel)` a un color fijo (lo que se afirma es que el chart pinta
su fondo, no que color le da el tema).

---

## 48. El crash de CW del #116: la ventana de mensajes con la tabla vacia le pasaba NULL al keyer

**Sintoma** (reportado por el usuario, issue #116): *"recien toque algo y quedo sin audio"*. La GUI
crasheo (el pid paso de 311 a 1119, el daemon 298 la relanzo) y la radio quedo sin audio (lo maneja
la GUI).

**Medido en la radio** (`/mnt/app_logs/x6200_log_2026-10-16_22-01-44.txt`, la ultima linea es el
volcado; el binario que corria es el build del trunk `23926241`,
`md5 3604cece0e4382229a2de328d9baa614`, reconstruido y verificado en
`docs/evidencia/2026-10-16-cw116-banco/`):

```
CRASHLOG sig=11 fallo=(nil) pc=0xb5de0064 lr=0xb5ddf714 sp=0xbeb89ae0 base=0x44ea74
CRASHLOG-STACK 00000001 00000000 00000001 0124fbcc beb89afc 0047dacc b5ece000 b5dd58ac ...
```

**La pila resuelta** (los artefactos que corre la radio: `x6200_gui` reconstruido del `23926241`,
`libc.so.6` `md5 412df301b11e` y `liblvgl.so` `md5 cd30ca6c58ad`; resolucion reproducible con
`docs/evidencia/2026-10-16-cw116-radio/resuelve-pila.py`):

```
pc=0xb5de0064 -> libc+0xa0064 : strlen@@GLIBC_2.4+0x23
lr=0xb5ddf714 -> libc+0x9f714 : __strdup@@GLIBC_2.4+0xc
0x0047dacc -> enlace 0x4dacc : cw_encoder_send+0x68
0x004773cc -> enlace 0x473cc : dialog_msg_cw_beacon_cb+0x50
```

El `pc` y el `lr` caen en **libc**: `lr` es el retorno del `blx strlen` de `__strdup` (`0x9f714`) y
`pc` es el `ldrd r2,r3,[r1]` de `strlen` (`0xa0064`), con `r1 = r0 & ~7`. Los dos marcos de la GUI
que cierran la cadena son exactos: `0x4dacc` es el retorno del `bl strdup` de `cw_encoder_send` y
`0x473cc` el retorno del `bl cw_encoder_send` de `dialog_msg_cw_beacon_cb` (desensamblado en
`docs/evidencia/2026-10-16-cw116-radio/desensamblado.txt`).

**Causa** (codigo): la ventana de mensajes de CW llamaba al keyer con un mensaje NULL y el keyer lo
pasa a `strdup()`:

* `get_msg()` (`src/dialog_msg_cw.c:256`) devuelve NULL cuando la tabla de mensajes esta vacia
  (`table_rows == 0`); `table_rows` solo crece al leer la tabla `msg_cw` del `params.db`, que el
  esquema de la ROM crea **sin filas** (`sql/params.sql:55`, ningun `INSERT`).
* `dialog_msg_cw_beacon_cb()` (`src/dialog_msg_cw.c:296`) y `dialog_msg_cw_send_cb()` (`:282`) le
  pasaban ese NULL a `cw_encoder_send()` sin mirarlo.
* `cw_encoder_send()` (`src/cw_encoder.c:167`) hace `current_msg = strdup(text)`: con `text == NULL`,
  `strdup` llama a `strlen(NULL)`, que lee la direccion 0 -> `SIGSEGV` con `si_addr = 0`
  (`fallo=(nil)`), el `pc` justo en ese `ldrd`.

Los `CWDBG show` que el issue tomo como contexto son del **CW Trainer** (su `refresh_ui()` por el
timer, con el trainer abierto): son ruido de diagnostico (escriben crudo, sin pasar por el logger) y
**no** la causa. El crash es otro dialogo, la ventana de mensajes de CW, que se abre con la tecla
`MSG` en modo CW (`src/main_screen.c:783`) y crasheaba al tocar **Beacon** con la tabla vacia.

**Arreglo**:

* `src/cw_encoder.c`: `cw_encoder_send()` rechaza `text == NULL` (no-op: no arma el keyer ni toca la
  radio). Es el sitio del crash, asi que cubre a los seis llamadores por igual. Rechaza tambien el
  mensaje **vacio**: el ASan del suite nativo mostro que `endecode_thread()` avanza el puntero mas
  alla del terminador del buffer de `strdup("")` y lee fuera de el (con `get_msg()` devolviendo `""`
  cuando la celda no es valida, el camino era alcanzable desde la misma ventana).
* `src/dialog_msg_cw.c`: los callbacks de Send y Beacon miran el mensaje antes de llamar al keyer
  (NULL o vacio), como ya hacia `dialog_msg_cw_edit_cb()` en el mismo archivo.

En el mismo archivo del keyer quedaron dos defectos que el suite nativo saca a la luz al compilarlo
con `-Wall -Wextra`/`-fanalyzer`: `endecode_thread()` (que retorna por el `break` del `while`)
devolvia sin valor y `cw_encoder_send()` leia `param.sched_priority` sin comprobar el
`pthread_attr_getschedparam()`.

**Verificacion, medida** (test nativo `tests/native/test_cw_encoder.c`, el `src/cw_encoder.c` real
compilado contra stubs, seccion nueva en `tests/native/run_native_tests.sh`):

```
$ tests/native/run_native_tests.sh          # con el src sin arreglar
== CW encoder (#116): cw_encoder_send(NULL) no desreferencia NULL ==
Segmentation fault (core dumped)            # exit 139 (128+11): strdup(NULL) -> strlen(NULL)
$ tests/native/run_native_tests.sh          # con el arreglo
== CW encoder (#116): cw_encoder_send(NULL) no desreferencia NULL ==
12 comprobaciones, 0 fallos
```

El test no usa hilos: `tests/native/cwenc_stubs.c` corre `endecode_thread()` **sincronicamente**
(`pthread_create` del stub llama a la rutina; `join`/`cancel`/`detach` son no-ops), asi que el proceso
del test no tiene un segundo hilo y no puede llegar al `CHECK` de `AsanThread::Destroy()` del runtime
ASan de GCC 13 del runner de CI (el PR #120 murio ahi). Corrida bajo clang-18: seccion CW con
`-fsanitize=address` (exit 0, `12 comprobaciones, 0 fallos`) y con `address,undefined` (exit 0); el
ASan fue ademas el que encontro el OOB del mensaje vacio.

El test fija que `cw_encoder_send(NULL)` es un no-op (el keyer queda IDLE, no se toca la radio) y
que un mensaje real (`"E"`) sigue armando el keyer.

**Sin fundar** (anexo A): la secuencia exacta de teclas del owner (el camino del crash bajo radio es
`requires-radio`); lo fundado sin radio es el mecanismo (desensamblado + test). El banco no alcanza
el camino: la ventana de mensajes de CW se abre por la tecla `MSG` y el control remoto no tiene
comando de teclado (ver `docs/evidencia/2026-10-16-cw116-banco/`).

## 49. El widget de CW tune en el tema claro: las barras desaparecen sobre su propio panel

**Sintoma**: en el tema claro (**Sunlit**, el unico de los seis temas v2 con fondo claro, `#F7F5EB`)
las quince barras del widget de CW tune no se distinguen del panel del widget: se ve un rectangulo
gris vacio en lugar de las barras que marcan donde esta la senal. En los cinco temas oscuros las
barras se leen. Lo encontro la fixture `cw-tune-on` del arnes (`tools/ui_harness/`), no el usuario.

**Medido** (captura del banco, binario ARM real `md5 aaa60c1d3ab5`, tema Sunlit; los colores salen
del PNG con Pillow sobre la region del widget, `(30,70)` `139x40` = `WIDTH`x`HEIGHT` de
`src/cw_tune_ui.c`; evidencia en `docs/evidencia/2026-10-16-cw122-banco/`, captura
`04-cw-tuner-on.png`):

```
panel del widget (125,124,119)  n=4136   bbox (30,70)-(168,109)
barras           (125,125,122)  n=1272
contraste barra vs panel = 1.01:1        (mismo tema, tema oscuro Redshift: 2.00:1)
```

La misma medicion sobre la fixture del arnes con el tema **Redshift** (`06-harness-cw-tune-on.png`)
da panel `(5,2,2)` y barras `(65,64,64)`: ahi el contraste es `2.00:1` y las barras se ven. El
bloque **activo** no esta afectado: en Redshift mide `(90,178,89)` contra el panel = `7.82:1`, y
ademas lleva borde (§27).

**Causa** (codigo + la cuenta del blend): el panel del widget es **negro a 50 %** sobre el fondo de
la pagina (`cw_tune_style`: `lv_style_set_bg_color(lv_color_black())` con
`lv_style_set_opa(&cw_tune_style, LV_OPA_50)`, `src/styles.c:290-296`) y las barras son **gris
`0x7f7f7f` a 50 %** sobre ese panel (`rect_dsc.bg_color = lv_color_hex(0x7f7f7f)` con
`rect_dsc.bg_opa = LV_OPA_50`, `src/cw_tune_ui.c:60-63`). Sobre un fondo oscuro el panel queda casi
negro y el gris al 50 % se despega; sobre el fondo claro el panel queda en gris medio
(`(247,245,235)` con negro al 50 % -> `(125,124,119)`, la cuenta da `(124,123,118)`) y el gris al
50 % sobre ese gris medio cae casi en el mismo valor (`(125,125,122)`, la cuenta da `(126,126,123)`):
los dos terminan con el mismo brillo. La relacion no depende del valor de las barras — subirlo o
bajarlo no separa los dos grises —, sino de que la barra se mezcla **sobre su propio panel** con la
misma opacidad que el panel sobre la pagina.

**Estado**: **arreglado** (2026-10-17, rama `factory/cw-tune-122`, issue #122). Este hallazgo es
distinto del §27: aquel arreglo el borde del bloque **activo** (`src/cw_tune_ui.c:45`, de `1.65:1` a
`8.65:1`); aca el problema es de las barras **inactivas** contra el **panel** del widget, y ninguno de
los dos temas tenia un chequeo que lo cubriera (el barrido WCAG de §27/§28 mira los colores de la UI,
no el blend de una barra semitransparente sobre un panel semitransparente). Se destapa con la fixture
`cw-tune-on` porque es la primera que enciende el widget con un tema claro (esa fixture se llama hoy
`cw-tune-tuner`: los nombres `on`/`off` dejan de tener sentido con el arreglo, ver
`docs/evidencia/2026-10-16-cw122-banco/README.md`).

**Arreglo** (codigo: `src/cw_tune_ui.c`): las barras inactivas llevan borde de **1 px**
(`rect_dsc.border_width = 1`, `border_opa = LV_OPA_COVER`, `cw_tune_init`) cuyo color se elige en cada
dibujo contra la superficie que las rodea —el **panel**, no su propio relleno—, con el mismo criterio
y la misma cuenta que el bloque activo (§27) y que `band_plan.c`:

```c
/* update_cb */
rect_dsc.border_color = rel_lum(panel_color()) < LUM_CORTE ? lv_color_white() : lv_color_black();
```

`panel_color()` calcula el color real del panel (negro al 50 % sobre el fondo del tema,
`src/styles.c:290-296`), que no es un rol que el chequeo estatico pueda resolver. El **relleno** gris
queda como estaba: la cuenta de arriba muestra que subirlo o bajarlo no separa los dos grises (los
dos terminan con el mismo brillo); lo que hace visible el componente es su **borde**, que es lo que
WCAG 1.4.11 mide. Ancho **1** y no 2 como el bloque activo porque la barra mide `BLOCK_W=5` px: con
2 px de borde el relleno que marca el tono queda reducido a un pixel.

**Verificacion, medida** (misma region `(30,70)` `139x40`, mismo metodo Pillow sobre el PNG; el
"despues" esta medido en el **binario ARM real** del banco —el mismo camino que la radio— y en el
render del arnes, y los dos coinciden):

```
$ python3 docs/evidencia/2026-10-16-cw122-banco/mide.py <png...>
banco, binario ARM real, Sunlit   (04-cw-tuner-on.png -> 11-banco-cw-tune-sunlit.png)   1.01:1 ->  5.02:1
banco, binario ARM real, Redshift (12-banco-cw-tune-redshift.png)                                 -> 20.67:1
arnes, Sunlit   (cw-tune-tema-sunlit.png)                                                         ->  5.02:1
arnes, Redshift (06-harness-cw-tune-on.png -> cw-tune-tuner.png)                        2.00:1 -> 20.67:1
```

El "antes" del banco es la corrida del 2026-10-16 (`04`, con el widget encendido a mano porque el
parametro era el gate); el "antes" de Redshift es el render del arnes. El binario que dibuja el
"despues" del banco es `md5 66b766562d75` (rebuild de estos fuentes, sello `d9fac5d89c57`, aceptado
por el gate fail-closed de #86) y el del "antes" es `md5 aaa60c1d3ab5`, el mismo que el
`/usr/sbin/x6200_gui` de la radio.

Y el chequeo queda automatico en dos lugares (los dos fallan si el borde se pierde):

- `tests/check_wcag.py`: el par `cw_tune_borde` sobre `cw_tune_panel` es **exigible** (>= 3:1) en las
  diez paletas; medido, el minimo es `4.88:1` (Sunlit) y el maximo `21.00:1`. El mismo script ademas
  exige, leyendo la fuente, que `cw_tune_ui.c` ponga el borde y lo elija por la luminancia del panel
  (`$ python3 tests/check_wcag.py` -> `pares OK: 1052 | exentos: 170 | informativos: 90 | FALLAS: 0`;
  quitando el borde de la fuente el chequeo sale con la falla, probado).
- `tools/ab_testing/quality/nontext.yaml`: el objeto `cw_tune_bars` mide el **pixel** renderizado
  (borde vs panel) sobre los renders dorados `cw-tune-tuner` (Redshift) y `cw-tune-tema-sunlit`
  (Sunlit), con `min_ratio: 3.0` (WCAG 1.4.11 para un componente). El mismo objeto medido sobre la
  captura del banco de antes del arreglo da `1.01:1`, o sea que falla si el borde se pierde.

**Sin fundar** (anexo A): nada de este hallazgo. La corrida del 2026-10-16 habia dejado sin fundar el
"despues" con el **binario ARM real** (el banco estaba ocupado por otra sesion); el banco se libero
durante esta corrida y la medicion de arriba es de ese binario, con las capturas `11`-`14` en
`docs/evidencia/2026-10-16-cw122-banco/`.

---

## 50. La parabola de ROE invertida en la radio: el ajuste libre del render, no el camino de datos

**Sintoma** (reporta el owner el 2026-10-17): en la radio la curva de ROE del barrido sale **dada
vuelta** — sube desde la esquina inferior-izquierda hasta ~4.0 cerca del 60 % del chart y cae a 1.0
al final, un **domo** — y pide "base abajo y las puntas tienen que tender a infinito positivo". Su
captura es `docs/evidencia/2026-10-15-validacion-arreglos/26-roe-multipasada-final.png` (commit
`b39d6014`): readout `min 1.0 @ 6.891.334 kHz`, banner `Error: timeout de RF`, `n=44/51`,
`Passes: 1`, `Span: 200 kHz`, `Scale: Log`.

**La hipotesis del setup (dos contadores en el camino de datos) queda refutada.** La spec propuso que
el `RADIO_SWRSCAN` de `radio.c` auto-steppea el VFO por paquete mientras la sesion del #125 lleva su
propio indice, y que el desencaje explicaba el domo y el readout fuera de rango. No es asi:

- codigo: `src/radio.c:245-246` el caso `RADIO_SWRSCAN` **solo reenvia** el paquete
  (`dialog_swrscan_update(pack->vswr * 0.1f)`), y `src/radio.c:269-270` solo llama al watchdog
  (`dialog_swrscan_poll()`). `radio_start_swrscan()` (`src/radio.c:686-699`) pone el estado y la
  potencia de TX; **no mueve el VFO ni lleva un contador de puntos**.
- codigo: el indice y la direccion son **de la sesion y de nadie mas**:
  `src/swr_session.c:229-242` — "the storage index IS the grid index" (`pass_first_index()` /
  `pass_step()`), la direccion alterna por pasada segun `limits.alternate_direction`, y el dialogo
  pide la frecuencia con `swr_session_freq()` (`src/swr_session.c:221-227`). Un unico dueño.
- codigo/medido: el `6.891.334` **no esta fuera del span**: es el **borde inferior** de la grilla.
  `swr_grid_build()` (`src/swr_session.c:50-73`) la arma `center ± span/2` = `6.991.334 ± 100.000`,
  o sea `6.891.334 .. 7.091.334` (las tres etiquetas del eje son ticks interiores cada 50 kHz). El
  `min` sale de `swr_session_minimum()` sobre las muestras medidas, **nunca del ajuste dibujado**
  (`src/dialog_swrscan.c:358-380`), y vale 1.0 en el primer punto de la grilla: la base del domo,
  exactamente donde el domo toca el piso. El `n=44/51` es el **timeout de RF** (los ultimos 7
  paquetes no llegaron; `abort_text(SWR_ABORT_TIMEOUT)` = `"timeout de RF"`,
  `src/dialog_swrscan.c:313`).

**Causa, medida**: es el **render**, no los datos. `roe_curve_resample()` (`src/roe_curve.c`) ajusta
una **cuadratica libre** por minimos cuadrados y la dibuja tal cual: la orientacion no esta
restringida, asi que un barrido con forma de domo (coeficiente `a < 0`) se dibuja como domo. El mapeo
vertical es correcto y no puede dar un domo: `calc_y()` (`src/dialog_swrscan.c:295-307`) devuelve
`(1-x)*h` con `x` creciente con la ROE (log o lineal), o sea ROE alta **arriba** y 1.0 en el piso.
Medido con el `quadratic_fit` en C sobre el domo del owner: `a = -0.0061` (concava); sobre el dataset
escalonado del #119: `a = +1.026e-5` (ya abria hacia arriba).

**Arreglo** (solo render, blast radius minimo): despues del ajuste libre, si `a < 0` se lo espeja al
valle que tiene la **misma columna del vertice** y su base en el **minimo medido**
(`src/roe_curve.c:166-185`):

```c
xv = -b / (2.0 * a);        /* clamp a [0, n-1] si el vertice se va del tramo */
a = -a; b = -2.0 * a * xv; c = ymin + a * xv * xv;
```

Un ajuste que ya abre hacia arriba se dibuja igual que antes: los casos que funcionaban no cambian
(el clamp del vertice evita que un ajuste casi plano se vaya al techo). Los samples, el barrido, el
RF y el DSP no se tocan.

**Verificacion, medida** (TDD RED→GREEN, con el `roe_curve.c` de la base y con el fix):

- Nativo, bloque nuevo en `tests/native/test_roe_curve.c` (domo -> valle: base interior, valor 1.0,
  los dos extremos suben, un solo valle):

  ```
  RED   (roe_curve.c de 37de9937):  18522 comprobaciones, 156 fallos
  GREEN (con el fix):               roe_curve #125-fix: domo (fit a=-0.0061) -> valle en x=20.00, base 1.00, extremos 3.43 / 4.19
                                    18522 comprobaciones, 0 fallos
  ```

  El test del #119 no se mueve: `valle dibujado en la muestra 48.88` (391 comprobaciones, 0 fallos).

- Arnes, `tests/check_harness_swrscan_curve.sh` + `tests/swrscan_curve_evidence.py` sobre la fixture
  nueva `swrscan-owner-40m-domo` (el domo del owner embebido en `src/swr_backend.c`), banda del chart
  `y=69..399`, cuarto inferior `y>=316`:

  ```
  RED   (roe_curve.c de 37de9937):  domo: valle y=398 en x=10 (el BORDE); bordes suben 0 / 0 px   -> 3 FAIL
  GREEN (con el fix):               domo: valle y=398 en x=284; borde izq y=191 (sube 207 px), borde der y=121 (sube 277 px)
                                    valle real del owner: y=398 en x=149; bordes suben 20 / 12 px  -> PASS
  ```

  La fixture del valle real (`swrscan-owner-40m`) queda valle: el fix no la toca.

- Radio, con las capturas del QA del #130 y el script corregido por el #132 (1 argumento; la tinta
  se detecta por tema dentro de la banda `y=69..399`, descartando la grilla y el fondo):

  ```
  $ python3 tests/swrscan_curve_evidence.py docs/evidencia/2026-10-17-qa-radio-swr125/01-swrscan-50k-dial7000-U.png
         tinta: RGB(255,179,160) (detectada en la banda y=69..399)
         valle: y=398 en x=255; bordes: izq y=304 (x=10), der y=230 (x=696)
         forma: U
    ok   captura: borde izquierdo sube 94 px sobre la base (>=3)
    ok   captura: borde derecho sube 168 px sobre la base (>=3)
  PASS: la curva dibujada es un valle (base abajo, puntas hacia arriba)     [exit 0]

  $ python3 tests/swrscan_curve_evidence.py docs/evidencia/2026-10-15-validacion-arreglos/26-roe-multipasada-final.png
         tinta: RGB(255,179,160) (detectada en la banda y=69..399)
         valle: y=398 en x=10; bordes: izq y=398 (x=10), der y=398 (x=679)
         forma: domo
  FAIL captura: valle interior (x=10, bordes 10..679)                       [exit 1]
  FAIL: 3 comprobacion(es) de forma
  ```

  Antes del #132 el script pedia 2 argumentos (exit 2 con 1) y su umbral `min(r,g,b)>=180` no veia
  la tinta de Redshift `RGB(255,179,160)` (`min=160`): la captura de radio daba "no encontre la
  curva". El #132 separa la deteccion de tinta (automatica, por tema) de los umbrales de forma, que
  no cambian.

**Sin fundar** (anexo A): **el mecanismo en hardware** que hizo que el barrido medido saliera en domo
(por que la ROE subio a ~3.8 en el medio en vez de bajar). El host no transmite, asi que el camino de
adquisicion real no se puede reproducir; la confirmacion final del U **en la radio** es del owner
(`requires-radio`, con una pasada y carga ficticia). El `timeout de RF` (`n=44/51`) del lado radio
tampoco se puede fundar desde el repo: el host entrega los 51 paquetes.


---

## 51. Las caidas del #121: una ultima respiracion persistente (y el aviso de bateria baja)

**Sintoma** (issue #121): cuatro caidas del **equipo entero**, sin CRASHLOG de la GUI. La cuarta
fue **en reposo**, tras 14 h 20 min de uptime, y no volvio sola (hizo falta ciclo de energia).
El log vivo de la GUI esta en `/tmp` (tmpfs) y solo se archiva al salir ordenadamente: cada corte
duro se lleva la evidencia. La radio no estuvo accesible en esta corrida, asi que no hubo prueba
en hardware.

**Lo implementado** (`src/lastbreath.c/h`, integrado en `main.c`, `clock.c`, `events.c`, `msg.c`,
`main_screen.c`, `radio.c`):

- cada 10 s (`X6200_LASTBREATH_SECS`) escribe de forma **atomica** (`tmp` + `fdatasync` +
  `rename`) una linea de ~180 B en `/mnt/lastbreath` con uptime, `boot_id`, estado de radio,
  TX, app abierta, voltaje de bateria/fuente (`bat=na` si el flow no publico muestra), cola de
  eventos, ultimo evento y ultimo mensaje de la GUI; sin heap en el camino de escritura;
- al arrancar, si la anterior existe y no tiene `clean=1`, deja **una linea** `LASTBREATH ...`
  para el triage; la salida limpia (`atexit` / apagado por software) marca `clean=1`;
- si el path no existe (sin `/mnt`, banco) avisa una vez y sigue: no rompe la GUI ni crea nada.

**Costo medido**: linea de 180 B cada 10 s = **1.080 B/min**; en host 0,32 us de formateo y 19 us
de escritura atomica; en QEMU `cortex-a7` el propio registro midio `wus=1021..3197` us por
escritura (con `fdatasync`). Es configurable para bajar la frecuencia si el owner prefiere menos
desgaste de SD.

**Bateria**: el firmware ya tenia el monitor (`radio.c` publica `vbat/vext/batcap/charging` a
`clock_update_power`) y los umbrales del perfil visual (`VP_CRITICAL_ENTER_MV 10800`,
`VP_CRITICAL_EXIT_MV 11300`), pero **no un aviso textual**. Se agrego solo eso: un mensaje
`Battery low: X.X V` + `BATTERY-LOW` en el log con los umbrales existentes. No bloquea TX ni
apaga nada. **El umbral requiere calibracion del owner en radio**: el issue midio 8,6 V, que cae
dentro del aviso, pero no se pudo confirmar contra el pack real.

**Verificacion**: TDD nativo RED→GREEN (formato determinista, rename atomico, deteccion de corte
sucio, degradacion sin filesystem; 48 comprobaciones). Banco QEMU: el archivo se escribe cada
10 s, y tras un `docker rm -f` (SIGKILL) el arranque siguiente lo reporto como `LASTBREATH` con
el uptime y el payload crudos. A/B `AB-HANG121` pixel-identico. Evidencia completa en
`docs/evidencia/2026-10-18-hang121-banco/`.

**Sin fundar**: la causa de las caidas. No hay `pstore`/ramoops ni consola serie; no se midio
tension, corriente, temperatura ni watchdog. La hipotesis de undervoltage (8,6 V; ~2,15 V/celda
si es 4S LiFePO4, ~2,87 V/celda si es 3S Li-ion) sigue sin fundar. En la proxima caida hay que
leer `/mnt/lastbreath` y la linea `LASTBREATH` antes de dar ciclo de energia; el procedimiento
esta en el README de la evidencia.


---

## 52. El tono de CW se mide con el signo invertido (y el auto-ajuste lo compensa)

**Sintoma**: ninguno visible por si solo; aparecio al escribir el auto-ajuste del #139, que necesita
saber para que lado mover el centro del DDS del decodificador.

**Medido**: la formula de `peak_freq` (`src/cw.cpp:261`) sobre un tono complejo de +100 Hz, con FFT
de 128 puntos a la tasa decimada (44100/64 = 689,0625 Hz), corrida con liquid 1.4.0 en el contenedor
del arnes (`docs/evidencia/2026-10-18-cw139-banco/signo-peak-freq.c`):

```
$ cc -O1 signo-peak-freq.c -o signo -lliquid -lm && ./signo
tone  -300.0 Hz -> bin  72 -> formula  +301.46 Hz
tone  -200.0 Hz -> bin  91 -> formula  +199.18 Hz
tone  -100.0 Hz -> bin 109 -> formula  +102.28 Hz
tone  +100.0 Hz -> bin  19 -> formula  -102.28 Hz
tone  +200.0 Hz -> bin  37 -> formula  -199.18 Hz
tone  +300.0 Hz -> bin  56 -> formula  -301.46 Hz
```

**Que significa**: la formula devuelve el **negativo** del corrimiento del tono respecto del centro
del DDS, no el corrimiento. Quien la use como "cuanto mover el centro" tiene que negarla; el
auto-ajuste aplica `freq = -peak_freq` (`src/cw_autotune.c`, `cw_autotune_compute`) y por eso
converge en el sentido correcto. El signo del widget de tune (`cw_tune_set_freq`, `src/cw_tune_ui.c`)
no se toca: es otra convencion y no es parte del #139.

**Sin fundar** (anexo A): si la convencion del widget (bloque a la izquierda/derecha) es la que el
owner quiere. El widget no tiene rotulos de eje, asi que no se puede afirmar que este "al reves".

---

## 53. El auto-ajuste de peak beta no puede variar con la WPM: el bloque de FFT es mas largo que medio punto

**Codigo**: `src/cw.cpp` hace una FFT de 128 muestras sobre el audio decimado 44100/64 = 689,0625 Hz,
sin solapamiento (`FFT=128`, `NUM_STAGES=6`, `DECIM_FACTOR=64`) y llama `update_thresholds()` una vez
por trama: **delta = 128/689,0625 = 185,76 ms** (5,38 tramas/s). El LPF del umbral de pico se
actualiza a ese ritmo.

**La formula del #139** es `beta = 1 - delta/tau` con `tau = 600/WPM ms` (medio punto Morse). Para
que de un beta mayor que 0,10 hace falta `tau > ~0,2 s`, es decir un punto de mas de 0,4 s: menos de
3 WPM. La tabla (derivada en `docs/cw139-spec.md` y fijada por `tests/native/test_cw_autotune.c`):

| WPM | punto (ms) | tau (ms) | 1 - delta/tau | beta |
|---|---|---|---|---|
| 5 | 240 | 120 | -0,55 | **0,10** |
| 10 | 120 | 60 | -2,10 | **0,10** |
| 20 | 60 | 30 | -5,19 | **0,10** |
| 30 | 40 | 20 | -8,29 | **0,10** |
| 40 | 30 | 15 | -11,38 | **0,10** |
| 60 | 20 | 10 | -17,58 | **0,10** |

**Medido**: la suite nativa da 0,10 para 5, 10, 20, 30, 40 y 60 WPM (vector
`test_peak_beta_clamps_and_short_block`, `docs/evidencia/2026-10-18-cw139-banco/tdd-green.log`). Con
el bloque sin decimar (128/44100 = 2,90 ms) la misma funcion da 0,95 @10, 0,90 @20, 0,81 @40 y
0,71 @60: la formula no es constante, el lazo es el que no puede cumplirla.

**Consecuencia**: con el lazo actual el unico valor correcto para `peak_beta` es el minimo (0,10),
que es el default del firmware. El AUTO lo deja ahi (no empeora el ajuste manual) y la adaptacion
real ocurre en `snr`, `gist`, `noise_beta` y el tono. Si en la radio se ve chatter con senales
fuertes, la alternativa es `tau = 8*delta` (beta 0,88) — **requires-radio**, no se cambio sin medir.

---

## 54. La linea central del waterfall del #140: el defecto de alto 0 no existe en el trunk

**Sintoma** (issue #140, lo reporta el owner): "la linea central del waterfall no se ve"; el issue
lo atribuye a que la `lv_line` se crea con **alto 0** y nadie vuelve a fijar sus puntos.

**Causa, medida**: esa premisa es falsa en el trunk actual. La linea se arma **al final** de
`waterfall_set_height()` (`codigo: src/waterfall.c:187`), despues de `height = lv_obj_get_height(obj)`
(`codigo: src/waterfall.c:169`), y `draw_middle_line()` ya hacia `middle_line_points[1].y = height` +
`lv_line_set_points()` (`codigo: src/waterfall.c:201-208` en `94c2c6ab`). Ademas la linea y las filas
del waterfall comparten `height`: si midiera 0, el waterfall tambien, y el waterfall se ve.

Medido en el banco QEMU (`cortex-a7`, tema Sunlit, binario del trunk `2a0be37fdb29`, 13.773.024 B;
el banco se corrió **antes** del rebase corto sobre el trunk, con el trunk de entonces — la nota del
rebase y qué queda pendiente están en el README de esa evidencia):

```
$ CHROOT=/tmp/opencode/vfix3-wf140 RECONSTRUIR=si ESPERA=12 tools/captura-banco.sh red-trunk.png '{"cmd":"get"}'
OK: /tmp/opencode/vfix3-wf140/usr/sbin/x6200_gui es el firmware del port (ELF32 ARM, 13773024 B, 3/3 marcas)
  respuesta: {"t":"state","vfo":"A","freq":6991334,...}
  version: 94c2c6ab QA: REPRODUCIDO el SIGSEGV del #131 ...
  fuente: md5 37f55d16fed4 (src/ + lvgl/src)
  binario: md5 2a0be37fdb29   qemu-cpu: cortex-a7

$ python3 docs/evidencia/2026-10-18-wf140-banco/medir-linea.py red-trunk.png
banda del waterfall: y=240..479 (240 filas)
columna central x=400 vs x-4 y x+4: 207/208 filas medibles con diferencia = 99.5 %
corrida constante sobre x=400: 189/240 filas, color RGB(11, 41, 66); x-1=RGB(11, 41, 66) x+1=RGB(0, 0, 0)
```

La corrida horizontal sobre `x=400` es de **3 px (x=398..400)** en **222/227** filas con color de
linea: es el ancho que le da `waterfall_signal_width=3` de Sunlit (`#92`), de punta a punta. En la
radio la linea tambien esta: `docs/evidencia/2026-10-17-qa-radio-swr125/07-restaurado.png` (Redshift,
1 px = ancho del tema) mide **217/217** filas medibles. Y en el arnes host, `ui_harness --inspect`
sobre `main-tema-sunlit-agua.scn` muestra la `lv_line` viva con `w=3 h=243` en `x=399 y=239`
(`h` = alto del waterfall 240 + trazo 3), `hidden:false`, clip de 240 px.

**Correccion de lo que decia antes este registro**: nada del registro previo; el #92 ya habia medido
la linea de 3 px el 2026-10-15. Lo que se corrige es la premisa del issue #140.

**Arreglo** (endurecimiento del invariante, sin cambio de render; commit `d2dd92a3`):

- `draw_middle_line()` crea la linea **una sola vez por `obj`** (una llamada de mas dejaba la linea
  vieja con el alto viejo y el callback duplicado), recalcula el punto inferior en cada llamada y
  resuelve la visibilidad con `params.waterfall_center_line` en el acto;
- `middle_line_cb()` NULL-safe; `waterfall_init()` resetea `middle_line` (la linea pertenece al
  `obj` de esa pantalla); `waterfall_middle_line()` expone el objeto vivo para el arnes;
- `tools/ui_harness/src/screens.c`: `scenario_settle()` corta la corrida si la linea no mide el alto
  del waterfall + trazo, no esta centrada, esta oculta con el parametro encendido o no es el ultimo
  hijo del waterfall.

**Verificacion, medida**:

- **A/B sin cambio de render**: el trunk y el fix dan la misma banda del waterfall y las mismas
  columnas de la linea (0 px distintos en `y>=240` y en `x=396..404`; los 1636 px que difieren son el
  reloj, capturas de distinto minuto). Las **87** fixtures del arnes dan PNG byte-identicos antes y
  despues (`x6200-ab compare --a b3c873c7 --b d2dd92a3`: `captures are pixel-identical to the
  baseline`, 87 escenarios capturados en las dos revisiones; eran 85 antes del rebase corto, que trae
  las dos del AUTO de CW del #139/#142), y las 11 goldens de `tools/ab_testing/golden/` quedan
  identicas (sin re-baseline; re-verificadas 11/11 contra las capturas del compare en
  `docs/evidencia/2026-10-18-wf140-contraste/gates-ab.log`).
- **Parametro**: con `waterfall_center_line=0` la linea no se dibuja (0/208 filas, `x=400` negro) y
  con `=1` vuelve (3 px).
- **Reconstruccion**: abrir `Status` y cerrarlo deja la home **byte-identica** a la pristina
  (`md5 c39744054f74...`), o sea el waterfall y su linea sobreviven al ciclo de la app.
- **El gate no es decorativo**: con `middle_line_points[1].y = 0` el arnes sale exit 1 con
  `la linea central mide 3 px y el waterfall 240 (+3 de trazo): largo cero o alto viejo (issue #140...)`.

Evidencia completa (capturas, mediciones, logs, md5): `docs/evidencia/2026-10-18-wf140-banco/`.

**Sin fundar** (anexo A): lo que **vio** el owner. En el trunk la linea esta, con el ancho del tema;
desde aca no se puede ver su `params.db` ni su build. Lo que no se veia era su **color**: ver el
[hallazgo 55](#55-la-linea-central-del-waterfall-del-140-la-geometria-estaba-el-color-no-medido-por-tema).

---

## 55. La linea central del waterfall del #140: la geometria estaba, el color no (medido por tema)

**Sintoma** (orden del owner, 2026-10-18): *"no veo la frecuencia que estoy sintonizando; no se ve la
linea"*. El [hallazgo 54](#54-la-linea-central-del-waterfall-del-140-el-defecto-de-alto-0-no-existe-en-el-trunk)
ya mostro que la linea **esta** en el trunk (3 px en Sunlit y Trail, 1 px en el resto, de punta a
punta). Lo que no se leia era su **color**.

**Causa, medida**: el trunk la dibujaba en el rol `panel_border` del tema, a `LV_OPA_60` (60 %) y con
`LV_BLEND_MODE_ADDITIVE`. Sobre el piso del waterfall eso da, por tema
(`medido: docs/evidencia/2026-10-18-wf140-contraste/derivar-piso.py`, que lee los temas del repo y
compone `piso + (canal * 153) >> 8`; el `>> 8` es lo que reproduce el `(11,41,66)` medido en el
banco sobre negro):

| Tema | piso del waterfall (`waterfall[0]`) | tinta del trunk | contraste | tinta del fix (`text`, opacidad plena) | contraste |
| --- | --- | --- | --- | --- | --- |
| Aurora | `#140d22` | `#665686` | 2.92:1 | `#f8eeff` | 16.81:1 |
| Cathode | `#090605` | `#59391f` | 1.95:1 | `#f9d9aa` | 14.94:1 |
| Redshift | `#000000` | `#5c352f` | 2.01:1 | `#ffb3a0` | 12.21:1 |
| Sprint | `#030a17` | `#2f4b70` | 2.23:1 | `#f5f7f9` | 18.45:1 |
| Sunlit | `#ffffff` | `#ffffff` | 1.00:1 | `#162436` | 15.67:1 |
| Trail | `#0a150d` | `#425d3f` | 2.55:1 | `#f2eed8` | 15.98:1 |

El piso del producto **no** es el negro del banco QEMU: el buffer del widget nace en cero
(`codigo: src/widgets/lv_waterfall.c:69-70` y `:81-88`) y con el waterfall corriendo cada fila se
pinta con `palette[id]`, el valor minimo con `palette[0] = waterfall[0]`
(`codigo: src/widgets/lv_waterfall.c:118-121` y `src/theme.c:441-454`); el piso (percentil 10) de los
seis fixtures del arnes cae en el extremo bajo de cada tema (Sunlit `#f1f7fb`, Redshift `#0a0203`,
Aurora `#23193e`, ...). En **Sunlit** —el tema claro— el piso es blanco y el 60 % aditivo del borde
del panel satura a `#ffffff`: la linea se dibujaba **del mismo color que el fondo**.

La composicion queda cruzada contra lo medido: la tinta derivada de Sunlit sobre negro (`#0b2942`) es
el `RGB(11,41,66)` que el banco mide en el trunk, y la de Redshift sobre el piso del tema
(`#5c352f`) es el `#5b352f` que aparece en 45 filas de la captura de la radio (1/256 de diferencia
en rojo).

**Medido en la radio** (Redshift, `docs/evidencia/2026-10-17-qa-radio-swr125/07-restaurado.png`, 1 px
= el ancho por default del trunk): `medido: medir-banco.py` da 2.27:1 en el par del gate
(`#6c3934` vs `#000000`) y 1.98:1 contra el vecino inmediato (mediana, tinta `#5b352f`..`#733b36`).

**Verificacion, medida**: el objeto `waterfall_center_line` declarado en
`tools/ab_testing/quality/nontext.yaml` **rechaza** los renders del trunk y pasa con los del fix:

```
$ uv run x6200-ab themes --repo ../.. --renders-dir /tmp/wf140/antes/on
  non-text (WCAG 1.4.11, pixels): 6 object(s) measured, 1 failure(s)
    FAIL waterfall_center_line on main-tema-sunlit-agua = 2.40:1 (min 3.0:1) [object darker; #51a9df vs #f1f7fb]
$ uv run x6200-ab themes
  non-text (WCAG 1.4.11, pixels): 19 object(s) measured, 0 failure(s)
    OK   waterfall_center_line on main-tema-sunlit-agua = 14.51:1 (min 3.0:1) [object darker; #162436 vs #f1f7fb]
```

**Arreglo** (commit `6cad2bf6`): la linea usa `TC(text)` del tema a `LV_OPA_COVER`, sin blending, y
el ancho por default pasa de 1 a 3 px (`src/theme.c` `use_base()`, clave `waterfall_signal_width`,
clamp 1..8). El invariante queda codificado en el arnes (`tools/ui_harness/src/screens.c`,
`settle_waterfall_center_line`: ancho == el del tema, `LV_OPA_COVER` y `TC(text)`); con el parametro
apagado el arnes exige que la linea no se vea. Los 7 renders con el parametro apagado y las otras 9
goldens quedan byte-identicas: se re-baselinearon 2 goldens (`main-espectro-agua.png`,
`main-tema-sunlit-agua.png`), declaradas en `tools/ab_testing/golden/README.md`.

Evidencia completa (renders del arnes, banco QEMU, radio, mediciones, crops x4):
`docs/evidencia/2026-10-18-wf140-contraste/`.

## 56. El SIGSEGV del #131: el hilo de radio tocaba LVGL con las teclas del panel (hkey_put)

**Sintoma** (issue #131, reabierto el 18/10 22:06): tercer `SIGSEGV` con la misma firma. El
`CRASHLOG` medido en la radio dice `sig=11 fallo=0x15 pc=0xb6186ae8 lr=0xb618d4f0` y las primeras
palabras de la pila son `b618db48 00000000 b6196128`, iguales a las de los dos crashes anteriores
(que tenian `fallo=0xf363`, o sea el mismo `pc`/`lr` y otro puntero roto). El contexto es
`on_high_filter_change: Radio set filter_high=3000` con `filter_low=200` y `Saved band=7`, y el
`LASTBREATH` del momento es `radio=RX tx=0 app=home bat_mv=8700`.

Con `liblvgl` `cd30ca6c58ad` y base de carga `0xb6170000` (resuelto en #137), `pc` cae en
`_lv_event_mark_deleted+0x30` y `lr` en `lv_obj_destructor+0x10`; la pila resuelve
`obj_del_core -> _lv_obj_destruct -> lv_obj_destructor -> _lv_event_mark_deleted`. En
`lvgl/src/core/lv_event.c`, `event_head` es un estatico (linea 37) que **solo** se asigna dentro de
`lv_event_send` (lineas 72 y 78) y que `_lv_event_mark_deleted` recorre sin lock (lineas 151-158):
la lista de eventos quedo apuntando a un frame de pila muerto.

**La hipotesis del ticket no se sostiene tal cual**. Se auditaron todas las suscripciones
**no diferidas** del camino banda/filtro/modo (`subject_add_observer*` y `->subscribe`, sin
`ObserverDelayed`): ninguna llama a LVGL. Todas copian escalares, escriben SQLite o hablan con la
SDK de la radio:

| Registro (codigo) | Observer | Hilo | Toca LVGL? | Que toca |
|---|---|---|---|---|
| `src/cfg/band.c:109-125` | `on_ab_*`, `on_band_id_change`, `on_vfo_change`, `on_cur_mode_change` | el que llama a `set()` | **no** | guarda/carga params en SQLite y copia valores a los sujetos `cfg_cur.*` |
| `src/cfg/mode.c:65-91` | `on_cur_mode_change`, `on_cur_filter_*`, `update_cur_low/high_filter`, `update_real_filters` | el que llama a `set()` | **no** | recalcula low/high/bw y los filtros reales; SQLite al cargar |
| `src/cfg/cfg.c:210,409-423` | `on_item_change`, `update_sql_level`, `update_fft_width` | el que llama a `set()` | **no** | escribe el item en params/SQLite y actualiza escalares |
| `src/cfg/atu.c:66-68` | `update_atu_network` | el que llama a `set()` | **no** | cache en RAM, SQLite y sujetos `atu_network.*` |
| `src/radio.c:505-525` | `on_vfo_*`, `on_low/high_filter_change` | el que llama a `set()` | **no** | `x6200_control_*` (SDK) y `LV_LOG_USER` |
| `src/spectrum.c:317-328`, `src/waterfall.c:92-109`, `src/band_info.c:158`, `src/dsp.cpp:152-153`, `src/cw.cpp:85-90`, `src/cat.cpp:1020` | observers de ancho de FFT, filtro real, modo, grilla y frecuencia | el que llama a `set()` | **no** | escalares, buffers de PSD/waterfall y la cola del CAT |

El mecanismo (`src/cfg/subjects.cpp:169-173`) notifica en el hilo que llama a `set()`, sin lock
sobre la lista de observers; por eso la auditoria se hizo observer por observer y no por el
mecanismo. En esta app esos `set()` del camino banda/filtro/modo llegan del hilo de radio
(`radio_tick`/SDK), del hilo de LVGL (dialogos, botones, rueda) o del hilo del CAT: por eso la
columna que decide es **toca LVGL?**, y en las 6 filas la respuesta es no.

**El defecto que si se fundo** es otro, en el mismo camino de teclas del panel: `hkey_put()` (base
`4e000bca`, `src/hkey.c:44`; el mismo cuerpo que tenia en `94c2c6ab` — el trunk no toco
`src/hkey.c` en el medio) lo llama `radio_tick()` desde el hilo de radio (`src/radio.c:290`,
`hkey_put(pack->hkey)`) y su cuerpo tocaba LVGL ahi mismo:

- `lv_group_get_editing()` para las teclas numericas;
- `event_send_key()` (`src/events.c:340-352`), que lee `lv_group_get_focused()`;
- `backlight_tick()` (`src/backlight.c:60-80`), que hace `lv_timer_create`, `lv_timer_reset`,
  `lv_timer_set_period` y `lv_timer_set_repeat_count`;
- el timer del long-press (`lv_timer_del/create/set_repeat_count`) y la mutacion de
  `event.state`/`timer`, compartidos con el hilo de LVGL.

`hkey_put()` tiene un solo llamador en `src/` (verificado con
`grep -rn "hkey_put" src`): `radio.c:290`. El resto del camino de `radio_tick` ya estaba diferido:
`msg_schedule_text_fmt` y `notify_tx`/`notify_rx` usan `event_send` (cola con mutex),
`tx_info_update`, `dialog_swrscan_update` y `apply_visual_profile` usan `scheduler_put*`,
`clock_update_power` y `visual_policy_eval` no tocan LVGL, y `dsp_samples` es DSP.

**Fix** (commit `7f36f607`, `src/hkey.c`; el mismo arbol de `src/hkey.c` que `5e425df7` — el rebase
sobre el trunk cambio el padre, no el fix): `hkey_put()` solo publica la tecla con
`scheduler_put(hkey_entry, &key, sizeof key)` y el cuerpo (`hkey_process`) corre en el hilo de
LVGL. Un `atomic_bool key_armed` (lo prende el productor al publicar una tecla y lo apaga el
consumidor al procesar el release) evita encolar los ceros que manda el flow cuando no hay tecla
apretada: la maquina de estados conserva su semantica exacta sin generar trabajo por tick.
`hkey_event()` ademas chequea el `malloc` que `-fanalyzer` marcaba en el archivo tocado. No cambia
nada visible.

**Verificacion** (todo en `docs/evidencia/2026-10-18-lvgl131/`):

- **TDD nativo**: `tests/native/test_hkey.c` corre el modulo real contra stubs que registran el
  hilo de cada llamada LVGL. Con el `hkey.c` de la base: `92 comprobaciones, 18 fallos` (exit 1);
  con el fix: `92 comprobaciones, 0 fallos` (exit 0). El caso cubre diferimiento, press/release,
  long press, cero sin press, digito con y sin edicion, y orden FIFO.
- **Suite nativa completa** (`tests/native/run_native_tests.sh`): exit 0, con el bloque nuevo y
  `-fanalyzer` sin observaciones sobre `hkey.c`.
- **8/8 chequeos estaticos** de la app y `bash -n` del script tocado: exit 0.
- **Cross-compile ARM** con el toolchain de buildroot: build completo y linkeado (`100% Built
  target x6200_gui`); los unicos warnings son 3 preexistentes en `src/widgets/lv_waterfall.c`
  (archivo no tocado). El binario que corrio en el banco es `ELF32 ARM`, 14.293.044 B,
  `md5 9042691874a754782601624cd4c4acc3` (`banco-smoke.log`). **Estos dos gates (ARM y banco) son
  del arbol pre-rebase `5e425df7`** y no se repitieron sobre el rebase: el `src/hkey.c` del fix es
  identico en `7f36f607`, pero la base cambio (el trunk toca `src/theme.c`, `waterfall.c`, `cw.cpp`),
  asi que ese `md5` es del binario de aquel arbol, no del commit de esta rama. Repetirlos pide el
  host del owner (chroot + buildroot), no el runner.
- **A/B `AB-LVGL131`** (`4e000bca` contra `7f36f607`, **re-generado tras el rebase**): PASS,
  87 escenarios **pixel-identicos** y 893 chequeos de contrato.
- **Determinismo del arnes de UI**: PASS (PNG identicos por fixture; el header del log dice
  `revision 7f36f607`). **Temas**: OK (90 pares, 0 fallos).
- **Matriz de la app** (`scripts/apps/x6200/check-matrix.sh`): exit 0. El banco QEMU figura
  **BLOCKED** porque el runner no tiene el chroot ni el arbol de buildroot en las rutas por
  defecto; se corrio aparte con `CHROOT`/`BUILDROOT` del host del owner (ver abajo).
- **Banco QEMU** (ARM real emulado, sin radio): smoke OK y **95 ordenes** de banda/filtro/modo
  (5 rondas de 19, `band`/`filter`/`mode` por el socket de control), **0 rechazadas**, GUI viva y
  **sin `crash.txt`**. Binario probado: `md5 9042691874a7`.

**Lo que el banco no puede probar, y receta para la radio**: el banco no tiene flow, asi que no
corre `radio_tick()` ni `hkey_put()`. La validacion dinamica del fix es en la radio, con el binario
de este commit:

1. desplegar con `tools/prueba-gui-en-radio.sh` (pasa antes por `verifica-firmware-port.py`) y
   confirmar el md5 que corre;
2. con la GUI en home y la radio en RX, repetir 200 veces este ciclo, sin tocar el control
   remoto: **tecla FIL** (abre/cierra el filtro), **rueda TUNE** para mover el filtro bajo y alto,
   **tecla MODE** (LSB -> USB -> CW -> AM -> LSB) y **tecla BAND / app Bands** para cambiar de
   banda; entre paso y paso, apretar **F1** y **F2** (las dos van por `hkey_put`);
3. al terminar cada ronda de 20 ciclos, verificar `CRASHLOG` 0 en el log vivo y que el `pid` de
   `x6200_gui` sea el mismo (`pidof x6200_gui`); si cambio, la GUI se relanzo;
4. cerrar con `tools/verifica-radio.sh` (no tiene que reportar senales de crash) y leer
   `/mnt/lastbreath`: la linea tiene que mostrar `clean=1` tras el apagado ordenado; si hay
   `LASTBREATH` al arrancar, el corte fue sucio.

**Sin fundar (anexo A)**: el paso exacto por el que ese acceso fuera de hilo termina corrompiendo
`event_head`. Se fundo el acceso LVGL fuera de hilo, no la escritura que rompe la lista: la
corrupcion necesita concurrencia real y el banco no ejecuta `radio_tick`. Tampoco se fundo que el
trigger sea *solo* el cambio de filtro; los dos crashes tienen el mismo `pc`/`lr` con contextos
distintos (`on_vfo_mode_change` y `on_high_filter_change`).

---

## 57. El espectro del #149: el dibujo leia los buffers sin lock (carrera confirmada con TSan)

**Sintoma** (issue #149, hallazgo estatico a verificar): `spectrum_data()` publicaba
`spectrum_buf[]`/`spectrum_peak[]` bajo `pthread_mutex_lock(&data_mux)`, pero `spectrum_draw_cb()`
los leia **sin** el mutex, y `spectrum_clear()` los reiniciaba sin lock. El issue pedia verificar
si escritor y lector corren en hilos distintos antes de atribuir nada.

**Los hilos** (codigo, base `a69ea288`): el escritor es `radio_thread` (el `pthread_create` de
`src/radio.c:614`) → `radio_tick` → `dsp_samples` (`src/dsp.cpp:168`, llamado desde
`src/radio.c:223`) → `spectrum_data()` (`src/spectrum.c:332`). El lector es el hilo principal, que
corre el lazo `scheduler_work(); lv_timer_handler();` (`src/main.c:613`) y desde ahi entra
`spectrum_draw_cb` (`src/spectrum.c:85`). El `data_mux` (`src/spectrum.c:64`) solo lo tomaba
`spectrum_data()` (`:335`): iban sin lock las lecturas `spectrum_buf[sp_ind]` (`:138`),
`spectrum_buf[i]` (`:141`), `spectrum_peak[i].val` (`:158`), `spectrum_tx` (`:93`),
`grid_min/grid_max` (`:98-99`), `width_hz` (`:196`), `filter_from/to` (`:201-202`), `dnf_*`
(`:212-219`) y `cur_mode` (`:268`), y las escrituras de `spectrum_clear()` (`:398-402`),
`on_cur_freq_change()` (`:452-462`), `spectrum_update_min/max()` (`:376-391`) y los observers de
grilla (`:422-431`). Los observers no diferidos notifican en el hilo que hace `set()`
(`src/cfg/subjects.cpp:169-173`), asi que esas escrituras llegan del hilo de LVGL, del de radio o
del del CAT segun quien cambie el parametro.

**Carrera confirmada con TSan** (medido, host x86_64 con gcc 13 dentro de `x6200-uiharness`; el
binario ARM no corre bajo TSan en este arnes, asi que el resultado es del codigo de
sincronizacion, no del Cortex-A7). Comando y salida completa en
`docs/evidencia/2026-10-19-spectrum-149/tsan-base.log`; 7 reportes, todos sobre los globales
`spectrum_buf`/`spectrum_peak`/`spectrum_tx`:

```text
SUMMARY: ThreadSanitizer: data race /sp/redbase/spectrum.c:141 in spectrum_draw_cb
  Write of size 4 by thread T7:  spectrum_data  /sp/redbase/spectrum.c:338   [hilo de radio]
  Previous read of size 4 by T8: spectrum_draw_cb /sp/redbase/spectrum.c:141 [hilo de GUI]
  Location is global 'spectrum_buf' of size 3200
SUMMARY: ThreadSanitizer: data race /sp/redbase/spectrum.c:399 in spectrum_clear
SUMMARY: ThreadSanitizer: data race /sp/redbase/spectrum.c:336 in spectrum_data
SUMMARY: ThreadSanitizer: data race /sp/redbase/spectrum.c:99 in spectrum_draw_cb
```

**Fix** (commit `75a48b89`, `src/spectrum.c`): el estado que el dibujo lee (800 muestras, 800
picos, `grid_min/max`, `width_hz`, `filter_from/to`, `dnf_*`, `mode`, `tx`) pasa a un `frame`
compartido que **solo** se toca bajo `data_mux`; `spectrum_draw_cb` lo copia entero a un `snapshot`
privado del hilo de GUI y dibuja de ahi, asi que el mutex se toma para la copia y **no** durante
las 800 `lv_draw_line`. `spectrum_clear()`, los observers de config, `spectrum_get_min()` y
`spectrum_update_min/max()` entran al mismo lock; `spectrum_data()` recorta `size > SPECTRUM_SIZE`
(la base escribia fuera del buffer); y el refresco se coalesce a uno encolado por vez
(`scheduler_put*()` ahora devuelve si el trabajo quedo encolado, para que el productor rearme el
ticket si la cola esta llena). LVGL se sigue tocando solo desde el hilo de GUI
(`scheduler_put_noargs`).

**Verificacion** (todo en `docs/evidencia/2026-10-19-spectrum-149/`):

- **TDD nativo** (`tests/native/test_spectrum.c`, modulo real staged contra `stubs_spectrum/`, con
  pausa determinista en la linea 700 del dibujo): con el `spectrum.c` de la base, `tdd-red.log` da
  **11 fallos** y `exit 139` (SIGSEGV: el caso de bounds escribe fuera del buffer y pisa el objeto
  contiguo; `tdd-red-asan.log` lo dice con UBSan: `runtime error: index 800 out of bounds for type
  'float [800]'`), con trazas mezcladas (`A=351 B=449`), el rectangulo del filtro de la generacion
  nueva, `50 refrescos encolados` y `estres: 120 de 120 dibujos con generaciones mezcladas`; con el
  fix, `65 comprobaciones, 0 fallos` (`native-tests.log`).
- **Sanitizers** (`native-tests-sanitizers.log`): ASan+UBSan 65/0, **TSan 65/0 sin reportes**, y el
  fixture de carrera deliberada (`tsan-fixture.log`) reportando `data race` con `exit=66`, o sea que
  el detector dispara en este entorno.
- **Suite nativa completa** del host: `exit 0`, con `-fanalyzer` sin observaciones sobre el modulo.
- **A/B** (`ab-compare.log`): 87 escenarios pixel-identicos y 893 chequeos de contrato, **PASS**. En
  el banco natural (`natural.log`) el perfil no se mueve: espectro 132000 px x 27 frames y el mismo
  sha256 de captura. El p95 del host no se usa como evidencia: el control A/A (dos corridas de la
  misma revision) lo mueve de -42.8 % a +50.7 % con 31/87 escenarios fuera del 25 %, mientras el p50
  queda en +0.1 % de mediana (ver `ab-benchmark-control.log`).
- **Banco QEMU** (`banco-smoke.log`, `banco-apps.log`): binario ARM del commit (`fuente md5
  13b4c4321a5b`, `binario md5 b5ac3b029818`), captura de home con 331 colores, barrido de las 23
  apps **23/23 vivas y 0 crashes**, y 106 ordenes de banda/filtro/modo/frecuencia confirmadas por el
  estado. El `SIGABRT` que el banco dejo en `banco-crash.txt` **no es de este cambio**: es el defecto
  ya registrado en `docs/evidencia/2026-10-14-caidas-aleatorias/README.md` (fortify de glibc en el
  camino de audio), con la misma firma de PC/LR que dejo el run `factory/cb-27mhz-146` ese mismo dia.

**Sin fundar (anexo A)**: que la carrera haya producido un artefacto visible o un crash en la radio
(no hay ningun crash atribuido a esto: lo que se fundo es la carrera y la mezcla de generaciones, con
el test determinista y con TSan, no en hardware). Tampoco se midio el costo de la copia del snapshot
en el Cortex-A7 (solo en el host).

---

## 58. `lv_async_call` no es thread-safe en LVGL 8: la app de Propagacion usa el scheduler

**Codigo** (`lvgl/src/misc/lv_async.c`): `lv_async_call()` llama `lv_mem_alloc()` y
`lv_timer_create()` desde el hilo que la invoca, sin ningun lock; `lv_async_call_cancel()` recorre
la lista de timers de LVGL, tambien sin lock. En LVGL 8.3.11 el "async" es un timer de un solo
disparo, no una cola con mutex.

**Codigo** (`src/scheduler.cpp`, `src/scheduler.h`): `scheduler_put()` / `scheduler_put_noargs()`
encolan bajo `std::mutex` y `scheduler_work()` las ejecuta en el hilo principal. Es el mecanismo
que ya usan Field Ops y SWR Scan para volver de un hilo de trabajo a LVGL.

**Consecuencia**: actualizar la UI con `lv_async_call()` desde el hilo del barrido (lo que sugeria
el boceto del issue #145) crea timers de LVGL desde ese hilo y puede corromper su lista. La app
Propagation no lo hace: publica el progreso bajo su mutex y despierta a la GUI con
`scheduler_put_noargs()`; su unico timer (1 s) se crea y se borra desde el hilo de LVGL.

---

## 59. El SIGSEGV del #131 (reabierto): otros tres caminos tocaban LVGL fuera del hilo de GUI

**Sintoma**: el `SIGSEGV` de `_lv_event_mark_deleted` / `lv_obj_destructor` reaparecio el 19/10 con
el build `02090454`, que **ya incluye** el fix del #144 (`hkey_put()` diferido). Es decir: el acceso
fuera de hilo del teclado del panel era **una** fuente, no la unica. La pila resuelta en #137
(`liblvgl` `cd30ca6c58ad`) es la misma: `obj_del_core -> _lv_obj_destruct -> lv_obj_destructor ->
_lv_event_mark_deleted`, y `event_head` (un estatico que solo se toca en `lv_event_send`,
`lvgl/src/core/lv_event.c:72,78`) queda apuntando a un frame de pila muerto. Eso solo pasa si **dos
hilos** entran a `lv_event_send` a la vez: uno empuja el frame y lo devuelve, el otro lo recorre en
`_lv_event_mark_deleted`.

**Auditoria** (codigo, base `b50303ed`): se inventariaron los caminos que tocan LVGL desde hilos que
no son el de GUI. La tabla separa lo que ya estaba diferido de lo que no:

| Funcion LVGL | Quien la llama | Hilo | Codigo (base) |
|---|---|---|---|
| `lv_event_send` (refresh) | `cw_tune_set_freq` | audio (PulseAudio mainloop) | `src/cw_tune_ui.c:126` |
| `lv_obj_has_flag` | `cw_tune_ui_visible` | audio (idem) | `src/cw_tune_ui.c:209` |
| `lv_label_set_text`, `lv_obj_set_user_data`, `lv_obj_clear_state`, `lv_obj_get_parent` | `buttons_load_page` / `buttons_unload_page` | keyer CW (`endecode_thread`) y mensajes de voz (`play`/`send`/`beacon`) | `src/cw_encoder.c:135-136`, `src/dialog_msg_voice.c:343-344,370-371,382-383` |
| `lv_label_set_text` (por `param_changed_cb`) | `lv_msg_send(MSG_PARAM_CHANGED)` | el que llama a `radio_change_*` o al observer de banda (hilo del CAT al cambiar de frecuencia) | `src/radio.c:79,688,700,906,985,1061,1113,1129,1146` |
| `lv_event_send` (refresh) | `meter_update` | radio | **safe**: usa `event_send` (cola con mutex, `src/events.c`) |
| `lv_event_send` (refresh) | `tx_info_update` -> `update_tx_info` | radio | **safe**: `scheduler_put_noargs` (`src/tx_info.c:274`) |
| `lv_obj_invalidate` | `spectrum_data`/`waterfall_data` -> `spectrum_refresh`/`refresh_waterfall` | radio | **safe**: `scheduler_put_noargs` (`src/spectrum.c:407`, `src/waterfall.c:155`) |
| `lv_event_send` | `dialog_swrscan_update` -> `swrscan_chart_refresh` | radio | **safe**: `event_send` (`src/dialog_swrscan.c:261`) |
| `lv_event_send` | `notify_tx`/`notify_rx` -> `main_screen_notify_tx/rx` | radio | **safe**: `event_send` (`src/main_screen.c:1530,1535`) |
| `lv_label_set_text` | `panel_add_text`/`panel_set_info` | CAT/otros | **safe**: `scheduler_put` (`src/panel.cpp:118,122`) |
| `lv_label_set_text` | `param_changed_cb` (desde el hilo de GUI) | GUI | **safe** |
| `lv_obj_set_style_*` | `wifi_state_change_cb` | GUI | **safe**: `g_main_context_iteration` corre en un `lv_timer` (`src/wifi.cpp:372`) |
| `lv_obj_*` | `apply_cmd` (control remoto) | GUI | **safe**: `scheduler_put` (`src/remote_server.cpp:933`) |
| `lv_obj_*` | observers diferidos (`info.c`, `main_screen.c`, `band_info.c`, `meter.c`, `panel.cpp`) | GUI | **safe**: `observer_delayed_notify_all()` |
| `lv_*` | updaters del flow (espectro/waterfall/metro/reloj/band_info/spot_marks/cluster_column) | radio/GUI | **safe**: escalares bajo mutex, cola de eventos o `scheduler` |

Los candidatos que el issue listaba (`tx_info_update`, `msg_update_text_fmt`/`voice_say_*`,
`dialog_*_remote`, `clock_update_power`, `meter_update`, `band_info`, `spot_marks`,
`cluster_column`) se revisaron uno por uno: **ya estaban diferidos** o no tocan LVGL. Los tres
caminos que si tocaban LVGL fuera de hilo son los de la tabla marcados con su `file:line`.

**Fix** (base `b50303ed`):

- `src/scheduler.cpp`/`scheduler.h`: `scheduler_thread_init()` (la llama `main()` antes de crear
  cualquier hilo) y `scheduler_in_main_thread()`. Un modulo que toca LVGL consulta si lo llamo el
  hilo de GUI y, si no, se difiere con `scheduler_put*`.
- `src/cw_tune_ui.c`: `cw_tune_ui_visible()` deja de hacer `lv_obj_has_flag()` y lee un
  `atomic_bool visible` que mantiene `update_visibility()` (observer diferido, hilo de GUI) y
  `obj_deleted_cb()`. `cw_tune_set_freq()` guarda la frecuencia en un atomico y encola
  `cw_tune_apply()` con `scheduler_put_noargs()`; `req_pending` coalesce los pedidos (el feed llama
  por trama) y el cuerpo (colores + `lv_event_send`) corre en el hilo de LVGL.
- `src/buttons.cpp`: `buttons_load_page()`/`buttons_unload_page()` se auto-difieren cuando los llama
  un hilo ajeno (el keyer CW, los mensajes de voz), en el mismo orden en que se pidieron; desde el
  hilo de GUI el comportamiento es identico. `param_changed_cb()` difiere el recorrido de labels si
  `MSG_PARAM_CHANGED` llega de un hilo ajeno.
- `src/main.c`: `scheduler_thread_init()` como primera linea de `main()`.

No cambia nada visible: las mismas paginas, el mismo bloque activo del widget de CW, las mismas
etiquetas de los botones; solo cambia **en que hilo** corre cada cuerpo.

**Verificacion** (todo en `docs/evidencia/2026-10-19-lvgl131b/`):

- **TDD nativo** (`tests/native/test_cw_tune.c`, stubs que registran el hilo de cada primitiva,
  como #149): con el `cw_tune_ui.c` de la base `35 comprobaciones, 13 fallos` (exit 1); con el fix
  `35 comprobaciones, 0 fallos` (exit 0). Cubre el pedido de frecuencia desde el hilo de audio, el
  refresco en el hilo de GUI, la consulta de visibilidad sin LVGL, el modo fuera de CW y la
  coalescencia de pedidos.
- **Suite nativa completa** (`tests/native/run_native_tests.sh`): exit 0, con el bloque nuevo y
  `-fanalyzer` sin observaciones sobre `cw_tune_ui.c`.
- **Chequeos estaticos** (`tests/check_*.py` + `audita-pruebas.py` + `check_cw_tune_feed.py`):
  10/10 OK. `check_cw_tune_feed.py` se actualizo: ahora exige que el accesor y `cw_tune_set_freq()`
  **no** tengan `lv_` y que el pedido use `scheduler_put`.
- **A/B `AB-LVGL131B`** (`b50303ed` contra `260602c5`, perfil `host-ui`): **PASS** (exit 0), 89
  escenarios pixel-identicos, 913 contratos; Hardware/Energy BLOCKED (advisory). Reporte en
  `tools/ab_testing/reports/AB-LVGL131B/`.
- **Determinismo del arnes de UI** (`check_harness_determinism.sh`): PASS (PNG identicos por
  fixture). **Temas** (`x6200-ab themes`): exit 0.
- **ruff / format / ty / pytest** (`tools/ab_testing`): todo OK; `pytest` **240 passed**.
- **`audita-hallazgos.sh`**: exit 0.
- **ARM cross-compile**: `make x6200-gui-dirclean && make x6200-gui-rebuild` en un volumen privado
  (`x6200-build-output-lvgl131b`); binario `ELF32 ARM`, **13.854.112 B**, **`md5
  2ddef0bebd7312bafe44b243dc401054`**; pasa `verifica-firmware-port.py`.
- **Banco QEMU** (ARM emulado `cortex-a7`, chroot propio): smoke OK (captura 20.891 B / 210
  colores) y **120 ordenes** de `freq`/`mode`/`band`/`filter` (6 rondas de 20), **0 errores**, GUI
  viva, framebuffer 8.388.608 B y **sin `crash.txt`**.

**Lo que el banco no puede probar, y receta para la radio**: el banco no tiene flow ni placa de
sonido, asi que no corre `radio_tick()` (teclas del panel), ni `cw_put_audio_samples()` (hilo de
audio), ni el keyer. La validacion dinamica es en la radio con el binario del fix, cambiando
frecuencia/modo con el teclado CW de CB abierto (la corrida del #159) y con el panel, verificando
`CRASHLOG` 0 y `pid` estable. La receta exacta esta en el README de la evidencia (§6).

**Sin fundar (anexo A)**: cual de los tres caminos disparo *este* crash. Los tres entran a
`lv_event_send` fuera del hilo de GUI y los tres producen la misma corrupcion de `event_head`; la
corrupcion es acumulativa y el crash cae en el siguiente borrado de objeto (un cambio de
frecuencia/modo/banda). El banco no reproduce la concurrencia (no hay flow ni audio), asi que la
atribucion a uno solo no se puede medir ahi.

## 60. El squelch del #166: la lib del control ya habilita el enable (el comentario de radio.c:611 era redundante)

**Sintoma** (issue #166): el squelch no corta el ruido de recepcion. El triage del issue apunta a
`// x6200_control_sql_enable_set(true);` comentado en `src/radio.c:611` como la causa: "el nivel se
manda al DSP pero el enable nunca se habilita".

**Lo que dice la lib (codigo)**: la lib `aether-x6200-control` no vive en este repo; el paquete de
Buildroot la fija en `v0.1.0` (2026-02-10) y su `CMakeLists.txt` declara `VERSION 0.15.3`, que es el
mismo `.so` que la radio usa (`libaether_x6200_control.so.0.15.3`). En su fuente:

- `src/control.c:303-316` — `x6200_control_sql_set(sql)` escribe el nivel en los bits 8..15 del
  registro 21 (`x6200_micsel_pttmode_chge_spmode_auxiqgen_sqlthr`) y **llama a
  `x6200_control_sql_enable_set(sql > 0)`** (linea 315).
- `src/control.c:318-323` — `x6200_control_sql_fm_set(sql)` escribe los bits 16..23 y tambien llama
  al enable con `sql > 0` (linea 322).
- `src/control.c:325-329` — `x6200_control_sql_enable_set(on)` setea el bit 24.
- `src/low/control.c:459-462` — `x6200_control_get()` devuelve el ultimo valor escrito (cache en
  RAM, `all_cmd.arg[cmd]`), asi que el read-modify-write del registro no pierde bits.
- `include/aether_radio/x6200_control/control.h:34` — el rango del nivel es **0..100**
  (`/* Voice SQL 0 - 100 */`), el mismo que usan la UI (`src/vol.c:64`) y el remoto
  (`src/remote_server.cpp:766`).

El `.so` desplegado coincide con esa fuente. Desensamblado de
`libaether_x6200_control.so.0.15.3` (`objdump -d` del toolchain
`arm-buildroot-linux-gnueabihf`), `x6200_control_sql_set` en `0x3a50`:

```
3a74: e2540000  subs  r0, r4, #0
3a78: 13a00001  movne r0, #1
...
3a80: eafff932  b     x6200_control_sql_enable_set@plt
```

Es decir, el enable es `(sql != 0)` y se llama **dentro** del setter.

**Consecuencia**: el enable del DSP **si** se habilita cuando el nivel es >0; el comentario de
`src/radio.c:611` era redundante, no la causa de "nunca se habilita". El dato que el issue cita (el
`get` refleja el nivel) tambien es consistente con esto.

**Lo que se cambio igual** (issue #166): la convencion se deja **explicita** en el camino de radio,
para que no dependa de la version de la lib y para cubrir el arranque:

- `src/sql_policy.[ch]`: `sql_should_enable()` (pura: `level > 0`) y `sql_policy_apply()`, que
  aplica el nivel segun el modo (voz o FM) y despues el enable.
- `src/radio.c`: el observer de `cfg_cur.sql_level` usa `sql_policy_apply()` bajo `radio_lock`; el
  valor persistido se aplica al final de `radio_init()`, despues de los setters del registro
  compartido (spmode/mic). El `and_call` se quito porque aplicaba antes de inicializar el mutex.

**Verificacion** (corrido en el worktree, base `c6761f80`):

- **TDD nativo** (`tests/native/test_sql_policy.c` con stubs del control en
  `tests/native/stubs_sql/`): con el enable comentado, `76 comprobaciones, 25 fallos` (exit 1); con
  el fix, `76 comprobaciones, 0 fallos` (exit 0).
- **Suite nativa completa** (`tests/native/run_native_tests.sh`): exit 0, con `-fanalyzer` sin
  observaciones sobre `sql_policy.c`.
- **Estaticos** (`check_cfg_fills`, `check_actions`, `check_settings_focus`, `check_apps_menu`,
  `check_app_handlers`, `check_manual_apps`, `check_wcag`, `check_cw_tune_feed`, `audita-pruebas.py`):
  9/9 OK.
- **Determinismo del arnes** (`tests/check_harness_determinism.sh`): PASS (PNG identicos por
  fixture).
- **A/B `AB-SQL166`** (`c6761f80` contra `0d7da153`, perfil `host-ui`): **PASS** (exit 0), 89
  escenarios **pixel-identicos** y 913 contratos; Hardware/Energy BLOCKED (advisory). Reporte en
  `tools/ab_testing/reports/AB-SQL166/`.
- **`x6200-ab audit`**: exit 0 (11 known issues, 0 reproducidos; 0 hallazgos estructurales sobre
  9810 objetos en 89 escenarios). **`x6200-ab themes`**: exit 0.
- **ruff / format / ty / pytest** (`tools/ab_testing`): todo OK; `pytest -m 'not e2e'` **227
  passed, 13 deselected**. Los 13 `e2e` fallan en el **setup** por un fixture previo
  (`tools/ab_testing/tests/test_e2e.py:189` espera `lv_label_set_text(...)` en `src/info.c`, que
  desde `5f3c52fe` dice `ui_label_set_text(...)`): desajuste del fixture, ajeno a este cambio.
- **ARM cross-compile**: `make x6200-gui-dirclean && make x6200-gui-rebuild` (volumen
  `x6200-build-output`, worktree montado en `/workspace_gui:ro`); binario `ELF32 ARM`,
  **13.855.224 B**, **`md5 adb4736f0c6d9b302767b48c61ef2bc6`**, `sql_policy.c.o` en el build, y pasa
  `verifica-firmware-port.py` (3/3 marcas).
- **Banco QEMU** (gate #86, chroot privado, `RECONSTRUIR=no` sobre el binario recien compilado):
  smoke OK (captura 25.888 B / 313 colores); por socket, `mode` USB y barridos de `sql` en
  0/1/8/50/100 y 0 (voz) mas `sql` en NFM/WFM/USB/LSB: el `get` refleja el valor puesto, GUI viva y
  **sin `crash.txt`**.

**Lo que el banco no puede probar, y receta para la radio**: el banco no tiene flow ni placa de
sonido, asi que **no puede medir si el ruido corta** (el audio de recepcion no pasa por el stream de
la GUI, ver el issue #166). La validacion es en la radio con el binario del fix:

1. Subir el squelch de 0 a 1..8 y verificar que el ruido de recepcion **corta**.
2. Bajarlo a 0 y verificar que el ruido **vuelve** a abrirse.
3. Repetir en FM (`NFM`/`WFM`).
4. Si el DSP mutea solo o hay que esperar el `pack->flag.sql_mute`, dejarlo asentado (el GUI hoy no
   usa ese flag; solo aparece en un `printf` comentado, `src/radio.c:224`).

**Sin fundar (anexo A)**: por que el owner no escucha el corte. La lib habilita el enable; lo que
queda sin medir es (a) el nivel de audio real del parlante, (b) si el umbral 0..100 necesita un
valor alto para cortar el ruido, y (c) el desajuste de bits del comentario de la lib
(`src/control.c:304-312` muestra al OEM alternando el bit 16 en la voz 0<->1, mientras la lib usa el
bit 24 para el enable). Nada de eso se puede dirimir sin la radio.

## 61. El md5 del build no distinguia commits: `rsync -au` de Buildroot no reponia la copia (#165)

**Sintoma** (issue #165, del QA): dos commits distintos dieron el MISMO md5 y el mismo tamano. Los
numeros del issue: el commit A (`1c599025`, fix del #131) y el B (`5f3c52fe`, OPT-4, 7 archivos)
dieron los dos `9a26b607c67d`, 13.855.068 B, con un rebuild que el QA creyo limpio (`rm -rf` de la
copia + `make x6200-gui-rebuild`). El QA verifico que la copia tenia el codigo nuevo mirando
`src/ui_set.h` (el archivo NUEVO del commit) y dio el build por bueno.

**Causa fundada** (mecanismo, medido): el paquete `x6200-gui` de Buildroot usa el override srcdir
(`local.mk` -> `X6200_GUI_OVERRIDE_SRCDIR=/workspace_gui`) y copia el arbol con `rsync -au`
(`buildroot/package/pkg-generic.mk:222`). `-u` saltea un archivo si en el destino tiene un timestamp
igual o mas nuevo, SIN mirar el contenido. Medido en el volumen privado, con el rsync EXACTO de
Buildroot (destino con `src/info.c` al futuro, fuente modificada):

```
$ rsync -au --chmod=u=rwX,go=rX --exclude .git ... /workspace_gui/ $DEST
dest src/info.c mtime = 2030-01-01 (mas nuevo que la fuente) -> NO se copia (el marker no llega)
dest src/bi165_new.txt (archivo NUEVO)                      -> SI se copia
```

Los archivos NUEVOS se copian (no existen en el destino) y los MODIFICADOS se saltean: por eso el QA
veia `ui_set.h` (nuevo) con el codigo nuevo y no veia que `info.c`, `main_screen.c`, `meter.c`,
`tx_info.c`, `clock.c`, `backlight.c` (modificados por el commit) quedaban viejos.

**Reproducido de punta a punta** (volumen privado `x6200-build-output-bi165`, `tools/compila-gui.sh`
sin el arreglo; `dirclean` + `rebuild`):

```
build A (1c599025), copia fresca:                        md5 bd28b95d4426
build B (5f3c52fe), copia con mtime al futuro:           md5 bd28b95d4426   <- el MISMO (bug)
  (la copia quedo con el `src/info.c` del commit A: 0 `ui_label_set_text`; el commit B usa 8)
```

Con la copia fresca los dos commits dan md5 distinto (el B da `9a26b607c67d`), asi que el md5 SI
distingue: lo que fallaba era que la copia no se reponia. El `rm -rf` del QA no alcanzaba porque el
`find ... -name '*.o' -delete` y el rebuild rearman la copia con el MISMO `rsync -au`.

**Arreglo**:
- `tools/fuente-md5.sh` (nuevo): el hash de las fuentes con rutas RELATIVAS (el repo y la copia dan
  el mismo numero) y `LC_ALL=C`; lo comparten `compila-gui.sh`, `captura-banco.sh` y CMake.
- `tools/compila-gui.sh`: antes de compilar hace un `rsync -a --checksum` (sin `-u`: compara
  CONTENIDO) de `/workspace_gui` a la copia, deja el sello `.fuente.md5` en la copia y usa
  `make x6200-gui-reconfigure` para que CMake relea el sello.
- Identidad en el binario (lo que pide el issue, punto 2): `CMakeLists.txt` lee el sello y define
  `X6200_FUENTE_MD5`; `src/CMakeLists.txt` lo aplica al target y `src/main.c` lo loguea al arrancar
  (`boot_identity()`), una linea, en el log de la GUI y en `/mnt/app_logs/boot.log`.

**Verificacion**:
- **A/B del md5** (volumen privado, con el arreglo): A (`1c599025`) `521869e4bd27` contra B
  (`5f3c52fe`) `54cd60858b97`: **distinto** (mismo estado que reproducia el bug: la copia con
  mtime al futuro). **Reproducibilidad**: dos builds del mismo commit (`16ff4cbc`) dan
  `b0bf91cff55b` las dos veces.
- **Identidad embebida** (`strings` del binario ARM): `fuente: %s (x6200_gui)` y el hash
  `1de404498d032638fa6b3c36b959ff7e` (`strings -a /b/target/usr/sbin/x6200_gui | grep 1de40449`).
- **Banco QEMU** (gate #86, chroot y volumen privados, `RECONSTRUIR=no`): el log de arranque muestra
  `fuente: 1de404498d032638fa6b3c36b959ff7e (x6200_gui)` (stdout del `boot_identity` y
  `boot.log`) y coincide con el sello `.fuente.md5` del volumen
  (`1de404498d032638fa6b3c36b959ff7e`). La GUI corrio, dibujo (captura 25.888 B / 313 colores) y
  paso `verifica-firmware-port.py` (3/3 marcas).
- **Estatico** `tests/check_build_identity.py`: el hash es estable y no depende del cwd; el tooling
  usa `--checksum`; CMake y `main.c` embeben/loguean el sello.
- **ARM cross-compile** (volumen privado): `ELF32 ARM`, 13.855.240 B, `md5 b0bf91cff55b`, pasa
  `verifica-firmware-port.py` (3/3 marcas).
- Gates: nativos (118 comprobaciones, 0 fallos), estaticos (10/10), determinismo del arnes (PASS),
  `ruff`/`format`/`ty` OK, `pytest -m 'not e2e'` 227 passed, A/B `AB-BUILD165`, contracts/audit/
  themes.

**Sin fundar (anexo A)**: que la copia del volumen del QA tuviera el mtime mas nuevo que su fuente
(el mecanismo medido lo explica; el `ls -l` de esa corrida no quedo). Tampoco queda medido cuanto
pesa el `--checksum` en un arbol mas grande que el de esta corrida.

