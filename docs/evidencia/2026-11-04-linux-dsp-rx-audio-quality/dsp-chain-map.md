# Mapa real de la cadena (código de base, auditado)

`capture` mono S16NE 44100 Hz, bloques 4410 (100 ms) → `audio_enh_process`:
1. `notch` (liquid-dsp FIR, hasta AUDIO_ENH_MAX_NOTCHES) por presets/estado.
2. `AF leveler` (existente; modos/ataque/release/target; puede estar off por preset).
3. `SpeexDSP preprocess` (NR/AGC propio, frames 10 ms/441) con rate real 44100.
4. Salida al recorder/decoders y (feature ON) al router QA.

Presets existentes: OFF, CW Weak, SSB Voice (NR on level 40, leveler off), AM Broadcast (NR 60 +
leveler), Digital (bypass). No se agregó motor, filtro, EQ ni preset nuevo. Feature OFF: cadena normal
del build BASE sin cambios.
