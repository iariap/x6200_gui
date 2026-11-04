# QA — Recuperación física a BASE y gate de producción (auditoría offline)

**Base verificada:** `681074e0` (`fork/main`) · **PRs auditados:** #397 `aef8f020`, #396 `c7916d42`,
#395 `ddec3f96`, #394 `56954c2d`; épica #362, issues #363/#364/#365.
**Tipo:** auditoría/passivo OFFLINE. **No se activa QA/MUX, no hay pruebas físicas, no deploy.**

## Decisión
- `LINUX_PCM_PROVENANCE` se mantiene **PASS (AUDIBLE/HARDWARE)** por la prueba de mute CB-D ch25 de
  #397 (dependencia audible del sink PulseAudio QA); no se re-ejecutó.
- La recuperación a BASE **lógica** está implementada y testeada offline; la recuperación **física**
  **NO está demostrada**: no hay readback del MUX, el watchdog/supervisor software no es independiente
  del proceso/transporte que falla, y no hay medición acústica externa de BASE ni del tiempo ≤250 ms.
- `INDEPENDENT_BASE_RESTORE_PHYSICAL` = **BLOCKED**; `RESTORE_250MS_PHYSICAL` = **PENDING** (harness
  listo, no ejecutado); `MUX_EFFECTIVE_ROUTE_VERIFICATION` = **BLOCKED**; **`PRODUCTION_READY=FAIL`**.

## Contenido
`logical-vs-physical.md`, `arbiter-failure-analysis.md`, `independence-analysis.md`,
`residual-risk.md`, `audible-protocol.md`, `recovery-measurement-plan.md`,
`authorization-conditions.md`, `gate-matrix.csv`, `rollback.md`, `commands.log`, y logs offline
(`offline-code-audit.txt`, `offline-model-qemu.log`, `offline-integration-tests.log`).
