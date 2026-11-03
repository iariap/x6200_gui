# QA10 — Aislamiento del eco: ZERO/RAW/PROCESSED

Binario QA10: 14.026.880 B, SHA256
`80d334fa2c0e0a6f7d1edee3e553c55cd084117d7090f4af3b9598ef355c8e33`,
md5 `fa1a8273013484ea26afab704785540b`; gate OK.

## Protocolo ejecutado

Un único `rx_speaker_echo`: preflight RX, mismo stream `X6200 RX Live`, un solo `MUX_ON`, misma
frecuencia (7.3555 MHz LSB), mismo volumen inicial (10), preset SSB voice. Tres fases de 5 s:

1. **DAC_ZERO** (t≈0,2–5,2 s): router recibe PCM cero; `audio_enh_process` sigue ejecutándose y
   recorder/decoders reciben la PCM procesada normal.
2. **DAC_RAW** (t≈5,2–10,2 s): router recibe la copia **pre-`audio_enh_process`**; los otros
   consumidores siguen recibiendo la PCM procesada.
3. **DAC_PROCESSED** (t≈10,2–15,2 s): router recibe la PCM post-Speex con SSB voice.

## Métricas

- 15 s: `writes=152` (~10/s), `drops=0`, `underruns=0`, `backpressure=0`.
- Un solo `MUX_ON` (198 ms) y un `MUX_OFF`/`BASE` al final; un solo stream RX Live.
- Captura 44.100 muestras/s; `read_index` avanza 1,0 s/s; `prebuf=0`; `tlength=134064 B`.
- No hay módulos loopback/echo/remap en PulseAudio; el sink input `X6200 GUI Play` está presente
  pero sin datos (Buffer Latency 0), además de `X6200 RX Live`.
- Snapshots de fase en `qa10/phase-*-snapshot.txt`; `/proc/asound` RUNNING.

## Clasificación audible

Pendiente de confirmación del operador:

- ¿Se oyó RX durante **ZERO**?
- ¿RAW limpio y PROCESSED con eco? ¿O las tres limpias?

Hipótesis a discriminar:

- ZERO audible → ruta adicional/MUX/mezcla fuera del PCM de QA.
- ZERO silencio, RAW/PROCESSED con eco → duplicación o realimentación antes del DSP/reproducción.
- ZERO silencio, RAW limpio y PROCESSED con eco → `audio_enh_process`/filtros.
- Tres limpias → no reproducido con este protocolo; repetir condiciones QA8.

## Estado

- `QA10_PHASES = PASS` (ejecución/stream/restore).
- `ECHO_CAUSE = PENDING_OPERATOR_AUDIBLE`.
- Rollback: original `e32a81f3…`, un GUI, RX, `tx=false`, volumen 1.
- `PRODUCTION_READY = NO`; sin merge.
