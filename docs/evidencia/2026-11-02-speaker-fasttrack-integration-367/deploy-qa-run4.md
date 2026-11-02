# QA4 — buffer 3000 ms y resultado de continuidad

Fecha: 2026-11-02 22:10–22:12 UTC. Variante QA (`X6200_RX_SPEAKER_QA=ON`), sólo cambia el
buffer del stream QA a **3000 ms**; producción sigue con 200 ms y fail-closed.

Binario QA4: 14.016.852 B, SHA256
`0b41ea8bfaa05e49ad09be010dfae936253bbfbc08dcf69d03936587865ac2d6`,
md5 `d454ecd4147f53df1c6d5ad2beb040fd`; gate OK.

## Resultados

Dos `rx_speaker_test`:

```
short1: writes=15 drops=0 underruns=0 backpressure=0 RESULT PASS_BASE_RESTORED
short2: writes=30 (acumulado) backpressure=0 RESULT PASS_BASE_RESTORED
```

`rx_speaker_soak` 30 s:

```
writes=284 drops=0 underruns=0 restore_fail=0 backpressure=16
RESULT PASS_BASE_RESTORED
```

GUI estable, ~15% CPU, 71% idle; sin TX/PTT/ATU. `reg12_shadow=0x00001000`, preset restaurado.

## Evolución del soak

| Buffer QA | writes | backpressure |
|---|---|---|
| 200 ms (QA1) | 80 | 77 |
| 600 ms (QA2) | 80 | 77 |
| 1600 ms (QA3) | 250 | 29 |
| 3000 ms (QA4) | 284 | 16 |

El buffer mayor reduce los cortes; QA4 entrega 28,4 s de PCM en 30 s. Queda backpressure residual
(16 eventos) que puede oírse como cortes breves. **El operador había reportado cortes en QA3; falta
su escucha de QA4.** Producción no se toca.

## Rollback final

Original `e32a81f3…` restaurado; un solo `x6200_gui`, RX, `tx=false`, volumen 1; sin librerías
LVGL/BASE/FPGA.

- `QA_DEPLOY = PASS`; `QA_ROLLBACK = PASS`.
- `SHORT_RESTORE = 2/2 PASS`.
- `SOAK_RESTORE = PASS`; `SOAK_AUDIO_CONTINUITY = IMPROVED_RESIDUAL_BACKPRESSURE`.
- `SPEAKER_AUDIO_HEARD` QA4: pendiente de escucha; histórico 2/11 PASS.
- `CRASH_RECOVERY = NOT_VERIFIED`; `PRODUCTION_READY = NO`.
