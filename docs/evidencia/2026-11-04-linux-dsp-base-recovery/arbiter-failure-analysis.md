# Análisis de fallos del árbitro y recuperación

| escenario | recuperación lógica | recuperación física | clasificación |
|---|---|---|---|
| GUI exit/freeze, árbitro vivo | lease/heartbeat → BASE (si sombra válida) | no observada | logical only |
| IPC loss (cliente desaparece) | árbitro restaura por lease; supervisor no escribe | no observada | logical only |
| Árbitro freeze/terminación | **no hay writer seguro**; no reintentar a ciegas | no observada/posible | BLOCKED_PHYSICAL_RECOVERY |
| Árbitro restart | boot BASE; sombra inválida; sin auto RX_LINUX | no observada; estado previo desconocido | logical, físico incierto |
| GUI restart | nuevo cliente, sin auto-enable; requiere gate QA | no observada | logical only |
| Escritura parcial/fallida | FAULT, sombra inválida, no reuse | incierta (puede quedar estado parcial) | BLOCKED/FAULT |
| Reinicio del sistema | el estado MCU/MUX no es observable al arrancar | **no garantizable sin readback** | BLOCKED_PHYSICAL_RECOVERY |
| Pérdida de PCM/sink/worker | disable-first+BASE por árbitro vivo | no observada | logical only |

## Independencia
El supervisor/watchdog corre en el mismo host Linux y su **única** vía de restauración es el árbitro y
el mismo transporte I2C. Si el árbitro/transporte falla, el watchdog **no es un mecanismo independiente**
(no puede escribir sin un segundo writer, prohibido). No se implementan reset ciegos ni escrituras
desconocidas. La independencia real requeriría un observador externo o readback del MCU, ausente hoy.
