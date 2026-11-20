# Próximo plan: #363 → #364 → #365

1. **#363 (físico):** con la build integrada (feature ON, default OFF en producción) y autorización
   del owner, ejecutar `physical-qa-protocol.md` F1–F3; medir restore real, no-interferencia y audio
   post-Speex. Obtener `RESTORE_250MS_PHYSICAL` y `PCM_TO_SPEAKER_PHYSICAL`. Este PR deja el protocolo.
2. **#364:** habilitar branch-local la feature con el wiring GUI↔IPC↔arbiter↔transporte real y el
   lanzamiento por `x6200_daemon` (default OFF, no operator-accessible); mantener router #364.
   Criterio de entrada: F1–F3 PASS (o FAIL documentado y no activar).
3. **#365:** watchdog/rollback físico final (GUI/worker/sink caídos, lease, bus errors, TX/PTT/ATU) y
   rollback a GUI original; sólo entonces evaluar `PRODUCTION_READY` (nunca sin físico).

Sin firmware/FPGA/readback nuevo: la seguridad lógica (árbitro+sombra) ya está implementada offline;
el resto es integración de arranque/servicio y verificación física.
