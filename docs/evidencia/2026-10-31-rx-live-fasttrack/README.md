# FAST-TRACK RX live: SpeexDSP -> DAC -> speaker interno (issue #364 / Epic #362)

Fecha: 2026-10-31 (America/Buenos_Aires). Base GUI `main` = `681074e0`; Buildroot `main` = `745af77e`.
Rama de esta unidad: `feat/rx-audio-live-fasttrack` (worktree aislado, sin merge).

## Estado

**`OFFLINE_READY / PHYSICAL_ACTIVATION_BLOCKED`.**

- El router RX está implementado, integrado **desactivado** y probado en host.
- El backend MUX del target es **fail-closed**: `mux_ready=false` y `mux_request=-1`. Aunque
  alguien llame a `rx_audio_live_set_enabled(true)`, el worker no pide DAC ni escribe el registro 12.
- No se ejecutó ninguna escritura de MUX, ni prueba física, ni TX/PTT/ATU.

La activación física queda bloqueada por las propiedades A/B/C del Epic, no por el router.

## Qué se implementó

- `src/rx_audio_live.c/.h`: ring fijo de 4 bloques de 4410 muestras (400 ms), callback de captura no
  bloqueante, worker separado, stream de reproducción propio, máquina BASE/PREPARE_DAC/RX_DAC,
  descarte del bloque más viejo en overflow, suspensión por TX, audio generado, captura fallida y
  staleness (200 ms).
- `src/audio.c`: usa el MISMO PCM post-`audio_enh_process()` que ya reciben recorder/decodificadores;
  no duplica SpeexDSP ni toca `play_stm`. El router arranca desactivado.
- `src/radio.c`: hook de prioridad TX/ATU (solo estado de audio, sin I2C).
- Tests host: 39 comprobaciones, 0 fallos, backend fake sin PulseAudio/hardware.
- Cross-compile ARM del módulo: OK.
- Build completo: ver `commands.log`.

## Gates del MUX (resumen)

Ver [`mux-gate.md`](mux-gate.md). A (verdad física) y C (restore independiente) siguen bloqueadas;
B tiene infraestructura reutilizable en PR #368 pero no está integrada. D no se ejecutó.
No se presenta `pactl RUNNING` como prueba acústica.

## Handoff físico

[`physical-experiment-plan.md`](physical-experiment-plan.md) queda preparado, **sin ejecutar**: sólo
se corre después de que A/B/C pasen. No se cambió nada del firmware/base/microSD.
