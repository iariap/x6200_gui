# Prueba 2 en radio — playback existente, tres beeps audibles

Fecha: 2026-11-02 17:37 UTC. Rama `test/speaker-fasttrack`; base `main` 681074e0.
Operador presente y prestando atención; volumen de radio 50 durante la prueba (el volumen había
quedado bajo en la prueba 1). RX 7.091200 MHz LSB-D, 40 m, antena conectada.

## Cambio respecto de la prueba 1

Se cambió el patrón del test para que entrara en el buffer de 0,4 s del `play_stm`:

- **3 ráfagas de 0,2 s de 880 Hz, separadas por 0,25 s** (0,6 s de audio total);
- volumen 50, sin cambiar HMic/IMic;
- sigue usando `audio_play_en(true) -> audio_play() -> audio_play_en(false)`;
- no hay I²C directo, TX, PTT ni ATU.

## Resultado medido

```
START rx=1
BEFORE reg12=0x00001000 voice_rec=0 rxvol=50 hmic=20 imic=30 state=0
AFTER  reg12=0x00001000 voice_rec=0 rxvol=50 hmic=20 imic=30 state=0 written=26460 failed=0
RESULT PASS_BASE_RESTORED
DONE
```

- `written=26.460` muestras = 0,6 s; `failed=0`; las tres ráfagas entraron completas.
- `reg12` idéntico antes/después (`0x00001000`, `voice_rec=0`), `rxvol=50`, HMic/IMic=20/30,
  `state=RX`, `tx=false`; sin PTT/ATU.
- La GUI se reinició con el binario original `e32a81f3…`; quedó operativa en RX.
- El test no modifica parámetros persistentes.

## Clasificación pendiente de la escucha

- `SPEAKER_AUDIO_HEARD`: **pendiente de confirmación del operador** (tres beeps de 880 Hz a las
  14:37:14 local).
- `NOMINAL_BASE_RETURN_PASS`: el estado de registro/radio volvió a BASE; falta confirmar
  auditivamente que volvió el audio RX.
- `PRODUCTION_NOT_READY`.

Si el operador confirma los beeps, el camino existente `audio_play_en`/DAC/MUX funciona y se puede
pasar a la ruta RX procesada continua. Si no, el fallo es de la cadena de parlante/PA/MUX y no del
buffer del `play_stm`.
