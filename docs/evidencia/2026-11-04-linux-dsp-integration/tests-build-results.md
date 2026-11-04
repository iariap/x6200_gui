# Resultados de tests/build (reales, reproducibles)

## Host (mock bus, IPC real por socketpair, procesos/threads reales)

```
tests/linux-dsp-integration/run_tests.sh
-> 208 comprobaciones, 0 fallos   (arbitro/IPC/supervisor)
-> 29 comprobaciones, 0 fallos    (wiring: worker PCM/control, sink, TTS, QA gate, IPC real)
-> 88 comprobaciones, 0 fallos    (proxy GUI/libaether: RMW serializado, voice_rec rechazado,
                                   PTT/ATU/split, TX disable-first ordenado, bus-fail cache inválida,
                                   timeout sin caché falsa, 40 escrituras serializadas)
-> 53 comprobaciones, 0 fallos    (call-sites interpuestos: delegación OFF, reg12 sólo proxy,
                                   record OWNERSHIP, host_cmd ATU directo, idle suprimido +
                                   ROUTE_ERR_UNSUPPORTED, 20 PTT concurrentes, timeout sin original)
-> 24 comprobaciones, 0 fallos    (snapshot all_cmd: captura 54 regs, wire 218 B, lease/versión,
                                   divergencia reg12 no autoritativa, no muta SDK)
   total 402 comprobaciones, 0 fallos
```
Cubre (208): boot/base sin auto-enable, boot bus-fail⇒FAULT, enable precondiciones, heartbeat write_ok,
TX/ATU disable-first y bus-fail, ownership/token/PID/STALE, apply_shared sin voice_rec, lease restore
≤250 ms, PCM inválido/duplicado/overflow/missing, sink/generado/captura, supervisor
(REQUEST_RESTORE/FAULT/restored), IPC timeout acotado, restart sombra inválida, serialización 50
requests sin deadlock, preservación de bits compartidos, PCM missing bloquea.
Cubre wiring (29): worker post-Speex (RAW rechazado), sink fake open/write/close, overflow/ring
acotado, TTS priority, QA gate OFF por defecto (env/archivo), wiring disabled, worker+IPC real
(heartbeat/enable/RX_LINUX/TX disable-first/PCM al sink), productor 200 bloques sin deadlock.

## ARM / QEMU

```
run_tests.sh (segunda parte)
-> route_arbiter.arm ELF32 ARM EABI5 (toolchain ROM)
-> qemu-arm-static -L <sysroot> route_service.arm --self-check
-> route-service self-check: 0 fails, writes=3
-> FEATURE_ON_BUILD=RUN_OK (host con stubs del SDK, X6200_LINUX_DSP_ROUTE=1)
-> route_transport_x6200.arm.on.o ELF32 ARM EABI5 (feature ON, toolchain ROM)
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

## Feature ON build

- Módulos feature ON compilados/ejecutados en host con stubs del SDK (`tests/linux-dsp-integration/stubs/`):
  `X6200_LINUX_DSP_ROUTE=1`, servicio `--self-check` OK; adaptador de transporte compilado ARM feature ON.
- **Feature ON configure completo**: con lvgl submodule inicializado (`git submodule update --init lvgl`,
  OK) y `PKG_CONFIG_PATH`/`PKG_CONFIG_SYSROOT_DIR` del sysroot ROM (busca `speexdsp 1.2.1`, libpulse,
  ALSA, libdrm, etc.) **`cmake -DX6200_LINUX_DSP_ROUTE=ON` COMPLETA** (configure+generate OK).
- **Build feature ON con toolchain Buildroot**: `CMAKE_TOOLCHAIN_FILE=rom-build/clone-buildroot/build/host/share/buildroot/toolchainfile.cmake`
  + `PKG_CONFIG_PATH/SYSROOT_DIR` del sysroot ROM ⇒ configure OK y **`x6200_route_service` ARM EABI5 compila y linkea OK** (incluye adaptador de transporte con libaether del sysroot).
- **GUI completa feature ON**: el target `x6200_gui` falla en `src/radio.c:65/69` por **static assertions
  preexistentes de ABI** (`samples[512]`, `sizeof(x6200_flow_t)==552`) contra la versión de
  X6200Control del sysroot disponible — mismatch repo/SDK del entorno, **no** de los módulos nuevos.
  Registrado **SKIP de entorno, no PASS** (capacidad faltante: sysroot/SDK X6200Control compatible con
  la base 681074e0; o actualizar el repo, fuera de esta tarea).

## Feature OFF / build normal

Normal build: `X6200_LINUX_DSP_ROUTE=OFF` ⇒ no se compila subdirectorio, no hooks en GUI, no binario
de servicio instalado, daemon no arranca nada (condición binario+gate). Sin cambios de
comportamiento/BASE. El build completo GUI feature-ON no se pudo verificar en este entorno (falta
sysroot/lvgl/buildroot); se registra como SKIP honesto, no como PASS.

## No físico

Ningún PASS físico. `SPEAKER_LINUX_AUDIO_PHYSICAL=PENDING`, `RESTORE_250MS_PHYSICAL=BLOCKED/PENDING`,
`MUX_PHYSICAL_STATE_VERIFICATION=BLOCKED`.

## Feature-ON build completo GUI+servicio (task-28, resuelto)

Con toolchain Buildroot + headers del checkout de X6200Control que usa la base:

```
TC=rom-build/clone-buildroot/build/host/share/buildroot/toolchainfile.cmake
REFS=refs/X6200Control/include   # headers ABI correctos (samples[512], flow 552)
PKG_CONFIG_PATH=.../sysroot/usr/lib/pkgconfig PKG_CONFIG_SYSROOT_DIR=.../sysroot \
cmake -S . -B cmake-refs -DX6200_LINUX_DSP_ROUTE=ON -DCMAKE_TOOLCHAIN_FILE=$TC \
      -DCMAKE_C_FLAGS="-I$REFS" -DCMAKE_CXX_FLAGS="-I$REFS"
cmake --build cmake-refs --target x6200_gui -j8        -> [100%] Built target x6200_gui (ARM EABI5)
cmake --build cmake-refs --target x6200_route_service  -> [100%] Built target x6200_route_service (ARM EABI5)
```

El mismatch ABI de task-27 era por headers del sysroot vs headers del repo; usando `refs/X6200Control/include`
la GUI feature ON compila/linkea. Logs en `logs/build-feature-on-gui-arm.log` y
`logs/cmake-feature-on-toolchain-refs-configure.log`. Dependencia exacta: checkout de X6200Control
(0.15.3) cuyo `flow.h` tenga `samples[512]`/552 B; sin él, sysroot del entorno no coincide.
