# Mediciones y verificaciones (clasificadas, sin inventar)

## 1. Verificaciones de esta corrida (OFFLINE, reproducibles)

Herramienta `tools/rx-speaker-quality/analyze_audio_ab.py`; salida en `logs/offline-model.json`;
tests `tools/rx-speaker-quality/tests/test_audio_ab.py` → **5 tests OK** (`logs/tests.txt`).

| verificación | resultado | clase |
|---|---|---|
| Constantes de la cadena: 44.1 kHz, trama 441, scratch 4410, bloque 100 ms, ring 400 ms, latencia NR 882 muestras=20 ms, `writes=18`→1.8 s | **10/10 PASS** (`consistency_checks` true) | `CODE`/`ARITH` sobre la base y evidencia previa |
| La captura actual no escribe al stream de reproducción (sólo recorder/decoder) | PASS (`capture_only`) | `CODE` (`src/audio.c:74-83`) |
| Router experimental ausente en base 681074e0 | PASS (`router_not_in_base`) | `CODE` |
| Modelo biquad pasabanda (RBJ, fc 700 Hz, Q=2): atenuación fuera de banda | **−15.7 dB** (1.5–2.5 kHz) | `MODEL` (modelo propio, **no firmware**) |
| Modelo pasabanda: cambio en banda | −0.014 dB | `MODEL` |
| Modelo notch 700 Hz Q=8: atenuación del tono | **−19.2 dB** | `MODEL` |
| Modelo AGC (sólo atenúa): pico 0.935→0.663; clipping 0→0 | PASS | `MODEL` |

> `MODEL` = medición real de un modelo offline de técnicas, **no** del DSP del X6200 ni validación acústica.

## 2. Evidencia previa real (citada, no re-ejecutada)

| dato | valor | fuente (con sha256 de los archivos) |
|---|---|---|
| Ruta audible experimental confirmada | `SPEAKER_AUDIO_HEARD=YES`, `PRESET_DIFFERENCE_HEARD=YES` (operador, 2026-11-02) | `x6200_qa/docs/evidencia/2026-11-02-speaker-fasttrack/rx-test2-repeat.md` (`55fabf14…`), `rx-test3-ab.md` (`d1ac289f…`) |
| Router en A/B | `writes=18 drops=0 underruns=0`; BASE restaurado | `rx-test3-ab.md` (`d1ac289f…`) |
| Router en tests previos | `underruns=319` (test1), `326` (test2); `writes` 17/16 | `rx-test1.md`, `rx-test2-repeat.md` |
| Seguridad MUX | `GATE_A=BLOCKED_SAFE_MUX`: sin readback, sin dueño fuera de la GUI, restore ≳4 s | `x6200_qa/docs/evidencia/2026-10-29-rx-audible-route-363/README.md` (`c340cd7e…`) |
| Cadena de captura actual | 44.1 kHz mono S16; notch→AF leveler→Speex NR; longitud preservada; latencia 882 muestras=20 ms | `docs/AUDIO_ENHANCEMENT_GUIDE.md` (base 681074e0), `src/audio.c`, `src/audio.h:15` |
| Bench host Speex/RNNoise (base previa #345) | Speex ≈0.17–0.23 % tiempo real / -12.7…-13.8 dB RMS en ruido/tonos; RNNoise ≈1.95 % frame tiempo, 21.38 ms latencia; 0 clipping en 21 casos | `x6200_qa/docs/evidencia/2026-10-28-rx-audio-feasibility/{README.md,measurements.csv,rnnoise-vs-speex.csv,gates...}` (`d55ef0f3…`, `78a5042a…`, `13d81c71…`) |
| CPU en la radio (R16) y latencia RX→parlante | **BLOCKED** (no medido) | #345 y QA363 |

## 3. Lo que NO se midió / PENDING

- Inteligibilidad con habla real: **SKIPPED** (no hay voz con licencia/consentimiento).
- CPU/RAM/latencia en la radio (R16), XRUN en hardware: **BLOCKED**.
- Audibilidad/calidad de cualquier opción nativa o Linux: **PENDING_PHYSICAL_VALIDATION** (esta tarea fue offline, sin radio, sin owner).
- Ninguna conclusión acústica se deriva de `writes/drops/underruns` ni del modelo.
