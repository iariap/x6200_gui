# Autorización y precondiciones (fases activas)

| fase | requiere autorización nueva del owner | precondiciones | criterio de stop |
|---|---|---|---|
| P0 Auditoría offline (este PR) | no | sólo lectura/offline | — |
| P1 Observación pasiva BASE (sin activar QA) | sí (sesión) | build oficial 1.0.8, rollback, operador | estado inesperado |
| P2 Activación QA + provenance A/B | **sí, explícita** (protocolo/riesgos/rollback) | P1, fuente autorizada, gate, sink real, protocolo provenance | sin BASE return / TX / MUX incierto |
| P3 Medición de recovery (fallos benignos) | **sí, explícita** | P2 PASS parcial, reloj externo, BASE return probado | no BASE audible ⇒ STOP |
| P4 Fallos destructivos/restore físico | **sí, explícita y separada** | P3 demuestra retorno BASE independiente | sin independencia ⇒ NO ejecutar |
| TX/PTT/ATU | fuera de alcance (autorización separada; no en este PR) | — | — |

Sin autorización específica no se activa MUX, no se corre QA ni pruebas de hardware. Cualquier fase
activa exige protocolo+riesgos+verificación de BASE return; si no hay recuperación independiente,
se detiene y se documenta (`BLOCKED_PHYSICAL_RECOVERY`).
