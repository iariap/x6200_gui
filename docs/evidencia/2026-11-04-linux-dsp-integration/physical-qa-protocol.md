# Protocolo de QA físico (NO ejecutado) — build integrada, autorización del owner

Precondiciones: owner autoriza sesión y build de prueba; firmware oficial 1.0.8 y GUI original
respaldadas; rollback conocido; radio en RX; sin TX/PTT/ATU en fases 1–4; sin USB/BT/audio externo;
una sesión, un solo GUI; feature habilitada sólo en build de prueba (no producción).

## F1 — BASE inicial y verdad de control
1. Snapshot: GET (`tx=false`), lastbreath, sha256 GUI/lib, pid GUI, `/proc/*/fd` de `/dev/i2c-0`.
2. Confirmar audio nativo BASE audible y router OFF; arbiter en BASE (status IPC); shadow válido.
3. Control independiente: matar/matar el cliente GUI y verificar que el árbitro (si está corriendo)
   reporta lease/FAULT sin escribir a ciegas. Registrar.

## F2 — activación y audio post-Speex
1. Activar ruta (sólo build de prueba, sin menú) con precondiciones: heartbeat, PCM post-Speex, sin
   TX/ATU/generado, sink/captura ok.
2. Confirmar que suena RX procesado por parlante; A/B OFF vs SSB voice con el owner.
3. Registrar `SPEAKER_LINUX_AUDIO_PHYSICAL`, preset difference, latencia percibida, dropouts/clipping,
   continuidad de RX.

## F3 — fallos y restore
| escenario | acción | verificación |
|---|---|---|
| GUI colgada | SIGSTOP / heartbeat stop | restore BASE ≤250 ms por árbitro/supervisor; estado DEGRADED_BASE |
| GUI exit/crash | kill GUI | supervisor/arbitro restauran sin GUI; después relanza daemon |
| sink caído | matar/desconectar stream | DEGRADED_BASE y BASE audible |
| PCM worker muerto | test hook de la build de prueba | suspensión+restore |
| IPC timeout/lost/dup/reordered | inyección de fallos en build de prueba | errores STALE/TIMEOUT/OWNERSHIP, sin doble escritura |
| bus/I2C error | fault injection | FAULT; no enable; fallback nativo |
| ring under/overflow | inyección de carga | contadores, sin bloqueo de callback |

Registrar `RESTORE_LATENCY_MS` real ⇒ `RESTORE_250MS_PHYSICAL`.

## F4 — TX/PTT/ATU (autorización adicional explícita)
1. Con RX Linux activo, solicitar PTT: verificar disable-first (0x8=0) antes de iptt, bits PTT/ATU
   preservados y sin audio Linux durante TX.
2. ATU/SWR/poweroff idem; al volver a RX no auto-enable.

## F5 — rollback
- Reponer GUI original, firmware oficial 1.0.8, feature OFF; verificar sha256/pid/tx=false, BASE
  audible, sin procesos árbitro/supervisor; restaurar ajustes/volumen del operador.

## Capacidad faltante
Build integrada con la feature habilitada y el arbiter/proxy/supervisor desplegados (hoy DEFAULT OFF);
owner presente para escucha/aborto; instrumentación mínima para medir latencia de restore y audio
(grabación externa/loopback autorizado). Sin firmware/FPGA/readback nuevo: la verdad física del MUX se
valida por efecto audible de BASE/RX y por ausencia de audio Linux en BASE.
