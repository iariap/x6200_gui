# Handoff de hardware: QA de audio de RX, sólo recepción

- **Estado global**: `PENDING-HARDWARE`. **Nada de este documento se ejecutó.** No se encendió la
  radio, no se transmitió, no se flasheó y no se cambió ningún registro. Lo ejecuta el pipeline físico
  separado (el issue lo dice explícitamente), no el agente que escribió este banco.
- **Issue**: [iariap/x6200_gui#345](https://github.com/iariap/x6200_gui/issues/345), contrato 5 (QA de
  presupuesto/escucha). Base `a1c667ee`; rama `factory/rx-audio-feasibility`.
- **Qué contrasta**: los riesgos medidos en el host y las hipótesis que **sólo** la radio puede cerrar
  (`README.md` §5 y §6). El objetivo es decidir si existe una ruta audible segura y si el
  procesamiento actual ayuda o perjudica la inteligibilidad — no implementar nada.
- **Qué NO es**: no es un procedimiento de flasheo, ni de cambio de configuración del equipo, ni una
  receta de DSP. No se toca TX, ni modo, ni ruteo, ni mute, ni registros.

## 0. Qué se quiere averiguar (y con qué evidencia se cierra)

| # | Pregunta abierta | Evidencia que la cerraría | Clase que pasaría a tener |
|---|---|---|---|
| H1 | ¿Qué salida suena hoy con la GUI andando: parlante del equipo, jack, auriculares A2DP? | señal conocida + escucha/medición en cada salida, con el volumen del operador sin tocar | `MEASURED-HARDWARE` |
| H2 | ¿El audio que procesa la GUI llega al parlante? | comparación de la copia de captura (WAV) contra lo que se escucha por el parlante | `MEASURED-HARDWARE` |
| H3 | ¿El loopback A2DP entrega RX **sin** el procesamiento de la GUI? | dos capturas simultáneas (monitor del sink A2DP vs copia de captura) con preset de NR fuerte | `MEASURED-HARDWARE` |
| H4 | ¿El salto de 2216 LSB en el borde de cada callback se oye como click? | escucha con voz y con tono, y conteo de clicks por minuto | `MEASURED-HARDWARE` |
| H5 | ¿El retardo de 5.805 ms y el `level` invertido cambian la inteligibilidad? | A/B controlado con voz SSB/AM real, NR OFF vs ON y niveles | `MEASURED-HARDWARE` |
| H6 | ¿Cuánto CPU consume la GUI en el R16 con y sin la cadena? | lectura de `/proc` de la GUI durante la prueba | `MEASURED-HARDWARE` |

Ninguna de estas seis se puede responder con los WAV sintéticos del banco: tonos y ruido prueban
integridad de procesamiento, **no** inteligibilidad (`measurements.csv`, fila `audible_intelligibility`
= `BLOCKED`).

## 1. Materiales

### 1.1 Señales de RF (todas de recepción)

| # | Señal | Para qué | Cómo se obtiene sin transmitir |
|---|---|---|---|
| S1 | Voz SSB real, estación fuerte y estable | inteligibilidad de referencia | una señal ya en el aire, elegida por el operador; **no** se transmite ni se pide reporte |
| S2 | Voz SSB débil (cerca del piso de ruido) | caso donde un NR puede "apagar" la voz (advertencia de KX3 del issue) | ídem |
| S3 | Voz AM real (portadora + modulación) | caso `AM_BROADCAST` (nivelador AF encendido) | ídem |
| S4 | Ruido constante (banda vacía, sin señal) | piso de ruido para comparar atenuación percibida | ídem |
| S5 | QRM cercano (otra estación a pocos kHz) | ¿el NR ayuda o arrastra la voz? | ídem |
| S6 | Impulsos (estática / ruido impulsivo) | clicks propios vs clicks de la cadena | ídem |

Reglas: **nunca** grabar ni subir QSO de terceros; si se guarda audio, que sea de una emisión
identificable como de dominio público o de una fuente propia, y sin datos privados (el issue lo
prohíbe). Si no hay una señal adecuada en el aire, la prueba se posterga: no se simula voz.

### 1.2 Instrumental

- Radio con la GUI de esta rama (sin flashear en esta corrida: la ROM que ya esté puesta).
- Auriculares/parlante **el mismo** en todas las pasadas, y el mismo jack si se usa línea.
- Cronómetro o reloj de pared para las ventanas de escucha.
- Si se captura audio por software: acceso de lectura a PulseAudio (`parec`) en la radio.
- Si se mide CPU: acceso de lectura a `/proc/<pid>/stat` de la GUI.
- Planilla de registro (tabla de §5) y, si se puede, micrófono externo para registrar el parlante
  **acústicamente** (se marca como tal, no como salida idéntica).

## 2. Precondiciones y línea base

1. **Anotar el estado del equipo antes de empezar** (sin cambiarlo): modo, VFO, filtro, volumen del
   operador, `Audio enhance` (Off / CW weak / SSB voice / AM broadcast / Digital), NR/NB/DNF del BASE y
   si hay auriculares A2DP conectados. Se registra, no se modifica.
2. **Un solo control por pasada**: el único parámetro que cambia entre pasadas es el que se está
   evaluando (NR del BASE, o `Audio enhance`, nunca los dos juntos).
3. **Mismo VFO, mismo volumen, mismo filtro y misma salida física** en todas las pasadas de una serie
   (requisito del issue). Si el operador tiene que mover el volumen para "escuchar mejor", la pasada se
   invalida y se repite.
4. **No tocar mute, ruteo, modo, registros ni `x6200_control_record_set()`**. Si algo queda mudo, se
   detiene la prueba y se reporta: no se "arregla" cambiando configuración.
5. **Sin TX**: la radio no transmite en ningún momento de este handoff.

## 3. Procedimiento (no ejecutado)

Cada pasada tiene una duración fija de escucha (sugerido: 30 s por señal) y se repite **dos veces**
alternando el orden (A/B/B/A) para reducir el sesgo de expectativa.

### Serie A — BASE NR (sin tocar la GUI)

| Paso | Acción | Qué se registra |
|---|---|---|
| A0 | Fijar S1 (voz SSB fuerte), anotar estado | línea base: NR del BASE **OFF** |
| A1 | Escuchar 30 s con NR OFF | valoración de inteligibilidad + notas |
| A2 | Encender NR del BASE (control de siempre, sin cambiar nada más) | valoración 30 s |
| A3 | Recorrer los niveles de NR del BASE que el operador use habitualmente (sin inventar valores nuevos) | valoración por nivel, anotando el número que muestra la UI |
| A4 | Repetir A1–A3 con S2 (voz débil) y S4 (ruido solo) | ídem |
| A5 | Apagar NR del BASE y verificar que vuelve el estado de A0 | confirmación |

### Serie B — `Audio enhance` de la GUI (copia de captura)

| Paso | Acción | Qué se registra |
|---|---|---|
| B0 | Fijar S1 y anotar el preset vigente | línea base |
| B1 | `Audio enhance = Off` durante 30 s | valoración |
| B2 | `Audio enhance = SSB voice` durante 30 s | valoración (NR de la GUI con `level` 40, `src/audio_enhancement.c:188-193`) |
| B3 | `Audio enhance = AM broadcast` con S3 | valoración (nivelador AF + `level` 60, `:195-201`) |
| B4 | Repetir B1–B2 con S2 (voz débil) | valoración: ¿la voz desaparece? |
| B5 | Repetir con S5 (QRM) y S6 (impulsos) | valoración + conteo de clicks |
| B6 | Volver a `Off` y confirmar | confirmación |

> Nota de alcance: en esta serie se evalúa **la copia de captura** (grabadora/decodificadores). Que
> eso suene por el parlante es H2, y hoy no está probado (`README.md` §6). **No** se debe presentar la
> serie B como "efecto en el parlante" si H1/H2 no se resolvieron.

### Serie C — CW y digital (control de no-regresión)

| Paso | Acción | Qué se registra |
|---|---|---|
| C1 | En CW, con `Audio enhance = SSB voice`, escuchar 30 s | ¿el tono se corta o bombea? (el código fuerza `CW_WEAK` sólo al cambiar de modo, `src/audio_enhancement.c:331-345`) |
| C2 | En el modo digital que el operador use, con el preset que tenga | ¿el decodificador sigue copiando? |
| C3 | Volver al preset y modo originales | confirmación |

## 4. Qué registrar y cómo (sin fingir equivalencia de muestras)

### 4.1 Si existe una salida capturable

La única salida digital capturable es la **copia de captura de la GUI** (el *source* de PulseAudio que
consume `read_callback()`, `src/audio.c:40`). Se puede grabar con `parec` a 44100 mono S16 y comparar
contra los WAV de referencia del banco. **Eso no es el parlante**: es exactamente la señal que
`audio_enh_process()` reescribe (`src/audio.c:63`), y **no** prueba lo que el operador escucha.

Reglas de captura:

- Mismo formato en todas las pasadas: `--format=s16le --rate=44100 --channels=1`.
- Misma duración y misma señal en A y B.
- Registrar el nombre del archivo con el parámetro de la pasada (`S1_nrbase_off.wav`,
  `S1_nrbase_on_nivelNN.wav`, …) y su sha256.
- Si la captura se corta o el *source* cambia de dispositivo, la pasada se invalida.

### 4.2 Si NO hay salida capturable idéntica (caso esperado para el parlante)

Se registran **valoraciones del operador**, explícitamente cualitativas:

- inteligibilidad en una escala fija y declarada (p. ej. 1 = no se entiende, 5 = copia completa);
- palabras/identificativo entendidos vs perdidos en la ventana;
- presencia de "voz bombeada", "voz que desaparece", "voz metálica";
- clicks contados en la ventana.

**Prohibido**: presentar la valoración como si fuera una medición de señal, o comparar un WAV de la
copia de captura contra una escucha del parlante como si fueran la misma señal. Son dos observaciones
distintas y se reportan por separado.

## 5. Planilla de registro (vacía; se llena en la radio)

| pasada | señal | control que cambia | valor | salida física | ¿WAV? (ruta/sha) | inteligibilidad 1–5 | clicks/30 s | notas |
|---|---|---|---|---|---|---|---|---|
| A1 | S1 | — (base) | NR BASE off | — | — | | | |
| A2 | S1 | NR BASE | on | — | — | | | |
| … | | | | | | | | |

Columnas obligatorias por fila: fecha/hora, commit de la GUI, modo, VFO, filtro, volumen, preset de
`Audio enhance`, NR/NB/DNF del BASE y si hay A2DP conectado.

## 6. Qué medir además de la escucha

| Métrica | Cómo | Unidad | Cuidado |
|---|---|---|---|
| CPU de la GUI | lectura de `/proc/<pid>/stat` (utime+stime) antes y después de una ventana fija, con y sin `Audio enhance` | % de un núcleo | no confundir con el CPU del host del banco (0.17–0.23 %, `README.md` §4.4): son máquinas distintas |
| Jitter / continuidad | diferencia entre bloques entregados y tiempo real; o `parec` y conteo de muestras por lectura | muestras, ms | si se instrumenta código, queda fuera de esta tarea |
| Latencia | **no** se puede medir extremo a extremo sin instrumentar; se registra `UNKNOWN` si no hay método | ms | el único dato firme es el retardo interno del NR: 256 muestras = 5.805 ms |
| Clicks | conteo auditivo por ventana de 30 s + escucha de S6 | clicks/30 s | distinguir clicks de la cadena de clicks de la banda |
| Voz / identificativo preservado | ¿se copia el identificativo completo en S1 y S2? | sí/no + notas | es el criterio "operator first" del issue |

## 7. Checklist de seguridad (obligatorio antes de cada pasada)

- [ ] **No TX**: la radio no transmite; el PTT no se toca. Si la prueba exige transmitir, se cancela.
- [ ] **No se cambia el modo** del equipo para esta prueba (sólo se observa el modo vigente).
- [ ] **No se cambia el ruteo**: ni `record`, ni `hmic`/`imic`, ni `spmode`, ni `rxvol` por I2C.
- [ ] **No se cambia el mute** ni el volumen por software; el volumen lo maneja el operador.
- [ ] **No se flashea** ni se reemplaza la ROM durante el handoff.
- [ ] **No se cargan/descargan módulos de PulseAudio** ni se reinicia el demonio.
- [ ] **No se toca la configuración de BT** (conectar auriculares A2DP es una acción del operador, se
      registra y no se cambia a mitad de una serie).
- [ ] Si algo queda mudo o raro: **detener** y reportar; no "arreglar" con registros.
- [ ] Al terminar, el equipo queda en el estado anotado en §2.1.

## 8. Estado de cada ítem

| Ítem | Estado |
|---|---|
| Materiales S1–S6 preparados | `PENDING-HARDWARE` |
| Serie A (NR del BASE) | `PENDING-HARDWARE` |
| Serie B (`Audio enhance`) | `PENDING-HARDWARE` |
| Serie C (CW/digital, no-regresión) | `PENDING-HARDWARE` |
| Captura WAV de la copia de captura | `PENDING-HARDWARE` |
| Valoraciones de inteligibilidad | `PENDING-HARDWARE` |
| CPU de la GUI en la radio | `PENDING-HARDWARE` |
| Jitter / latencia / clicks | `PENDING-HARDWARE` |
| H1 (qué salida suena) | `PENDING-HARDWARE` |
| H2 (¿el procesado llega al parlante?) | `PENDING-HARDWARE` |
| H3 (loopback A2DP sin procesar) | `PENDING-HARDWARE` |
| Cualquier medición `MEASURED-HARDWARE` | **0 filas**: no hay ninguna |

## 9. Criterio de cierre de este handoff

El handoff se considera ejecutado cuando existan, para cada serie: (1) la planilla de §5 completa con
el estado del equipo, (2) los WAV con sha256 **si** hubo salida capturable, (3) las valoraciones del
operador separadas de las capturas, y (4) los números de CPU/jitter/clicks con su método. Recién
entonces esas filas pueden pasar de `PENDING-HARDWARE` a `MEASURED-HARDWARE` en
`measurements.csv`. Hasta entonces, cualquier afirmación de mejora audible es `HYPOTHESIS`.
