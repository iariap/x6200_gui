# Plan mínimo de implementación (sin tocar radio/firmware en esta etapa)

## Fase 0 — esta tarea (cerrada con PR documental)
Sin código de producción. Evidencia + herramienta offline + protocolo A/B.

## Fase 1 — A/B físico autorizado (próximo paso real)
1. Owner autoriza sesión RX-only; GUI original/firmware 1.0.8; sin MUX en esta fase.
2. Ejecutar `ab-test-plan.md`: A0 (BASE) vs A1–A6 (nativo optimizado), ciego, con señales representativas.
3. Registrar parámetros y resultados; si no hay mejora audible clara, **A queda como está** y se detiene.
4. B (ruta audible Linux) **no entra** en Fase 1.

## Fase 2 — sólo si Fase 1 muestra que el nativo es insuficiente
- Reevaluar B: primero cerrar #363 (readback/owner/restore), #364/#365; estabilizar router (0 underruns reales), verificar `audio_enh_process` actual (latencia 882 muestras, sin defectos de la auditoría vieja) y A/B audible Off vs SSB voice con owner.
- Recién entonces decidir si se implementa algo. Sin firmware/FPGA.

## Fase 3 — mantenimiento/limpieza (opcional, sin riesgo)
- Documentar en docs de usuario los controles nativos recomendados (sin defaults nuevos).
- No agregar menús/ajustes nuevos sin evidencia de uso.

## Criterios de parada
- No hay evidencia audible → no implementar.
- MUX/restore inseguro → B no se integra.
- Cualquier cambio de radio/firmware queda fuera de alcance.
