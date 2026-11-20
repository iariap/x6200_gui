# QA sink + contrato de lanzamiento (task-33)

## Hook implementado
- `src/audio.c`: en `audio_init()` (hilo de init, NO callback RT) llama `x6200_route_audio_init()`
  (sólo `#ifdef X6200_LINUX_DSP_ROUTE`). Crea `route_wiring` con **sink PulseAudio real**
  (`route_pulse_sink.[ch]`, `pa_simple_*`), IPC/token QA y worker dedicado (`route_qa_client.[ch]`).
- `capture_deliver()` ya entrega el **mismo buffer post-SpeexDSP** a recorder/decoders y llama
  `x6200_route_audio_submit()`; no hay segundo Speex ni copia grande en callback: sólo enqueue SPSC al
  ring del worker. Fallo de sink/IPC/gate ⇒ wiring BLOQUEADO, no activa.
- `audio_play_en()`/hooks TX usan generated/TX (prioridad/disable-first); sin auto-enable.

## Contrato exacto de lanzamiento QA (owner-authorized)
Build/instalación de prueba (NO producción):
```
# 1) build feature ON patcheada (reproducible-build.sh) e instalar binarios QA
# 2) gate runtime explícito (no operator-accessible):
echo 1 > /etc/x6200_route_qa.enable
# 3) servicio árbitro (dueño único I2C), socket QA:
/usr/sbin/x6200_route_service --arbiter /run/x6200_route.sock --transport x6200 &
# 4) GUI de prueba con QA:
X6200_LINUX_DSP_ROUTE_QA=1 \
X6200_ROUTE_QA_FILE=/etc/x6200_route_qa.enable \
X6200_ROUTE_QA_IPC=/run/x6200_route.sock \
X6200_ROUTE_QA_TOKEN='RXDP-ARB-TOKEN01' \
X6200_ROUTE_QA_ACTIVATE=1 \
/usr/sbin/x6200_gui
```
- `X6200_ROUTE_QA_ACTIVATE=1` es opt-in **explícito**; sin él el wiring queda READY y no activa
  (no auto-enable). El hilo de activación espera PCM post-Speex real y llama
  `route_wiring_set_rx_enabled(true)` (PCM_READY → HEARTBEAT → ENABLE al árbitro).
- Token opcional: si no se define se usa el token QA por defecto (`RXDP-ARB-TOKEN01`); debe coincidir
  con el del servicio. Sink: PulseAudio default; si no abre, falla cerrado (nunca silencioso).
- Rollback: matar GUI/servicio QA, borrar `/etc/x6200_route_qa.enable`, reponer GUI/firmware oficiales;
  no hay cambios MCU/FPGA.

## Tests
`test_qa_sink.c` (38 checks): gate off/archivo ausente ⇒ no activa; IPC inexistente ⇒ falla cerrado y
sin writes extra; gate on + sink mock + IPC real ⇒ post-Speex recibido, activación por árbitro
(RX_LINUX, único writer), 100 submits en callback << 200 ms (sin IPC en callback); RAW (no post-Speex)
no activa; generated/TX/worker/PCM deniegan. Total suite: **847 checks, 0 fallos**.
