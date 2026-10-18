# Registro de pruebas del X6200

Que una versión **funcione** no se sostiene con "debería andar": se sostiene con corridas de prueba
medidas. Este registro es el hermano de [`docs/HALLAZGOS.md`](HALLAZGOS.md) —ahí van los hallazgos,
acá van las **corridas**— y usa su misma regla, que es del dueño del proyecto:

> todos los hallazgos deben estar fundados, si no se pueden fundar no debe ser un hallazgo

## Cómo se registra una corrida

Cada vez que se compila y se despliega una versión nueva en la radio se agrega **una fila** a la
tabla de abajo y **una sección** al final. Cada afirmación lleva, en el mismo lugar, una de estas
tres cosas:

| Estado | Qué se escribe | Ejemplo de este registro |
|---|---|---|
| **medido** | el comando y su salida pegada, o el número con su origen y fecha | `md5sum /usr/sbin/x6200_gui` → `d0ff2dba9c85` |
| **código** | el archivo (y la línea) que lo dice | `codigo: src/spot_marks.c:181` |
| **no medido** | se dice así, y va al **anexo B** | "no se probó transmisión" |

Un número redondeado está bien **si está dicho como redondeo**. Lo que no puede aparecer es un
número inventado presentado como medición. Si algo se midió en su momento y no quedó la captura, se
dice; si una comprobación no se pudo repetir, también.

**Evidencia cruda:** las capturas, logs y pilas van a `docs/evidencia/<fecha>-<tema>/` y cada
sección los enlaza. La auditoría `tests/audita-pruebas.sh` verifica que la estructura cierre y que
**cada archivo citado exista**.

## Resumen

| Fecha (2026) | Build (md5) | Commit | verifica-radio | Barrido de apps | Caídas | Veredicto |
|---|---|---|---|---|---|---|
| 18/10 ~12:40 (hora radio) | GUI **ffef63df58cb** (build limpio; **NO desplegado**) | `94b9989` | (sin radio) | — | 0 | **Radio inalcanzable en las dos redes** ✗: build limpio + gate #86 OK (`ffef63df58cb`, 13.757.724 B, 3/3 marcas), pero `tools/prueba-gui-en-radio.sh` abortó porque `radio_ip()` no encontró la radio. **#133/#114/#121 quedan SIN VEREDICTO**: no se pudo correr ninguna de las tres pruebas. La 4.ª caída del #121 sigue sin recuperación (necesita ciclo de energía; la batería estaba en 8,6 V, anotado en el #121). Evidencia: [`2026-10-18-qa-radio-backlog/`](evidencia/2026-10-18-qa-radio-backlog/README.md) |
| 17/10 ~22:20 (hora radio) | ROM `c640645d810c` + GUI **b1ba67520079** | `63dbf7a4` (#130) | **SWRScan** | **23 ok** | 0 (1 crash, ya en #131) | **VALIDADA EN RADIO la U del #130** ✓✓: build limpio desde el tip + gate #86 + deploy (idempotente, ya corría el mismo md5). Con el dial en la resonancia (~7.000) la curva es una **U**: base `y=398` en `x=255` (interior), bordes suben **+94/+168 px**; reproducido en 2ª pasada (`x=232`, +80/+174) y con span 200 kHz/Log (`x=238`, +140/+233). **Hallazgo nuevo**: `tests/swrscan_curve_evidence.py` del #125 **no corre sobre una captura de radio** (pide 2 args, no 1; su umbral `min≥180` no ve la curva del tema del owner `Redshift` = RGB(255,179,160)) → **issue nuevo**. Sigue `Error: timeout de RF` (`n≈44/51`, nunca `Completed`). Evidencia `docs/evidencia/2026-10-17-qa-radio-swr125/` |
| 18/10 ~15:50 | (sin radio) | `a3dc4a2` | — | — | 0 | **Sigue caída ~5h45** ✗ (4ª muerte sin recuperación). Fix `5d787cf` pendiente de publicar. 0 merges |
| 18/10 ~15:15 | (sin radio) | `8d41897` | — | — | 0 | **Sigue caída ~5h15** ✗ (la caída más larga; 4ª muerte sin recuperación). El fix `5d787cf` (LASTBREATH) sigue pendiente de publicar. 0 merges |
| 18/10 ~14:40 | (sin radio) + **merge `5d787cf` pendiente de publicar** | `5d787cf` | — | — | 0 | **Llegó el fix del #121**: `5d787cf` *"Hang #121: ultima respiracion persistente (LASTBREATH) y aviso de bateria baja (#138)"* ✓✓ — **es exactamente la evidencia que faltaba**. **La radio sigue caída** ✗ (~4h45 desde la 4ª muerte) → **deploy PENDIENTE**. Comentado en #121 |
| 18/10 ~14:05 | (sin radio) | `59b1cf0` | — | — | 0 | **Sigue caída ~4h15** ✗ (4ª caída sin recuperación). 0 merges |
| 18/10 ~13:30 | (sin radio) | `efae955` | — | — | 0 | **Sigue caída ~3h45 después de la 4ª caída** ✗ (3 de 4 muertes no se recuperan solas). 0 merges; nueva rama `factory/swrscan-stopfix` |
| 18/10 ~12:55 | (sin radio) | `94b9989` | — | — | 0 | **Sigue caída 3h después de la 4ª caída** ✗ (no se recuperó sola, igual que la 1ª y la 2ª). 0 merges. Pasa a la mañana para el ciclo de energía del owner |
| 18/10 ~12:20 | (sin radio) | `07321a5` | — | — | 0 | **Sigue caída 1h30min después de la 4ª caída** ✗: 0 hosts con SSH en 192.168.0.0/24 y 192.168.18.0/24. Sin merges nuevos. Pendiente: que el owner la mire (ciclo de energía) y **medir el uptime al volver** para saber si reinició o se colgó sin reiniciar |
| 18/10 ~11:45 | ROM `c640645d810c` + GUI ffef63df58cb | `896a3cf` | (sin radio) | — | 0 | **4ª CAÍDA del #121** ✗: última vez viva 00:09 (uptime 51644 s = **14 h 20 min**, pid 7317, CRASHLOG 0) → a las 00:45 **0 hosts con SSH en las dos redes** (mi red OK). **Murió en reposo, sin actividad ni TX** ✓ → **no hace falta actividad para que se caiga**. Sin `pstore` sigue sin haber evidencia del kernel (4 caídas, 0 evidencia). Comentado en #121 |
| 18/10 ~11:05 | ROM `c640645d810c` + GUI ffef63df58cb | `b6be6ee` | (no corrido) | (no corrido) | 0 | **Estable 14h20min** ✓ (pid 7317, uptime 51644 s, CRASHLOG 0). 0 merges |
| 18/10 ~10:30 | ROM `c640645d810c` + GUI ffef63df58cb | `837bbf2` | (no corrido) | (no corrido) | 0 | **Estable 13h50min** ✓ (pid 7317, uptime 49843 s, CRASHLOG 0). 0 merges |
| 18/10 ~09:55 | ROM `c640645d810c` + GUI ffef63df58cb | `d5019a7` | (no corrido) | (no corrido) | 0 | **Estable 13h20min** ✓ (pid 7317, uptime 48045 s, CRASHLOG 0). 0 merges |
| 18/10 ~09:20 | ROM `c640645d810c` + GUI ffef63df58cb | `eba2230` | (no corrido) | (no corrido) | 0 | **Estable 12h50min** ✓ (pid 7317, uptime 46243 s, CRASHLOG 0). 0 merges |
| 18/10 ~08:45 | ROM `c640645d810c` + GUI ffef63df58cb | `7aea604` | (no corrido) | (no corrido) | 0 | **Estable 12h20min** ✓ (pid 7317, uptime 44444 s, CRASHLOG 0). 0 merges |
| 18/10 ~08:10 | ROM `c640645d810c` + GUI ffef63df58cb | `d1cefe7` | (no corrido) | (no corrido) | 0 | **Estable 11h50min** ✓ (pid 7317, uptime 42645 s, CRASHLOG 0). 0 merges |
| 18/10 ~07:35 | ROM `c640645d810c` + **GUI ffef63df58cb** (merges `d3e84ed`/`f53c2ab`) | `d3e84ed` | **comando remoto swrscan** | **2 ok (Run+Stop)** ✓ | 0 | **VALIDADO el comando `swrscan` del #135** ✓✓: `val=1` → `ok` (Run ✓), `val=0` → `ok` (Stop ✓), guarda `"app not open"` ✓ sin la app. La captura durante el barrido tiene **3083 px de tinta de curva** ✓ = el barrido corrió. **Valor correcto es `1\|0`**, no `"run"/"stop"` (mi primer intento dio `Invalid value` ✗). TX 0.1 W con autorización. pid 7317, CRASHLOG 0 |
| 18/10 ~07:00 | ROM `c640645d810c` + GUI b1ba67520079 | `c55e8b9` | (no corrido) | (no corrido) | 0 | **Estable 10h50min** ✓ (pid 3931, uptime 39044 s, CRASHLOG 0). 0 merges; 3 ramas del otro agente activas (roe-staircase-119, swr125-capturas, swr125-direction) |
| 18/10 ~06:25 | ROM `c640645d810c` + GUI b1ba67520079 | `b8441a6` | (no corrido) | (no corrido) | 0 | **Estable 10h20min** ✓ (pid 3931, uptime 37245 s, CRASHLOG 0 en el log vivo). 0 merges. El app_log nuevo (22-38-46) es el del SIGSEGV del #131, ya reportado |
| 18/10 ~05:40 | ROM `c640645d810c` + GUI b1ba67520079 | `885f823` | **modo/filtros** | **no repro** | 0 | **CRASH NUEVO detectado en el app_log** ✗: `CRASHLOG sig=11 fallo=0xf363` justo después de `on_vfo_mode_change: mode=2` + carga de `filter_low/high`; la GUI se relanzó sola (pid 2728→3931) y el equipo siguió. **NO es el del #116**. **Stack sin resolver** (direcciones en rango de .so, sin md5 de las libs). **No reproducible**: LSB→USB→CW→AM + 4 botones → sin crash (pid 3931, CRASHLOG 0). **Issue nuevo cargado** |
| 18/10 ~05:10 | ROM `c640645d810c` + **GUI b1ba67520079** (merge `885f823`) | `885f823` | **SWRScan** | **1 ok — VALLE** ✓✓ | 0 | **PUBLICADO Y VALIDADO `885f823`**: la curva de ROE **ya NO está invertida** ✓✓. Números: centro y=395 (antes 118) → ROE baja ✓, bordes y=300/309 → ROE alta ✓ → **el valle está en la resonancia** ✓. `min 1.0 @ 6.780.000 kHz`, `Passes: 1`, span 500 kHz, Scale Linear. **Pendiente**: sigue saliendo `Error: timeout de RF` (barrido cortado en n=44/51) ✗ |
| 18/10 ~04:40 | ROM `c640645d810c` + GUI 846afac6ed47 | `c67a30b` | (no corrido) | (no corrido) | 0 | **Estable 9h07min** ✓ (pid 1133, uptime 32798 s, 1 GUI, CRASHLOG 0) — la racha más larga de la sesión. 0 merges |
| 18/10 ~01:55 | ROM `c640645d810c` + GUI 846afac6ed47 | `e38267c` | (no corrido) | (no corrido) | 0 | **Estable 5h50min** ✓ (pid 1133, uptime 21043 s, 1 GUI, CRASHLOG 0). 0 merges |
| 18/10 ~01:20 | ROM `c640645d810c` + GUI 846afac6ed47 | `37de993` | (no corrido) | (no corrido) | 0 | **Estable 5h20min** ✓ (pid 1133, uptime 19243 s, 1 GUI, CRASHLOG 0). 0 merges |
| 18/10 ~00:45 | ROM `c640645d810c` + GUI 846afac6ed47 | `40f649f` | **SWRScan** | **1 repro** | 0 | **Curva de ROE INVERTIDA confirmada con números** ✗✓ (lo que vio el owner): la curva extraída de `26-roe-multipasada-final.png` da **ROE ~4.5 en el CENTRO** y **~1.4 en los extremos** (601 columnas, y=118 centro vs y=399 borde) → **donde debería haber un valle hay una montaña**. El readout dice `min 1.0 @ 6.891.334` y **es coherente** con la curva (mínimo en el borde) → **el dato parece bien, el render parece invertido** (sin fundar). Comentado en el #119 con las 2 capturas (curva invertida + error de multipasadas) |
| 18/10 ~00:20 | ROM `c640645d810c` + GUI 846afac6ed47 | `4ee935c` | **SWRScan Passes>1** | **1 repro — gate** | 0 | **Reproducido el error del owner**: con `Passes` en 5 y `Run` sale **`Error: 3/5 pasadas requieren validacion RF`** + `sin datos medidos` ✗. **Causa en código**: `SWRSCAN_MULTIPASS_TX_ENABLED = 0` (dialog_swrscan.h:61) → `limits->max_passes = 1` → `swr_session.c:281` aborta con `SWR_ABORT_MULTIPASS_GATED`. **El gate protege el PA y está bien**; **el defecto es de UX**: el botón `Passes` cicla 1→3→5 y el `Run` lo rechaza sin aviso. Evidencia `28-roe-multipasada-error.png` |
| 17/10 ~23:40 | ROM `c640645d810c` + GUI 846afac6ed47 | `a1a537c` | (no corrido) | (no corrido) | 0 | **Estable 4h50min** ✓ (pid 1133, uptime 17443 s, 1 GUI, CRASHLOG 0). 0 merges |
| 17/10 ~23:05 | ROM `c640645d810c` + GUI 846afac6ed47 | `94bfbe6` | (no corrido) | (no corrido) | 0 | **Estable 4h20min** ✓ (pid 1133, uptime 15645 s, 1 GUI, CRASHLOG 0). 0 merges |
| 17/10 ~22:30 | ROM `c640645d810c` + GUI 846afac6ed47 | `f275073` | (no corrido) | (no corrido) | 0 | **Estable 3h50min** ✓ (pid 1133, uptime 13844 s, 1 GUI, CRASHLOG 0). 0 merges |
| 17/10 ~21:55 | ROM `c640645d810c` + GUI 846afac6ed47 | `dbd6174` | (no corrido) | (no corrido) | 0 | **Estable 3h20min** ✓ (pid 1133, uptime 12043 s, 1 GUI, CRASHLOG 0). 0 merges |
| 17/10 ~21:20 | ROM `c640645d810c` + GUI 846afac6ed47 | `298f895` | (no corrido) | (no corrido) | 0 | **Estable 2h50min** ✓ (pid 1133, uptime 10244 s, 1 GUI, CRASHLOG 0). 0 merges |
| 17/10 ~20:45 | ROM `c640645d810c` + GUI 846afac6ed47 | `1da0530` | (no corrido) | (no corrido) | 0 | **Estable 2h20min con la GUI nueva** ✓ (pid 1133, uptime 8445 s, 1 GUI, CRASHLOG 0) · **red OK** ✓ (`scp` de prueba pasa ✓) · `audio_init: Conected` ✓ (el sink sigue en ceros: parte 2 del #116). 0 merges |
| 17/10 ~19:55 | ROM `c640645d810c` + GUI 846afac6ed47 | `bb76d74` | **SWRScan multipasada (final)** | **1 ok — PARÁBOLA** ✓✓ | 0 | **CONFIRMADO: la curva de ROE ya es una PARÁBOLA** ✓✓ (captura `26-roe-multipasada-final.png`): arranca ~1.0 a la izq, sube suave, pico ~3.7 al centro (6.99 MHz), baja suave a ~1.0 der — **sin escalones** ✓. UI con `min 1.0 @ 6.891.334 kHz (n=44/51)`, `Passes: 1`, span 200 kHz. **Nuevo**: mensaje `Error: timeout de RF` (el barrido se cortó en 44/51; no medí si es nuevo). App abierta tras 45 s, pid 1133, CRASHLOG 0 |
| 17/10 ~19:20 | ROM `c640645d810c` + **GUI nueva 846afac6ed47** (996b7fa + aa1945e) | `b00255d` | **SWRScan multipasada** | **1 ok (UI + scan)** | 0 | **PUBLICADO Y VALIDADO el build del #119** ✓✓: primer deploy por WiFi exitoso del día (el reinicio del owner arregló la red ✓). La UI muestra **`Scanning pass 1/1 p.6/51`**, **`min 1.0 @ 6.891.334 kHz (n=5/51)`**, **`Passes: 1`**, span 200 kHz, modo LSB forzado ✓; `Run` responde, pid 1133, CRASHLOG 0. **Pendiente**: si la curva final es parabólica (en la captura iba a mitad de barrido) |
| 17/10 ~18:35 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `147713b` | (sin radio) | — | 0 | **Radio inalcanzable ~2 h** ✗ tras la 3ª caída y el reinicio del owner: **0 hosts con SSH** en 192.168.0.0/24 y 192.168.18.0/24, mi red OK (gateway responde). El build del #119 (`996b7fa` → GUI 846afac6ed47) **no se pudo publicar** (la red corta toda transferencia: 3 intentos con `lost connection`/`Connection closed`, ni 100 KB). **Queda por confirmar** (no lo veo yo): si la radio está encendida → es el WiFi; si está negra → otra muerte (crítico en #121). Nueva rama `factory/swr125-capturas` |
| 17/10 ~17:30 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `b285a73` | — | — | 0 | **Red degradada tras la 3ª caída** ✗ (medido): ping a la radio **0/4 (100% pérdida)** ✗, `scp` de **100 KB no termina** ✗, 13,7 MB y 5,9 MB comprimido tampoco ✗, pero **SSH de comandos anda** ✓ → **el canal de datos quedó roto**; **no es MTU** (ping chico también falla) **ni tamaño** (100 KB falla). Causa sin fundar. **Deploy por WiFi imposible** → el build `996b7fa` (#119 multipasada) **no llegó** |
| 17/10 ~17:15 | ROM `c640645d810c` + GUI aaa60c1d3ab5 (el build nuevo NO llegó) | `12ace3e` | (no corrido) | (no corrido) | 0 | **3ª CAÍDA del #121, esta vez durante un scp** ✗: antes pid 2452/uptime 44906 s → `scp` cortó al 1,9% (`Connection closed`) → después **pid 311/uptime 456 s = SE REINICIÓ SOLO** (a diferencia de la 1ª caída, que no se recuperó en ~2 h). **La red quedó mal**: los `scp` de varios MB se cuelgan siempre (6 intentos), aunque SSH/`ls`/`md5sum` andan. **El build `996b7fa` (multipasada #119) NO se pudo publicar** — sigue el binario anterior |
| 17/10 ~15:35 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `a777fbd` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 12h21min** ✓ (pid 2452, uptime 44467 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~15:00 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `98f1231` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 11h51min** ✓ (pid 2452, uptime 42668 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~14:25 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `11ff9ca` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 11h21min** ✓ (pid 2452, uptime 40875 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~13:50 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `92fd0ae` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 10h51min** ✓ (pid 2452, uptime 39069 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~13:15 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `251ab90` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 10h21min** ✓ (pid 2452, uptime 37270 s, 1 GUI, CRASHLOG 0). La IP volvió a 192.168.18.67. 0 merges; 5 ramas en curso |
| 17/10 ~12:40 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `0498593` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 9h51min** ✓ (pid 2452, uptime 35471 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~12:05 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `162bb4c` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 9h21min** ✓ (pid 2452, uptime 33671 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~11:30 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `1700cd8` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 8h51min** ✓ (pid 2452, uptime 31870 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~10:55 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `1129424` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 8h21min** ✓ (pid 2452, uptime 30070 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~10:20 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `1236440` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 7h51min** ✓ (pid 2452, uptime 28271 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~09:45 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `7acc96a` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 7h21min** ✓ (pid 2452, uptime 26471 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~09:10 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `fd16712` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 6h51min** ✓ (pid 2452, uptime 24671 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~08:35 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `fc35445` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 6h21min** ✓ (pid 2452, uptime 22870 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~08:00 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `5c44d3f` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 5h51min** ✓ (pid 2452, uptime 21071 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso (sin cambios) |
| 17/10 ~07:25 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `aad2a5a` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 5h21min** ✓ (pid 2452, uptime 19273 s, 1 GUI, CRASHLOG 0). 0 merges; 5 ramas en curso |
| 17/10 ~06:50 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `563bfd3` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 4h51min** ✓ (pid 2452, uptime 17470 s, 1 GUI, CRASHLOG 0). 0 merges; **nueva rama `factory/swrscan-multipass-125`** (el próximo paso del #119: el escalón). 5 ramas en curso |
| 17/10 ~06:15 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `e717d9f` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 4h21min** ✓ (pid 2452, uptime del sistema 15673 s, 1 GUI, CRASHLOG 0). 0 merges; **nueva rama `factory/cw-tune-122`** (issue nuevo, no mío). Siguen 116, 119 y 125 |
| 17/10 ~05:40 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `d7d0ca2` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 3h51min sin caídas** ✓ (pid 2452, uptime 13872 s, 1 GUI, CRASHLOG 0). 0 merges. 3 ramas en curso: cw-sigsegv-116, roe-staircase-119 y swrscan-mock-fixtures-125 |
| 17/10 ~05:05 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `52a0204` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 3h21min** ✓ (pid 2452, uptime 12071 s, 1 GUI, CRASHLOG 0). La radio cambió de IP a 192.168.18.67 (normal). 0 merges; **nueva rama `lu2dsx/swrscan-mock-fixtures-125`** (fixtures mock del SWR Scan = los datos del #119) y siguen 116/119 |
| 17/10 ~04:35 | ROM `c640645d810c` + GUI aaa60c1d3ab5 | `38e2186` | (no corrido) | (no corrido) | 0 | **GUI nueva estable 2h51min** ✓ (pid 2452, uptime del sistema 10271 s, 1 GUI, CRASHLOG 0): los fixes #116/#119 aguantan. 0 merges nuevos; siguen `factory/cw-sigsegv-116` y `factory/roe-staircase-119` |
| 17/10 ~04:00 | ROM `c640645d810c` + GUI nueva aaa60c1d3ab5 (merges `3553a47`/`e2edd91`) | `3553a47` | **CW + SWRScan** | **1 ok (CW), 1 parcial** | 0 | **VALIDACIÓN DE LOS 2 FIXES**: (1) **#116 ✓**: abrir CW, apretar los 4 botones de mensajes y cerrar → **no crashea** (pid 2452, CRASHLOG 0, sin errores); el HALLAZGOS #48 confirma el análisis (NULL al keyer con tabla vacía) **con mi medición**. (2) **#119 parcial**: el **fondo del chart pasó a negro** ✓ (antes gris, `c54-nf-10.png`) pero **la curva no apareció** ✗ — y **el barrido NO corrió** (botón en `Run`, modo `USB-D`): **no es evidencia** de que no dibuje. Escalon: pendiente |
| 17/10 ~03:50 | ROM `c640645d810c` (GUI e294229dd32d) | `c1ca41b` | (no corrido) | (no corrido) | 0 | **111 min estable sin caídas** ✓ (pid 309 desde el barrido, uptime 6670 s ≈ 1 h 51 min, 1 GUI, CRASHLOG 0): el #121 sin reproducirse en casi 2 h. **Audio: el race persiste**. 0 merges; siguen las dos ramas (116/119) |
| 17/10 ~03:20 | ROM `c640645d810c` (GUI e294229dd32d) | `a6b6663` | (no corrido) | (no corrido) | 0 | **81 min estable sin caídas** ✓ (pid 309 desde el barrido, uptime 4872 s, 1 GUI, CRASHLOG 0): el #121 sin reproducirse. **Audio: el race persiste**. 0 merges; siguen `factory/cw-sigsegv-116` y `factory/roe-staircase-119` |
| 17/10 ~02:50 | ROM `c640645d810c` (GUI e294229dd32d) | `4d913ac` | (no corrido) | (no corrido) | 0 | **51 min estable sin caídas** ✓ (pid 309 — el mismo del barrido, uptime 3071 s, 1 GUI, CRASHLOG 0): el #121 sigue sin reproducirse. **Audio: el race persiste**. 0 merges; siguen `factory/cw-sigsegv-116` y `factory/roe-staircase-119` en curso |
| 17/10 ~02:20 | ROM `c640645d810c` (GUI e294229dd32d) | `c0d6184` | (no corrido) | (no corrido) | 0 | **Radio estable 21 min, sin caídas nuevas** ✓ (pid 309 desde las ~02:00, uptime 1279 s, 1 GUI, CRASHLOG 0) → el #121 **no** se reprodujo tras el barrido. **Audio: el race persiste** (audio_init_sync: pulseaudio no disponible). **0 merges**, pero el otro agente abrió **`factory/roe-staircase-119`** (mi #119 ✓) y sigue **`factory/cw-sigsegv-116`** (#116 ✓) |
| 17/10 ~02:00 | ROM `c640645d810c` (GUI e294229dd32d) | `0cc4357` | **PERMANENCIA=14, 23 apps** | **23 ok, 0 caídas** ✓ | — | **PRIMER BARRIDO LIMPIO SOBRE LA ROM NUEVA**: 23/23 ok (FT8…SWRScan), 0 cuelgues, con **monitor de evidencia en paralelo** (uptime 340→584 s avanzando, pid 309 constante, CRASHLOG 0) → **el barrido de apps NO dispara la caída del #121** (dato negativo útil). Comentado en #121 |
| 17/10 ~01:35 | ROM `c640645d810c` / GUI e294229dd32d | `44b225f` | (pendiente) | (pendiente) | — | **Datos nuevos de las caídas**: (1) **la 1ª caída NO se recuperó sola**: estuvo muerta de ~22:57 a ~01:09 (**~2 h**) y el **watchdog de 16 s no la levantó** ✗ (dato medido, causa sin fundar); (2) **la 2ª caída no dejó log** (el app_log más nuevo sigue siendo el de la 1ª; el log vivo es tmpfs y se archiva sólo si la GUI sale ordenada) → **la evidencia se perdió con el ciclo de energía**; (3) la radio volvió: uptime 38 s, GUI e294229dd32d, pid 309, CRASHLOG 0. Comentado en #121 |
| 17/10 ~01:25 | ROM `c640645d810c` / GUI e294229dd32d | `91adc22` | (barrido inútil) | **0 apps** | — | **Segunda caída de la radio en ~30 min** ✗ (a las 01:10 respondía con pid 311; a las 01:25 **0 hosts con SSH en las dos redes**, mi red OK). El barrido arrancó con **IP vacía** (la radio ya no respondía) → 23 apps con "no responde", **sin dato de apps**. **Sin barrido de SWR mío antes de esta caída** → la correlación con el SWR se **debilita**. Comentado en #121 |
| 17/10 ~01:10 | ROM `c640645d810c` / GUI e294229dd32d | `54ebfb1` | (pendiente) | (pendiente) | — | **La radio se colgó entera** ✗ (necesitó ciclo de energía): el último log (`22-57-10`) **corta a mitad de línea en medio de un barrido de SWR** (pasos de ~100 Hz cada 50-90 ms = el sweep del #113/#117, que es una TX). **CRASHLOG=0** en el log vivo y en los 4 últimos → **no es la GUI, se va el equipo**. La radio volvió con el ciclo de energía (uptime 27 s, GUI e294229dd32d ✓). **Causa sin fundar** (sin pstore ni consola). Issue nuevo, relacionado con el #114 (mismo tipo de muerte). Cliente remoto repuesto ✓ |
| 17/10 ~00:50 | ROM `c640645d810c` / GUI e294229dd32d | `d857be3` | (barrido fallido) | **0 ok** | — | **La radio quedo INALCANZABLE** ✗ durante el barrido de apps sobre la ROM nueva: no responde en 192.168.0.0/24 ni 192.168.18.0/24 (mi red esta bien: gateway OK). **El 'CUELGUE' de las 23 apps era mi herramienta**: el boot del flash borro `/tmp/x6200-remote` y el script no lo repone (0 apps probadas). **La radio estaba VIVA durante el barrido** (tras cada app la SSH respondia y el uptime avanzaba: 1601->1626->1653->... y el barrido llego al final trayendo el boot.log). Murio despues. **Causa del fallo del script (mi herramienta)**: el boot del flash borro `/tmp/x6200-remote` y `prueba-apps-radio.sh` **no lo repone** → `ok: 0`. **La caida de la radio es un hecho separado y sin diagnosticar** (necesita ciclo de energia) |
| 17/10 ~00:30 | ROM `c640645d810c` / GUI e294229dd32d | `223b30a` (con #117) | (pendiente) | (pendiente) | — | **#119**: la curva de ROE **no es una parábola**: se dibuja **escalonada** (6 cuadros del framebuffer leídos en bucle rápido, `frame-2.png` lo muestra) → el #117 dice "fitted to a single parabola" en el código pero el render sale en escalera. **Y la app se cierra sola** al terminar el barrido (3 capturas muestran la pantalla principal, mismo pid, sin CRASHLOG). Además: fantasma de repintado. Causa **sin fundar**. **Datos anexados al issue**: `roe-muestras-extraidas.csv/.json` (496 puntos extraidos de la pantalla; el log no guarda las muestras) — **y muestran que la curva viene cuantizada en escalones** (12 muestras identicas seguidas, p.ej. 1.33) |
| 17/10 ~00:10 | **ROM nueva corriendo** e294229dd32d (imagen `c640645d810c`) | `223b30a` | (pendiente barrido) | (pendiente) | — | **La ROM flasheada arrancó bien** ✓: GUI e294229dd32d ✓, lib 5ed07c1719f8 ✓, `init.d`=`S20gui` ✓ (fix #89 en la imagen ✓), CRASHLOG 0, **y los ajustes del owner intactos** (`Redshift`/`vol=66`/`span=50000`, DATA 3,2G) ✓✓. **El race del #116 se reproduce en la instalación limpia** ✗✓ → queda fundado que es determinista |
| 16/10 ~23:40 | **ROM nueva flasheada** `c640645d810c` | `223b30a` (incluye #117) | (pendiente: validar en la radio) | (pendiente) | — | **Flasheo de la SD** ✓: verificado por md5 ida y vuelta (`c640645d810c…` idéntico) ✓, `init.d`=sólo `S20gui` ✓. **DATA restaurada en slot 3** ✓ y **ajustes intactos** (params.db 106 KB, `theme_name=Redshift`, `vol=66`, `swrscan_span=50000`, 662 app_logs) ✓. Los deploys por WiFi fallaron 2 veces (script colgado + scp text-busy) → se fue por SD |
| 16/10 ~23:00 | `3604cece0e43` (sigue) | `246ffa5` | (no corrido) | (no corrido) | — | 0 merges nuevos; **el otro agente abrió `factory/events-queue-111`** (trabajando el #111). Audio: `Conected` ✓ y streams 1+2 ✓ pero el sink sigue `rms=0.0` (la parte 2 del #116 sin fix). Radio estable, CRASHLOG 0 |
| 16/10 ~22:50 | `3604cece0e43` (sigue) | `420d5fc` | (no corrido) | (no corrido) | — | **Causa raíz del audio FUNDADA**: un **race de arranque** — `audio_init_sync: pulseaudio no disponible (Connection refused)` a los ~6 s (la GUI no reintenta) → **0 streams en frío**; al reiniciar la GUI con PA arriba dice **`Conected`** y abre los 3 streams. **PERO**: aun conectada, **el sink sigue en ceros** (captura ✓ + reproduce silencio ✗) → **segunda parte sin causa**. Es lo que dejó sin audio tras el crash del #116 |
| 16/10 ~22:40 | `3604cece0e43` (sigue) tras **reboot** | `538fffc` | (no corrido) | (no corrido) | — | **Prueba decisiva: reiniciar el EQUIPO tampoco devuelve el audio** (boot limpio pid 308, sink `rms=0.0`) → la falla es **determinista** (no corrupción, no códec, no volátil). Queda: un **parámetro guardado** o un **defecto del código** de la GUI. **El DAC vuelve a quedar apagado al arrancar** (igual que se lo encontró) |
| 16/10 ~22:35 | `3604cece0e43` (sigue) | `fb38e1d` | (no corrido) | (no corrido) | — | **Prueba grounded: reiniciar la GUI NO recupera el audio** (pid 1119→1950, sink sigue `rms=0.0`) → la falla es **persistente y dentro de la GUI**. Streams: los 3 abiertos/sin mute/100% (Play 44100, Capture 44100 y **un source-output a 30 Hz inusual**). DAC ya corregido. Falta solo **reiniciar el equipo** (decisivo), propuesto al owner |
| 16/10 ~22:30 | `3604cece0e43` (sigue) | `a118c18` | (no corrido) | (no corrido) | — | **Prueba del DAC hecha** (autorizada): `amixer sset DAC on` + nivel 160 → el DAC quedó **160 [83%] [0.00dB] [on]** ✓ pero **el sink sigue en ceros** (`rms=0.0`) → **el DAC no era la causa** (era un **segundo** problema, ahora corregido). **Error mío de razonamiento**: el monitor del sink está **antes** del codec, así que esa grabación no podía medir el efecto del DAC. Queda probado que **la GUI sigue escribiendo ceros** → el problema está en su cadena de audio |
| 16/10 ~22:25 | `3604cece0e43` (sigue) | `67efc1d` | (no corrido) | (no corrido) | — | **Cadena de salida medida eslabón por eslabón**: `AIF1 DA0` 160 [83%] [0.00dB] (sana) → **`DAC` 0 [0%] [-99999.99dB] [off] (APAGADO)** → `Headphone` 58 [92%] [on] (sana) · `Headphone Source = DAC` → **el corte está en el DAC** (coherente con que el sink reciba ceros). Prueba propuesta al owner: `amixer sset DAC on` (reversible) |
| 16/10 ~22:20 | `3604cece0e43` (sigue) | `c5658f9` | (no corrido) | (no corrido) | — | **Confirmado que la radio RECIBE** (captura `24-recepcion-sin-audio.png`: 4-5 portadoras en el espectro + trazas en el waterfall) → **el RF no es la causa** (el owner tenía razón, mi candidato del RF gain queda muerto). **Pista fuerte**: `DAC: Playback 0 [0%] [-99999.99dB] [off]` con `Headphone Source = DAC` → el corte estaría en la conversión D/A. **Sin fundar**: si el DAC cambió (la GUI no lo vuelca, no hay línea base) |
| 16/10 ~22:15 | `3604cece0e43` (sigue) | `21a4116` | (no corrido) | (no corrido) | — | **Audio localizado por medicion** (en el #116): la **entrada tiene señal** (`rms=141.3`) y el **sink recibe ceros** (`rms=0.0`), con el stream de la GUI abierto, sin mute y al 100% → **el silencio se genera dentro de la GUI**. Descartados PulseAudio, el mixer de salida, la squelch y el volumen. Sin tocar nada |
| 16/10 ~22:05 | `3604cece0e43` (sigue) | `ab7710b` | (no corrido) | (no corrido) | — | **El owner se quedó sin audio y era un CRASH de la GUI**: `CRASHLOG sig=11 fallo=(nil)` con el contexto lleno de `CWDBG` (sale al sintonizar) → **issue #116**. La GUI pasó de pid 311 a 1119 (el daemon no cambió: no hubo reboot). Audio medido por partes: pulse RUNNING/no mute/83% y Headphone on 92% → el silencio está **antes** del mixer; `rfgain=0` (ayer 84) sin poder fundarlo. **La pila no se pudo resolver** (el binario que quedó en el volumen es de **otro build**, no el de la radio) |
| 16/10 ~21:45 | `3604cece0e43` (sigue) | `864d001` | (no corrido) | (no corrido) | — | Monitoreo del flood para completar el #114: se mide el ritmo del log cada ciclo. Radio estable |
| 16/10 ~21:35 | `3604cece0e43` (sigue) | `917604e` | (no corrido) | (no corrido) | — | **Reproducción del #114 intentada y NO lograda** (autorizada por el owner): `nb_level` 5→0, `nb_width` 5→0 y los tres juntos → el equipo **sobrevivió** (uptime continuo 448→508 s, mismo pid, CRASHLOG 0) y el NB quedó **restaurado**. **Pista**: cuando murió el log iba a **~16/s** y ahora a **~0,4/s** → la precondición parece ser el **flood del #111** (intermitente) |
| 16/10 ~21:25 | `3604cece0e43` (sigue) tras **reinicio del equipo** | `a4ba51f` | (no corrido) | (no corrido) | — | **El equipo se reinició al aplicar el NB** (uptime 50 s): **issue #114**. Sin CRASHLOG (murió por debajo de la GUI), el log del run colgado (77.020 líneas, **99% overflows**) **termina cortado en medio del flood a ~16/s**, `nb=1` con `nb_level=0`/`nb_width=0`, y el kernel tiene `panic=10` + watchdog de 16 s (explica el reboot). Reproducción **no hecha** (colgaría la radio del owner otra vez) |
| 16/10 ~21:10 | `3604cece0e43` (sin cambios) | `2e61ace` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0. #111 sigue |
| 16/10 ~20:40 | `3604cece0e43` (sin cambios) | `06cf0f7` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0. #111 sigue |
| 16/10 ~20:10 | `3604cece0e43` (sin cambios) | `7b56feb` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0. #111 sigue |
| 16/10 ~19:40 | `3604cece0e43` (sin cambios) | `1ba6f07` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0. #111 sigue |
| 16/10 ~19:10 | `3604cece0e43` (sin cambios) | `f1d37a3` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0. #111 sigue |
| 16/10 ~18:40 | `3604cece0e43` (sin cambios) | `69a6501` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0. #111 sigue |
| 16/10 ~18:15 | `3604cece0e43` (sin cambios) | `fec1c2a` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0. #111 sigue |
| 16/10 ~17:50 | `3604cece0e43` (sin cambios) | `3d6c022` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0. El owner sigue en CW 7.012. #111 sigue |
| 16/10 ~17:45 | `3604cece0e43` (sin cambios) | `7ddfa88` | (se cuelga) | (ya corrido) | **0** | **Rechequeo tras un salto de ~5,5 h** (equipo suspendido): 0 merges, 1 GUI (pid 2889 estable), **CRASHLOG 0 en los 6 últimos app_logs** (ninguno en el hueco), DATA 1% usada, load 0,92. El owner está en **CW 7.012000**. El #111 sigue: log nuevo con 3.857 overflows de 3.814 líneas |
| 16/10 ~18:45 | `3604cece0e43` (sin cambios) | `31b8d8a` | (se cuelga) | (ya corrido) | **0 en 60 s** | **#99 VALIDADO**: con el dial en 7.012 (el owner lo movió) el barrido **dibuja la curva** (17,77% no-fondo vs 17,83% del build sin #99) y se ve el valle de resonancia (~1,0-1,4 en 7,01). Cerrar no reinicia la GUI (pid 2889) y **devuelve el modo a CW** (#84) con CRASHLOG 0 |
| 16/10 ~18:40 | `3604cece0e43` (sin cambios) | `789ee5c` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0. #111 sigue; dial fuera del rango del #99 |
| 16/10 ~18:30 | `3604cece0e43` (sin cambios) | `a94d0c3` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0, load ok. #111: 20143 de 20820 overflows. Dial fuera del rango del #99 |
| 16/10 ~18:20 | `3604cece0e43` (sin cambios) | `fea20c2` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0. **#111**: 18.856 de 19.533 (96%) y **los logs no llenan el disco** (DATA 1% usado, app_logs 26 MB en 657 archivos) → el impacto es **eventos descartados + desgaste de SD**, no espacio |
| 16/10 ~18:15 | `3604cece0e43` (sin cambios) | `6da72f7` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI, CRASHLOG 0; #111 sigue; dial fuera del rango del #99 |
| 16/10 ~18:05 | `3604cece0e43` (sin cambios) | `9b36c0f` | (se cuelga) | (ya corrido) | **0 en 45 s** | Sin novedades: 0 merges, 1 GUI (pid 2889), CRASHLOG 0, load 0,62. #111: +1.463 en ~10 min (2,4/s). #99: dial 7.158, fuera de rango |
| 16/10 ~17:55 | `3604cece0e43` (sin cambios) | `026b2f3` | (se cuelga) | (ya corrido) | **0 en 60 s** | Sin novedades: 0 merges, 1 GUI (pid 2889), CRASHLOG 0. **#111**: +1.580 en ~7 min (3,8/s). **Aclaración**: los `app_logs` se **rotan por tamaño** (aparece un log guardado nuevo con el mismo pid) → los 635 archivos **no** son caídas |
| 16/10 ~17:45 | `3604cece0e43` (sin cambios) | `0dd02f2` | (se cuelga) | (ya corrido) | **0 en 60 s** | Sin novedades: 0 merges, 1 GUI (pid 2889), CRASHLOG 0, load 0,80. **#111**: +702 overflows en ~5 min (2,3/s) — el ritmo varía (18/s en la ventana de 4 min anterior) |
| 16/10 ~17:35 | `3604cece0e43` (sin cambios) | `b1d48bb` | (se cuelga) | (ya corrido) | (sin cambios) | Estable (pid 2889, CRASHLOG 0). **#111 medido en ventana larga**: +4.447 líneas en 240 s = **18/s, el 100% Overflows**, con la radio en reposo (load 0,84) → es **sostenido**, no ráfagas (corregido en el issue). ~18 eventos/s descartados y ~18 escrituras/s a la SD |
| 16/10 ~17:30 | `f88813ae85e3` (sin cambios) | `0a7479d` | (no corrido) | (ya corrido) | (sin cambios) | Estable (pid 1099, CRASHLOG 0). **#111 acumula 19.387 overflows** de 20.030 líneas y el ritmo confirmó ser **en ráfagas** (1,0 / 2,1 / 5,9 / 17,6 líneas/s según la ventana). **No pude medir la correlación con el CPU**: 3 intentos fallidos por scripts no-POSIX y `2>/dev/null` que ocultaba el error (anotado en el skill) |
| 16/10 ~17:30 | **`3604cece0e43`** | `2392624` (#110) | (se cuelga) | **23 ok** | **0 en 90 s** | **Desplegado y sano**: build exit 0 (7 s), gate OK (13.742.964 B, 72 B menos que el anterior = el fix del #110); 1 GUI; barrido 23/0; estado idéntico antes/después; CRASHLOG 0. El #110 (caja del campo de año) **no lo verifiqué** (es visual y profundo en Ajustes). El **#111 persiste** |
| 16/10 ~17:15 | `f88813ae85e3` (sin cambios) | `7a80645` | (se cuelga) | (ya corrido) | (sin cambios) | Estable (pid 1099, CRASHLOG 0). **#111 en ráfagas**: +1.873 líneas en ~15 min (**100% Overflows**, ~2,1/s de promedio) contra 17,6/s instantáneos medidos antes → el ritmo **varía mucho**, no es un goteo constante. Total acumulado: 13.477 overflows |
| 16/10 ~17:00 | `f88813ae85e3` (sin cambios) | `cd9bfb3` | (se cuelga) | (ya corrido) | **0 en 60 s** | Sano y estable: 1 GUI (pid 1099), tema `Redshift`, CRASHLOG 0. **#111 sigue activo**: 11.604 de 12.247 líneas (94%) son `Overflow`, ahora a 17,6 líneas/s. El dial está en 7.158, así que **el #99 sigue sin poder validarse** (necesita 7,0-7,03) |
| 16/10 ~16:50 | `f88813ae85e3` (sin cambios) | `f6bbfca` | (se cuelga) | (ya corrido) | (sin cambios) | **#111: causa raíz leída en el código** — la cola es de 64, la drena `event_obj_check()` **desde el hilo principal** (el que dibuja) y el productor de más ritmo es el **S-meter** (`meter.c:319`, un REFRESH por muestra) → si el hilo está saturado dibujando (~60%, #91) drena más lento y **descarta eventos**. El overflow sigue: 10.434 de 11.077 líneas (94%), ~15/s |
| 16/10 ~16:35 | `f88813ae85e3` (sin cambios) | `ff27afd` | (se cuelga) | (ya corrido) | (sin cambios) | **#111 agravado y medido**: el log vivo tiene 7.684 `Overflow` de 8.327 líneas (92%) y **crece a 15,6-17,6 líneas/s con la radio EN REPOSO** (load 0,73) y **el 100% del crecimiento son Overflows** → el disparador **no** es la carga de la UI y los eventos se descartan de forma continua |
| 16/10 ~16:30 | `f88813ae85e3` (sin cambios) | `5122ec2` | (no corrido) | **23 ok** | **0 en 60 s** | **Barrido completo** (el primero sobre este build): 23 ok / 0 caídas, estado idéntico antes/después (`7.157640 LSB vol=0`, el fix del #94 sigue), pid estable y CRASHLOG 0. Se chequeó antes que la radio estuviera en reposo (log 0 líneas en 8 s) para no molestar al owner |
| 16/10 ~16:15 | `f88813ae85e3` (sin cambios) | `e5d17cc` | (no corrido) | no se corrió | 0 tras el A/B | **A/B del tema (#91/#95)**: `Redshift` 58/61/59% vs `Aurora` 62/64/63% (rangos sin solaparse) → el tema influye ~4-5 puntos pero **no es la causa**; el CPU queda ~60% ✗. Tema **restaurado** a `Redshift`, CRASHLOG 0 |
| 16/10 ~16:00 | `f88813ae85e3` (sigue, tras reinicio del owner) | `e8082ce` (0 merges) | (no corrido) | no se corrió | **0 en 60 s** | **Rechequeo tras 15 h**: la radio se reinició (uptime 45 min, pid nuevo) pero tiene mi build, 1 GUI, `init.d=S20gui`, CRASHLOG 0. El owner puso **`Redshift`** (CPU 59-61% ✗ vs 65-70% con Aurora). **Sale el #111**: 25.624 `Overflow` de `events.c:90` en un run (los eventos se pierden) |
| 16/10 ~00:45 | **`f88813ae85e3`** | `c60af91` (#95/#91) + `aa05ef9` (#99) | (no corrido) | no se corrió | 0 en las pruebas | **#95 desplegado** (fallback de temas determinista) y **#99** (curva suavizada) — pero **no pude validar el #99**: a 7.152 la ROE sigue fuera de escala y el gráfico queda **vacío** (hace falta el rango resonante). **El CPU sigue ~65%** ✗ (#91 abierto) |
| 16/10 ~00:20 | **`da1055ddb0c3`** (tip `7032d3b`) | `9153a40` (#100) + `65cdda6` (#107) | (no corrido) | **23 ok** | **0 en 60 s** | **Desplegado y sano**: build exit 0 (13 s) con el gate OK (13.742.896 B); 1 GUI; barrido 23/0; estado idéntico antes/después; CRASHLOG 0. Incluye el asistente de sintonía (#100) y el fix del bench de CI (#107) |
| 15/10 ~23:10 | `3ead572b6fe2` (sin cambios) | `e5e6be5` | (no corrido) | no se corrió | **0 en 180 s** | Sano: 1 GUI, pid 18345 estable 3 min, CRASHLOG 0. CPU del hilo principal **68%** (en la sesión anterior medí 104-120% con el mismo build: varía). **#94 cerrado** (ya lo habían cerrado; dejé la validación igual) |
| 15/10 ~23:35 | `3ead572b6fe2` (sin cambios) | `ed6455d` | (no corrido) | no se corrió | 0 en la muestra | **#91 acotado**: el consumo está en el **hilo principal** (75,8%) y es **user-space** (58% user vs 11% sys), y **NO depende del waterfall** (tapar el waterfall: 68,8→71,0%, igual). Descartados el tema, `pre`/`att` y el kworker del 200% (transitorio mío) |
| 15/10 ~22:55 | **`3ead572b6fe2`** (build del clon con los merges) | `8d3dd73`..`45da382` | (pendiente) | **23 ok** | **0 en 60 s** | **#94 VALIDADO** (el barrido deja modo/dial/pre/att idénticos) y **#105 corregido** (no era bug del repo: copia vieja del build; se arregla borrándola). **#91 corregido dos veces**: el tema es `Aurora` (no `Sunlit`), el lavado no se reproduce y el kworker del 202% era un transitorio mío. Queda el **GUI ~1,2 núcleos** sostenido |
| 15/10 ~22:40 | **la rama NO compila** | `45da382` / `ed5b01e` | — | — | — | **#105**: `params/migrations.c:125` (generado por `src/params/CMakeLists.txt`) escribe `THEME_DEFAULT` sin comillas → exit 2 en los tres commits. **No se puede desplegar el fix del #94**. La radio queda con el build que habia en el volumen compartido. **#101 fundado** (la curva aparece en 7.012: 10,82% → **17,83%**) |
| 15/10 ~22:10 | `2c02321cc5ff` (sin cambios) | `45da382` (2 merges) | (no corrido) | no se corrió | **0 en 180 s** | **Sin desplegar**: llegó el fix de fondo del #94 (*snapshot* del estado de radio en `src/dialog.c`/`radio.c`, para todas las apps) y un ajuste de temas. Volumen ocupado (24 contenedores, su build de 00:49 UTC). Radio sana: 1 GUI, pid estable, 0 CRASHLOG. CPU ~101-103% (#91) |
| 15/10 ~21:45 | `2c02321cc5ff` (sin cambios) | `5e252df` | OK entrada (verifica se cuelga) | **23 ok** | 0 en el barrido y 0 en 90 s | **Sano**: 1 GUI, mismo pid, 0 CRASHLOG. El barrido **ya no cambia el modo/dial** (la última app, `SWRScan`, ahora los restaura por el fix del #96) → #94 no se reproduce. Sigue el CPU ~108% (#91) |
| 15/10 ~21:30 | **`2c02321cc5ff`** (build del clon) | `ed5b01e` | (pendiente) | no se corrió | 0 en las pruebas | **#84 y #85 validados** en la radio (modo/frecuencia/potencia restaurados, sin crash, pid estable). **Sale el #99**: el barrido arranca pero el gráfico queda vacío (10,82% constante a 10..50 s) |
| 15/10 ~21:10 | imagen `cb2da072b7b0` (sin cambios) | `e37db08` | (no corrido) | no se corrió | (no se corrió) | **Sin desplegar (2ª)**: el volumen sigue con el A/B del otro agente (26 contenedores, con su sello de fuentes). Se validó la herramienta nueva del #86: gate fail-closed, positivo con el firmware de la imagen y negativo con un archivo chico |
| 15/10 ~20:40 | imagen `cb2da072b7b0` (sin cambios) | `ed5b01e` (4 merges) | (no corrido) | no se corrió | 0 en 90 s | **Sin desplegar**: llegaron #96 (#84/#85, en `src/`), #97 (temas v2), #98 (gate del #86, en `tools/`) pero el volumen lo tiene el otro agente. La radio sigue sana (1 GUI, pid estable, 0 CRASHLOG) |
| 15/10 ~20:15 | imagen `cb2da072b7b0` (sin cambios) | `ad2bd88` | TODO OK | (ya corrido) | **0 en 180 s** | Radio en **192.168.0.236** (cambió de IP); 0 caídas en la vigilancia; el log de 8.388 líneas es histórico del barrido; sale **#92**: el barrido deja modo/dial cambiados. El **CPU ~99% sigue** y no es del barrido |
| 15/10 ~19:50 | imagen `cb2da072b7b0` (sin cambios) | `b78b779` | TODO OK | **23 ok** | 0 en la muestra | Sigue **#91**: CPU ~114,8% (3ª muestra: 118,8/115,2/114,8 + load 1,11) y la pantalla lavada; **fundado que el tema activo es `Sunlit`** (migrado desde `Outdoor`) por los colores de la captura |
| 15/10 ~19:40 | **imagen con #89** `cb2da072b7b0` | `d1d645d` | **TODO OK** | no se corrió | 0 en la muestra | **#89 arreglado** (1 GUI, sin kworker al 98%, entrada OK) pero sale **#91**: el GUI come ~1,2 núcleos y la pantalla se ve lavada |
| 15/10 ~19:00 | **imagen nueva en la SD** `cb2da072b7b0` | `5ebe692` (+#87) | 1 falló | **23 ok** | — | **Funciona**, pero la imagen **parpadea**: el rootfs trae DOS scripts de init (`S20gui` + `S95gui`) → dos GUIs con `/dev/fb0` abierto → flicker + el doble de CPU (**#89**). DATA y ajustes intactos; ROE no se midió |
| 15/10 ~18:45 | **imagen en la SD** (no deploy) | `5ebe692` (+ #87) | — | — | — | **Imagen armada y flasheada** (`sdcard.img` md5 `25e38412aefc…`, verificada leyendo de vuelta). La **DATA se conservó** (params.db + themes). Detalle en el anexo de imagen |
| 15/10 ~18:15 | `main` desplegado a la radio | `main` = `e1476ec` | — | — | **la GUI no arranca** | **`main` no es desplegable en el X6200**: el binario pide `libRHVoice.so.5` (del rootfs del X6100). Restaurado el port (`d9ccb7d4c224`). Comentado en #86 |
| 15/10 ~18:05 | (sin desplegar) | `main` = `e1476ec` | — | — | — | **El build de `main` en verde no produce el firmware** (776.884 B vs 13.737.784, PIE vs non-PIE, 0 simbolos x6100): issue #86. La radio no se toco |
| 15/10 ~17:50 | `d9ccb7d4c224` | `c2f88e1` | TODO OK | **23 ok** | **1 crash al cerrar el SWR Scan** (el barrido de apps, 0) | **Funciona** con dos defectos: #64, #77 y #78/#80 validados, ROE de 40 m medida (1.4–1.9); salen **#84** (queda en LSB) y **#85** (crash al cerrar con el barrido corriendo) |
| 15/10 ~17:10 | `d0ff2dba9c85` (sin cambios) | `1a56556` | TODO OK | no se corrió | **0 en 300 s** | **Sin desplegar (3ª)**: el volumen tiene 25 contenedores del arnés del otro agente; se dobló la vigilancia más larga (180 → 300 s) |
| 15/10 ~16:40 | `d0ff2dba9c85` (sin cambios) | `122bea3` | TODO OK | no se corrió | no medido | **Sin desplegar (2ª)**: quedan `4e9b5f4` (#77), `072f2c7` (#79) y `122bea3` (#80); hay builds corriendo en el volumen |
| 15/10 ~16:10 | `d0ff2dba9c85` (sin cambios) | `f6d1cb1` | TODO OK | no se corrió | 0 en 90 s | **Sin desplegar**: `4e9b5f4` (#77) queda pendiente, el volumen del build lo tiene el A/B del otro agente |
| 15/10 ~15:20 | `d0ff2dba9c85` | `337036c` (incluye `457a9a3`) | TODO OK | 22 ok | 0 en el barrido y 0 en 180 s de vigilancia | **Funciona** |
| 15/10 ~14:05 | `5a6e561e502f` | `e5a869b` | TODO OK | (no se corrió) | no medido | **Funciona** para #19/#26/#30/#47 |

---

---

## 18/10 ~12:40 (hora radio) — `ffef63df58cb` (#133/#114/#121): la radio no responde; **pruebas NO ejecutadas**

**Qué se intentó:** cerrar los tres issues del backlog con la punta del trunk (`94b9989`; el último
commit de código es `d3e84ed`, con el #130, el #132 y el comando `swrscan` del #133).

**Medido:**

```
  git fetch fork                                  -> 94b9989 (sin cambios)
  tools/compila-gui.sh                            -> exit 0, md5 ffef63df58cb (13.757.724 B)
  python3 tools/verifica-firmware-port.py <bin>   -> OK, ELF32 ARM, 3/3 marcas (gate #86, exit 0)
  tools/prueba-gui-en-radio.sh                    -> exit 1 (radio_ip no encontró la radio)
  tools/verifica-radio.sh 192.168.0.236           -> "la radio (192.168.0.236) no responde" (exit 1)
```

**La radio está inalcanzable en las dos redes** (medido a las 01:35 en 5 GHz y a las 01:36 en 2,4 GHz):

| red | host | radio | ping | SSH | hosts con :22 |
|---|---|---|---|---|---|
| DharmaNet (5 GHz) | `192.168.0.195/24` | `192.168.0.236` | **0/2** | `No route to host` | **0** |
| DharmaNet 2.4 | `192.168.18.68/24` | `192.168.18.67` | **0/2** | `No route to host` | **0** |

La red del host está sana (el gateway `192.168.0.1` responde). Es la **4.ª caída del #121**: la última
medición viva fue a las 00:13 local (uptime 51.644 s, pid 7317) y a las 00:33 ya no respondía; **no
volvió sola** y necesita ciclo de energía. En la medición de ROE del 17/10 la batería figuraba en
**8,6 V**.

**Veredicto:**

| Issue | Veredicto de esta corrida |
|---|---|
| **#133** | **NO MEDIDO**: el comando está mergeado y se había validado en `ffef63df58cb` el 17/10, pero no se repitió en esta punta. |
| **#114** | **NO MEDIDO**: no se corrieron los 16 ciclos de NB en este build. |
| **#121** | **NO MEDIDO**: no se corrieron los 5 barridos. La caída en reposo (4.ª) sigue abierta. |

No se cierra ningún issue y no se crea ninguno nuevo: la radio caída ya está reportada en el #121. No
se tocaron params, temas ni apps, y no se desplegó. Evidencia:
[`docs/evidencia/2026-10-18-qa-radio-backlog/`](evidencia/2026-10-18-qa-radio-backlog/README.md).

---

## 17/10 ~22:20 (hora radio) — `63dbf7a4` (#130): la parábola de ROE en la radio es una **U** ✓ (cierre requires-radio del #125)

**Qué se probó:** el fix del **#130** (`src/roe_curve.c`: si el ajuste libre sale con `a<0` —un domo— se
espeja al valle con la misma columna del vértice y la base en el mínimo medido). Es el `requires-radio`
que dejó abierto el #125. El criterio del owner (paso 3 del re-test del #125): **la curva dibujada es una
U** — base en el cuarto inferior (`y≥316`), columna **interior**, y **los dos bordes suben**.

**Método (medido):**

```
  clon x6200_qa → tip 63dbf7a4
  RECONSTRUIR=si tools/compila-gui.sh  → binario md5 b1ba67520079 (13.753.408 B)
  tools/prueba-gui-en-radio.sh         → gate #86: OK ELF32 ARM 13753408 B 3/3 marcas (exit 0)
  radio md5 desplegado                 → b1ba67520079  (idéntico: el deploy fue idempotente)
```

El equipo **no se reinició** (uptime continuo 34729→36509 s); la GUI se relanzó (pid 3931). Se corrió
**1 pasada (Quick)** del SWRScan con **TX a 0.1 W** (lo fuerza `radio_start_swrscan()`,
`SWRSCAN_TX_POWER_DW=1`), **ATU sin tocar** (`atu=1`, como en la captura del owner; el remoto no expone
comando de ATU) y sin XPA125B.

**Resultado — la curva es una U (medición de píxeles, banda del chart `y=69..399`):**

| Captura | dial / span / escala | valle (y) | col. valle | bordes (y) izq/der | suben (px) |
|---|---|---|---|---|---|
| `01-swrscan-50k-dial7000-U.png` | 7.000 / 50 kHz / Linear | **398** | 255 | 304 / 230 | **+94 / +168** |
| `02-swrscan-50k-dial7000-U-2da.png` | 7.000 / 50 kHz / Linear | **398** | 232 | 318 / 224 | **+80 / +174** |
| `03-swrscan-200k-log-dial7016-U.png` | 7.016,334 / 200 kHz / Log | **398** | 238 | 258 / 165 | **+140 / +233** |
| `04-swrscan-500k-log-dial7016-U.png` | 7.016,334 / 500 kHz / Log | **398** | 273 | 227 / 167 | **+171 / +231** |

Base en el cuarto inferior (`y=398 ≥ 316`) ✓, valle **interior** ✓, **ambos bordes suben** ✓ → **U**.
Reproducido en la 2ª pasada (lección del #122) y también con **span 200 kHz / Log**, que son las
condiciones exactas de la captura del domo del owner (`26-roe-multipasada-final.png`): **el domo ya no
se reproduce, la curva sale U**. Con el dial **fuera de la resonancia** (7.020/7.040) la curva es una
rama que sube (el mínimo cae en el borde), porque el rango no contiene el valle — no es una inversión.

**El script de la aserción del #125 no corría en la radio; corregido en el #132.** El re-test del #125
documenta `python3 tests/swrscan_curve_evidence.py /tmp/swrscan-u.png` (1 argumento); el script de
entonces pedía **2** (`len(argv)!=3` → docstring y **exit 2**) y con 2 argumentos la captura de radio
**igual fallaba**: `FAIL … no encontre la curva dibujada`, porque `curve_mask()` exigía
`min(r,g,b)≥180` y la curva del tema del owner (`Redshift`) es `RGB(255,179,160)` (`min=160`); el
fixture del arnés pasaba porque su curva es `RGB(248,238,255)` (`min=238`). Salida cruda de la
corrida: [`asercion-script.txt`](evidencia/2026-10-17-qa-radio-swr125/asercion-script.txt).

El **#132** (ya en el trunk) lo corrigió: acepta **1 argumento** (clasifica la forma por sí mismo:
`U` / `domo` / `rama`; el comando de radio queda `python3 tests/swrscan_curve_evidence.py
<captura>.png`) y **2** (modo histórico del arnés), y detecta la tinta **por tema** dentro de la
banda `y=69..399` (descarta la grilla `0xAAAAAA` y el fondo/panel, toma el color dominante de
texto/curva). Verificado sobre las capturas de esta corrida: `01` → **U** (valle `y=398` en `x=255`,
bordes +94/+168), `02` y `03` → **U**, y el domo viejo `26-roe-multipasada-final.png` → **domo**
medido (exit 1, ya no "no encontré la curva"). El RED/GREEN del arnés sigue **PASS**
(`03-harness-domo-valle.png` + `04-harness-valle-real.png`). El test determinista del script es
`python3 tests/test_swrscan_curve_evidence.py` (capturas reales + los seis temas sintéticos, sin
radio ni Docker).

**Barrido de apps (no regresión):** `PERMANENCIA=14 tools/prueba-apps-radio.sh` → **23/23 ok, 0 caídas**
(incluye **SWRScan**, que ahora sí abre por el control remoto; el #78 quedó resuelto). Log en
[`apps-sweep.log`](evidencia/2026-10-17-qa-radio-swr125/apps-sweep.log).

**Dos cosas que no son del fix y quedan abiertas:**

- **`Error: timeout de RF`**: **todas** las pasadas terminan en `Error: timeout de RF` con `n≈43-45/51`,
  **nunca `Completed`**. El watchdog de la pasada (`SWRSCAN_PASS_TIMEOUT_MS = 2285 ms` para 51 puntos =
  44,8 ms/punto) se dispara antes porque el tiempo real por punto supera el `SWRSCAN_DWELL_MS=35` asumido
  (TX medida ~2,3-2,6 s). Ya anotado en #119 y por el otro QA; sigue abierto.
- **SIGSEGV (1 vez, no reproducido)**: `CRASHLOG sig=11 fallo=0xf363 pc=0xb6186ae8 lr=0xb618d4f0 …` tras
  `on_vfo_mode_change: mode=2` + carga de `filter_low/high`; la GUI se relanzó (pid 2728→3931).
  **Ya reportado en el #131** (mismo evento). Un barrido dirigido de las 23 apps (abrir 6 s / cerrar /
  comparar pid) **no lo reprodujo** (pid 3931, CRASHLOG 0). Ojo: `verifica-radio` dice "sin senales de
  crash" porque el log vivo post-relanzamiento arranca limpio; el CRASHLOG está en el `app_log`
  guardado `x6200_log_2026-10-17_22-38-46.txt`.

**Restauración:** dial `7.030.000`, modo `LSB`, banda `40m CW`, `pwr` 5.0 W, app cerrada, RX;
`swrscan_span=500000`, `swrscan_linear=1`; `cw_tune=cw_decoder=key_train=0`; tema `Redshift`; **ATU sin
tocar**. md5 `b1ba67520079`. Evidencia completa:
[`docs/evidencia/2026-10-17-qa-radio-swr125/`](evidencia/2026-10-17-qa-radio-swr125/README.md).

---

---

## 16/10 ~23:40 — ROM nueva flasheada en la SD: `c640645d810c` (GUI e294229dd32d)

**Qué se hizo:** build de la ROM con todo lo mergeado (HEAD `223b30a`, incluye `fe67ded` = issue #117) y
**flasheo de la SD**, porque los dos intentos de deploy por WiFi fallaron (el script quedó colgado y el
`scp` cortó con "text file busy").

**Verificación del artefacto:**

```
  sdcard.img: 571473920 bytes  md5 c640645d810c1488682668eae62204f3
  init.d en el rootfs:  solo ./etc/init.d/S20gui        (fix del #89)
  GUI en la imagen:     13257128 bytes  md5 e294229dd32d
  lib en la imagen:     md5 5ed07c1719f8
```

**Flasheo y verificación, medido:**

```
  dd: 571473920 bytes en 118 s (4.6 MB/s)
  md5 leido de vuelta de la SD: c640645d810c1488682668eae62204f3   -> IGUAL a la imagen
```

**La DATA se conservó** (se re-declaró en el slot 3 desde el sector 1116160):

```
  /proc/partitions: sda3 3314176 bloques  (== 6628352 sectores)
  desde el mount del host: params.db 106496 bytes, 78 parametros
    theme_name=Redshift  vol=66  swrscan_span=50000  nb=0
    themes: Aurora, Holo, Material 3, Redshift, RS, Sprint, Sunlit, Trail
    app_logs: 662 archivos
```

**Nota**: el md5 de los primeros 571 MB despues del flasheo es `f1ac0dc42452fde034041319bae59dc2`, distinto
del de la imagen, **esperado**: se reescribió el MBR (offset 478) para volver a declarar la DATA.

**Pendiente**: poner la SD en la radio y validar (GUI e294229dd32d, `init.d`=`S20gui`, barrido de apps, y
si el audio arranca — el race del #116 es un bug de código).

---

## 16/10 ~17:30 — `3604cece0e43` (merge `2392624`, issue #110): el campo de año de Ajustes

**Qué se probó:** el merge `2392624` (*"Ajustes: el campo de año con la misma caja que Day/Month"*,
issue #110), que toca `src/dialog_settings.c`.

**Build y deploy, medido:**

```
$ make x6200-gui-rebuild        -> exit 0 en 7 s
$ gate: OK: es el firmware del port (ELF32 ARM, 13742964 B, 3/3 marcas)
  radio: gui=3604cece0e43 (1 GUI, pid 2889)
```

El binario pesa **72 B menos** que el anterior (`f88813ae85e3`, 13.743.036 B), consistente con un cambio
chico de layout.

**Pruebas en la radio:**

```
  barrido de las 23 apps:  23 ok | 0 caídas   (406 s)
  estado antes:   7.157640 MHz LSB 40m SSB vol=0 pre=1 att=0
  estado despues: 7.157640 MHz LSB 40m SSB vol=0 pre=1 att=0     <- IDÉNTICO (fix del #94 andando)
  vigilancia 90 s: pid 2889 sin cambios, 0 caídas
  CRASHLOG: 0 (log vivo y el ultimo guardado)
```

**No verificado**: el fix del #110 es **visual** (el ancho de la caja del campo de año en la pantalla de
fecha de Ajustes). No navegué hasta ahí, así que **no lo confirmé**: la navegación profunda de Ajustes no
la hice. Queda para el dueño del cambio o para el owner.

**El #111 sigue**: en el run nuevo, 7.739 de 8.416 lineas (92%) con la firma `Overflow`.

---

## 16/10 ~00:45 — `f88813ae85e3` (tip `c60af91`): el #95 de temas y el #99 de la curva

**Qué se probó:** los merges `c60af91` (temas: fallback determinista fijado por tests, issue #91 — *"el
supuesto estaba desmentido"*) y `aa05ef9` (SWR Scan: curva de ROE suavizada con Catmull-Rom, issue #99).

**Build y deploy, medido:**

```
$ make x6200-gui-rebuild        -> exit 0 en 6 s
$ gate: OK: es el firmware del port (ELF32 ARM, 13743036 B, 3/3 marcas)
  radio: gui=f88813ae85e3 (1 GUI, pid 25647)
```

**El #99 no se pudo validar en este dial.** El barrido de ROE a **7.152000** (donde el owner tiene la
radio) deja el gráfico **vacío**, igual que antes:

```
  7.012 (resonante, build viejo): 17.83% de pixeles no-fondo   <- con curva
  7.152 (build nuevo #99):        12.27%                       <- SIN curva
```

Ojo: ese 12,27% vs el 10,82% medido antes **no es una curva**: son los **rótulos del eje**, que ahora
dicen `7.139.500 / 7.152.000 / 7.164.500` (antes `7.155.471 / 7.167.971 / 7.180.471`) y ocupan más
pixeles. La captura `22-curva-suavizada.png` lo muestra: el chart está **vacío**.

Es consistente con el hallazgo del #101: **una ROE fuera de escala no se dibuja**, y a 7.152 la antena de
40 m está fuera de resonancia. Para validar el #99 hay que barrer dentro de **7,0-7,03**.

**El #84/#85 siguen bien** con este build: cerrar el barrido no reinicia la GUI (pid 25647 sin cambios) y
el estado queda intacto (`7.152000 MHz LSB vol=0`), con `CRASHLOG=0`.

**#91**: el fix del #95 confirma por escrito que **el supuesto del tema estaba desmentido** (lo que yo
habia medido), pero **el CPU sigue en ~65%** en el hilo principal y en user-space. El issue queda abierto
por esa parte.

## 16/10 ~00:20 — `da1055ddb0c3` (tip `7032d3b`): el asistente de sintonía (#100) desplegado

**Qué se probó:** el build del tip, que ya traía los merges `9153a40` (RX: asistente de sintonía, buscar el
mejor spot alrededor, issue #100) y `65cdda6` (bench de CI: el ruido del runner sale `INCONCLUSIVE`).

**Build y deploy, medido:**

```
$ make x6200-gui-rebuild        -> exit 0 en 13 s
$ gate: OK: es el firmware del port (ELF32 ARM, 13742896 B, 3/3 marcas)
$ tools/prueba-gui-en-radio.sh  -> GUI lista
  radio: gui=da1055ddb0c3 (1 GUI, init=1)
```

El binario pesa ~5 KB más que el anterior (`3ead572b6fe2`), consistente con la funcionalidad nueva.

**Pruebas en la radio:**

```
  barrido de las 23 apps:  23 ok | 0 caídas   (405 s)
  estado antes:  7.152000 MHz LSB 40m SSB vol=0
  estado despues: 7.152000 MHz LSB 40m SSB vol=0     <- IDÉNTICO (el fix del #94 sigue andando)
  vigilancia 60 s: pid 24361 sin cambios, 0 caídas
  CRASHLOG: 0 (log nuevo 03-19-14 y los anteriores)
```

**No medido**: el asistente de sintonía del #100 **no lo probé a fondo** (es una funcionalidad de RX; en la
captura `21-build-7032d3b.png` la pantalla principal se ve normal, pero no ejercité el asistente). Queda
para el dueño de la funcionalidad o para cuando el owner lo pruebe.

## 15/10 ~22:55 — `3ead572b6fe2` (build del clon con los merges del #94/#92): #94 validado

**Qué se probó:** el build con `cd3bc97` (restore del estado de radio por snapshot, #94) y `45da382`
(ancho de línea del waterfall, #92), compilado desde el clon y desplegado.

**El build, medido** (y un escollo que vale registrar):

```
$ make x6200-gui-rebuild        -> exit 2   (params/migrations.c:125, THEME_DEFAULT)
$ grep -n theme_name src/params/migrations.c                       # en el árbol: NADA
$ grep -n theme_name /b/build/x6200-gui-custom/src/params/migrations.c
  125: rc = sqlite3_exec(db, "UPDATE params SET val='" THEME_DEFAULT "' ...   <- copia VIEJA
$ rm -f /b/build/x6200-gui-custom/src/params/migrations.c
$ make x6200-gui-rebuild        -> exit 0 en 12 s
$ gate: OK: es el firmware del port (ELF32 ARM, 13738076 B, 3/3 marcas)
  radio: gui=3ead572b6fe2 (1 GUI)
```

O sea: **el árbol compila**; lo que rompía era una **copia vieja de una fuente** en el directorio de
build compartido, que el `make x6200-gui-rebuild` no refresca. Queda anotado como trampa del volumen
compartido (y como sugerencia en el issue #105, que se corrigió tres veces hasta dar con esto).

**#94 validado en la radio** — barrido completo de las 23 apps, midiendo antes y después:

```
  ANTES:   7.012000 MHz LSB  pre=0 att=0 pwr=50
  barrido: 23 ok | 0 caídas
  DESPUES: 7.012000 MHz LSB  pre=0 att=0 pwr=50      <- IDÉNTICO
  vigilancia 60 s: pid 15466 sin cambios
```

Salvedad honesta: la última app del barrido (orden alfabético) es `SWRScan`, que **también** restaura
por el fix del #96, así que este test **no separa** los dos mecanismos.

**#91 corregido dos veces** (el registro de correcciones es parte del valor):

1. El tema activo es **`Aurora`** (no `Sunlit`): `params.db` lo dice y la captura
   `19-tema-aurora.png` muestra el violeta oscuro de Aurora. El "lavado" era el **transitorio** de la
   migración de temas al arrancar.
2. El **kworker del 202%** que reporté **no se reproduce**: era el **arranque de la GUI** que yo mismo
   reinicié para probar el tema. Retirado.

**Lo que queda medido**: el GUI consume **~1,2 núcleos sostenidos** (5 muestras: 103,8 / 110,8 / 113,2 /
116,5 / 120,4%) **con los dos temas**, sin causa fundada.

---

## 15/10 ~21:30 — `2c02321cc5ff` (build del clon, commit `ed5b01e`): #84/#85 validados y sale el #101

**Qué se probó:** el build de los 4 merges (`01d632e` #84/#85, `ed5b01e` #86, `aa8de14` temas v2,
`2663a04` Cathode) compilado **desde el clon** y desplegado por WiFi.

**Build y deploy, medido:**

```
$ tools/compila-gui.sh          # dijo FALLO (carrera con los 26 contenedores del otro agente)
$ make x6200-gui-rebuild        # a mano: exit 0 en 11 s   <- el script escondía el error
$ gate: OK: es el firmware del port (ELF32 ARM, 13737932 B, 3/3 marcas)
$ tools/prueba-gui-en-radio.sh  # pasó el gate y subió
  radio: gui=2c02321cc5ff (1 GUI)
```

**#84 y #85 validados en el equipo** (dos corridas del barrido de ROE):

```
  abrir: ok | Run: ok | off: ok
  antes:   7.167971 MHz USB-D pwr=50
  despues: 7.167971 MHz USB-D pwr=50      <- modo, frecuencia y potencia restaurados (#84)
  pid 10809 sin cambios y CRASHLOG=0       <- no reinicia la GUI (#85)
```

Comentados en [#84](https://github.com/iariap/x6200_gui/issues/84#issuecomment-6205247712) y
[#85](https://github.com/iariap/x6200_gui/issues/85#issuecomment-6205247981).

**Lo nuevo: [#101](https://github.com/iariap/x6200_gui/issues/101)** — el barrido corre (`Stop`) pero
**el gráfico queda vacío**: los píxeles no-fondo del chart son **10,82% constantes** a los 10, 20, 30, 40
y 50 s. Dos pistas **sin fundar**: (1) con el código previo al #96 la curva aparecía (otro build, otra
banda); (2) el estado es `pre=1 att=0` y en aquella corrida figuraba `ATT` (lo dejó el barrido de apps).
Al buscar duplicados apareció el **#99** (la curva *dibujada*, con sierra), lo que **debilita la pista 1**:
quedó corregido en el issue.

**Ciclo 12 (~21:45):** barrido de las 23 apps → **23 ok, 0 caídas** (405 s) y el **modo/dial/pre/att
quedaron idénticos** (`7.167971 USB-D pre=1 att=0` antes y después): **#94 ya no se reproduce**, porque la
última app del barrido es `SWRScan` y su fix del #96 restaura frecuencia/modo/potencia. Vigilancia 90 s:
pid 10809 sin cambios, 0 caídas. Sigue el **CPU ~108%** (#91). Evidencia:
[`barrido-3-23ok-0caidas.log`](evidencia/2026-10-15-validacion-arreglos/barrido-3-23ok-0caidas.log).

---

## 15/10 ~19:00 — la imagen flasheada: `cb2da072b7b0` (commit `5ebe692`, imagen `25e38412aefc…`)

**Qué se probó:** la **imagen nueva**, arrancada desde la SD flasheada (no un deploy por WiFi).

**Arranque y datos, medido:**

```
$ ssh root@192.168.18.67 'md5sum /usr/sbin/x6200_gui; cut -d" " -f1 /proc/uptime'
  cb2da072b7b0            (el GUI de la imagen)
  30.14                   (uptime: arranque limpio)
$ df -h /mnt
  3.2G montado, 3.1G libre      (la particion DATA que volvi a declarar, montada)
$ sqlite3 /mnt/params.db ...
  theme_name=Outdoor   cw_tune|cw_decoder|key_train=0|0|0   swrscan_span=50000
```

O sea: **el flasheo y la conservación de la DATA funcionaron** (los ajustes del owner intactos).
Barrido de apps: **23 ok, 0 caídas**. `verifica-radio`: **1 comprobación falló** (no llegué a ver cuál;
el chequeo de crashes dio OK).

**Y el defecto que apareció: flickering y CPU alta (issue [#89](https://github.com/iariap/x6200_gui/issues/89)).**

Causa medida: **dos scripts de init arrancan el mismo daemon** —`/etc/init.d/S20gui` (del repo, commit
`29c5896`) y `/etc/init.d/S95gui` (**no está en el repo**: resto del rootfs del vendor)— → **dos
`x6200_daemon`** → **dos `x6200_gui`, los dos con `/dev/fb0` abierto** → parpadeo.

```
  pid 295/533  {x6200_daemon}      pid 307/542  /usr/sbin/x6200_gui  (fd 3 -> /dev/fb0)
  CPU: x6200_gui 542 ~20.7% | x6200_gui 307 ~23.3% | kworker/u8:2-events_unbound ~97.7%
  /dev/fb0: 24 lecturas -> 6 hashes distintos; entre dos cuadros distintos cambia el 10.4%
```

**Por qué nunca se vio con los deploys por WiFi:** `tools/prueba-gui-en-radio.sh` copia el binario y las
`liblv*.so*`, **no** `/etc/init.d/` → este defecto sólo aparece al arrancar una imagen flasheada.

Evidencia: `docs/evidencia/2026-10-15-validacion-arreglos/flicker/` (medición + 2 cuadros + GIF).
**No se midió la ROE** en esta corrida (se priorizó el flicker que pidió el owner).

## 15/10 ~17:50 — `d9ccb7d4c224` (commit `c2f88e1`): despliegue y validación

**Qué se hizo:** build limpio + deploy + la batería de validación que pidió el owner.

**Build y deploy, medido:**

```
$ tools/compila-gui.sh
  binario: md5 d9ccb7d4c224   (commit c2f88e1 - qa-radio: la regla del owner queda explicita)  [43 s]
$ tools/prueba-gui-en-radio.sh
  GUI lista: 5886.6 s
$ md5sum /usr/sbin/x6200_gui        # en la radio
  d9ccb7d4c224                       (la lib también viajó: liblvgl.so cd30ca6c58ad)
```

El commit incluye los tres fixes que estaban pendientes: `4e9b5f4` (#77, tipografías), `072f2c7` (#79,
Field Ops) y `122bea3` (#80, SWR Scan por socket).

**Nota de honestidad:** el volumen del build estaba ocupado por el A/B del otro agente y el owner indicó
compilar ahí mismo. Antes de compilar se **respaldaron** sus artefactos dentro del volumen
(`/b/ab-backup-237f1f6bc1da`, `/b/ab-backup-liblvgl-cd30ca6c58ad`, `/b/ab-backup-sello-8b10fe903942`).
Si su A/B estaba midiendo en ese momento, esa corrida pudo verse afectada.

**Validaciones:**

| Qué | Resultado medido |
|---|---|
| `tools/verifica-radio.sh` | **TODO OK** |
| **#64** Field Ops | **0 errores de archivo** (antes del fix: 3); los íconos cargan |
| **#78 / #80** SWR Scan | Abre por socket (`{"cmd":"app","text":"SWRScan"}` → `ok`) y el barrido corre (`btn 0` → `ok`) |
| Barrido de apps | **23 de 23 ok**, 0 caídas (antes informaba 22: faltaba justamente el SWR Scan) |
| Vigilancia 60 s | PID estable, 0 caídas |
| **ROE de 40 m** | **~1.4 a ~1.9** en 7.005.792–7.030.792 (`Span: 50 kHz`): la antena de 40 m está bien |

**La transmisión que estaba bloqueada, hecha:** 3 W (QRP, autorizado por el dueño), LSB forzado por la
app, dial 7.018.3, `swrscan_span=50000`. El log lo confirma: `on_vfo_mode_change: Radio set vfo 1
mode=0`. Evidencia:
[`10-swrscan-tx-resultado.png`](evidencia/2026-10-15-validacion-arreglos/10-swrscan-tx-resultado.png).

**Tres cosas que salieron mal en la prueba y se corrigieron:**

1. El **modo quedó en LSB** (el barrido lo fuerza y no lo devuelve): se restauró a **CW** a mano y quedó
   reportado en [#84](https://github.com/iariap/x6200_gui/issues/84).
2. El **cliente del control remoto** no viaja en el repo (`tools/remote/x6200-remote` se compila): en el
   worktree no estaba y el `scp` falló en silencio. Se copió del árbol compartido.
3. Los rótulos grises del gráfico llegan a **6.994** (unos 6 kHz por debajo de 7.000), aunque el barrido
   útil quedó entre 7.005 y 7.031.

**Corrección de lo que informé primero:** dije "0 caídas" en esta corrida y **no era cierto**: hubo **un
crash**, en el momento en que cerré la app del barrido con el barrido corriendo. Aparece en el log
guardado (`x6200_log_2026-10-15_20-42-17.txt`) y no en el log vivo, que es el que mira
`tools/verifica-radio.sh`, por eso el chequeo dio `TODO OK` igual. Firma y pila resuelta:

```
CRASHLOG sig=11 fallo=0x4 pc=0x4bf320 lr=0x4b2010 sp=0xbeda2c08 base=0x48ea64
main+0x4ac -> scheduler_work+0x28 -> main_screen_close_app+0x14 -> dialog_destruct+0x74 -> apply_cmd+0x0
```

(Resuelta con `nm -n` y el símbolo más cercano: `addr2line` no da símbolos para este binario, que tiene
symtab pero sin debug.) El barrido de las 23 apps cerró todas sin problema, así que se necesita **el
barrido corriendo**. Reportado en [#85](https://github.com/iariap/x6200_gui/issues/85). Evidencia:
[`crash-swrscan-al-cerrar.txt`](evidencia/2026-10-15-validacion-arreglos/crash-swrscan-al-cerrar.txt).

---

## 15/10 ~16:10 — corrida sin despliegue (la radio sigue en `d0ff2dba9c85`)

**Qué se hizo:** el ciclo de monitoreo. `git fetch fork` y `HEAD..fork/lu2dsx/x6200-rom` dieron **0
commits**, pero **la radio no tiene el build del tip**: el commit de fuentes `4e9b5f4` (#77,
tipografías) está en la rama y **no desplegado**.

**Por qué no se desplegó:** el volumen del build (`x6200-build-output`) lo está usando el otro agente
para su A/B de fuentes:

```
$ docker run --rm --entrypoint "" -v x6200-build-output:/b x6200-buildroot sh -c 'ls -l /b/target/usr/sbin/x6200_gui; md5sum ...; cat /b/.fuente.md5'
  -rwxr-xr-x ... Oct 15 19:04 /b/target/usr/sbin/x6200_gui     (compilado 5 minutos antes)
  md5 237f1f6bc1da
  sello .fuente.md5 = 8b10fe903942
$ (hash de mis fuentes)          = b3334bef0b7d               (distinto: el volumen es suyo)
```

Un build limpio le pisaría el binario del A/B, así que el despliegue queda **pendiente** (regla del
skill `qa-radio`).

**Salud de la radio, medido:** `tools/verifica-radio.sh` → `TODO OK`; los dos últimos logs con
`CRASHLOG=0` y `buffer overflow=0`; vigilancia de 90 s con el PID estable (`309 → 309`) y **0 caídas**.
El equipo reinició (uptime `712.97` s) y el build desplegado persistió.

**Ajustes del dueño, sin tocar:** `theme_name=IU1WXY`, `cw_tune=cw_decoder=key_train=0`,
`swrscan_span=50000`.

**Hueco del proceso que salió de esta corrida:** la idempotencia se medía contra git (`HEAD..remote`), y
eso no dice si la radio tiene el build del tip. Corregido en el skill `qa-radio`: ahora se compara el md5
**desplegado** y, antes de compilar, se mira el sello del volumen compartido.

### Segunda corrida (16:40): siguen 2 commits más y el volumen está compilando

`git fetch fork` trajo **2 commits nuevos**, y los dos son arreglos de issues de este QA:

- `072f2c7` (#79) — Field Ops: `HELPER` pasa de `"x6100-field"` a `"x6200-field"` en
  `src/dialog_field.c` (el hallazgo del #64) y los spots de POTA usan el nombre real.
- `122bea3` (#80) — Control remoto: entra `SWRScan` en **las tres tablas** de `src/remote_server.cpp`
  (`app_names[]`, `actions[]`, `dialog_apps[]`), que es exactamente lo que faltaba según el triage del
  #78. De paso el barrido de apps ahora cubre las 23.

Los dos tocan **sólo `src/`**, así que el deploy del GUI alcanza (el helper del rootfs en la radio ya
coincide con el repo: `md5 efcc0b0ce095` en los dos).

**Por qué tampoco se desplegó:** hay **builds corriendo ahora** en el volumen compartido:

```
$ docker ps --format '{{.Image}} {{.Command}}'
  x6200-buildroot:latest  "sh -c 'mkdir -p /t/…"        <- dirclean / rebuild del otro agente
  x6200-buildroot         "./entrypoint.sh sh …"
  9c4ba5849793            "./entrypoint.sh bas…"   (x3, en paralelo)
```

La hora del binario del volumen **no alcanzaba** para saberlo (marcaba 35 minutos, del último build
terminado), así que la regla del skill se amplió: además del sello `.fuente.md5`, mirar `docker ps`.
El binario del A/B que estaba en el volumen quedó **respaldado** (`/b/ab-backup-237f1f6bc1da`) para no
perderle su corrida.

**Pendiente para la próxima corrida (cuando el volumen esté libre):** build limpio + deploy de los tres
commits y después, además del chequeo general, **la prueba de TX que quedó bloqueada**: con el #80 el
SWR Scan ya se puede abrir por socket, la antena es de 40 m, el dueño autorizó QRP y `swrscan_span` está
en 50000 Hz (7.05–7.15 MHz con el dial en 7.100).

### Tercera corrida (17:10): el volumen sigue ocupado, y una vigilancia más larga

`git fetch fork` trajo **1 commit más** (`1a56556`, #81: temas v2 Redshift, issue #68). El volumen
compartido estaba **saturado** — **25 contenedores** corriendo (`x6200-uiharness` y varios
`x6200-buildroot` con `chroot` y `dirclean`) —, así que tampoco se compiló.

Como el despliegue no dependía de mí, la ventana se usó para **alargar la vigilancia de estabilidad**,
que era el pendiente más flojo del anexo B (180 s):

```
  t=0    pid=309  logs=187  uptime=4410.34s  (17:11:50)
  t=300  pid=309  logs=187  uptime=4711.57s  (17:16:51)
  caidas: 0 | PID estable: si | el equipo reinicio: no
```

Más `tools/verifica-radio.sh` → `TODO OK` y los dos últimos logs con `CRASHLOG=0`. Ajustes del dueño sin
tocar (`theme_name=IU1WXY`, CW en 0, `swrscan_span=50000`).

**Conflicto de fondo (para el owner):** el volumen `x6200-build-output` es **uno solo** y el otro agente
lo usa para sus A/B y su arnés. En tres corridas seguidas (16:10, 16:40, 17:10) no se pudo compilar sin
pisarle un build en curso. O se le da un volumen propio (`x6200-build-output-ab`) o se acuerda una
ventana; si no, el ciclo «cada commit → build + deploy + prueba» se atrasa aunque la rama avance.

## 15/10 ~15:20 — `d0ff2dba9c85` (commit `337036c`)

**Qué se probó:** el build que trae el arreglo del #53 (iconos de Field Ops), el del #63 (crash del
WSPR) y los cambios de fuentes/ECO.

**Compilación y despliegue, medido:**

```
$ tools/compila-gui.sh
  binario: md5 d0ff2dba9c85   (commit 337036c - evidencia: barrido de las 23 apps ...)
$ tools/prueba-gui-en-radio.sh
  GUI lista: 7643.1 s
$ md5sum /usr/sbin/x6200_gui     # en la radio (192.168.0.236, encontrada sola por tools/lib-radio.sh)
  d0ff2dba9c85
```

**Chequeo general:**

```
$ tools/verifica-radio.sh
  TODO OK        (assets, 6 dispositivos de entrada, sink de PulseAudio, sin crashes)
```

**Barrido de apps, medido** (`PERMANENCIA=14`, evidencia:
[`barrido-23-apps-0-caidas.log`](evidencia/2026-10-15-validacion-arreglos/barrido-23-apps-0-caidas.log)):

```
22 apps con "ok"      (abre: {"t":"ok"}, cierra: {"t":"ok"})
0 menciones de caída  ($ grep -ci "cayo" → 0)
```

Es el mejor barrido registrado: en versiones anteriores caían 5 a 8 apps por barrido (issue #19).

**Vigilancia de 3 minutos** (PID y cantidad de logs):

```
  t=0    pid=2956  logs=185
  t=180  pid=2956  logs=185   → caídas: 0
  params CW: cw_tune=0 cw_decoder=0 key_train=0
```

**Lo que se probó puntualmente, y salió bien:**

- **WSPR (#63)**, que antes reiniciaba la GUI: `{"cmd":"app","text":"WSPR"}` → `{"t":"ok"}`, el
  proceso siguió vivo (`pid=4161`) y el último log quedó con `0` CRASHLOG. El arreglo `457a9a3` sí
  está en este build: `git merge-base --is-ancestor 457a9a3 337036c` → **sí** (código/medido).
  Evidencia: [`07-wspr.png`](evidencia/2026-10-15-validacion-arreglos/07-wspr.png).
- **Field Ops** abre, pero sigue con **3 errores de archivo** (rutas `/usr/share/x6100/field/*_96.png`):
  es un defecto **abierto y reportado** en el issue
  [#64](https://github.com/iariap/x6200_gui/issues/64), no una falla de esta corrida. Evidencia:
  [`errores-field-ops.txt`](evidencia/2026-10-15-validacion-arreglos/errores-field-ops.txt) y
  [`06-field-ops.png`](evidencia/2026-10-15-validacion-arreglos/06-field-ops.png).

**Transmisión: intento, y no se pudo medir.** Con la antena de 40 m ya conectada y autorización del
dueño (QRP, 40 m) se intentó disparar el **barrido de ROE**, que es el único camino de transmisión de
las apps (`Run` → `radio_start_swrscan()` → 3 W en LSB; `codigo: src/radio.c:667-690`). **No se
transmitió**, porque la app no se puede abrir por el control remoto:

```
$ for n in SWRSCAN Swrscan swrscan "SWR Scan" SWR; do
    ssh root@radio "/tmp/x6200-remote '{\"cmd\":\"app\",\"text\":\"$n\"}'"; done
  'SWRSCAN'  -> {"t":"err","err":"Unknown app"}
  'Swrscan'  -> {"t":"err","err":"Unknown app"}
  'swrscan'  -> {"t":"err","err":"Unknown app"}
  'SWR Scan' -> {"t":"err","err":"Unknown app"}
  'SWR'      -> {"t":"err","err":"Unknown app"}
```

Que **no hubo emisión** está medido, no supuesto: el log de la radio no registra TX y el estado quedó
igual antes y después (`7.1001 MHz LSB 40m SSB pwr=50`). El defecto quedó reportado (issue
[#78](https://github.com/iariap/x6200_gui/issues/78), previa búsqueda de duplicados).

Antes de intentarlo se acotó el barrido: `swrscan_span` pasó de **200000** a **50000** Hz (el log de
arranque lo confirma: `Loaded swrscan_span=50000`), así que con el dial en 7.100 el barrido quedaría
en 7.05–7.15, dentro de 40 m. Evidencia:
[`08-swrscan-tx.png`](evidencia/2026-10-15-validacion-arreglos/08-swrscan-tx.png).

## 15/10 ~14:05 — `5a6e561e502f` (commit `e5a869b`)

**Qué se probó:** la primera compilación **limpia** (el build incremental dejaba un binario viejo:
dio `30bef0dbec2a` contra `5a6e561e502f` del limpio) y los arreglos de audio/temas.

**Medido, uno por uno:**

| Issue | Antes (medido) | Después (medido) | Evidencia |
|---|---|---|---|
| **#30** volumen | GUI en `0` con la base en `8`; el comando remoto contestaba `ok` pero no cambiaba nada | GUI `4` con la base `4`; `{"cmd":"vol","val":30}` → `{"t":"ok"}` y GUI `30` | [`01-vol-30.png`](evidencia/2026-10-15-validacion-arreglos/01-vol-30.png) (el pie muestra `Volume: 30`) |
| **#44** medidor de RS | `#ff4040`, contraste **2.72:1** | `#f08080`, contraste **3.64:1** (351 píxeles del color puro) | [`04-rs-despues.png`](evidencia/2026-10-15-validacion-arreglos/04-rs-despues.png) |
| **#51/#19** CW | `sig=6`, `buffer overflow detected` de glibc, `key_train` quedaba en `1` | ese `sig=6` no volvió a aparecer y `key_train` quedó en `0` | [`crashes-sig11.txt`](evidencia/2026-10-15-validacion-arreglos/crashes-sig11.txt) |

**Y una causa que apareció midiendo (#44):** el color del tema no llegaba a la radio porque la GUI
lee los temas de `/mnt/themes` y **sólo los copia si no existen** (`codigo: src/main.c:300-301`,
`src/theme.c:21`), así que la copia vieja en `DATA` gana. Con `/mnt/themes/RS/theme.json` corregido
a mano, el medidor pasó a `#f08080` y **3.64:1**. Quedó documentado en el issue
[#44](https://github.com/iariap/x6200_gui/issues/44).

---

## Anexo B: lo que **no** está medido

| Pendiente | Por qué no está fundado | Qué haría falta |
|---|---|---|
| **Transmisión (TX) y RF** | Intentado el 15/10 y **no medido**: la app del barrido de ROE no se puede abrir por el control remoto (issue #78), así que `radio_start_swrscan()` nunca corrió. La antena es de 40 m. | Que el remoto pueda abrir esa app; después, un barrido de ROE con la potencia al mínimo |
| **Estabilidad de horas** | La vigilancia más larga pasó a **300 s** el 15/10 17:11 (0 caídas), todavía lejos de "horas". | Dejar la radio corriendo varias horas y contar caídas |
| **El #63 con spots reales decodificados** | Se abrió WSPR y quedó viva, pero no se esperó un ciclo completo con decodificación. | Dejarla en WSPR varios ciclos y mirar el log |

**Cerrado de este anexo:** *"el barrido dice 22 y el menú tiene 23"* — medido: el que falta es el
**SWR Scan**, que no está en la lista de apps del control remoto (`src/remote_server.cpp:285-292`,
12 entradas; probadas 5 variantes del nombre, todas `Unknown app`). Ver issue
[#78](https://github.com/iariap/x6200_gui/issues/78).
