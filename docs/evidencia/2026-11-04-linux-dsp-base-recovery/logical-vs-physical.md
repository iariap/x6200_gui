# Separación de evidencia: lógico / comando / transporte / físico / audible

| nivel | qué prueba | qué NO prueba | fuente |
|---|---|---|---|
| Pedido lógico | el software decidió restaurar | que se enviara o aplicara | logs/contadores |
| Comando | se invocó la API/escritura | que el bus/MCU lo aceptara | logs |
| Transporte aceptado | bus/ACK aceptó la escritura | que el MUX/DAC cambiara | bus/ACK, tests |
| Cambio físico | estado MUX/reg12 observado | audible/efecto | readback (NO disponible, R′ #370) |
| BASE audible | efecto acústico externo de BASE | registro/MUX exacto | grabación externa (no ejecutada) |

## Hechos de código auditados (#396)
- `route_arbiter_physical_confirmed()` devuelve **siempre false** (no hay readback): ningún estado
  lógico declara BASE físico.
- `route_arbiter_shutdown_safe()` escribe BASE **si** el árbitro está disponible y la sombra válida,
  pero invalida la sombra y deja `physical_confirmed=0` (log de sesión: `shutdown_safe
  physical_confirmed=0`). Eso es transporte/logical, no observación física.
- `route_arbiter_mark_unavailable()` **no escribe reg12**: invalida sombra, FAULT y prohíbe RX_LINUX.
- El supervisor pide restore al árbitro, **nunca escribe reg12**; si el árbitro no responde queda
  `BLOCKED_PHYSICAL_RECOVERY`.
- Bits compartidos: el proxy preserva PTT/ATU/modem/swrscan/split/calib/poweroff; `voice_rec` lo
  decide el árbitro (test offline).
