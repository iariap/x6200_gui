# A/B/C/D harness con DSP REAL (offline)

`tools/linux-dsp-rx-quality/build_bench.sh` compila `src/audio_enhancement.c` (código real) con
SpeexDSP y liquid **reales** del sysroot ARM, + stubs de LVGL/config; ejecuta bajo `qemu-arm-static`
(estimación ARM, no CPU física de la radio).

Modos (mismo PCM de entrada exacto):
- **A BASE**: passthrough (referencia de entrada).
- **B Linux bypass**: `AUDIO_ENH_PRESET_OFF` (sin NR/nivelador).
- **C actual**: `AUDIO_ENH_PRESET_SSB_VOICE`.
- **D1**: SSB Voice + leveler existente (-20 dB target, medium) + NR level 30 (uso de APIs existentes).
- **D2**: SSB Voice + NR level 55 (más NR; candidato a rechazar si artefacta).

Fixtures sintéticas **generadas y redistribuibles** (no voz de terceros), 1.2 s, mono 44.1k:
male/female voiced sintética, weak+noise, band noise, QRM tone, impulses, level steps, silence,
max-level. Hashes en `fixtures.sha256`; WAV regenerables con el harness (no se versionan).
Métricas descriptivas: RMS/peak/clip, SNR descriptivo vs señal sintética conocida, correlación de
envolvente, brillo (diferencia media/RMS), pumping (std/mean de envolvente), CPU/RTF estimada y
partición/continuidad. **STOI/ESTOI no se calcularon**: no hay corpus de voz autorizado/redistribuible;
no se presentan métricas inválidas como inteligibilidad.
