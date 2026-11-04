# Rendimiento/latencia offline (sin medición física)

| ítem | valor | clase |
|---|---|---|
| PCM | 44 100 Hz mono S16, bloque 4410 (=100 ms), ring 4 bloques (400 ms) | CODE (#364 router / contrato) |
| post-SpeexDSP | una sola pasada; `audio_enh_process` ya existe; router consume el mismo PCM; duplicado rechazado | CODE + test (`double_process`) |
| lease/heartbeat | heartbeat ≤50 ms; lease 100 ms; restore budget 250 ms | CODE/ARITH |
| restore medido (test host) | supervisor: 230 ms desde el último heartbeat (incluye ventana de lease); árbitro modelo previo: 110 ms | HOST-TEST |
| IPC | SOCK_SEQPACKET, request/response con timeout; callbacks de audio no esperan (worker) | CODE/HOST |
| CPU/RAM en la radio | **BLOCKED** (no medido; no hay build/radio en esta fase) | BLOCKED |
| latencia extremo a extremo RX→parlante | **PENDING** (físico) | PENDING |

No se extrapola CPU del host a la radio. Sin PASS físico.
