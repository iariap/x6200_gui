# Decisión de producción

**PRODUCTION_READY = FAIL.** No se promueve a producción ni se habilita por defecto:
- Activación QA es offline/QAs-gated y la provenance Linux no está aislada.
- MUX y estado físico no observados (`BLOCKED`); restore físico sin medición independiente
  (`PENDING`) y retorno BASE independiente sólo parcial/no demostrado.
- La sesión previa confirma activación lógica y audio audible declarado, no diferencia DSP Linux ni
  selección física del MUX.
- Programa oficial 1.0.8/estado del operador intactos; GUI original restaurada; sin servicios QA
  permanentes. Sin cambios en `main`; sin merge.

Capacidad exacta faltante: (1) autorización del owner para sesión P2/P3 con protocolo de provenance y
recovery; (2) observación no invasiva del MUX/estado físico o instrumentación autorizada; (3) medición
con reloj externo de restore ≤250 ms y BASE return independiente; (4) log capturado de sink-inputs y
grabación externa A/B. Hasta entonces, gates físicos PENDING/BLOCKED y activación deshabilitada.
