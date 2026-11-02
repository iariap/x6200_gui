# RX procesado al parlante — A/B Off vs SSB voice

Fecha: 2026-11-02 17:58:16 UTC (14:58 local). Rama `test/rx-speaker-fasttrack`.
Radio RX 7.103000 LSB, 40 m, antena conectada, volumen 40.

Ruta: captura → `audio_enh_process` → `rx_audio_live` (ring + worker + stream propio) → DAC/MUX.
Sin `audio_play_en`; HMic/IMic sin tocar; `radio_audio_route_set()` serializado.

Durante la misma prueba, con el MUX ya en DAC:

1. `PHASE OFF` (0,7 s) — preset Off.
2. `PHASE SSB_VOICE` (0,7 s) — preset SSB voice.

```
BEFORE reg12=0x00001000 voice_rec=0 rxvol=40 hmic=20 imic=30 state=RX preset=OFF
DAC state=RX_DAC dac=1
PHASE OFF
PHASE SSB_VOICE
AFTER  reg12=0x00001000 voice_rec=0 rxvol=40 hmic=20 imic=30 state=RX preset=OFF
writes=18 drops=0 underruns=0
RESULT PASS_BASE_RESTORED
```

- Se corrigió el contador de underruns: ahora mide huecos reales; dio **0**.
- MUX seleccionado y retorno a BASE; preset, reg12, HMic/IMic y volumen restaurados.
- Sin TX/PTT/ATU; GUI original restaurada; un solo `x6200_gui`; volumen final 10.
- `SPEAKER_AUDIO_HEARD`: ya confirmado para la ruta. Para esta A/B falta que el operador
  confirme si distinguió **Off** de **SSB voice**.
- `PRODUCTION_NOT_READY`: sin restore automático ante crash ni interlocks permanentes.
