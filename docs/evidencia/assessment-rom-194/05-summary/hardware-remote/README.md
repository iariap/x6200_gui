# Fase remota RX — estado en la fase A (sin radio)

- **Campaña:** #194, unidad 5/5 (#199).
- **Alcance previsto:** HR-01..HR-06 del issue #199 (exclusividad/identidad, baseline,
  navegación, QSY reversible, 60 min de estabilidad y cierre/restauración).
- **Estado:** **NO EJECUTADA — BLOCKED**. No se abrió SSH, no se usó el socket, no se
  capturó framebuffer ni se emitió RF en esta fase.
- **Causa exacta:** la fase A de esta unidad es **consolidación documental**. La directiva
  del orquestador indica que la radio no responde ahora, que no debe contactarse (sin
  SSH/socket/RF) y que la fase B será coordinada con el owner. Por lo tanto no hay sesión
  exclusiva verificada, ni identidad del binario instalado, ni snapshot inicial.
- **No es un fallo del candidato:** es una dependencia externa de hardware/ventana. No se
  declara PASS ni se inventan resultados.

## Lo que sí se verificó (documental, sin radio)

- El candidato congelado es `c888308eede71205605c06383432ee7e165f202e` y el artefacto de
  #195 tiene los SHA256 de `05-summary/artifact-manifest.json`.
- El estado de orquestación registra que la radio tenía desplegado un build del **tip**
  (GUI md5 `3b22a03a…`, 13.412.248 B), no el binario del candidato (md5 `1f998c46…`).
  Ese dato es **externo al dossier** y no se revalidó en esta fase.
- Consecuencia para la fase B: si la radio sigue con el binario del tip, cualquier evidencia
  física es del **software instalado**, no del candidato. No se autoriza desplegar el
  candidato: la fase B debe reportar la identidad por separado y el candidato seguirá
  `PENDING_HARDWARE` salvo que el owner autorice una SD/prueba con el artefacto exacto.

## Resultado por caso

| Caso | Estado | Motivo |
|---|---|---|
| HR-01 exclusividad/identidad | BLOCKED | sin acceso ni sesión exclusiva verificada |
| HR-02 baseline física | BLOCKED | depende de HR-01 |
| HR-03 navegación RX | BLOCKED | depende de HR-02 |
| HR-04 QSY reversible 20 reps | BLOCKED | depende de HR-02 |
| HR-05 60 min RX | BLOCKED | depende de HR-02 |
| HR-06 cierre/restauración | BLOCKED | no hubo estado que restaurar |

Los detalles y criterios de aceptación están en [`../hardware-plan.csv`](../hardware-plan.csv)
(HR-01..HR-06) y el runbook en [`../hardware-plan.md`](../hardware-plan.md).
