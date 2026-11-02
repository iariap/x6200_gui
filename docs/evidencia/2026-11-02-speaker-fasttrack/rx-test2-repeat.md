# RX procesado al parlante — repetición audible (volumen 40)

Fecha: 2026-11-02 17:55:01 UTC (14:55 local). Rama `test/rx-speaker-fasttrack`.
Radio RX 7.091200 MHz LSB-D, 40 m, antena conectada; operador presente y atento; volumen 40.

Mismo test que `rx-test1.md`, con `SSB voice` forzado temporalmente y 1,5 s de audio.

```
BEFORE reg12=0x00001000 voice_rec=0 rxvol=40 hmic=20 imic=30 state=RX preset=OFF
DAC state=RX_DAC dac=1
AFTER  reg12=0x00001000 voice_rec=0 rxvol=40 hmic=20 imic=30 state=RX preset=OFF
writes=16 drops=0 underruns=326
RESULT PASS_BASE_RESTORED
```

- MUX seleccionado; 16 bloques de PCM aceptados; retorno a BASE y preset restaurados.
- Sin TX/PTT/ATU; HMic/IMic y volumen sin cambios.
- GUI original restaurada; un solo `x6200_gui`; volumen final 10.
- `SPEAKER_AUDIO_HEARD`: **pendiente de la confirmación del operador** para esta repetición.
- `underruns=326`: el stream todavía no es estable; si el operador no escuchó, hay que corregir el
  pacing/buffer antes de una nueva prueba.
