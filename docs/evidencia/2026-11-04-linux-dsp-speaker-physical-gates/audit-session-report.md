# Auditoría offline de la sesión física 2026-11-04

| ítem | estado | evidencia | límite/falta |
|---|---|---|---|
| Activación lógica QA (gate→IPC→ENABLE→ACTIVE) | CONFIRMADO (log) | gui-activation.log: gate OK, IPC conectado, PCM fresco, activation err=0 state=ACTIVE | sólo lógico; no prueba MUX |
| Gate runtime QA | CONFIRMADO (log) | gui-activation.log líneas 1-2, 185-186 | build QA owner-authorized (no reproducido aquí) |
| Sink-inputs (PCM QA en sink PulseAudio) | NO INSTRUMENTADO | claim en physical-session-2026-11-04.md; no hay salida `pactl` capturada en logs | falta log/comando; 2 sink-inputs no auditables |
| Audio audible durante la prueba | RELATO DEL OWNER (no instrumentado) | physical-session-2026-11-04.md | sin grabación externa; no distingue bypass vs preset |
| Provenance Linux (DSP Linux vs BASE) | NO AISLADO | no hay A/B OFF/ON con misma fuente/preset; no hay parámetros DSP nativos congelados | no se puede atribuir diferencia al DSP Linux |
| BASE fw tras cierre | CONFIRMADO (log) | service.log: BASE fw version: 1.0.8 | lectura MCU; no es estado MUX observado |
| Shutdown seguro / restore lógico | PARCIAL | service.log/service-shutdown.log: shutdown_safe physical_confirmed=0 | sin timestamp físico ni efecto audible medido; logs duplicados |
| MUX físico / state verification | BLOCKED | sin readback MCU (R′ #370); sin instrumentación del MUX | requiere observación no invasiva o readback |
| Restore ≤250 ms físico | PENDING/NO MEDIDO | no hay reloj externo ni transición audible grabada | protocolo de medición independiente pendiente |
| Rollback operador | PARCIAL | GUI original sha256 en md, gate eliminado, sin QA/service, CB-D 27.635 USB | state-prephysical.json vol=20 vs md "volumen 0"; contradicción |
| Contradicción estado pre-sesión | DETECTADA | state-prephysical.json vol=20 | el md dice volumen restaurado 0; no coincide con snapshot pre-sesión |

## Decisión
Activación lógica y sink-input **declarado** no son provenance Linux ni MUX físico.
No se promueve ningún PARTIAL a PASS. `PRODUCTION_READY=FAIL`.
