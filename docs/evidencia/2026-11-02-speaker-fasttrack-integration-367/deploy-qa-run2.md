# QA real en radio — segunda ventana (power-cycle)

Fecha: 2026-11-02 21:56–21:59 UTC. Worktree ASF `factory/rx-speaker-integration-367`; QA build
`X6200_RX_SPEAKER_QA=ON`, binario 14.016.852 B SHA256
`b4714114da636accbf843aa50158be7f39afebc1eb496bfd080d7f3c366d1daa`.

## Preflight

- GUI original `e32a81f3…`, un solo proceso, RX, `tx=false`.
- Backups exactos en `/mnt`: `x6200_gui.pre-qa-367-20261102T213413Z` (md5 `e32a81f3…`) y
  `params.db.pre-qa-367-…`; rollback copiado a `/tmp` y hash verificado antes de instalar.
- Deploy hot-swap por SSH; se reinició solo la GUI/supervisor. Un proceso `x6200_gui`; Fase A
  `verifica-radio.sh` **TODO OK** (assets, entrada, PulseAudio, sin crash).

## Fase B — dos short

Con volumen 30, dos `rx_speaker_test` consecutivos:

```
START mode=short ... preset=OFF
PHASE OFF
PHASE SSB_VOICE
STATS writes=4 drops=0 underruns=0 restore_fail=0 backpressure=3
RESULT PASS_BASE_RESTORED
```

Segunda corrida: `writes=8` (acumulado), `backpressure=6`, también `PASS_BASE_RESTORED`.
En ambos: `reg12_shadow=0x00001000`, preset restaurado a `OFF`, HMic/IMic=20/30, RX, `tx=false`.

## Fase C — soak 30 s

```
START mode=soak ... preset=OFF
SOAK 30000ms
STATS writes=80 drops=0 underruns=0 restore_fail=0 backpressure=77
RESULT PASS_BASE_RESTORED
```

- GUI estable; CPU ~14% del proceso GUI, 79% idle; sin TX/PTT/ATU; sin reinicio.
- **Pero** 80 bloques de 0,1 s = 8 s de PCM en 30 s y `backpressure=77`: el stream no sostuvo el
  audio continuo. El soak **restaura BASE correctamente**, pero la continuidad audible queda
  **DEGRADED/UNCONFIRMED**; no se declara audio utilizable sin cortes.
- La variante QA restauró BASE, preset, volumen y HMic/IMic.

## Rollback

Se restauró el binario original desde `/mnt/backups/x6200_gui.pre-qa-367-20261102T213413Z`
(md5 `e32a81f3…`), se reinició el supervisor y quedó **un solo** `x6200_gui` en RX, `tx=false`,
volumen 1. No se reemplazaron librerías LVGL. Radio operativa con GUI original.

## Clasificación

- `QA_DEPLOY = PASS` (deploy/rollback reales).
- `SHORT_RESTORE = PASS` (2/2).
- `SOAK_RESTORE = PASS`; `SOAK_AUDIO_CONTINUITY = DEGRADED` (backpressure 77; 8 s PCM/30 s).
- `SPEAKER_AUDIO_HEARD`: pendiente de confirmación del operador para esta ventana; la evidencia
  histórica del 2/11 sigue siendo PASS.
- `CRASH_RECOVERY = NOT_VERIFIED`; `PRODUCTION_READY = NO`.
