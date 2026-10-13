# Hallazgos — registro de lo investigado en el X6200 con esta ROM

Este archivo es el **registro de hallazgos**: qué se investigó, qué se encontró, con qué evidencia
y qué queda pendiente. La idea es no volver a perseguir lo mismo dos veces.

Índice:

1. [El audio del equipo no sale por el códec](#1-el-audio-del-equipo-no-sale-por-el-códec)
2. [La voz (TTS) se sintetiza pero no se oye](#2-la-voz-tts-se-sintetiza-pero-no-se-oye)
3. [Tiempos de arranque medidos](#3-tiempos-de-arranque-medidos)
4. [La actualización en el lugar no es confiable](#4-la-actualización-en-lugar-no-es-confiable)
5. [El desplegable de temas](#5-el-desplegable-de-temas)
6. [Reinicios suaves vs arranques reales](#6-reinicios-suaves-vs-arranques-reales)
7. [La copia de assets a /dev/shm](#7-la-copia-de-assets-a-devshm)
8. [Herramientas de banco que quedaron](#8-herramientas-de-banco-que-quedaron)
9. [Pendientes ordenados por valor](#9-pendientes-ordenados-por-valor)
10. [`params_init`: los 2 segundos de la carga de parámetros](#10-params_init-los-2-segundos-de-la-carga-de-parámetros)

---

## 1. El audio del equipo no sale por el códec

**Síntoma**: el receptor se escucha ✓ pero **nada** de lo que genera la GUI (la voz, el audio de
TX del FT8/JS8) suena ✗.

**Cadena de causas encontrada**

1. La GUI reproduce por **PulseAudio** (`audio.c`: `struct` con `pa_stream`, sink
   `alsa_output.platform-sound.stereo-fallback`).
2. PulseAudio corría como el usuario **`pulse`**, y ese usuario **no estaba en el grupo `audio`**:
   ```
   id pulse → uid=1000(pulse) gid=1000(pulse) groups=1000(pulse)
   /dev/snd/pcmC0D0p → crw-rw---- root audio
   audio:x:29:            ← el grupo estaba VACÍO
   ```
   → no podía abrir la placa ✗ → creaba el sink de emergencia **`auto_null`** (module-null-sink).
3. La GUI le mandaba el audio a `alsa_output.platform-sound.stereo-fallback`, que **no existía**
   → PulseAudio lo mandaba al **nulo** → **se reproducía en el vacío, sin ningún error** (el
   `pa_stream_write` daba OK).

**Arreglo aplicado** (en vivo, por SSH):

```
sed -i 's/^audio:x:29:/audio:x:29:pulse/' /etc/group
/etc/init.d/S50pulseaudio restart
```
→ el sink real **apareció** (`module-alsa-card.c` = PulseAudio detectó la placa ✓) y la
reproducción pasó a `State: RUNNING` sobre ese sink ✓. **Pendiente**: ponerlo en el
`post-build.sh` para que salga de fábrica (hoy está sólo aplicado a mano en la tarjeta).

**Pero igual no se oye** ✗ — con todo verificado:

| Comprobación | Resultado |
|---|---|
| `speaker-test -t sine` (ALSA directo) | corre sin error ✗ sin sonido |
| `amixer sget 'Headphone'` | 58 [92%] **[on]** ✓ |
| `amixer sget 'AIF1 DA0'` | 160 [83%] [0.00dB] ✓ |
| `amixer sget 'Headphone Source'` | Item1: **'DAC'** ✓ (el DAC va al auricular) |
| `amixer sget 'AIF1 Slot 0 Digital DAC'` | **[on]** ✓ (el DAC recibe el I2S) |
| `pactl list sinks` | `alsa_output.platform-sound.stereo-fallback` **RUNNING** ✓ |
| `/proc/asound/card0/pcm0p/sub0/hw_params` | S16_LE · 2ch · **44100** · period 14700 ✓ |
| `dmesg` | `#0: sun8i-a33-audio` (la placa se registró ✓) |

**Conclusión**: el camino de audio del **SoC** (I2S `AIF1` → códec → auricular) está
**configurado correctamente** y **no entrega sonido** ✗. El receptor que se escucha viene del
**DSP propio del equipo** ✓, no de este camino. Es una **limitación de plataforma**, no del código
de la GUI. Por eso: la voz y el audio de TX del FT8/JS8 (que salen por el SoC) **no se oyen** ✗.

**Camino viable**: los **auriculares Bluetooth** (A2DP). El audio sale
`GUI → PulseAudio → bluealsa → Bluetooth` y **no pasa por el códec** ✓. Requiere un cambio
acotado: que el stream apunte al sink del Bluetooth cuando hay auriculares conectados (hoy
apunta fijo al códec). *No verificado* (los auriculares estaban descargados al momento de
escribir esto).

**Nota metodológica**: en su momento se afirmó que "es el mismo camino que usa FT8, así que va a
funcionar"; eso era **incorrecto** — ese camino nunca se había probado en la radio. Lección: no
dar por buena una ruta de audio sin probarla en el equipo.

---

## 2. La voz (TTS) se sintetiza pero no se oye

El firmware base traía `voice_stub.h` con las 12 funciones de voz como **no-ops**. Se implementó
TTS real (espeak, en español, en un hilo con cola, sin hablar durante TX).

**Diagnóstico** (`DATA/app_logs/voice.log`, agregado a propósito para esto):

```
descartado: modo=1 lcd=1 despierta=1 tx=0 [Application|page 2]
encolado:   modo=2 lcd=1 [FFT width: 48]
sintetizado: 58026 muestras de 22050 Hz (reloj de la GUI: 44100 Hz)
```
- `descartado` = no habló por el ajuste (modo=1 "When LCD off" con `lcd=1` encendida → correcto).
- `encolado` + `sintetizado` = espeak sintetizó y se remuestreó (22050 → 44100) ✓.
- El aviso `- NO SE ESCRIBIO NADA AL AUDIO` sólo aparece si la escritura falla; **nunca
  apareció** → el audio **sí** se escribió al stream ✓.

**Conclusión**: la voz funciona de punta a punta **dentro del software**; el silencio es el
problema del punto 1 (el SoC no llega al parlante). Evidencia de la mezcla: CPU barata, hilo con
`condition` (duerme cuando no hay nada que decir), y `espeak` de 18 KB + 1,2 MB de datos (podados
a español e inglés desde 2,8 MB).

---

## 3. Tiempos de arranque medidos

Instrumentación puesta a propósito (`/mnt/app_logs/boot.log`, acumulativo):

| Quién escribe | Qué mide |
|---|---|
| `S01boot_time` | fin del kernel (`uptime`) |
| `S20gui` | cuándo se lanza la GUI |
| la GUI (`boot_mark`) | `audio_init`, `params_init`, `fuentes y estilos`, `pantalla principal`, `control remoto` |
| la GUI | primer frame dibujado (`GUI lista`) |

**Arranque de referencia (antes de optimizar)**:

```
kernel listo:        3,13 s
GUI arrancando:      7,39 s     → scripts: 4,26 s
GUI lista:          12,4 s      → la GUI: 5,0 s
```

**Hallazgo 1 — la GUI arrancaba en `S95gui`** ✗: después de `S40network`,
`S45network-manager`, `S48sntp` (**6 esperas**, sincronización de hora), `S49ntp`,
`S40bluetoothd` y `S94btcat`. La GUI sólo necesita `DATA`, `udev`, el daemon de radio y
`/dev/shm`, todo listo en `S11`. **Arreglado**: `git mv S95gui → S20gui` (con guarda de
`/dev/shm`), y los servicios lentos quedan después.

**Hallazgo 2 — `audio_init` esperaba a PulseAudio (1,7 s)** ✗ antes del primer dibujo.
**Arreglado**: la inicialización de audio corre en un **hilo** (`audio_init` → `audio_init_sync`)
y la GUI dibuja de inmediato. Mientras el audio no está listo, `audio_play()` devuelve 0.

**Arranque después de las optimizaciones** ✓:

```
kernel listo:        3,15 s
GUI arrancando:      6,96 s     → scripts: 3,81 s
  audio_init         8,1 s      → ¡ya no espera! ✓
  params_init       10,1 s      → ✗ 2,0 s (carga de la base de parámetros)
  fuentes y estilos 10,2 s      → 0,1 s
  pantalla principal 10,2 s     → 0,0 s
  control remoto    10,7 s      → 0,5 s
GUI lista:          10,8 s      → la GUI: 3,85 s (antes 5,0 s) ✓
```

| | Antes | Ahora |
|---|---|---|
| Total | 11,3–12,4 s | **10,8 s** |
| La GUI | 5,0 s | **3,85 s** |

**Nuevo objetivo**: `params_init` (**2,0 s**, la carga de la base de parámetros) y los scripts
(**3,81 s**: udev, `create_data`, `tune_cache`, el daemon de radio). Los dos se pueden diferir o
medir mejor.

---

## 4. La actualización en el lugar no es confiable

El `S02update` busca `x6200_r1cbu_*.tgz` en `DATA`, respalda y aplica. **Dos intentos, dos
fallos**:

1. El primero se cortó por un corte de energía (quedó a medias → no arrancaba).
2. El segundo: el `boot.log` mostró `kernel listo` y **nada más** (no llegó a `S20gui`), el testigo
   quedó **en `DATA`** (ni aplicado ni borrado) y apareció un `backup_*.tgz` de **29 MB** (cuando
   el completo son ~90 MB) → murió **durante el respaldo**.

**Lo bueno**: como respalda **antes** de tocar la raíz, al morir no rompió nada — la raíz quedó
**idéntica** (binario, scripts y contenidos verificados) y el equipo volvió a arrancar sacando el
tgz de `DATA`.

**Conclusión**: usar la **imagen completa** para actualizar. Si se usa el tgz: **cargador puesto**
y **esperar ~10 minutos** sin apagar (la pantalla queda en el logo, sin aviso — eso hay que
mejorarlo).

---

## 5. El desplegable de temas

**Síntoma**: en la radio sólo se veían **2 temas** (y en la referencia del X6100 hay 3 archivos +
los incorporados = **8**).

**Causa**: la fila de Ajustes estaba **escrita a mano** con dos opciones del sistema viejo
(`" Simple \n Legacy"`, `params.theme`), mientras que los temas del X6100 (`params.theme_name`)
se listaban con `theme_list()` pero **sólo** los usaba la acción `ACTION_THEME`. **Arreglado**:
el desplegable arma sus opciones con `theme_list()` y guarda `theme_name` + reinicia (como el
X6100).

**Regresión propia**: al reescribirlo se creó un `lv_dropdown_create` crudo **sin**
`dialog_item(&dialog, obj)` — que es lo que registra el widget en el **grupo de foco**. Sin eso el
MFK y las teclas no llegaban al desplegable y **no se podía seleccionar**. Arreglado.

**Chequeo automático agregado**: `tests/check_actions.py` verifica que toda acción ofrecida en
Ajustes tenga su `case` — es el que encontró `ACTION_WF_MODE` sin manejador (esa feature no está
portada: ni este port ni la base tienen `wf_mode`).

---

## 6. Reinicios suaves vs arranques reales

En el `boot.log` aparecen líneas de `S01boot_time` con `uptime` **grande** (376 s, 144 s,
3298 s). **No son arranques fríos** ✗: el botón de power del equipo apaga por **software** y
**vuelve a correr el userspace** con el kernel ya arriba → `uptime` acumulado. Los arranques
**reales** son los que empiezan en ~3 s ✓. Al medir, ignorar las líneas con uptime grande.

---

## 7. La copia de assets a /dev/shm

El daemon hacía `cp -R /usr/share/x6200/images /dev/shm/` **en cada arranque**. Se había dejado
**condicional** (`[ -d /dev/shm/images ] || cp ...`) pero **eso no alcanza** ✗: `/dev/shm` es
**tmpfs** y se vacía en cada arranque → la condición nunca se cumple.

**Medido en el equipo** (no estimado): los assets son **4,4 MB en 16 archivos** y la copia a
tmpfs cuesta **0,24 s**; además ocupa esos 4,4 MB de RAM.

**Implementado**: el daemon engancha la carpeta con un **bind mount**
(`mount --bind /usr/share/x6200/images /dev/shm/images`), que es instantáneo, y **cae a la
copia** si el bind falla. El mountpoint lo asegura `S20gui` (que ya monta `/dev/shm`).

---

## 8. Herramientas de banco que quedaron

- **`captura-banco.sh`**: corre la GUI en la reproducción (chroot) y convierte el framebuffer
  falso a PNG. El fb es **480×800 a 32 bits** (BGRA) y la imagen se ve rotando **270°**. Es la
  forma de **ver** un cambio antes de grabarlo en la tarjeta: así se verificó que los temas
  funcionan (se veían las marcas de spots y los temas aplicados).
- **`{"cmd":"cluster_feed","text":"<línea>"}`** (control remoto): inyecta un spot del clúster sin
  servidor. La línea necesita la hora `HHMMZ` (si falta, el parser la descarta en silencio).
- **`X6200_VOICE_TEST="texto"`**: dice una frase por voz al arrancar, para probar el TTS sin UI.
- **`DATA/app_logs/voice.log`** y **`boot.log`**: los diagnósticos de voz y arranque.
- **`./tools/prueba-gui-en-radio.sh`**: copia la GUI recién compilada del volumen
  `x6200-build-output` a la radio por SSH, la reinicia (el daemon la vuelve a levantar solo) y
  muestra las marcas nuevas del `boot.log`. **Es el banco rápido**: mide el efecto de un cambio
  en el equipo real en ~20 s, sin rehacer la tarjeta ni flashear.
- **`./tools/mide-arranque-banco.sh`**: corre la GUI en el chroot del banco (sin radio, con QEMU
  y pantalla falsa) y muestra el reparto por etapas. **Ojo**: en el banco los tramos de hardware
  dan casi cero (los `event*` los resuelve el sistema de archivos del host) y `control remoto` se
  estira ~15 s esperando a la radio, así que sirve para **atribución**, no para tiempos
  absolutos. Los 2 s que se buscaban **no aparecían** en el banco.
- **`boot_mark()`** (main.c, `util.h`): cada etapa deja su renglón en `boot.log` con el `uptime`
  **y cuánto duró la etapa** (`(+0,16 s)`). Con eso se ubica un tramo lento sin adivinar. Las
  marcas cubren: `GUI inicio`, `lv_init`, `display init`, `audio_init`, `eventos/usb`, `display`,
  `keyboard`, `entradas`, `db_init`, `cfg:*`, `params_load`, `params_init`, `fuentes y estilos`,
  `pantalla principal`, `cw/rtty`, `radio`, `cat/qso_log`, `wfview`, `control remoto`.
- **SSH**: `sshpass -p 123 ssh -F /dev/null -o StrictHostKeyChecking=no -o
  PreferredAuthentications=password -o PubkeyAuthentication=no root@<ip>` (la opción `-F /dev/null`
  evita un problema de permisos de la config local de ssh; `addgroup` de busybox **crea** grupos,
  no agrega usuarios: editar `/etc/group` a mano).

---

## 9. Pendientes ordenados por valor

1. **Arranque — el replay del journal del rootfs (1,34 s)**: ver §11. Es lo más grande que queda
   y el único que necesita una decisión (rootfs read-only).
2. **Arranque — leer el binario y las bibliotecas (~1 s)**: ver §12.
3. **Arranque — los scripts (~1,4-3,8 s según el arranque)**: ver §12.
4. **Pantalla de arranque** (logo + progreso): no baja el tiempo pero es lo que más **se siente**.
5. **Auriculares Bluetooth**: el único camino viable para que se oiga la voz.
6. **Features que faltan de la migración**: app de **canales/memorias** (la más grande), app de
   **modos**, `broadcast_db`, `freq_scale`, `sstv_resources`, waterfall fijo/centrado y Snake.
7. **Imposibles / no aplicables**: TX Cal / CESSB / filtro TX (el baseband del X6200 no los
   expone) y la app Android "X6100 Link" (nunca se publicó; el protocolo del socket sí está).

---

## 10. Los 2 segundos del arranque: **no** eran SQLite (hipótesis corregida y arreglada)

**Lo que se creía** (y estaba mal ✗): que `params_init` gastaba 2,0 s porque `load_items_from_db()`
hacía **una consulta por item** (`cfg_params_load_item`) contra SQLite en la microSD.

**Cómo se midió** (sin adivinar):

1. **SQLite, cronometrado**: la tabla `params` tiene **67 filas** y un **índice único por nombre**
   (`sqlite_autoindex_params_1`). 67 consultas con `SELECT val FROM params WHERE name=?` tardan
   **0,6 ms** en total. Los cientos de líneas del log eran **solo el log** ✗: su costo es escribir
   texto, no consultar.
2. **Marcas por etapa** (`boot_mark()`, §8): separaron el tramo `audio_init → params_init` en
   pedazos y apareció el culpable.

**La causa real**: **abrir un `/dev/input/eventN`** en este equipo. Las perillas (`rotary0..3`) y
el teclado del panel (`matrix_keypad`) cuelgan del **bus RSB**, y su driver hace una transacción
por apertura: **~50 ms cada una**. En el arranque la GUI abría **22 veces**:

- `keyboard_init()` → `search_kbd_device()` probaba `has_letters()` (open + `EVIOCGBIT`) con
  **cada** `/dev/input/event*` del equipo: **6 aperturas** → **0,60 s**.
- `keypad_init()` ×2 (teclado del panel y encoder del MFK) + `input_dev_by_name()` ×4, que abría
  los `event*` de a uno hasta encontrar el nombre buscado (**1+2+3+4 = 10 aperturas**) +
  las perillas: **0,97 s**.

**Medición en el equipo** (reinicio suave, mismas condiciones):

```
keyboard        +0.60 s   ← 6 aperturas para buscar un teclado USB que no está
entradas        +0.97 s   ← 16 aperturas para ubicar lo que sí está
cfg:items       +0.01 s   ← SQLite: 0,01 s (la hipótesis vieja, descartada)
params_init     +0.00 s
```

**El arreglo** (sin cambiar qué dispositivos se abren, sólo cómo se los encuentra):

- `input_dev.c`: el nombre se lee de **sysfs** (`/sys/class/input/eventN/device/name`), que **no
  abre** el dispositivo. Si sysfs no está, se cae al `EVIOCGNAME` de siempre.
- `keyboard.c`: las capacidades se leen de
  `/sys/class/input/eventN/device/capabilities/key` (mapa hexadecimal; la última palabra son los
  bits 0..63) y se buscan las teclas `Q..P` + espacio. Tampoco abre. Validado contra un teclado
  real (`AT Translated Set 2 keyboard` → detectado; sin falsos positivos en 16 dispositivos).
- `main.c`: los dos teclados se buscan **por nombre** (`matrix_keypad`, `rotary3`) con la misma
  búsqueda por sysfs, igual que ya se hacía con las perillas.

**Verificación en la radio** (A/B con los dos binarios, mismo equipo y sesión):

| | antes | después |
|---|---|---|
| `audio_init → params_init` | **2,0 s** | **0,2 s** |
| GUI propia (`GUI arrancando → GUI lista`) | 3,69 s | **2,11 s** |
| `GUI lista` (arranque total) | 9,2 s | **7,9 s** |

Y los dispositivos abiertos quedan **idénticos** (`/proc/<pid>/fd`): `event0` (teclado del panel)
×1, `event1..3` ×1, `event4` ×2 (teclado del MFK + perilla interna).

---

## 11. Los 1,34 s del journal ext4 del rootfs (pendiente, con opciones)

`dmesg` muestra que **en todos los arranques** el rootfs pide replay del journal:

```
[    2.659459] EXT4-fs (mmcblk0p2): INFO: recovery required on readonly filesystem
[    2.666858] EXT4-fs (mmcblk0p2): write access will be enabled during recovery
[    4.001406] EXT4-fs (mmcblk0p2): recovery complete        ← 1,34 s
```

No es un caso raro: `dumpe2fs` muestra el feature `needs_recovery` y `Filesystem state: clean`
sólo después del replay. El rootfs se monta **rw** y con sólo leer archivos (atime) ya queda
sucio; al cortar la alimentación (o al `reboot` de busybox, que hace `sync` pero no desmonta) el
journal queda pendiente y hay que replayarlo en el próximo arranque.

**Opciones** (de mayor a menor beneficio/riesgo):

1. **Montar el rootfs read-only** (`rootflags=ro` en la línea de comandos del kernel) + tmpfs o
   bind de los pocos caminos que se escriben (perfiles de NetworkManager en
   `/etc/NetworkManager/system-connections`, `/var`, `resolv.conf`). Elimina el replay **y** protege
   la microSD. Es el arreglo correcto, pero hay que inventariar los que escriben y probarlo.
2. **Sacar el journal** (`tune2fs -O ^has_journal`): no hay replay, pero un corte de luz deja el
   rootfs sin recuperación automática → si se corrompe, hay que reflashear. **No** sin decisión.
3. **No desmontar mal**: `umount -a -r` antes del apagado sirve para reboots por software, no para
   el interruptor.

---

## 12. Lo que queda del arranque, medido

Reparto del arranque en frío (7,9-8,3 s con el arreglo de §10):

- **kernel ~2,7 s** hasta `udevd` (`dmesg`); dentro, el panel `JLT4013A` se lleva ~0,4 s.
- **journal ext4 1,34 s** (§11).
- **scripts ~1,4-3,8 s** entre `kernel listo` y `GUI arrancando` (varía mucho entre arranques;
  conviene volver a medir con una marca por script).
- **daemon + enlace dinámico ~1 s**: el binario tiene **11,5 MB y no está stripped**, y enlaza
  **28 bibliotecas** (libnm/libgio/libglib, libfftw3f, liblvgl, libstdc++…). Leerlo desde la raíz
  cuesta **0,47 s** con la caché vacía; la copia de assets que hacía el daemon, 0,24 s (§7).
  Ideas: precargar binario + bibliotecas en un hilo temprano (antes de que la GUI las necesite) y
  `strip` del binario para achicar la imagen.
- **GUI ~2,1 s** (ya con el arreglo): `main()` hasta `audio_init` es **0,02 s** (¡el proceso entra
  rapidísimo!); los tramos son `cw/rtty` 0,16 s, `radio` 0,21 s, `cat/qso_log` 0,07 s,
  `db_init` 0,17 s, `fuentes y estilos` 0,04 s, `pantalla principal` 0,03 s y el primer cuadro.

---

## 13. Cómo se mide el arranque (para no volver a adivinar)

- `boot_mark("etapa")` deja en `/mnt/app_logs/boot.log`: `uptime` **y** la duración de la etapa.
- **Medir en la radio, no en el banco**: el banco (QEMU + fb falso) da 0,15 s donde la radio da
  2,0 s, porque los `open()`/`ioctl()` de los `event*` ahí son del sistema de archivos del host.
  El banco sirve para **ver** y para atribuir, no para tiempos.
- **Reinicio suave ≠ arranque en frío** (§6): los deltas por etapa son iguales, así que el
  reinicio suave alcanza para comparar un cambio; para el número final, reiniciar de verdad.
- Un **A/B honesto** es con los dos binarios y el mismo equipo (los dos `md5` y el `boot.log`
  completo en el reporte).
