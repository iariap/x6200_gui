# Observación del Lead — sesión física 2026-11-04

Estos datos fueron observados por el Lead en la sesión de terminal que ejecutó la prueba QA.
No quedaron persistidos en un archivo de la radio en ese momento; se registran aquí para que la
auditoría no los trate como inexistentes. No constituyen aislamiento de procedencia Linux.

## Activación (log de GUI QA)

```text
x6200 QA: gate OK ipc=/run/x6200_route.sock
x6200 QA: IPC conectado, wiring READY (sin activar)
x6200 QA: PCM post-Speex fresco; pidiendo ENABLE al arbitro
x6200 QA: activation err=0 state=ACTIVE
```

## Sink-inputs PulseAudio mientras QA activo

Comando observado: `pactl list short sink-inputs`

```text
6	0	58	protocol-native.c	s16le 1ch 44100Hz
7	0	61	protocol-native.c	s16le 1ch 44100Hz
```

Al terminar la GUI QA con SIGTERM, el mismo comando quedó vacío (0 sink-inputs) y el servicio
registró `shutdown_safe physical_confirmed=0`.

## Lectura correcta

- Hay evidencia de que existían dos sink-inputs PulseAudio durante la sesión QA.
- No hay captura de sus propiedades/propietario que demuestre que ambos pertenecían al flujo QA.
- No hay A/B de fuente/preset ni verificación del MUX físico.
- `LINUX_PCM_PROVENANCE` sigue **PARTIAL**, no PASS.
