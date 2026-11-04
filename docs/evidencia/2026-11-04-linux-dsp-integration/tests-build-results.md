# Resultados de tests/build (reales, reproducibles)

## Host (mock bus, IPC real por socketpair, procesos/threads reales)

```
tests/linux-dsp-integration/run_tests.sh
-> 208 comprobaciones, 0 fallos
```
Cubre: boot/base sin auto-enable, boot bus-fail⇒FAULT, enable precondiciones, heartbeat write_ok,
TX/ATU disable-first y bus-fail, ownership/token/PID/STALE, apply_shared sin voice_rec, lease restore
≤250 ms, PCM inválido/duplicado/overflow/missing, sink/generado/captura, supervisor
(REQUEST_RESTORE/FAULT/restored), IPC timeout acotado, restart sombra inválida, serialización 50
requests sin deadlock, preservación de bits compartidos, PCM missing bloquea.

## ARM / QEMU

```
run_tests.sh (segunda parte)
-> route_arbiter.arm ELF32 ARM EABI5 (toolchain ROM)
-> qemu-arm-static -L <sysroot> route_arbiter.arm --self-check
-> route-arbiter self-check: 0 fails, writes=3
-> QEMU_SELF_CHECK=PASS
```

## CMake

Opción `X6200_LINUX_DSP_ROUTE` (default **OFF**) y subdirectorio añadidos; el build normal no compila
ni instala la feature. El configure completo del árbol requiere submódulos/buildroot no inicializados
en este worktree (mismo límite de entorno que check-matrix); los módulos se compilan con gcc y con el
toolchain ARM directamente, que es la evidencia usada.

## check-matrix / risk

- `git diff --check` → exit 0.
- `check-matrix` → exit 1 **por entorno** (uv cache read-only + lvgl no inicializado); no por el diff.
- `risk.py --files` → ver `commands.log`.

## No físico

Ningún PASS físico. `SPEAKER_LINUX_AUDIO_PHYSICAL=PENDING`, `RESTORE_250MS_PHYSICAL=BLOCKED/PENDING`,
`MUX_PHYSICAL_STATE_VERIFICATION=BLOCKED`.
