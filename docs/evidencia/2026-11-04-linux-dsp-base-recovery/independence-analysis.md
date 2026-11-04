# ¿Qué es "independiente" aquí?

- **Independiente del GUI:** sí. Árbitro y supervisor son procesos separados; el restore por lease no
  depende de que el GUI esté vivo. Esto está testeado offline (`test_hardening`, `test_qa_enable`).
- **Independiente del árbitro/transporte:** **NO**. Supervisor y árbitro comparten host, bus/transporte
  y el único writer del reg12. Si el árbitro muere, el supervisor no escribe (contrato) y no hay
  segunda vía segura: `BLOCKED_PHYSICAL_RECOVERY`. Un software watchdog que dependa del proceso caído
  no cuenta como recuperación independiente.
- **Independiente de ambos (hardware):** sólo podría darla un reset/observador externo del MCU/FPGA o
  readback; no existe en este alcance y no se agrega firmware/FPGA.
- Solución adoptada: fail-closed explícito, sin escrituras a ciegas, mantener RX_LINUX bloqueado; no
  se reclama BASE físico.

## Verificable y corregible branch-local
No hay corrección segura adicional que implementar offline: cualquier "recuperación" extra requeriría
un segundo writer (prohibido) o readback (no disponible). Se entrega modelo/medición y gates; la
corrección de hardware queda fuera de alcance y documentada.
