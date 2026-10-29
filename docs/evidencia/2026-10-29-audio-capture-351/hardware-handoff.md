# Handoff de hardware: QA de la copia de captura RX (issue #351)

- **Estado global**: `VALIDADO-PARCIAL-HARDWARE` (2026-10-29). La sesión física separada se
  ejecutó; los resultados están en
  [`../2026-10-29-audio-capture-351-radio/README.md`](../2026-10-29-audio-capture-351-radio/README.md)
  y en §9. El resto de este documento se conserva como el plan original escrito antes de la sesión,
  sin reescribir sus mediciones.
- **Issue**: [iariap/x6200_gui#351](https://github.com/iariap/x6200_gui/issues/351), punto 6 de
  «Tests y regresión obligatorios» y «Criterios de aceptación y cierre» (handoff de QA físico).
- **Base**: `169bb807b0086dbb91d29a9ce6770d25ca396859` (`origin/main` al arrancar la unidad).
  Rama `factory/audio-capture-pipeline-351`; el sha exacto medido es el HEAD de esa rama al abrir
  el PR (`git log -1 --format=%H`).
- **Qué cubre**: la **copia de captura** que `src/audio.c::read_callback()` entrega a
  `screen_rec_put_audio_samples()` (grabador de pantalla, AVI) y `dsp_put_audio_samples()`
  (grabador de audio, decodificadores CW/RTTY/FT8/JS8 y diálogos). Es la señal que procesa
  `audio_enh_process()` con el preset **`Capture filters`** de Ajustes.
- **Qué NO cubre y no se puede concluir de acá**: la ruta **audible** del BASE (parlante, jack,
  A2DP). La cadena de este issue no escribe en el sink y no promete audio escuchado mejor: la ruta
  RX procesado → parlante sigue `NOT_VERIFIED` (#347, `G1=NOT_VERIFIED`). Un PASS de este handoff
  **no** es un PASS de calidad audible del equipo.

## 0. Preguntas abiertas y qué evidencia las cierra

| # | Pregunta | Evidencia que la cierra | Clase que pasaría a tener |
|---|---|---|---|
| H1 | ¿El PCM capturado llega **completo y en orden** a la grabación de audio y a la de pantalla? | grabaciones simultáneas (AVI + audio) de una señal conocida, con conteo de muestras y comparación contra una referencia | `MEASURED-HARDWARE` |
| H2 | ¿La copia de captura deja de tener cortes/pérdidas con fragmentos de PulseAudio distintos? | ventana larga (≥ 10 min) con `parec` en paralelo y conteo de muestras por bloque; cero discontinuidades nuevas | `MEASURED-HARDWARE` |
| H3 | ¿Un **hole** de PulseAudio se entrega como silencio y no como audio viejo ni basura? | forzar un underrun (suspender el proceso consumidor / carga) y escuchar/medir el tramo | `MEASURED-HARDWARE` |
| H4 | ¿Los cinco presets cambian **sólo** la copia de captura y no el audio del BASE? | A/B con la misma señal: `Off` vs `SSB voice` vs `AM broadcast` vs `Digital` escuchando el parlante (debe ser idéntico) y midiendo la grabación (debe cambiar) | `MEASURED-HARDWARE` |
| H5 | ¿Los decodificadores (CW/RTTY/FT8) siguen copiando con la cadena nueva, en cada preset? | decodificación real de una señal en el aire por modo, con y sin preset | `MEASURED-HARDWARE` |
| H6 | ¿El grabador de pantalla (AVI) mantiene el audio sincronizado y sin cortes? | AVI de ≥ 60 s, verificación de pista de audio, duración y sincronía | `MEASURED-HARDWARE` |
| H7 | ¿El grabador de audio (MP3) produce un archivo íntegro? | archivo completo, duración, sin truncamiento, reproducible | `MEASURED-HARDWARE` |
| H8 | ¿El **waterfall** responde igual con cualquier preset? (no debe cambiar) | captura del waterfall con los cinco presets, misma señal: las trazas deben ser idénticas | `MEASURED-HARDWARE` |
| H9 | ¿Cuánto cuesta el callback en el R16 (CPU y p99) y aparecen pops/crashes? | `/proc/<pid>/stat` + instrumentación de tiempos del callback; escucha de pops; `crashlog` | `MEASURED-HARDWARE` |

El retardo **interno** de la cadena ya está medido en el host y **no** se vuelve a medir acá:
**882 muestras (20 ms)** = 441 de la máquina de tramas + 441 dentro de `libspeexdsp`
(`README.md` §3.1 y §5.2, `measurements.csv`). La latencia **extremo a extremo** RX→grabación
sigue `BLOCKED` (incluye PulseAudio y el disco) y no se declara.

## 1. Materiales

### 1.1 Señales de RF (todas de recepción)

| # | Señal | Para qué | Cómo se obtiene sin transmitir |
|---|---|---|---|
| S1 | Voz SSB real y estable (LSB o USB) | caso base de los decodificadores de voz y del grabador | una señal ya en el aire; **no** se transmite ni se pide reporte |
| S2 | CW real (y CWR si hay estación del otro lado de la portadora) | decodificador CW, preset `CW weak` (sin NR de Speex) | ídem |
| S3 | Portadora AM modulada | preset `AM broadcast` (nivelador AF + NR) | ídem |
| S4 | Banda vacía (sólo ruido) | piso de ruido y comparación de atenuación | ídem |
| S5 | Señal digital FT8 (o JS8) decodificable | decodificador digital con la cadena nueva | ídem |
| S6 | Impulsos / QRM cercano | pops propios vs. clicks de la banda | ídem |

Reglas: **nunca** grabar ni subir QSO de terceros; si se guarda audio, que sea de una emisión
identificable como de dominio público o de fuente propia, sin datos privados.

### 1.2 Instrumental

- Radio con la ROM que ya tenga puesta el operador (**este handoff no flashea**; si la ROM no
  incluye el cambio, el handoff queda pendiente hasta que exista una ROM con él, decisión del
  owner).
- Tarjeta SD con espacio suficiente para AVI + audio (el grabador de pantalla escribe video).
- Acceso de lectura a PulseAudio en la radio (`parec`) si se quiere la referencia de la copia de
  captura: `parec --format=s16le --rate=44100 --channels=1`.
- Acceso de lectura a `/proc/<pid>/stat` de la GUI para CPU.
- Cronómetro y planilla de registro (§5).
- Auriculares/parlante **los mismos** en todas las pasadas (para H4, que escucha el parlante).

## 2. Precondiciones y línea base

1. **Anotar el estado del equipo antes de empezar, sin cambiarlo**: modo, VFO, filtro, volumen del
   operador, `Capture filters` (Off / CW weak / SSB voice / AM broadcast / Digital), NR/NB/DNF del
   BASE, si hay A2DP conectado, y el estado de los grabadores.
2. **Un solo control por pasada**: el único parámetro que cambia entre pasadas es el que se evalúa.
3. **Mismo VFO, filtro, volumen y salida física** en todas las pasadas de una serie.
4. **No tocar mute, ruteo, modo, registros ni `x6200_control_record_set()`**. Si algo queda mudo o
   raro, se **detiene** y se reporta: no se "arregla" cambiando configuración.
5. **Sin TX**: la radio no transmite en ningún momento de este handoff.

## 3. Procedimiento (no ejecutado)

Cada serie fija la señal y varía **un** control. Las ventanas sugeridas son 60 s de escucha /
grabación; cada serie se repite dos veces alternando el orden (A/B/B/A) para reducir el sesgo.

### Serie A — decodificadores por modo (H5)

| Paso | Acción | Qué se registra |
|---|---|---|
| A1 | S1 en LSB con `Capture filters = Off` | ¿el decodificador de voz/el operador copia? texto decodificado |
| A2 | S1 en USB, mismo preset | ídem |
| A3 | S2 en CW con `Capture filters = CW weak` | texto decodificado de CW; ¿el tono bombea? |
| A4 | S2 en CWR, mismo preset | ídem |
| A5 | S5 (FT8) con el preset que use el operador | mensajes decodificados (conteo y contenido) |
| A6 | Repetir A1–A5 con `SSB voice` | comparación: ¿se pierden o se agregan decodificaciones? |

### Serie B — grabadores (H1, H6, H7)

| Paso | Acción | Qué se registra |
|---|---|---|
| B1 | Grabar pantalla (AVI, `/mnt/screenrec/screen-YYYYMMDD-HHMMSS.avi`) 60 s con S1 y `Off` | ruta, tamaño, duración, pista de audio presente |
| B2 | Grabar audio (MP3, `REC_YYYYMMDD-HHMMSS.mp3`) 60 s con S1 y `Off` | ruta, tamaño, duración, reproducible |
| B3 | Repetir B1–B2 con `SSB voice` | ídem; comparar duración y conteo de muestras |
| B4 | Repetir B1 con `AM broadcast` y S3 | ídem |
| B5 | Grabar 10 min seguidos (H2) | conteo de muestras por bloque, cortes, `dmesg`/log de la GUI |

### Serie C — presets y waterfall (H4, H8)

| Paso | Acción | Qué se registra |
|---|---|---|
| C1 | Con S1 en el aire, escuchar el **parlante** con `Off` y luego con `SSB voice` | ¿cambia algo audible? (la expectativa es **no**) |
| C2 | Ídem con `AM broadcast` (S3) y con `Digital` | ídem |
| C3 | Capturar el waterfall con cada uno de los cinco presets, misma señal y misma escala | las trazas deben ser idénticas |
| C4 | Volver a `Off` y confirmar el estado inicial | confirmación |

> Nota de alcance: en C1/C2 se escucha el **parlante del BASE**, que **no** pasa por esta cadena.
> Que no cambie es la prueba de que el cambio quedó acotado a la copia de captura; **no** es una
> prueba de que el NR de captura mejore nada audible.

### Serie D — carga, pops y estabilidad (H9, H3)

| Paso | Acción | Qué se registra |
|---|---|---|
| D1 | Con `SSB voice` y grabación activa, leer `/proc/<pid>/stat` antes y después de 120 s | utime+stime, % de un núcleo |
| D2 | Instrumentar el tiempo por callback si el build lo permite (o registrar `p99` sólo si hay instrumentación; si no, `UNKNOWN`) | p50/p99/max del callback |
| D3 | Escuchar S6 (impulsos) con `Off` y con `SSB voice` | pops por minuto, en cada preset |
| D4 | Provocar un underrun (carga de CPU, suspender el consumidor) y escuchar/medir el tramo | ¿silencio o audio viejo/basura? |
| D5 | Revisar `crashlog` de la GUI al final | crashes nuevos: 0 esperados |

## 4. Qué registrar y cómo (sin fingir equivalencia)

- La **copia de captura** se puede grabar con `parec` (`s16le`, 44100, mono). Esa señal **es** la
  que procesa `audio_enh_process()` y la que va a los grabadores/decodificadores. Compararla con
  los WAV sintéticos del banco (`docs/evidencia/2026-10-29-audio-capture-351/pcm-manifest.json`)
  sólo vale para el preset `Off` (bit-exacto): con NR activo la salida difiere por diseño.
- Lo que se escuche por el **parlante** se registra como **valoración del operador**, separada, y
  **nunca** se presenta como la misma señal que la copia de captura.
- El **retardo de 882 muestras (20 ms)** es interno de la copia; no se mide extremo a extremo sin
  instrumentar. Si no hay método, se anota `UNKNOWN`.
- Los tiempos del host (x86_64) **no** se extrapolan al R16: el CPU del R16 se mide en la radio o
  queda `BLOCKED`.

## 5. Planilla de registro (vacía; se llena en la radio)

| pasada | serie | señal | control que cambia | valor | grabación (ruta/sha) | decodificado | pops/60 s | CPU % | notas |
|---|---|---|---|---|---|---|---|---|---|
| A1 | A | S1 LSB | — (base) | Off | | | | | |
| A2 | A | S1 USB | — | Off | | | | | |
| A3 | A | S2 CW | preset | CW weak | | | | | |
| B1 | B | S1 | — | Off | | — | — | | |
| C1 | C | S1 | preset | SSB voice | — | — | | | |
| … | | | | | | | | | |

Columnas obligatorias por fila: fecha/hora, commit de la GUI, modo, VFO, filtro, volumen, preset de
`Capture filters`, NR/NB/DNF del BASE, A2DP conectado (sí/no) y salida física usada.

## 6. Checklist de seguridad (obligatorio antes de cada pasada)

- [ ] **No TX**: la radio no transmite; el **PTT no se toca**. Si la prueba exige transmitir, se
      cancela (este issue es sólo recepción).
- [ ] **No se cambia el modo** del equipo para la prueba (sólo se observa el modo vigente; el
      cambio de modo entre pasadas es una acción del operador y se registra).
- [ ] **No se cambia el ruteo**: ni `record`, ni `hmic`/`imic`, ni `spmode`, ni `rxvol` por I2C.
- [ ] **No se cambia el mute** ni el volumen por software; el volumen lo maneja el operador.
- [ ] **No se tocan registros** del códec/ALSA ni `x6200_control_record_set()`.
- [ ] **No se flashea** ni se reemplaza la ROM durante el handoff.
- [ ] **No se cargan/descargan módulos de PulseAudio** ni se reinicia el demonio.
- [ ] **No se toca la configuración de BT** (conectar A2DP es acción del operador: se registra y no
      se cambia a mitad de una serie).
- [ ] Si algo queda mudo o raro: **detener** y reportar; no "arreglar" con registros.
- [ ] Al terminar, el equipo queda en el estado anotado en §2.1 y los grabadores apagados.

## 7. Estado de cada ítem

| Ítem | Estado |
|---|---|
| Materiales S1–S6 y SD preparados | `PARCIAL` (ver §9) |
| Serie A (decodificadores LSB/USB/CW/CWR/FT8) | `PENDIENTE` (H5) |
| Serie B (grabador de pantalla AVI y de audio MP3) | `MEDIDO` (H1/H6/H7) |
| Serie C (presets `Off`/`SSB`/`AM`/`Digital` y waterfall) | `MEDIDO` (H4/H8) |
| Serie D (CPU/p99 del callback, pops, underrun, crashlog) | `PARCIAL`: CPU/crashlog/pops medidos; p99 y underrun pendientes (H9/H3) |
| Captura `parec` de la copia de captura | `MEDIDO` (H1/H2) |
| H1–H9 | `VALIDADO-PARCIAL-HARDWARE` (ver §9) |
| Cualquier medición `MEASURED-HARDWARE` de #351 | **ver §9 y `measurements.csv` de la nueva evidencia** |

## 8. Criterio de cierre de este handoff

Se considera ejecutado cuando existan, para cada serie: (1) la planilla de §5 completa con el estado
del equipo, (2) los archivos grabados con su sha256 y su duración, (3) las valoraciones del operador
separadas de las capturas, y (4) los números de CPU/pops/crashlog con su método. Recién entonces
esas filas pueden pasar de `PENDING-HARDWARE` a `MEASURED-HARDWARE` en `measurements.csv`. Hasta
entonces, cualquier afirmación de mejora audible, de latencia extremo a extremo o de CPU en la radio
es `HYPOTHESIS`, no un resultado.

## 9. Actualización 2026-10-29: validación física ejecutada

Sesión separada con el operador (owner) presente para escucha y para disparar el grabador de
pantalla. GUI de `main` en `fc18a831` (md5 `80bbe5bd70b3`, sha256 `4ee87aef…dd66b0`) desplegada
**sólo por SSH**, sin flashear ROM y sin tocar el BASE. Evidencia completa, con comandos y hashes:
[`../2026-10-29-audio-capture-351-radio/`](../2026-10-29-audio-capture-351-radio/README.md).

| Ítem | Estado | Resultado | Evidencia |
|---|---|---|---|
| H1 | `PASS` | AVI de 95,50 s con audio PCM de 4.214.248 muestras (95,56 s); referencia `parec` de 4.585.904 muestras; envolvente corr 0,856, lag -0,28 s | [`logs/h1-h6-analysis.txt`](../2026-10-29-audio-capture-351-radio/logs/h1-h6-analysis.txt) |
| H2 | `PASS` | 600 s de `parec`: 26.460.000 muestras en 599,78 s; sin pérdida observable a nivel de total | [`logs/h2-h9.log`](../2026-10-29-audio-capture-351-radio/logs/h2-h9.log) |
| H3 | `NO EJECUTADO` | No se forzaron underruns; no se observó un hole natural | — |
| H4/C1/C2 | `PASS` | Operador: "no noté ningún cambio audible" en Off/SSB/AM/Digital; la copia sí cambia (Off rms 2023 -> SSB rms 926) | [`logs/h4-operator-listening.txt`](../2026-10-29-audio-capture-351-radio/logs/h4-operator-listening.txt) |
| H5 | `NO EJECUTADO` | Sin señal CW/FT8 y sin cambio de modo | — |
| H6 | `PASS` | AVI 95,50 s: video 764 frames @ 8 fps; audio 95,56 s; diferencia 0,06 s (< 1 frame) | [`logs/h1-h6-ffprobe.txt`](../2026-10-29-audio-capture-351-radio/logs/h1-h6-ffprobe.txt) |
| H7 | `PASS` (Off vs SSB) | MP3 60,04 s reproducible; Off rms 2023 vs SSB 926; AM/Digital pendientes | [`logs/h7-metadata.txt`](../2026-10-29-audio-capture-351-radio/logs/h7-metadata.txt) |
| H8 | `PASS` | 5 capturas: std 14,71-15,15; sin diferencia atribuible al preset | [`logs/h8-waterfall-stats.txt`](../2026-10-29-audio-capture-351-radio/logs/h8-waterfall-stats.txt) |
| H9 | `PARCIAL` | CPU GUI 65,7 % con captura / 63,4 % baseline; crashlog 0; sin pops; p99 callback UNKNOWN | [`logs/h2-cpu.log`](../2026-10-29-audio-capture-351-radio/logs/h2-cpu.log) |

**Pendientes**: H3 (no se ejecuta por prohibición de forzar underruns), H5 (requiere señal
CW/RTTY/FT8 y cambio de modo) y H7 AM/Digital (el preset sólo se cambia desde la UI física).

El incidente de UI (contenido central en negro con Ajustes abierto) se resolvió reiniciando la GUI y
no reapareció; queda documentado en
[`logs/ui-incident.txt`](../2026-10-29-audio-capture-351-radio/logs/ui-incident.txt) sin declararlo
resuelto. Los MP3/AVI y el audio crudo no se publican; sólo metadatos, hashes y capturas de
UI/waterfall.
