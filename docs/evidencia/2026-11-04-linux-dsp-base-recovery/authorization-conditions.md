# Condiciones para autorizar la fase física

Requiere **nueva autorización específica del owner** (esta tarea no autoriza activación):
- Protocolo+riesgos+rollback y **método probado de retorno a BASE** (sin recuperación independiente no
  se hacen fallos destructivos con RX_LINUX seleccionado).
- Evidencia de provenance Linux (PASS audible #397) y build/paquete QA reproducible; gate QA temporal.
- Observador/reloj externo y grabación acústica autorizada; fuente autorizada (no terceros).
- Criterios de aborto: TX/ATU inesperado, BASE ausente en ventana, MUX incierto, pérdida de control;
  rollback inmediato a GUI/firmware 1.0.8, gate eliminado, verificar estado del operador.
- Prohibido: firmware/FPGA/flash, servicios permanentes, cambios de config/defaults/UI, escrituras
  directas I2C fuera del árbitro, segunda vía I2C, tests destructivos sin recovery probado.
Sin autorización: sólo offline. Sin readback/observación externa: MUX y restore físico quedan
BLOCKED/PENDING.
