# Contrato del guardián independiente

**Estado: implementado y probado con mocks/procesos reales (G2 host) + ARM build. En producción
queda INACTIVO/OFF** porque la Fase R no pudo demostrar readback real del registro 12
(`R_BLOCKED_NO_HARDWARE_TRUTH`). Un guardián sin readback no puede afirmar que restauró: el contrato
prohíbe escribir en ese caso.

## 1. Binario y empaquetado

- Fuente: `src/rx_audio_mux_guard_main.c` + `src/rx_audio_mux_guard.c`.
- Target CMake: `x6200_audio_guard` (mismo toolchain de la ROM) e
  `install(TARGETS … x6200_audio_guard DESTINATION sbin)` ⇒ llega a `/usr/sbin/x6200_audio_guard`
  en la imagen. No es un script shell.
- Socket: `/run/x6200_audio_guard.sock` (UNIX `SOCK_SEQPACKET`), permisos `0600`.
- **Arranca INACTIVO**: no restaura nada hasta un `ARM` válido. En esta entrega ningún `ARM` es
  aceptado (falta readback). `x6200_daemon` **no lo lanza** todavía (cambio de supervisor diferido
  hasta que exista readback y G4/G5).

## 2. Handshake y lease (implementado en el módulo)

`rxmux_ipc_msg_t` (packed, magic `RXMG`, version 1):

| campo | uso |
|---|---|
| `type` | ARM / ARM_ACK / HEARTBEAT / DISARM / RESTORE_REQ / RESTORE_ACK / STOP_WRITER / STATUS |
| `token[16]` | token de instancia único (secreto por sesión) |
| `pid`, `starttime` | campo 22 de `/proc/<pid>/stat`: antirreciclado; ambos obligatorios |
| `mono_ms` | `CLOCK_MONOTONIC` en ms |
| `seq` | monotonía del heartbeat (los repetidos/atrasados no renuevan el lease) |
| `flags` | `READBACK_CONFIRMED` (0x1): obliga a que el ARM venga con valor real leído |
| `base_reg12` | valor real leído antes de armar |
| `lease_ms` | ≤ `RXMUX_LEASE_TIMEOUT_MS` (100 ms) |

Validaciones: magic/version/tipo, token exacto, `pid+starttime` exactos, `mono_ms≠0`, `lease` válido,
`READBACK_CONFIRMED` presente. Sin todas ⇒ `BLOCKED_INVALID`/`BLOCKED_NO_READBACK` y **no se arma**.

- Heartbeat: cada **≤50 ms** (`RXMUX_HEARTBEAT_MAX_MS`), sólo con avance real del stream.
- Lease: >**100 ms** sin progreso ⇒ vencido (`RXMUX_LEASE_TIMEOUT_MS`).
- Presupuesto total: **250 ms** desde el último progreso válido (`RXMUX_RESTORE_BUDGET_MS`).

## 3. Máquina de estados del guardián (módulo, determinista por reloj monótono)

```
inactivo --ARM válido (readback confirmado)--> armado
armado --heartbeat seq nuevo--> renueva lease
armado --tick: elapsed > 100 ms--> REQUEST_RESTORE      (pide restore al coordinador por socket)
armado --sin ack y elapsed > 125 ms--> STOP_WRITER       (SIGKILL al escritor)
armado --writer_stopped--> RESTORE_NOW                   (readback→clear→write→readback)
armado --sin readback confirmado--> REFUSE_NO_READBACK   (NO escribe; se identifica)
```

El restore de emergencia (`rxmux_restore_apply`):
1. exige backend de readback real; si no hay ⇒ **no escribe** (`BLOCKED_NO_READBACK`);
2. lee el registro real; si el bit 0x8 ya está en 0 ⇒ OK sin escribir;
3. `next = real & ~0x8` (preserva PTT/ATU/modem/resto de bits **tal cual**, incluso con TX activo en
   el valor real — no se toca ningún otro bit);
4. escribe y **verifica por readback** que `back == next`; si no ⇒ `BLOCKED_VERIFY` (no se asume).

## 4. Qué se midió (G2, host, procesos reales, bus mock)

Harness: `tests/native/test_rx_audio_mux_guard.c` — `fork` + `socketpair` reales, bus mock en
`mmap(MAP_SHARED)` con `pthread_mutex` PROCESS_SHARED; reloj `CLOCK_MONOTONIC`; 100 repeticiones
(50 GUI responsiva + 50 GUI bloqueada/SIGKILL); latencia = último heartbeat válido → readback
verificado con el bit en 0 y bits PTT/split intactos.

```
G2 restore: 100 repeticiones, p50=101.7 ms, p99=103.3 ms, max=103.3 ms (objetivo <= 250 ms)
9188 comprobaciones, 0 fallos
```

Casos negativos cubiertos: heartbeat de otra instancia/token (no renueva lease), seq repetido, ARM
sin `READBACK_CONFIRMED` (rechazado), lease >100 ms (rechazado), readback corrupto (no escribe/no
verifica), bus busy/error, writer que no cierra el bus (el test exige bus libre antes del restore),
proceso bloqueado sin heartbeat (SIGKILL + restore), helper lifecycle (arranca INACTIVO, rechaza ARM,
SIGTERM limpia socket, exit 0).

**Esto es host/ARM y bus mock: NO es `A_PASS` ni reemplaza la prueba física de #363.**

## 5. Paquete de pruebas / evidencia

- `tests/native/run_native_tests.sh` (stage propio), `--ci-parity` OK.
- Logs: `logs/native-tests.log`, `logs/g2-timing-extract.txt`, `logs/arm-build.txt`.
- ARM: helper ELF32 ARM EABI5 cross-compilado con el toolchain de la ROM y `--self-check` bajo
  `qemu-arm-static` (exit 0); `src/radio.c` cross-compila a objeto ARM.

## 6. Lo que falta para habilitar el camino activo (fuera de este PR)

1. **Readback real** del registro 12 (Fase R) o de una fuente de verdad físicamente equivalente que
   mida el estado del MUX (no la sombra). Sin eso, el guardián no puede verificar su restore.
2. Contrato de arbitraje con `audio_play_en`/TTS/FT8 (prioridad de audio generado) y con el estado
   TX físico del flow (hoy `flag.tx` es la única verdad física disponible, ~35 ms de cadencia).
3. Integración del lanzamiento en `x6200_daemon` **después** de G2+G3 y con #363 en `A_PASS`; antes,
   el daemon no se toca.
4. Prueba física A4 en #363 (≤2 s a ≤−24 dBFS, parlante/jack instrumentado) y #365 (watchdog/rollback).
