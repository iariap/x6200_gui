# QA real en radio — QA2/QA3, backpressure y rollback final

Fecha: 2026-11-02 22:01–22:05 UTC. Misma variante QA (`X6200_RX_SPEAKER_QA=ON`), iterando sólo el
buffer del stream QA: 600 ms (QA2) y 1600 ms (QA3). Producción sigue con objetivo 200 ms y no se
tocó.

## Resultados

QA2 (600 ms), dos short: `writes=9` y `writes=18` acumulados; `backpressure=2/4`; soak 30 s:
`writes=80`, `backpressure=77`; todo `PASS_BASE_RESTORED`.

QA3 (1600 ms), dos short:
- short1: `writes=13`, `backpressure=1`, `PASS_BASE_RESTORED`.
- short2: `writes=26` acumulados, `backpressure=2`, `PASS_BASE_RESTORED`.

QA3 soak 30 s:
```
SOAK 30000ms
STATS writes=250 drops=0 underruns=0 restore_fail=0 backpressure=29
USEFUL writes>0=1
RESULT PASS_BASE_RESTORED
```
GUI estable, ~16% CPU del proceso, 77% idle, sin TX/PTT/ATU.

## Interpretación

- El buffer QA de 1600 ms mejora mucho la continuidad (250 bloques = 25 s de PCM en 30 s vs 80 en
  QA1), pero quedan **29 eventos de backpressure** y ~5 s sin escritura. La restauración a BASE es
  correcta; la continuidad audible queda **residual/degradada**, no se declara estable sin cortes.
- Este ajuste es **sólo QA**: el router de producción no cambia (`RX_AUDIO_LIVE_TARGET_BUFFER_MS`
  200 ms y backend fail-closed).

## Rollback final

Se restauró `x6200_gui` original `e32a81f3…` desde
`/mnt/backups/x6200_gui.pre-qa-367-20261102T213413Z`; supervisor reiniciado; **un solo** proceso,
RX, `tx=false`, volumen 1. Sin librerías LVGL reemplazadas; sin BASE/FPGA.

## Clasificación

- `QA_DEPLOY = PASS`; `QA_ROLLBACK = PASS`.
- `SHORT_RESTORE = 2/2 PASS`.
- `SOAK_RESTORE = PASS`; `SOAK_AUDIO_CONTINUITY = DEGRADED_RESIDUAL_BACKPRESSURE`.
- `SPEAKER_AUDIO_HEARD` de esta ventana: pendiente de confirmación del operador; histórico 2/11 = PASS.
- `CRASH_RECOVERY = NOT_VERIFIED`; `PRODUCTION_READY = NO`.
