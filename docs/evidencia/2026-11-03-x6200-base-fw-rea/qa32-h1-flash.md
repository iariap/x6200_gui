# QA32 — Flash H1 y validación RX

Fecha: 2026-11-03. Autorización explícita del propietario para H1
(`df1e274f9e6fe59c00c1ebe6aeec5dced3a07f398ecd04ed503380df75c5d488`).
No se reutilizó la autorización del candidato anterior.

## Flash

- Overlay vendor con **únicamente** H1 en `/usr/firmware`; se ocultaron todos los `.xgf` de
  la capa inferior con whiteouts y se copió H1.
- Hash del archivo visible: `df1e274f…`.
- `open firmware file "/usr/firmware/x6200-108-h1-reg34.xgf" ok.`
- IAP: `XMWorker started, 524288`; erase; 512 paquetes XMODEM; `XMWorker finished`;
  `UiFwUpgrade finished 0`.
- La radio se reinició; BASE respondió `BASE fw version 1.0.8`.

## A/B/A directo

GUI/daemon/PulseAudio detenidos; helper `libaether_x6200_control`; `arecord -D hw:0,0`
(s16le/48k/2ch, 3 s):

| Fase | RMS | Pico |
|---|---:|---:|
| A BASE | 2160,7 | 24130 |
| B DAC+ZERO (H1) | **143,0** | 4927 |
| C BASE | 1655,3 | 8535 |

- B estable sigue sin RX útil.
- H1 **no recupera RX**.
- `pcm_status` con DAC: `OPEN ok`, `WRITE frames=96000`, `RUNNING`, `DRAIN=0`,
  `AVAIL=256`; PCM aceptado, pero no se declara audible.
- Sin TX/PTT/ATU/Bluetooth.

## Conclusión

`H1_RESULT=FAIL_FUNCTIONAL`: eliminar el bit `0x40` de `reg0x34` en la rama DAC no
preserva RX. El bit `0x40` **no era el mute restante** (o no era el único).
No se hizo otro flash; no se hizo barrido de registros.

Estado restaurado: GUI original, un proceso, RX, tx=false, ALSA diff vacío, volumen17,
freq7103000; sin montajes. MCU con H1 instalado.

## Gates

- `H1_FLASHED = YES`
- `BASE_RX_PRESERVED_WITH_DAC = NO`
- `DAC_FEEDBACK_IN_CAPTURE = INCONCLUSIVE`
- `LINUX_DSP_RX_CAPTURE = FAIL`
- `LINUX_DSP_SPEAKER = NOT_VERIFIED`
- `BASE_RETURN = PASS`
