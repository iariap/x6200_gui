# Tres caminos de audio RX (A/B/C), reconstruidos con evidencia

Base `681074e0`. Clases: **CODE** (árbol actual), **AUDIBLE** (prueba física previa confirmada por el
operador), **MEASURED-HOST/HARDWARE** (citado), **UNKNOWN/PENDING**.

## Tabla comparativa

| atributo | **A — DSP nativo (BASE)** | **B — PCM Linux → DAC/MUX → parlante (experimental)** | **C — híbrido audible** |
|---|---|---|---|
| Acceso real a RX | RF→ADC/FPGA→DSP nativo; el parlante lo alimenta la ruta nativa | captura A33 Mic1 del audio analógico del BASE (mono S16NE 44,1 kHz) | no existe ruta de retorno de PCM procesado al parlante sin B |
| Formato/tasa | nativo del firmware (no expuesto; `UNKNOWN` exacto) | 44 100 Hz mono S16 (`src/audio.h:15`, `src/audio.c`) | n/a |
| Procesamiento disponible | DFL/NR/NB/DNF/AGC/RX EQ nativos, controlables por `x6200_control_*` (QA-NATIVE-DSP/#392: PARTIAL, algoritmo UNKNOWN) | SpeexDSP NR + notch + AF leveler (`audio_enh_process`) + presets | podría combinar A (parlante) con procesamiento Linux solo para captura/analysis, no audible |
| Posición vs AGC/demod | nativa, aguas arriba/aguas abajo del demod según filtro; exacta `UNKNOWN` | después del demod nativo y de la conversión A33 (captura); no puede actuar antes del AGC nativo | no aplica |
| Latencia | la nativa (no medida; `UNKNOWN`) | captura 100 ms + NR 20 ms (882 muestras, guía) + ring 400 ms + stream; total `UNKNOWN` | n/a |
| CPU/RAM | nativo; sin costo A33 | bench host #345: Speex ≈0,17–0,23 % del tiempo real por 60 s; RAM +~0,5 MB; CPU en radio `BLOCKED` (no medido) | n/a |
| Clipping/fallas | no medido; el firmware opera | bancos: 0 clipping (host); router: `drops=0`; `underruns=319/326` en tests 1–2 y **0** en test 3 | n/a |
| Efecto en parlante | audible nativo (default, producción) | **AUDIBLE experimental** (`SPEAKER_AUDIO_HEARD=YES`, `PRESET_DIFFERENCE_HEARD=YES`, 2026-11-02) | no demostrado |
| Restricciones TX/RX | filtros RX; AGC por VFO; sin TX en alcance | `audio_play_en()` pone HMic/IMic=0 (evitado con router); MUX/`voice_rec` sin readback; QA363 `BLOCKED_SAFE_MUX` | n/a |
| Madurez | producción (fabricante) | experimental, **no en base 681074e0** (`src/rx_audio_live.*` ausente), sin restore ante crash ni interlocks | inexistente |

## A — DSP nativo (producción)

```text
RF → front-end/ADC/FPGA → DSP nativo (DFL/NR/NB/DNF/AGC/RX EQ) → parlante interno
                                   └─ (audífono/PA según modo)
```
- El usuario ya selecciona filtros/AGC/NR/NB/DNF/EQ desde la GUI; el efecto es del firmware nativo (QA-NATIVE-DSP: control VERIFIED, algoritmo PARTIAL/UNKNOWN).
- **No requiere cambios** de radio/firmware para existir ni para controlarse desde Linux.

## B — capturado por Linux y devuelto al parlante (experimental)

```text
BASE audio analógico → Mic1 A33 → PulseAudio capture 44.1k mono S16
   → audio_enh_process (notch → AF leveler → Speex NR)
   → rx_audio_live (ring 4x4410=400 ms, worker) → stream PulseAudio propio
   → DAC SoC → MUX (voice_rec) → parlante interno
```
- Evidencia audible: 2026-11-02, operador confirmó escucha y diferencia Off vs SSB voice.
- Evidencia de seguridad: QA363 `GATE_A = BLOCKED_SAFE_MUX` (sin readback real, sin dueño serializado, restore ≳4 s reiniciando GUI).
- Evidencia de estabilidad: test1 `underruns=319`, test2 `326`, test3 corregido `0`; `writes=18 drops=0`.
- **No está en la base** `681074e0` (verificado: `src/rx_audio_live.c` no existe).

## C — híbrido audible

No hay topología demostrada que permita que el PCM procesado por Linux retorne al parlante sin B
(no hay entrada digital expuesta al A33 para el audio nativo; QA390 dejó el endpoint I2S3 UNKNOWN).
El híbrido posible es **A audible + procesamiento Linux sólo para grabadora/decoders** (no audible),
que es lo que ya existe. Por eso C audible = **NO_GO**; el híbrido de captura ya está en la GUI.
