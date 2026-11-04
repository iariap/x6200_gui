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
