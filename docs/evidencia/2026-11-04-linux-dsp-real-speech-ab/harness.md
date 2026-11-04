# Harness DSP real (offline)

- Código real `src/audio_enhancement.c` (notch liquid-dsp + AF leveler + SpeexDSP real del sysroot
  ARM), stubs sólo de LVGL/config; compila con toolchain ARM y corre bajo `qemu-arm-static`.
- `dsp_run.c`: lee WAV 44.1k mono S16, aplica modo, escribe WAV; **reinicia estado por corrida**
  (`audio_enh_init/deinit`), frames 441; no normaliza ni oculta ganancia.
- Modos: B=bootstrap bypass (`OFF`), C=SSB Voice actual, D1=SSB+leveler(-20,medium)+NR30,
  D2=SSB+NR55. A/B/C/D sobre la **misma entrada**; BASE físico original no se simula (sólo bypass
  digital), no se afirma BASE como audio PRE-DSP.
- `run_all.sh`: prepara corpus/degradaciones → DSP real bajo QEMU → métricas → paquete ciego.
- Determinismo: dos corridas completas, 600 WAVs, hashes idénticos (`WAV_FILES=600`,
  `DETERMINISM_OK`); integridad de longitud y continuidad.
