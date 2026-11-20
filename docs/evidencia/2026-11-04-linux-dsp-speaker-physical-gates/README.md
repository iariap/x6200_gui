# QA — Provenance del parlante Linux-DSP y gates físicos (auditoría)

**Fecha:** 2026-11-04 · **Base verificada:** `681074e0` (`fork/main`) · **PRs auditados:** #396
`c7916d42`, #395 `ddec3f96`, #394 `56954c2d`; épica #362, issues #363/#364/#365.
**Tipo:** auditoría/validación OFFLINE; **no se activó MUX, no se desplegó QA, sin pruebas activas**.

## Decisión ejecutiva
- La sesión física 2026-11-04 **confirma activación lógica** (gate→IPC→ENABLE→ACTIVE) y **relato del
  owner de audio audible**; **no confirma provenance Linux** (falta aislamiento A/B, fuente controlada,
  parámetros DSP nativos constantes y grabación externa) ni **estado físico del MUX** (sin readback,
  `MUX_PHYSICAL_STATE_VERIFICATION=BLOCKED`).
- `PCM_TO_SPEAKER_PHYSICAL` queda **PARTIAL**; **no se promueve a PASS** sin aislamiento.
- `INDEPENDENT_BASE_RESTORE_PHYSICAL` **PARTIAL/NO DEMOSTRADO**; `RESTORE_250MS_PHYSICAL`
  **PENDING/NO MEDIDO** (sin reloj independiente ni transición audible medida).
- **`PRODUCTION_READY=FAIL`**: activación QA deshabilitada por defecto; falta la sesión autorizada con
  protocolo de provenance + observación física no invasiva + medición independiente de recovery.

## Evidencia auditada
- `prior-session/physical-session-2026-11-04.md` + logs (`gui-activation.log`, `gui.log`, `service*.log`,
  `state-prephysical.json`) copiados de #396 (`c7916d42`).
- `audit-session-report.md`: separación activación / sink-input / audible / provenance / MUX / restore.
- `offline-pcm-provenance.md`: chequeo estático del camino post-Speex (sin doble Speex/NR, mismo buffer
  a recorder/decoders/router, QA init fuera de callback).
- `offline-tests.log`: suite 847 checks, 0 fallos, ARM/QEMU PASS (offline).
- `provenance-analysis.md`, `provenance-protocol.md`, `mux-physical.md`, `recovery-measurement-plan.md`,
  `authorization-matrix.md`, `gate-matrix.csv`, `production-decision.md`, `rollback.md`.
