# Activación QA RX_LINUX (task-32)

## Blocker 2 corregido
`route_wiring_set_rx_enabled(true)` ya no devuelve `OWNERSHIP` por diseño: activa realmente por el
camino IPC/árbitro, **sólo** con gate QA y precondiciones, sin bypass ni segundo writer.

## Camino exacto (QA-only)
1. Compilar `-DX6200_LINUX_DSP_ROUTE=ON` (+ `X6200_CTRL_HAVE_BULK_WRITE=ON` para build parcheada).
2. Runtime gate: `X6200_LINUX_DSP_ROUTE_QA=1` **y** archivo `/etc/x6200_route_qa.enable`
   (en tests: `X6200_ROUTE_QA_FILE`).
3. Cliente: `route_wiring_create(sink, ipc_path, token)` + `route_wiring_attach_ipc(w, fd)` (o
   conexión al socket del servicio) → crea `route_client_worker` (hilo dedicado) con el mismo token.
4. PCM real post-Speex: `route_wiring_submit_pcm(w, samples, count, true)` (worker, sin bloquear
   callback); `route_wiring_set_current_reg(w, reg12_actual & ~0x8)`.
5. `route_wiring_set_rx_enabled(w, true)`:
   - exige gate, worker/IPC, `pcm_fresh`, no generado/TTS, sin bits TX en `current_reg`;
   - envía `ROUTE_IPC_REQ_PCM_READY` (bloque real) → `route_arbiter_pcm_submit`;
   - envía `HEARTBEAT` con `current_reg` y `write_ok=1`;
   - envía `ENABLE`; el árbitro aplica precondiciones y escribe `voice_rec` (único writer).
   - éxito ⇒ `ROUTE_WIRING_ACTIVE`; error ⇒ `BLOCKED`, `enabled=false`, error explícito.
6. `false` ⇒ `DISABLE` al árbitro; no automático al reiniciar.

Tests: `tests/linux-dsp-integration/test_qa_enable.c` (27 checks): gate off/archivo ausente ⇒
`NOT_READY` sin RX; sin PCM ⇒ NOT_READY; generado ⇒ NOT_READY; bits TX ⇒ `TX_ACTIVE`; camino válido
⇒ RX_LINUX, bus escrito por el árbitro (único writer), DISABLE vuelve BASE; QA env retirado ⇒ no activa.

## Limitación para sesión física
La API/worker IPC está lista y probada; el **hook GUI** todavía no inyecta el sink PulseAudio ni
arranca `route_wiring` en `src/audio.c` (default OFF/skeleton). Para la sesión autorizada hay que
inyectar un `route_sink_t` real (PulseAudio del router) y el `ipc_path`/token del servicio QA.
Sin sink real, `set_rx_enabled` debe fallar cerrado (no se activa RX_LINUX). Physical gates PENDING.
