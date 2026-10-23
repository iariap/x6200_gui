# Unidad #198 — Performance y eficiencia del candidato (render, recursos y audio)

- **Campaña:** assessment pre-release ROM #194 (`iariap/x6200_gui`), unidad 4/5.
- **Ejecutada:** 2026-10-21/22 (UTC) por un agente **ejecutor de evidencia**. No se implementó, rediseñó ni optimizó firmware; no se abrieron tickets; no se tocó la radio, la SD, el banco compartido, los build de otras sesiones ni el volumen `x6200-build-output`.
- **Carpeta:** `docs/evidencia/assessment-rom-194/04-performance/` (rama `assessment/198-evidence` → PR a `assessment/rom-194`).
- **Capa de medición:** HOST (benchmark y gates x86), QEMU_USER (binario ARM del candidato en un chroot/container privado), HARDWARE: **no ejecutada** en esta unidad. Los límites físicos (energía, térmica, boot-to-ready, audio analógico, LCD/input) quedan `PENDING_HARDWARE` para #199.

## 0. Resumen de estados

| Escenario | Estado | Capa | Cobertura | Qué se midió realmente |
|---|---|---|---|---|
| PERF-01 baseline de render (199 fixtures, 20 frames c/u) | **INCONCLUSIVE** | HOST | REAL | 3.980 frames con p50/p95/p99/FPS/dispersión/flushes/área y RSS/PSS del harness; 198/199 escenarios estables, 1 con MAD-CV 26,9 % (>25 %) |
| PERF-02 control A/A (mismo SHA en A y B) | **INCONCLUSIVE** | HOST | REAL | 194/199 comparaciones PASS, 5/199 INCONCLUSIVE por `host noise` (proxies deterministas en 0 %), 0 FAIL; el host estaba cargado (~7–10 de load, `banco-apps` a ~101 % de un core) |
| PERF-03 natural / wakeups / waterfall / ECO / display-off | **PASS** en los tres gates existentes | HOST | REAL | perfiles deterministas del harness: 11.208.000 px en 66 rect/flush sobre 40 frames, 0 px con display-off, 14 wakeups/s del lazo (11 sin drive), tick 0, 195 checks del waterfall ×2 (normal + ASan/UBSan) |
| PERF-04 recursos de la GUI real (QEMU_USER) | **PASS (parcial)** | QEMU_USER | PARTIAL | 10 estados × 3 ventanas de 60 s, 1 muestra/s: CPU de un core, RSS/PSS, fds, threads, context switches, IO, logs y RTT del `get`; sin radio/FFT/servicios reales |
| PERF-05 audio del camino digital (voz/control) | **PASS** en 3 pares voz/silencio | QEMU_USER | SYNTHETIC | 3 capturas con voz (RMS −26,53 dBFS, pico −3,70 dBFS) y 3 controles con **silencio digital exacto** (0 muestras ≠ 0); null-sink de PulseAudio, no mixer/codec/parlante |
| PERF-06 perfil estático de costes/riesgos | **PASS** (análisis) | HOST | REAL (código) | timers/hilos/buffers/IO citados por archivo y línea; estimaciones separadas de medidas |
| PERF-05 códecs (WSPR/SSTV/WeFax/JS8) | **PASS** (reuso #195) | HOST | REAL | se citan las suites ya ejecutadas en #195; no se repitieron |
| PERF-05 FFT/carga DSP en el binario | **BLOCKED** | QEMU_USER | UNSUPPORTED | sin radio no llegan paquetes `x6200_flow_*`; no hay FFT que medir y no se inyectaron muestras |
| Energía/térmica/boot-to-ready/LCD/audio analógico | **PENDING_HARDWARE** | HARDWARE | UNSUPPORTED | requiere instrumentos y la radio; tabla para #199 en §11 |

**No hay aprobación de release.** El informe contiene un baseline INCONCLUSIVE por ruido de host, un control A/A INCONCLUSIVE y varios límites duros de hardware; no se declara “mejora” ni “regresión” de release.

## 1. Base congelada, procedencia y artefacto

| Elemento | Valor |
|---|---|
| GUI candidato (medido) | `c888308eede71205605c06383432ee7e165f202e` |
| `fuente-md5` del checkout | `964193ae9306b5127845cec44a57d52c` |
| Gitlink LVGL | `74d0a816440eea53e030c4f1af842a94f7ce3d3` (v8.3.11) |
| Buildroot | `d084e2870a00b64d9c57474e7eceeef80f43f25b` (`iariap/AetherX6200Buildroot`) |
| Artefacto | `assessment-194/clone-buildroot/build/images/` (copia durable de #195, fuera de Git) |
| GUI en la rootfs | 13.407.624 B, md5 `1f998c46909307397525c975a712adfd`, sha256 `7e2e9331c4cbc51727a098996d98e263c45dd57cb8fff8d1f2aea2964c3849c5` |
| `params.default.db` | 102.400 B, sha256 `bc18ba10ca503b518771fa9c9a5649644f675d7ff0b8c87d03940969f64c2867` |

Los SHA256 del manifiesto de #195 se **revalidaron antes de medir** ([`logs/step0-identity/artifact-sha256.log`](logs/step0-identity/artifact-sha256.log)):

| Archivo | Bytes | SHA256 |
|---|---:|---|
| `sdcard.img` | 571.473.920 | `d0ff4435ad1d068e25bf2c357a4e93478463b187e7745a9a54844488e44e33a5` |
| `rootfs.tar` | 242.790.400 | `7c26c76b6fc27bc12dc8d5e012e51d5c2c0568fa72a8e02cad5432dfcd36a2dd` |
| `rootfs.ext4` → `rootfs.ext2` | 536.870.912 | `9365b52f9aef33e0623ae8271aaf415483fe72e1afbc8ef99f93e15437bc620a` |
| `boot.vfat` | 33.554.432 | `914f07eb00d1e5afee56ab795ce6b7b1d4410d774a3534881da494319c30b17d` |
| `zImage` | 5.567.760 | `660c94c6f7231e7dc262fefb295aae1a8dfcf9b9c2b46cdc1eced2c3296bcc14` |
| `sun8i-r16-x6200.dtb` | 26.544 | `4d73f8a5d5f846c21e4ac30550a5811443a01666f9ee57880b5316e78627cc38` |

No se usó el ROM del orquestador (`rom-build`, GUI `3b22a03a…`) ni el volumen compartido: el binario medido es el del artefacto de #195 (md5 `1f998c46…`, fuente-md5 `964193ae…`), revalidado en el chroot privado y en el comando de audio.

### 1.1 STALE respecto del tip

Al **abrir** la unidad, el orquestador informó el tip `dadb1705`. Al **cerrar** esta evidencia (2026-10-21 23:20Z) la revalidación de `lu2dsx/x6200-rom` da **`d4fb57f8`** ([`logs/step0-identity/refs-tip.log`](logs/step0-identity/refs-tip.log)), con `c888308e` como ancestro (`git merge-base --is-ancestor` = 0):

```
d4fb57f8 [BOOT][UX] Continuous Decrypt, Beams and Rain until GUI handoff (#206)
dadb1705 QA: ROM sin logo de kernel verificada en la radio
...
0d00e7fc QA: arranque de la ROM del tip validado; reparado defecto de MBR
9ed25fa9 chore: remove the old prebuilt vendor u-boot image
```

Frente al candidato, el tip tiene **26 archivos de producción** fuera de `docs/` (`src/bootanim/*`, `src/boot_splash.*`, `src/main.c`, `src/CMakeLists.txt`, `CMakeLists.txt`, `rootfs/etc/init.d/S01bootanim`, `tests/bootanim/`, `.github/workflows/{main,boot-rootfs}.yml`, `u-boot/u-boot-sunxi-with-spl.bin` borrado). **Todos los resultados de este informe son válidos sólo para `c888308e`; no para `dadb1705` ni `d4fb57f8`.** Si se adopta el tip hay que repetir al menos: build/identidad, benchmark de render (cambia `main.c`/handoff), perfil natural/wakeups, arranque/recursos del proceso y toda la fase de hardware. El contenido de bootanimation del tip **no se midió** acá (no es performance del candidato); su coste queda para #199/hardware si se adopta.

## 2. Entorno, aislamiento y método

- **Host:** AMD Ryzen 7 PRO 8840HS (8C/16T), 96.630.480.896 B RAM, Fedora 44, kernel 7.2.8, governor `powersave`; Docker 29.8.2; QEMU ARM 10.2.2; Python 3.14.7; `uv` 0.9.18. Carga durante las corridas: load average 2–10; **`banco-apps` (sesión ajena) ocupó ~101 % de un core durante PERF-01/02** y no se detuvo. El detalle está en [`environment.json`](environment.json) y [`logs/step0-identity/host.txt`](logs/step0-identity/host.txt).
- **Imágenes Docker usadas tal cual:** `x6200-uiharness` = `x6200-uiharness-ci` `sha256:9eedee0e5605…` para el harness; `x6200-buildroot` `sha256:b3e7601313d6…` para QEMU_USER. No se retagueó ni reconstruyó ninguna.
- **Aislamiento:** clon de medición `assessment-194/clone-gui` detached en `c888308e` (`git status --porcelain` vacío, `lvgl` inicializado); worktree de reporte `wt-assessment-198`; scratch privado `assessment-194/scratch-198/` (rootfs extraído del `rootfs.tar`, contenedores `assessment198-res`/`assessment198-audio` con nombres y montajes propios, volumen local `assessment198-build` con bind al build privado de #195). **No** se usó `banco-apps`, `banco-audio`, `x6200-build-output`, `rom-build`, `x6200_qa` ni la radio.
- **Presupuestos usados tal cual (no se editaron):** `tools/ab_testing/quality/budgets.yaml` → warmup 3, repeats 5, rounds 4, min_frames 12, dispersión MAD-CV ≤25 %, p95 delta ≤25 %, flush/área ≤20 %; `performance_exceptions.yaml` sin excepciones usadas.
- **Comandos completos, cwd, env no secreto, UTC y exit code:** [`commands.jsonl`](commands.jsonl). Crudos: [`capturas/`](capturas/), [`bench/`](bench/), [`bench-aa/`](bench-aa/), [`logs/`](logs/).

## 3. PERF-01 — baseline de render (HOST, 199 fixtures)

Comando exacto (con workspace/cache privados):

```
cd clone-gui/tools/ab_testing
UV_CACHE_DIR=$SCR/uv-cache uv run x6200-ab benchmark \
  --revision c888308eede71205605c06383432ee7e165f202e \
  --repeats 5 --warmup 3 --rounds 4 \
  --experiment-id ROM194-host --reports-dir "$OUT/bench" --json \
  --workspace "$SCR/ab-workspace-host" --cache-dir "$SCR/ab-cache"
```

Resultado crudo: [`logs/step1-benchmark/benchmark-host.json`](logs/step1-benchmark/benchmark-host.json) (exit **3 = INCONCLUSIVE**), reporte del tool: [`bench/ROM194-host/report.md`](bench/ROM194-host/report.md). Se ejecutaron **199 escenarios × 20 frames = 3.980 frames** (5 repeats × 4 rounds por escenario).

| Métrica | Valor | Nota |
|---|---:|---|
| Escenarios / frames | 199 / 3.980 | todas las fixtures vigentes |
| p50 de render (mediana entre escenarios) | **4,2946 ms** | distribución de los 199 p50: 3,6422–9,0663 ms |
| p95 de render (mediana) | **5,3839 ms** | p95 de la distribución: 10,5582 ms; máximo 11,7862 ms (`settings-audio`) |
| p99 de render (mediana) | **5,5980 ms** | máximo 12,3133 ms |
| FPS derivado (1000/p50, mediana) | **232,85** | **NO es FPS del LCD ni cadencia natural**: es el frame forzado del harness (`--bench`), 1 flush de 384.000 px por frame |
| Dispersión MAD-CV (mediana) | **7,579 %** | 198/199 ≤25 %; `dfl-presets-graph-tema-redshift` = 26,898 % → escenario inestable |
| Flushes / área redibujada / única | **1,0 / 384.000 px / 384.000 px** | frame forzado de pantalla completa, no el dirty-rect natural |
| RSS / PSS del proceso del harness | 10.448.896–11.075.584 B / ≈10,4–11,0 MB | memoria del **harness x86** (10–11 MB), no del binario ARM ni del Cortex-A7 |

**Lectura crítica:** este benchmark mide coste de render forzado en x86 y detecta regresiones relativas; **no mide la cadencia natural, ni el FPS del LCD, ni el dirty-rect de la radio, ni el coste del Cortex-A7**. El perfil natural (dirty rects, display-off, ECO) está en PERF-03. El estado global es **INCONCLUSIVE** por un único escenario con MAD-CV 26,898 % (>25 %), con el host cargado por otra sesión; **no se repitió para “obtener verde”**. Los 199 p95, RSS y PSS por escenario están en [`capturas/benchmark-per-scenario.csv`](capturas/benchmark-per-scenario.csv) y en el JSON crudo.

## 4. PERF-02 — control A/A (mismo SHA en ambos lados)

```
uv run x6200-ab benchmark --a <CANDIDATE> --b <CANDIDATE> \
  --repeats 5 --warmup 3 --rounds 4 --experiment-id ROM194-AA \
  --reports-dir "$OUT/bench-aa" --json \
  --workspace "$SCR/ab-workspace-aa" --cache-dir "$SCR/ab-cache"
```

Crudo: [`logs/step2-aa/benchmark-aa.json`](logs/step2-aa/benchmark-aa.json) (exit **3**), reporte: [`bench-aa/ROM194-AA/report.md`](bench-aa/ROM194-AA/report.md).

| Resultado | Cantidad |
|---|---:|
| Comparaciones | 199 |
| PASS | **194** |
| INCONCLUSIVE (`host_noise`, proxies deterministas en 0 %) | **5** |
| FAIL | **0** |
| Proxies deterministas sin cambio (flushes 0,0 %, área 0,0 %) | **199/199** |
| Delta p95 (B vs A) mínimo / mediana / máximo | **−33,02 % / −1,645 % / +54,39 %** |

Los 5 INCONCLUSIVE son `main-foco` (+54,4 %), `dfl-presets-more-tema-redshift` (+43,5 %), `dfl-presets-confirm-tema-redshift` (+30,6 %), `main-espectro-agua` (+28,5 %) y `dfl-presets-fav-tema-sunlit` (+27,3 %); el tool los clasifica `host_noise` porque **los dos proxies deterministas no se movieron**. El ancho del A/A (**−33 % a +54 %**) cuantifica el ruido del host cargado: en esta máquina, diferencias de p95 de decenas de por ciento **no** son atribuibles al firmware. No se compara contra `main` ni contra un baseline histórico no autenticado y no se promete mejora/regresión de release. Detalle por escenario: [`capturas/aa-comparisons.csv`](capturas/aa-comparisons.csv).

## 5. PERF-03 — natural, wakeups, waterfall, ECO y display-off (HOST)

Los tres gates existentes se ejecutaron **sin editarlos** sobre el candidato ([`logs/step3-gates/`](logs/step3-gates/)):

| Gate | Resultado | Números |
|---|---|---|
| `tests/check_harness_natural.sh` | **PASS** | 40 frames de 50 ms (LV_DISP_DEF_REFR_PERIOD), 28 drives de spectrum / 40 filas de waterfall; **11.208.000 px invalidados en 66 rectángulos/flushes**; spectrum 132.000 px × 27, waterfall 196.000 px × 39; clock 0 ticks (guard de igualdad #150); control `--natural-static` = 190.400 px (1,7 %); **display-off = 0 px en 40 frames** y captura 384.000 px más clara; `--bench-natural 10` = 10 frames; modo forzado = 3 frames × 1 flush × 384.000 px |
| `tests/check_harness_wakeups.sh` | **PASS** | ventana 1.000 ms: **14 wakeups del lazo con drive / 11 sin drive**, 8/3 con flush, 0 `immediate`, 0 `no_timer`, sleep 10–200 ms, **tick thread 0** (LV_TICK_CUSTOM, CLOCK_MONOTONIC); ventana 3.000 ms = 31 wakeups (escala); captura plana sin bloque `wakeups` |
| `tests/check_waterfall_incremental.sh` | **PASS** | **195 checks, 0 fallas** en modo normal y **195 checks, 0 fallas bajo ASan+UBSan**; cubre 20 vueltas de anillo, cambio de frecuencia/zoom/paleta, RX↔TX, ECO (refresh_period 4), OFF/ON, límites y el instrumento de rasterizado (800 px/fila) |

**Alcance de estos números:** son del **modelo host** (reloj virtual, framebuffer 800×480, mocks). “0 px con display-off” vale para el modelo host, no prueba el panel físico; “14 wakeups/s” es el lazo LVGL del harness, que **no incluye** el `loop_timer` de 30 ms de `src/wifi.cpp` ni los servicios del rootfs. **No se convierte ningún porcentaje de wakeups/píxeles en mA ahorrados.** El binario real en QEMU, sin radio, no permite aislar el reposo normal: el `radio_thread` gira (~194 cambios de contexto/s, ver §6); por eso el perfil de wakeups del harness (14/s) y la suma estática de timers (41,8/s) se informan como cotas separadas, no como el reposo medido del equipo. JSON crudos: [`capturas/natural/`](capturas/natural/), [`capturas/wakeups/`](capturas/wakeups/).

## 6. PERF-04 — recursos de la GUI real en QEMU_USER

Método: binario ARM del artefacto en un chroot privado (`scratch-198/rootfs-res`), contenedor `assessment198-res` (`QEMU_CPU=cortex-a7`), socket de control existente. Por estado: **10 s de estabilización + 60 s de ventana, 1 muestra/s, 3 repeticiones**, retorno al snapshot inicial (home, USB, 14.074 MHz, filtro 2400) entre estados. `res-sampler` lee `/proc/<qemu>` dentro del namespace del contenedor; `rtt-sampler` mide `get` de una conexión por muestra (inicio de `send` → línea completa recibida) con `CLOCK_MONOTONIC`. Scripts: [`scripts/run-resources.sh`](scripts/run-resources.sh), [`scripts/res-sampler.c`](scripts/res-sampler.c), [`scripts/rtt-sampler.c`](scripts/rtt-sampler.c). **Primer intento (transparencia):** se lanzó una corrida con muestreo sólo del proceso; al ver que el total (~100 %) estaba dominado por el lazo del `radio_thread` y no por la GUI, se detuvo y se repitió con muestreo por hilo. El parcial queda en [`logs/step4-resources/run1-partial/`](logs/step4-resources/run1-partial/) y no se usa en las conclusiones.

```
Fórmula CPU% = 100 * (Δ(utime+stime)/CLK_TCK) / Δt_s     (porcentaje de UN core; puede superar 100 % en multihilo)
RTT = fin de recepción completa − inicio de envío      (socket observable, no latencia de tecla)
```

- **Ámbito de memoria:** RSS/PSS son del **proceso QEMU** (emulador + mapeos + guest), leídos de `/proc` **dentro del contenedor**; **no** son la RSS nativa de un Cortex-A7 y no incluyen los servicios del rootfs.
- `/proc/PID/io` es la contabilidad cacheada del kernel: **no** mide desgaste de SD ni energía.
- Boot-to-socket: [`capturas/boot-to-socket-qemu.json`](capturas/boot-to-socket-qemu.json) — **15.865 ms hasta el socket, 15.953 ms hasta el primer `get`**; es wall del contenedor QEMU_USER (sin kernel/init), **no** boot-to-ready de la radio.

**Separación por hilo (necesaria en este entorno):** el proceso QEMU completo mide ~100 % de un core en todos los estados, pero **eso no es la GUI**. El hilo principal (TID 1, lazo LVGL) mide ~1,7 % y el resto lo pone el **TID 16**, que corre en userspace al ~100 % con ~630.000 `read`/s. Por creación (después de los hilos de audio y antes de `remote_server_init`) y por comportamiento, es consistente con el `radio_thread` (`src/radio.c:318-330`): es el único lazo que sólo duerme 15 ms cuando `x6200_flow_read()` devuelve un paquete, y sin BASE/i2c en QEMU la lectura falla y gira. No se simbolizó el stack del hilo (no hay gdb en el chroot); se declara la correlación, no una prueba de nombre. Por eso cada ventana reporta **CPU del hilo principal** y **CPU de los demás hilos** por separado; el total del proceso se conserva en el CSV crudo. En la radio, con paquetes cada ~15 ms, el `radio_thread` duerme y este lazo no debería existir; el número de la GUI es el del hilo principal, y el lazo se documenta como **modo degradado sin radio**, no como reposo del equipo.

| Estado | Reps | CPU hilo GUI (mediana; rango) | CPU otros hilos | ctxt/s | RSS (kB) | PSS (kB) | fds | threads | RTT p50/p95/p99 (µs) | log (B/min) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|
| CW decoder sin señal | 3 | 9.033 % (7.29–9.17) | 99.479 % | 207.41 | 58020.0 | 57692.0 | 26 | 12 | 333/441/549 (n=60) | 0.0 |
| DFLEditor abierto | 3 | 1.675 % (1.29–1.78) | 99.501 % | 212.72 | 58020.0 | 57692.0 | 26 | 12 | 335/470/570 (n=60) | 0.0 |
| home RX (sin cliente) | 3 | 1.675 % (1.25–1.83) | 99.498 % | 220.26 | 58020.0 | 57693.0 | 26 | 12 | — | 0.0 |
| home RX + get 1 Hz | 3 | 1.691 % (1.22–1.78) | 99.474 % | 203.17 | 58020.0 | 57692.0 | 26 | 12 | 320/493/631 (n=60) | 0.0 |
| home RX + get 5 Hz | 3 | 1.303 % (1.22–1.66) | 99.797 % | 205.14 | 58020.0 | 57696.0 | 25 | 11 | 304/413/472 (n=300) | 0.0 |
| Propagation cancelado | 3 | 1.37 % (1.30–1.73) | 99.646 % | 209.17 | 58020.0 | 57696.0 | 26 | 11 | 333/475/605 (n=60) | 0.0 |
| Propagation abierto | 3 | 1.641 % (1.18–1.69) | 99.55 % | 201.29 | 58020.0 | 57692.0 | 25 | 11 | 328/449/551 (n=60) | 0.0 |
| Propagation barrido | 3 | 2.859 % (2.20–2.93) | 99.559 % | 189.53 | 58020.0 | 57696.0 | 26 | 12 | 340/456/679 (n=60) | 220.2 |
| Recorder inactivo | 3 | 1.759 % (1.29–1.84) | 99.574 % | 222.32 | 58020.0 | 57692.0 | 26 | 12 | 322/466/530 (n=60) | 0.0 |
| Settings abierto | 3 | 1.708 % (1.22–1.73) | 99.489 % | 200.45 | 58020.0 | 57692.0 | 26 | 12 | 330/486/552 (n=60) | 0.0 |

Reps = ventanas de 60 s; CPU% = porcentaje de UN core del host (QEMU_USER), no Cortex-A7. 
`otros hilos` incluye el lazo degradado del radio_thread sin BASE (~100 %); no es reposo del equipo.
RTT = socket `get` observable; p99 sólo si n≥60.


**Lectura de las ventanas (medianas de 3 repeticiones; rango entre reps en la tabla):**

- **home RX** ≈ **1,675 %** de un core (hilo GUI; rango 1,25–1,83 %). Con `get` a 1 Hz ≈1,691 % y RTT p50 **320 µs**, p95 **493 µs**, p99 **631 µs** (n=60). Con `get` a 5 Hz ≈1,303 % y RTT p50 **304 µs**, p95 **413 µs**, p99 **472 µs** (n=300): **no hay coste medible del `get` a 5 Hz** en esta resolución (la diferencia entre reps es mayor que el efecto).
- **CW decoder sin señal** (app CW, `dec=1`, `lastbreath app=CW-Kbd`) ≈ **9,033 %** (rango 7,29–9,17 %), **~5,4× home**. Es el mayor coste de UI medido. En este entorno no hay audio/radio que alimente `cw_decoder_signal()` (`src/cw.cpp:494`); el trabajo periódico del diálogo es `timer_cb`→`update_view()` cada 100 ms (`src/dialog_cw_kbd.c:170-189`), que reescribe un label largo con markup de color en cada tick (`:139-167`) aunque el texto no cambie. Se declara **consistente con** ese camino; no se aisló `cw_dec` on/off en una ventana separada.
- **Propagation barrido** ≈ **2,859 %** (2,20–2,93 %) vs **abierto** 1,641 % (1,18–1,69 %): +1,2 pp por el worker + tick de 1 s. El barrido **terminó dentro de la ventana** (`state-before` Measuring → `state-after` Result, `done=5`, `heard=0` sin radio); el estado “barrido” cubre la transición, no 60 s continuos de barrido. **Cancelado** ≈1,37 % (1,30–1,73 %).
- **Recorder inactivo** ≈ **1,759 %** (1,29–1,84 %): el timer de 30 ms no produce un salto grande frente a home en esta resolución. **Settings** 1,708 % y **DFLEditor** 1,675 %: +1 MB de RSS frente a home, sin coste de CPU destacable.
- **Memoria:** RSS/PSS del proceso QEMU en las 30 ventanas: RSS **55.816–59.940 kB**, PSS **55.292–61.436 kB**; fds **25–27**, threads **11–13**. Es el emulador + guest, no la RSS nativa del Cortex-A7.
- **IO/logs:** `/mnt` y el log a stdout no crecen en reposo (0 B/min); sólo el barrido de Propagation escribe ~220 B/min. `/proc/io` del proceso está dominado por el lazo del `radio_thread` (~630.000 `read`/s); el `rchar` es ~73 kB/s en home, **~4,2 MB/s con CW** y ~0,77 MB/s con el barrido. No se identificó el descriptor/fuente de esas lecturas (no hay `strace` en el chroot) y `/proc/io` no mide SD ni energía: queda como observación, no como conclusión.
- **Carga del host:** rep1/rep2 se hicieron con load ~7 (`banco-apps` ajeno ~101 % de un core); rep3 con load ~1,9 al detenerse ese banco. Por eso la CPU del hilo GUI de rep3 es ~20–35 % menor; se informa mediana + rango y la fila `P04-HOST-LOAD` queda **INCONCLUSIVE**. La brecha CW (~9 %) vs home (~1,7 %) es robusta a ese cambio.

**Boot-to-socket (QEMU_USER):** 15.865 ms al socket y 15.953 ms al primer `get`; es wall del contenedor sin kernel/init, **no** boot-to-ready de la radio.


## 7. PERF-05 — audio del camino digital (QEMU_USER, SYNTHETIC)

Se ejecutó el modo audio **existente** de `tools/captura-banco.sh` con `CHROOT`/`VOLUMEN`/`BUILDROOT`/`IMAGEN` privados (`RECONSTRUIR=no`), nunca sobre `banco-apps` ni el volumen compartido. La señal es la **prueba de voz de la propia GUI** (`X6200_VOICE_TEST`), que usa el mismo `audio_play()` que voz/FT8/JS8/SSTV; se captura el monitor de un **sink nulo de PulseAudio** con el nombre fijo que busca `src/audio.c` (`alsa_output.platform-sound.stereo-fallback`), con `parec`, dentro del chroot. El control es la misma corrida con `CAP_VOZ=` (sin voz).

| Corrida | Muestras/canal | Duración | RMS | Pico | Muestras ≠ 0 |
|---|---:|---:|---:|---:|---:|
| voice-1 | 277.256 | 6,287 s | **−26,528 dBFS** | −3,70 dBFS | 67.974 (24,5 %) |
| voice-2 | 277.611 | 6,295 s | **−26,534 dBFS** | −3,70 dBFS | 67.974 (24,5 %) |
| voice-3 | 277.352 | 6,289 s | **−26,530 dBFS** | −3,70 dBFS | 67.974 (24,5 %) |
| silence-1 | 444.521 | 10,080 s | −∞ (todo 0) | −∞ | **0** |
| silence-2 | 444.531 | 10,080 s | −∞ (todo 0) | −∞ | **0** |
| silence-3 | 427.768 | 9,700 s | −∞ (todo 0) | −∞ | **0** |

- Sample rate 44.100 Hz, estéreo, **canales idénticos** en las 6 capturas (la GUI manda mono).
- Los tres controles son **silencio digital exacto** (0 de 444k/427k muestras ≠ 0), así que el nivel de las corridas con voz viene de la GUI y no del camino de captura. **PASS** del par voz/control.
- La **duración de voz varía** (6,29 s) porque la prueba termina sola al acabar la frase; el control corre la ventana de 8 s + overhead. La variabilidad de arranque del sink ya está documentada en `docs/banco-audio.md`; **no se impone bit-identidad** ni se adjudica al DSP.
- La GUI registra `audio_set_play_vol: No se pudo preparar el mixer de 'default'` y `audio_set_rec_vol`: **el mixer/volumen no entra en la captura** porque el chroot no tiene `/dev/snd`; eso requiere la Parte B (loopback del host, `sudo modprobe snd-aloop`) y queda **BLOCKED**, no PASS. Tampoco mide squelch RF, latencia de recepción, ganancia real del mixer, underruns físicos ni calidad del parlante (**PENDING_HARDWARE**).
- El script avisó `el hash de los fuentes cambió` porque el sello `.banco-fuente.md5` no existía en el chroot nuevo; con `RECONSTRUIR=no` usó el binario del artefacto y la propia salida confirma **fuente md5 `964193ae`** y **binario md5 `1f998c46`** (no se reconstruyó nada).
- Los WAV originales **no se conservan como archivo**: se convirtieron sin pérdida a FLAC, se registran su SHA256/tamaño en [`capturas/audio-metrics.json`](capturas/audio-metrics.json), y se verificó que el PCM decodificado del FLAC es idéntico al WAV (`cmp` sobre s16le). En Git van los FLAC (`audio/*.flac`, 288 KiB los seis) y el JSON con ambos hashes. Logs: [`logs/step5-audio/`](logs/step5-audio/).

**Códecs (reuso #195, no repetido):** WSPR 158 checks, SSTV 7 modos (correlación 0,956–0,982), WeFax 300/300 líneas (r=0,9953), JS8 1994 aserciones/88 casos, en imagen `x6200-uiharness-full`; ver `01-baseline/logs/step4b/codec-*.log`. Son **corrección funcional**, no un perfil de carga numérica del DSP. La carga FFT/coeficientes en el binario ARM **no se pudo medir**: sin radio no llegan paquetes al `radio_thread` y no se inyectaron muestras (habría sido implementar un instrumento/API, prohibido); queda **BLOCKED** con esa causa.

## 8. PERF-06 — perfil estático de costes y riesgos (HOST, código)

Detalle completo, con archivo/línea y estimaciones separadas: [`capturas/static-profile.json`](capturas/static-profile.json). Resumen:

| Componente | Hallazgo estático | Evidencia |
|---|---|---|
| Framebuffer / draw buffer | `DISP_BUF_LINES=128` → **409.600 B** de BSS (`800*128*4`, `LV_COLOR_DEPTH 32`); un redibujo completo necesita 4 llenados (1.536.000 B de píxeles) | `src/main.c:76-77,87,495`, `lv_conf.h:27` |
| Pool LVGL | 48 KiB | `lv_conf.h:52` |
| Tick | `LV_TICK_CUSTOM 1` (CLOCK_MONOTONIC): **no hay hilo de tick**; el lazo duerme hasta el próximo timer | `lv_conf.h:96-107`, `src/main.c:634-637,669-681` |
| Timers permanentes | `wifi.cpp` 30 ms (**33,3 wakeups/s**) + clock 200/500 ms (7/s) + info 2 s (0,5/s) + spot 1 s (1/s) → **≈41,8/s** sin apps; el timer de 30 ms existe **aunque WiFi esté apagado** (`g_main_context_iteration(..., FALSE)`) | `src/wifi.cpp:84,371-373`, `src/clock.c:224,227`, `src/info.c:167`, `src/spot_marks.c:277` |
| Timer de cluster | 250 ms, pero **se autopausa** si la columna está off/oculta (no es coste permanente) | `src/cluster_column.c:280-287,440` |
| Apps abiertas | Recorder 30 ms (33/s), JS8 **5 ms (200/s)** + 250 ms, SSTV/WeFax 30 ms, NavTex 50 ms, FT8 250 ms, CW kbd 100 ms; la mayoría borra el timer en `destruct_cb` | `dialog_recorder.c:341,356`; `dialog_js8.c:2019,2148`; `dialog_sstv.c:844`; `dialog_wefax.c:47,1178`; `dialog_navtex.c:38,515`; `dialog_ft8.c:241,875`; `dialog_cw_kbd.c:29,433` |
| Memoria DSP estática | ≈**40.832 B** (spectrum 800 floats, waterfall 512, `AveragedPSD` 512→800/512→512, buffers complejos); `tx_player` 4 KiB de stack; WSPR pide **8 MiB de stack** por hilo de decode | `src/dsp.cpp:41-105,106-122`, `src/tx_player.c:108`, `dialog_wspr.c:218-224` |
| Sincronización | `scheduler_work()` corre en el hilo de GUI en cada vuelta: un callback lento bloquea el render; cola de 64 items con mutex; `params_lock`/SQLite en el camino de settings | `src/scheduler.cpp:23-70`, `src/main.c:671` |
| IO frecuente | lastbreath cada 10 s con `open/write/fdatasync/rename` en `/mnt`; `log_rev()` hace `opendir(/mnt/field/logs)` hasta cada 100 ms por cliente de estado; params SQLite al cambiar settings/banda; log de GUI en tmpfs, movido a `/mnt/app_logs` sólo al terminar (sin rotación en caliente) | `src/main.c:166,240-270`; `src/lastbreath.c:152-200`; `remote_server.cpp:562-572`; `rootfs/usr/sbin/x6200_daemon:14-16` |
| Servicios del rootfs | pulseaudio system, dbus, NetworkManager, sshd, ntpd, webserver Python, btcat | `rootfs/etc/init.d/S*` del artefacto; RSS **no medido** (sin kernel/init en QEMU_USER) |

Estimaciones de bytes por frame (fórmula, **no medidas**; `LV_COLOR_DEPTH 32`): spectrum 132.000 px = **528.000 B/frame**; waterfall 196.000 px = **784.000 B/fila**; clock 15.840 px = **63.360 B/tick**. Son cotas de área invalidada si cada píxel se transfiere una vez, no flush medido.

## 9. Hallazgos principales (sin abrir tickets)

1. **La app CW con el decoder activado (sin señal) cuesta ~9,0 % de un core en QEMU, ~5,4× el home (~1,7 %).** `lastbreath app=CW-Kbd`, `get.ax.dec=1`. No hay audio/radio que alimente `cw_decoder_signal()` (`src/cw.cpp:494`); el trabajo periódico es `timer_cb`→`update_view()` cada 100 ms (`src/dialog_cw_kbd.c:170-189`), que reescribe un label largo con markup de color en cada tick (`:139-167`) aunque el texto no cambie. Medido en 3 ventanas (9,17/9,03/7,29 %; mediana 9,03 %). Es coste de UI ociosa, no de decodificación; la atribución al label es **consistente con el código**, no una prueba de aislamiento on/off.
2. **Timers permanentes: 41,8 wakeups/s estáticos, dominados por el `loop_timer` de 30 ms de WiFi, que existe aunque WiFi esté apagado** (`src/wifi.cpp:84`; callback sólo drena `GMainContext`, `:371-373`). El gate de wakeups del harness mide **14/s** porque no compila `main.c`/`wifi.cpp` y por lo tanto **no cubre** ese timer. Recorder inactivo añade su timer de 30 ms (`src/dialog_recorder.c:341`; medido 1,76 % vs 1,68 % home) y JS8 mantiene uno de 5 ms/200/s (`src/dialog_js8.c:2019`, no medido). No se convierte a mA sin instrumento.
3. **Sin BASE/i2c, el `radio_thread` gira al ~100 % de un core y ~630.000 `read`/s** (TID 16; `src/radio.c:318-330` sólo duerme si `x6200_flow_read()` devuelve un paquete). Es el **modo degradado sin radio** de QEMU, no el reposo del equipo; el total del proceso (~101 %) no debe citarse como consumo de la GUI. Se separó por hilo (GUI ~1,7 %); si en la radio la BASE no responde, el riesgo de lazo activo existe pero **no se midió en hardware**. Evidencia: [`capturas/resources-summary.csv`](capturas/resources-summary.csv), [`logs/step4-resources/threads.txt`](logs/step4-resources/threads.txt), `P04-RADIO-SPIN`.
4. **Baseline de render host y ruido:** p95 mediano **5,3839 ms**, p95 de la distribución **10,5582 ms**, peor `settings-audio` **11,7862 ms** para un frame forzado de 384.000 px; **no** es FPS del LCD ni Cortex-A7. El A/A en el mismo host cargado da deltas de **−33,0 % a +54,4 %** con proxies en 0 %, así que este host **no sirve** para atribuir regresiones de p95 por debajo de ese ruido. Propagation barrido añade +1,2 pp sobre abierto (2,86 % vs 1,64 %) y completa la transición Measuring→Result dentro de la ventana.
5. **Audio digital voz/control PASS en null-sink:** 3/3 voces no nulas (RMS −26,53 dBFS, pico −3,70 dBFS, 24,5 % muestras ≠ 0) y 3/3 controles con **silencio digital exacto** (0/444k). El mixer/volumen, el camino analógico y la FFT/carga DSP quedan **BLOCKED/PENDING_HARDWARE** (sin `/dev/snd`, sin radio; no se inyectaron muestras). Antecedente: #30 y `docs/banco-audio.md`.

## 10. Capas, cobertura y limitaciones

| Escenario | Capa | Cobertura | Comentario |
|---|---|---|---|
| PERF-01 benchmark 199 fixtures | HOST | REAL | x86, frame forzado; no Cortex-A7/LCD |
| PERF-02 A/A | HOST | REAL | cuantifica ruido del host; no es una comparación de release |
| PERF-03 natural/wakeups/waterfall | HOST | REAL | modelo host determinista; no incluye `wifi.cpp` ni servicios |
| PERF-03 display-off/ECO | HOST | PARTIAL | modelo host, panel físico no |
| PERF-04 recursos | QEMU_USER | PARTIAL | proceso QEMU, no servicios/radio; CPU de un core host |
| PERF-04 boot-to-socket | QEMU_USER | PARTIAL | wall de contenedor, no kernel/init |
| PERF-05 audio voz/control | QEMU_USER | SYNTHETIC | null-sink; no RF/mixer/parlante |
| PERF-05 FFT/carga DSP | — | UNSUPPORTED/BLOCKED | sin radio no hay paquetes/FFT; no se inyectó nada |
| PERF-05 códecs | HOST | REAL | reuso #195, corrección funcional |
| PERF-06 perfil estático | HOST | REAL (código) | estimaciones separadas de medidas |
| Energía/térmica/boot real/LCD/input | HARDWARE | UNSUPPORTED | PENDING_HARDWARE (§11) |

Limitaciones principales: (1) **host compartido y cargado** (`banco-apps` a ~101 % de un core) → PERF-01 y PERF-02 INCONCLUSIVE, no repetidos para “verde”; (2) **QEMU_USER no es kernel/init/Cortex-A7**: CPU, RSS/PSS, boot y context switches son proxies del emulador; (3) **sin radio** no hay FFT, CAT, audio real ni servicios del rootfs; (4) no se midió energía, temperatura, brillo, LCD, latencia de tecla, underruns ni audio analógico; (5) el tip avanzó a `d4fb57f8` (STALE, §1.1); (6) el artefacto es el de #195 y se usó tal cual, sin reconstruir.

## 11. Pendientes de hardware para #199

| Métrica | Instrumento / método necesario | Estado |
|---|---|---|
| Consumo en reposo, RX, display-off, TX | fuente de banco + medidor (INA219/equivalente) o shunt calibrado; ventanas con la GUI real | **PENDING_HARDWARE** |
| Temperatura SoC/PA y throttling | sensores del equipo + carga controlada; no se tocó la radio | **PENDING_HARDWARE** |
| boot-to-ready real (kernel → init → GUI) | boot.log de la radio + marca externa; `S01bootanim`/handoff del tip si se adopta | **PENDING_HARDWARE** |
| FPS/dirty-rect del LCD y latencia input→feedback | captura de pantalla de alta velocidad / fotodiodo + inyección de teclas; no hay actuador | **PENDING_HARDWARE** |
| Audio: volumen del mixer, dropout, latencia RX, quality de parlante | loopback `snd-aloop` o `/dev/snd` real; señal RF o generador; no se transmitió | **PENDING_HARDWARE** (Parte B **BLOCKED** por falta de `sudo`/`/dev/snd`) |
| FFT/coeficientes DSP y carga numérica | muestras reales de la radio o inyección autorizada en el `radio_thread`; no se implementó | **BLOCKED** |
| Servicios residentes (pulseaudio/webserver/NM/sshd) RSS/CPU | kernel/init real o qemu-system con rootfs completa; no disponible | **UNSUPPORTED** en esta unidad |

## 12. Reproducción y entregables

- [`artifact-manifest.json`](artifact-manifest.json): SHAs de GUI/Buildroot/LVGL/fuente, artefacto, toolchain, imágenes Docker, QEMU, seed, presupuestos, contenedores y limitaciones.
- [`results.csv`](results.csv): una fila por métrica (escenario, capa, cobertura, precondiciones, acciones, esperado, observado, estado, métrica/valor/unidad/muestras, evidencia, limitación).
- [`commands.jsonl`](commands.jsonl): comando exacto, cwd, env no secreto, versiones, inicio/fin UTC, exit code, stdout/stderr y log.
- Logs completos: [`logs/`](logs/); crudos: [`capturas/`](capturas/), [`bench/`](bench/), [`bench-aa/`](bench-aa/), [`audio/`](audio/).
- Scripts locales (sólo orquestan herramientas existentes; no sustituyen harness/tests): [`scripts/`](scripts/).
- Hashes de todos los archivos de esta carpeta: [`SHA256SUMS`](SHA256SUMS).

Comandos base reproducibles (detalle y timestamps en `commands.jsonl`):

```bash
# PERF-01/02 (desde clone-gui/tools/ab_testing)
uv run x6200-ab benchmark --revision "$CANDIDATE" --repeats 5 --warmup 3 --rounds 4 \
  --experiment-id ROM194-host --reports-dir "$OUT/bench" --json
uv run x6200-ab benchmark --a "$CANDIDATE" --b "$CANDIDATE" --repeats 5 --warmup 3 --rounds 4 \
  --experiment-id ROM194-AA --reports-dir "$OUT/bench-aa" --json

# PERF-03
bash tests/check_harness_natural.sh
SKIP_BUILD=1 bash tests/check_harness_wakeups.sh
bash tests/check_waterfall_incremental.sh

# PERF-04 (contenedor privado assessment198-res; ver scripts/)
bash scripts/start-gui.sh
bash scripts/run-resources.sh

# PERF-05 (CHROOT/VOLUMEN/BUILDROOT/IMAGEN privados; nunca el banco ajeno)
CHROOT=... VOLUMEN=assessment198-build BUILDROOT=... IMAGEN=x6200-buildroot RECONSTRUIR=no \
  CAP_AUDIO="$OUT/audio/voice-1.wav" CAP_SEGUNDOS=8 tools/captura-banco.sh
CHROOT=... ... CAP_VOZ= CAP_AUDIO="$OUT/audio/silence-1.wav" tools/captura-banco.sh
```
