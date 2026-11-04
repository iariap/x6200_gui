# Recuperación y análisis de fallos

| fallo | respuesta integrada | evidencia host |
|---|---|---|
| heartbeat/lease vencido (GUI colgada) | árbitro escribe BASE (`shadow & ~0x8`), DEGRADED_BASE; supervisor pide restore y mide latencia | test_lease_restore, test_supervisor |
| GUI exit/crash | supervisor detecta lease; árbitro independiente restaura sin GUI | test_supervisor; test_arbiter directo |
| arbiter caído/no responde | supervisor **no** escribe reg12 (evita race); FAULT/bloqueo; fallback nativo/GUI restart | test_supervisor (arbiter_responsive=false ⇒ FAULT) |
| IPC timeout/lost/duplicate/reordered | request timeout acotado; STALE por seq; OWNERSHIP por token/PID; nunca retry indefinido | test_ipc_timeout, test_ownership_and_stale, test_serialization |
| bus error en enable/restore/TX | sombra inválida + FAULT; no se habilita más hasta restart | test_boot_fail, test_tx_bus_fail, test_restore_bus_failure (modelo previo) |
| sink/captura/generado | disable-first + restore BASE | test_suspend_sources |
| PCM missing/inválido/duplicado/overflow | no inventa audio; suspende/restore; duplicado rechazado; overflow acotado | test_pcm_rules |
| restart del árbitro | sombra inválida hasta boot; no auto-enable | test_restart |
| ownership loss (segundo cliente/PID) | OWNERSHIP; el árbitro no aplica | test_ownership_and_stale |

## Límite físico declarado

El árbitro no tiene readback del MCU (R′ #370: lecturas 00000000, `R_INCONCLUSIVE`). La seguridad
lógica se basa en dueño único + sombra; la verificación **física** del MUX y del restore queda
PENDING/BLOCKED (physical QA). Si el árbitro no responde, el supervisor **no** puede restaurar por
sí mismo de forma independiente sin arriesgar una segunda escritura/race: bloquea activación (FAULT)
y el fallback es el servicio/GUI nativo.

## Latencia (offline)

Lease 100 ms + restore medido en tests host: `restore_latency_ms` bajo 250 ms (modelo previo 110 ms;
supervisor test 230 ms desde heartbeat incluyendo ventana). No es medición física.
