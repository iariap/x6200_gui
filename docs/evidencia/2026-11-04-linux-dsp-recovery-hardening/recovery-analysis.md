# Problema A — recuperación del árbitro

## Estados separados
- **Lógico** (`route_state_t`): BASE/READY/RX_LINUX/DEGRADED_BASE/FAULT, más `available`,
  `shutdown_done`, `blocked_physical_recovery`.
- **Físico**: NO observable (sin readback MCU, R′ #370). `route_arbiter_physical_confirmed()` devuelve
  **siempre false**; ningún mock/estado lógico declara BASE físico.

## Escenarios y manejo
| escenario | manejo |
|---|---|
| GUI exit/freeze / lease/heartbeat perdido | árbitro (si vive) escribe BASE (disable-first), DEGRADED_BASE; supervisor REQUEST_RESTORE; latencia lógica medida (budget 250 ms contrato) |
| PCM/router stop, sink loss, generado | disable-first + BASE; no se inventa audio |
| arbiter terminado/bloqueado/transporte error/IPC bloqueado | `mark_unavailable`: **no escribe reg12**, sombra inválida, FAULT, RX_LINUX prohibido; sin segundo writer; sin writes ciegos |
| supervisor caído/reiniciado | detecta lease; no escribe reg12; si árbitro no responde ⇒ FAULT + `BLOCKED_PHYSICAL_RECOVERY` |
| restart del árbitro/GUI | boot escribe BASE (seguro) pero **no** restaura RX ni clearing autónomo; re-enable exige heartbeat/PCM/write_ok/sink y gate QA |
| write/mid-register error | FAULT inmediato, sombra inválida, sin reuse; reinicio explícito |
| shutdown (`SIGTERM`) | `shutdown_safe`: un write BASE si disponible/sombra válida, invalida sombra, DEGRADED_BASE, físico no confirmado |

## Bloqueo explícito
`BLOCKED_PHYSICAL_RECOVERY=true` cuando el árbitro no es observable/confiable: no hay evidencia de que
la radio esté en BASE; **no se declara recuperación física** y no se habilita RX_LINUX. Falta de datos:
readback del reg12/estado MUX del MCU (o instrumentación auditiva autorizada).

Heartbeat≤50 ms / lease 100 ms / restore≤250 ms son **contratos offline**, no prueba física.
