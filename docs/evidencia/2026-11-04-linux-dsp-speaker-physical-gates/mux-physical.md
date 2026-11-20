# Estado físico del MUX — observación no invasiva

No existe readback del registro 12/MUX (R′ #370: R_INCONCLUSIVE). Opciones no invasivas evaluadas:
1. **Efecto audible BASE vs Linux** con fuente estable (mismo protocolo de provenance): si BASE
   desaparece al activar y vuelve al desactivar, es evidencia audible indirecta; no identifica el bit
   físico ni descarta mezcla.
2. **Grabación externa del parlante** (no invasiva) para medir transición/restore; sólo audible.
3. Instrumentación eléctrica/óptica del MUX/DAC requeriría acceso físico y autorización específica;
   **no se propone ni ejecuta** (nada de hacks MCU/FPGA/registros no documentados).

Conclusión: `MUX_PHYSICAL_STATE_VERIFICATION=BLOCKED`; `PCM_TO_SPEAKER_PHYSICAL=PARTIAL` (activación
+ sink-input declarado + audible), no PASS. Datos faltantes exactos: lectura/observación independiente
del estado MUX (o instrumentación autorizada) y registro del sink-input del flujo QA.
