# Prueba de mute en CB-D canal 25 — 2026-11-04

## Condiciones
- Radio: CB-D canal 25, **27.695.000 Hz**, AM, filtro 4500, volumen 20, RX-only.
- Build QA: `x6200_gui` + `x6200_route_service` (feature ON, gate QA temporal).
- Sin TX/PTT/ATU; sin firmware/flash; sin escritura directa de registros fuera del árbitro.
- La radio fue restaurada a la GUI normal y dejada en CB-D canal 25 después de la prueba.

## Evidencia
- Activación QA:
  - `x6200 QA: gate OK ipc=/run/x6200_route.sock`
  - `x6200 QA: IPC conectado, wiring READY (sin activar)`
  - `x6200 QA: PCM post-Speex fresco; pidiendo ENABLE al arbitro`
  - `x6200 QA: activation err=0 state=ACTIVE`
- Mientras QA activo: `pactl list short sink-inputs` mostró **2 sink-inputs** hacia el sink
  `alsa_output.platform-sound.stereo-fallback` (capturado en `pactl-sink-inputs.txt`).
- Se muteó el sink PulseAudio durante **5 segundos** y luego se desmuteó.
- El owner informó: **el audio del parlante desapareció durante el mute y volvió al desmutear**.
- Tras cerrar QA: `shutdown_safe physical_confirmed=0`, BASE 1.0.8; GUI normal restaurada.

## Interpretación
El parlante interno dependió del sink PulseAudio de la ruta QA: al mutearlo, el audio desapareció;
al desmutearlo, volvió. Esto es evidencia **AUDIBLE/HARDWARE** de que el audio escuchado durante
la prueba provenía del camino Linux/PulseAudio, no de una fuente nativa independiente.

No es readback del MUX físico ni medición temporal de recuperación. `MUX_PHYSICAL_STATE_VERIFICATION`
sigue BLOCKED y `RESTORE_250MS_PHYSICAL` sigue PENDING/UNMEASURED.

## Gate impact
- `LINUX_PCM_PROVENANCE`: **PASS (AUDIBLE/HARDWARE)** en esta prueba, por dependencia audible del sink QA.
- `LINUX_DSP_AUDIBLE_DIFFERENCE`: sigue **PENDING** (no se hizo A/B bypass vs preset).
- `MUX_PHYSICAL_STATE_VERIFICATION`: **BLOCKED** (sin readback).
- `RESTORE_250MS_PHYSICAL`: **PENDING/UNMEASURED**.
- `PRODUCTION_READY`: **FAIL**.
