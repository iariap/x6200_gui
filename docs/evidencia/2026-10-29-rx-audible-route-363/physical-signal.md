# Señal física — qué hay medido, qué NO (#363)

**Veredicto de señal física: `PHYSICAL_OUTPUT_NOT_VERIFIED`.** En esta corrida **no** se conmutó el
MUX, **no** se reprodujo audio por el DAC hacia el parlante/jack y **no** hubo escucha ni instrumentación.
A4 no se ejecutó porque A0-A3 no son PASS. Este documento separa lo que ya estaba medido por el proyecto
de lo que esta corrida simplemente **no midió**.

## 1. Lo que esta corrida NO hizo (por diseño y por gate)

- No se escribió el registro 12 ni ningún otro registro del BASE (ni siquiera `voice_rec=0`).
- No se llamó `audio_play_en()` ni `x6200_control_record_set()`.
- No se reprodujo PCM al sink (`aplay`/`paplay`/`speaker-test`/tono): no hubo audio audible nuevo.
- No se conmutó el MUX, no se cambió `rxvol`, `spmode`, modo, frecuencia, volumen, mute, NR/DFL ni ganancias
  de micrófono. No se tocó PTT/ATU/HMic/IMic/TX.
- No hubo instrumentación física: sin micrófono/loopback acústico, sin osciloscopio, sin analizador, sin
  carga ficticia, sin interfaz de audio aislada. No hubo escucha humana autorizada en esta corrida.
- No se grabó ni publicó ningún QSO ni audio de terceros.

## 2. Lo que sí está medido (de esta corrida, solo lectura)

- La cadena SoC está armada y **corriendo**: sink `alsa_output.platform-sound.stereo-fallback` RUNNING con
  un sink-input de la GUI (`s16le 1ch 44100 Hz`, pid 4716); source
  `alsa_input.platform-sound.stereo-fallback` RUNNING; PCM capture/playback `RUNNING`, owner PulseAudio
  (`pactl`/`/proc/asound`, `logs/a0-preflight.txt`).
- El códec `sun8i-a33-audio` y la DAI `1c22c00.dai-sun8i-codec-aif1` están presentes; niveles de mezcla
  medidos (tabla en `registers.md` §4; crudo en `logs/mixer-live.txt`).
- La captura del SoC recibe la señal del BASE: `mixer_setup()` configura `Mic1` como "capture audio from
  radio" (`src/audio.c:156-174`) y el source está RUNNING. **Esto no es salida audible**: es RX→Linux.

## 3. Evidencia física previa del proyecto (citada, no producida por esta corrida)

| medición | resultado | fuente |
|---|---|---|
| Captura RX real en la radio (`parec` 2 s del source) | RMS **−22,44 dBFS**, pico **−10,51 dBFS**, 100 % de muestras ≠ 0 | `docs/evidencia/2026-10-28-rx-audio-hardware/logs/g1-parec-stats.txt` |
| Monitor del sink durante #347 | silencio digital (0/78.569 muestras ≠ 0): la GUI no generaba audio | idem, §3 |
| Cadena del códec configurada y **sin sonido** en el parlante (prueba del 12/10, GUI parada) | `speaker-test` sin sonido; **no quedó captura** y hoy no se puede repetir con la GUI andando (`Playback open error: -16 Device or resource busy`) | `docs/HALLAZGOS.md` §1 |
| Capture filters (Off/SSB/AM/Digital) vs parlante | el operador **no notó cambio audible**; "la ausencia de cambio audible es lo esperado y no se declara mejora audible" | `docs/evidencia/2026-10-29-audio-capture-351-radio/README.md` (§H4, limitación 5) |
| PTT por I2C con `iptt` (7,100 MHz) | `tx=True` medido; PTT del micro en CB verificado por el dueño (reporte, no instrumentado) | `docs/HALLAZGOS.md` §76 |
| `rxvol=0` = mute del audio de recepción (reg 14) | medido que el nivel persistido 0 explica el silencio; recuperación por power-cycle | `docs/HALLAZGOS.md` §64 |

## 4. Lo que la documentación externa dice (no es medición nuestra)

El proyecto [Mestre](https://git.sa6anw.se/sa6anw/mestre/src/branch/main/audio-architecture.md) documenta
para el X6200: MUX físico interno con `voice_rec` eligiendo entre audio del receptor y DAC del SoC; PA
habilitada por `rxvol_set()>0`; dependencia del clock `pll-audio` del códec; y la tabla de mezcladores
necesaria. Es una pista de terceros (otro equipo/ROM). En nuestro snapshot, **7 de las 8 filas de su
tabla coinciden** con lo medido; la octava (`DAC Playback Switch`) está **on** en nuestro equipo y su
documento la pide **off** (ver `registers.md` §4). **No se usó como prueba de PASS**: A4 hubiera exigido
medir en nuestro ejemplar.

## 5. Por qué A4 no puede ser PASS sin señal física

Un `PASS` de ruta audible requiere (regla del issue): fuente PCM propia de bajo nivel (≤ −24 dBFS) por
stream dedicado, conmutación transitoria de ≤2 s por el dueño autorizado del reg 12, restauración
automática ≤250 ms, sin `audio_play_en`, sin TX/PTT/ATU/mic, y **verificación audible o instrumentada en
el parlante/jack**. Nada de eso se hizo ni se podía hacer sin A2/A3 PASS, y no hay instrumentación
disponible en esta corrida. Un sink RUNNING o el monitor del DAC **no** demuestran sonido: por eso el
estado correcto es `PHYSICAL_OUTPUT_NOT_VERIFIED`, no `PASS`.

## 6. Protocolo mínimo para una futura A4 (lo que falta)

1. Dueño serializado del reg 12 **dentro** del proceso que hoy controla el I2C (o un mecanismo probado
   que lo desplace), con interlock de TX y restore selectivo verificado.
2. Tono/PCM propio de nivel seguro (≤ −24 dBFS, WAV S16NE 44100 mono de fixture, sin voz de terceros),
   reproducido por un stream **separado** de TTS/digital.
3. Instrumentación: captura del parlante (micrófono a distancia fija/atenuada) y del jack (interfaz
   aislada), o escucha humana autorizada con registro de quién/cuándo.
4. Medir con MUX en BASE y con MUX conmutado: diferencia de nivel/espectro, retorno a BASE, reloj ALSA
   estable, sin clicks/pops, sin TX/PTT.
5. Reportar el resultado como `A4=PASS` sólo con la captura de la señal; si no hay instrumento,
   `PHYSICAL_OUTPUT_NOT_VERIFIED`.
