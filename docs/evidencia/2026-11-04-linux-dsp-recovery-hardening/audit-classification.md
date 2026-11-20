# Auditoría de componentes (baseline: PR #395 incorporado, ref ddec3f96; base 681074e0)

| componente | IMPLEMENTED | WIRED | TESTED_OFFLINE | TESTED_HARDWARE | PRODUCTION_ENABLED |
|---|---|---|---|---|---|
| route_arbiter (FSM, sombra, refresh) | sí | sí (feature ON) | sí (717 checks) | no | **no** |
| route_ipc (proxy, timeout, ownership) | sí | sí | sí | no | no |
| route_proxy + interposición libaether | sí | sí (feature ON) | sí | no | no |
| route_client_worker / PCM post-Speex | sí | sí (hooks `#ifdef`) | sí | no | no |
| route_supervisor | sí | sí (servicio `--supervisor`) | sí | no | no |
| X6200Control shadow/bulk patch | sí (copia temporal) | sí en build feature-ON parcheada | sí | no | no (repo externo) |
| idle refresh | sí (sólo patched build) | QA | sí | no | no |
| `x6200_daemon` lifecycle | sí (condicional binario+gate) | sí | sí (revisión de script) | no | no |
| `src/audio.c` hooks, `src/radio.c` hooks | sí | sólo feature ON | sí (build OFF) | no | no |
| Feature OFF normal build | sí | sí | sí (sin ruta/servicio/símbolos) | no | sí (BASE) |

Contradicciones de #395 resueltas: (1) "PASS offline" NO es PASS físico; (2) idle refresh no es
"deshabilitado" absoluto: está deshabilitado salvo build patcheada QA-gated; (3) build feature-ON usa
headers refs + lib parcheada (no sysroot solo); (4) supervisor no escribe reg12: recuperación física
desde supervisor = `BLOCKED_PHYSICAL_RECOVERY`; (5) `x6200_daemon` no garantiza readiness de socket.
