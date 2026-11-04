# Contrato IPC (GUI/router ↔ árbitro ↔ supervisor)

- Socket `SOCK_SEQPACKET` (tests: `socketpair`), token de 16 B por sesión, `pid+starttime` anti-PID
  reciclado, mensajes `route_ipc_msg_t` con magic `RXDP`, version 1, secuencia monótona.
- Requests: `HEARTBEAT` (seq, valor completo del reg12, write_ok en bit0 de lease), `APPLY_SHARED`,
  `ENABLE`, `DISABLE`, `TX_PREPARE`, `ATU_PREPARE`, `STATUS`, `SHUTDOWN`.
- Responses: mismo tipo, `err` (`OK/INVALID/NOT_READY/UNKNOWN_STATE/BUS/TIMEOUT/STALE/TX_ACTIVE/
  OWNERSHIP`), `state`, stats. Sin retries indefinidos: cada request tiene timeout (tests 100–500 ms).
- Reglas: token/pid/starttime obligatorios; secuencia duplicada/atrasada ⇒ `STALE`; cliente distinto ⇒
  `OWNERSHIP`; llamada con el bus en error ⇒ `BUS` (y sombra inválida/FAULT según caso); heartbeat
  repetido no renueva lease (seq monotónica); cliente caído/lease vencido ⇒ restore sin GUI.
- No bloquea callbacks de audio: el callback sólo copia a ring; el envío IPC lo hace el worker con
  timeout; el callback nunca espera al árbitro.

## Proxy de libaether (nueva etapa)

`src/linux_dsp_route/route_proxy.[ch]` implementa el lado GUI del contrato:

- `route_proxy_write_reg12(target)`: serializa por IPC; **rechaza** targets con `voice_rec` (`OWNERSHIP`);
  si hay bits TX envía primero `TX_PREPARE` (disable-first) y luego `APPLY_SHARED`.
- Wrappers con la misma semántica RMW de libaether: `ptt_set`, `atu_tune`, `atu_set`, `modem_set`,
  `swrscan_set`, `split_set`, `poweroff`; `record_set` ⇒ `OWNERSHIP` (el bit 0x8 es del árbitro).
- Cache del reg12 = último valor aplicado por el árbitro; se invalida en error/timeout ⇒ lecturas
  fallan (`UNKNOWN_STATE`) y RX_LINUX queda bloqueado; no hay readback MCU.
- Timeout explícito (100 ms) por request; sin retries indefinidos; no se llama desde callbacks RT
  (el callback sólo encola PCM en el worker).
- Cobertura: `tests/linux-dsp-integration/test_proxy.c` (88 checks: serialización/RMW/voice rechazado/
  PTT/ATU/split/orden TX/ bus-fail caché inválida/ timeout sin caché falsa/40 escrituras).
- **Pendiente**: reemplazo de call-sites reales de libaether/GUI e `x6200_control_idle` por el proxy;
  hasta entonces la GUI puede escribir directo y RX_LINUX sigue bloqueado.
