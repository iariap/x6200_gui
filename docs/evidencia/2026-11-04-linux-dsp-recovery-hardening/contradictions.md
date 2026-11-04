# Resolución de contradicciones (#395 ↔ hardening)

1. #395 decía "SINGLE_WRITER_CONTROL PASS": es **PASS offline**; no hay prueba física ni activación.
2. #395 decía idle "deshabilitado": ahora es **deshabilitado por defecto** y habilitado sólo en build
   patcheada QA-gated; además el fallo es explícito (status/coalescing), no silencioso.
3. #395 decía supervisor "restaura": el supervisor **no escribe reg12**; sólo pide restore al árbitro.
   Si el árbitro falló: `BLOCKED_PHYSICAL_RECOVERY`, sin claim físico.
4. #395 reportaba build feature-ON con headers refs; se mantiene, y se documenta dependencia de lib
   parcheada para bulk/sombra autoritativa.
5. Restore ≤250 ms es **offline**: el gate físico queda PENDING/BLOCKED.
