# Recuperación independiente del MUX/audio — #362

**Requisito del épico:** ante `crash` de la GUI, bloqueo de PulseAudio, pérdida del stream PCM, reinicio del servicio
y transición RX/TX, un mecanismo **independiente de la GUI** debe devolver el BASE a su estado de audio normal en
**≤250 ms**, **sin tocar PTT/ATU ni otros registros compartidos**, y de forma **verificable**. Reiniciar la GUI (~4 s)
**no es suficiente**.

**Resultado:** la ROM actual **no tiene** ningún mecanismo que cumpla eso. La limitación no es la velocidad del I2C
(un write de 6 bytes es sub-milisegundo), sino que **no existe una fuente de verdad del estado del MUX ni una escritura
atómica/verificable que preserve PTT/ATU**: todo restore sería una escritura "a ciegas" sobre un registro compartido,
no verificable físicamente. Por eso el gate sigue **`BLOCKED_SAFE_MUX`** y el guardián de #367 (DRAFT) **no se habilita**.

---

## 1. Inventario de mecanismos existentes (todo lo que la ROM tiene hoy)

| Mecanismo | Qué hace | ¿Selectivo (sólo audio)? | ¿Independiente de la GUI? | ¿≤250 ms? | ¿Verificable? | Evidencia |
|---|---|---|---|---|---|---|
| `audio_play_en(false)` | `record_set(false)` + repone HMic/IMic | **sí** (bit 0x8) | **no**: corre en el proceso GUI | **sí** (ms) — pero sólo si el proceso sigue vivo y llega a ejecutarlo | sólo por sombra (no hay readback) | `src/audio.c:458-478` |
| `radio_init` al arrancar la GUI | `x6200_control_init()` resetea `all_cmd` a defaults y `record_set(false)`: saca el modo record latcheado | **no**: reescribe los 54 registros (218 B) con defaults + calibración | **no** | **no**: supervisor ≥2 s + init 1,7–1,9 s ⇒ **≈4 s** | parcial: el audio de RX vuelve (medido prior), pero sin verdad del bit | `src/radio.c:779-782`; #363 A3 (`boot.log`) |
| `x6200_daemon` (supervisor) | relanza la GUI si el proceso **termina**; backoff determinista 2/5/15/60 s | **no** (no toca audio; sólo procesos) | **no** (es el supervisor de la GUI) | **no** | **no** | `rootfs/usr/sbin/x6200_daemon:38,80-87,219-271`; `commands.log` §3.11/§3.12/§8.6 |
| `x6200_daemon` ante GUI **colgada** | nada: `wait "$child"` no vuelve si el proceso vive | — | — | — | — | `rootfs/usr/sbin/x6200_daemon:225-229` (sin heartbeat) |
| PulseAudio | puede suspender/reanudar sinks, loopbacks, etc. | **no**: no conoce el MUX del BASE | sí del proceso GUI, pero **no aplica** | — | — | `rootfs-overlay/etc/pulse/system.pa`; `btaudio.pa` |
| ALSA/códec (DAPM) | mute/activación de rutas del **códec A33** | **no**: no es el MUX del BASE | n/a | — | — | `sun8i-codec-analog.c`; `audio-route-map.md` §4 |
| Watchdog del SoC | `CONFIG_SUNXI_WATCHDOG=y`; resetea el SoC si el kernel deja de refrescar | **no**: resetea Linux entero; **el BASE no se resetea** | sí del proceso | **no**: boot completo >250 ms | **no** | `build/.../.config:2381-2404`; `commands.log` §2.2 |
| Reset del BASE por GPIO 204 | `gpioset gpiochip1 204=1/0` (probado en #244) | **no**: resetea todo el BASE (frecuencia/modo/estado interno) | sí | **no medido** | **no** (y de hecho no recuperó el audio con la GUI andando) | `HALLAZGOS §70`; DTS **no** modela ese GPIO |
| Apagado del BASE por UART | `S11radio_daemon stop`: `echo s > /dev/ttyS1` | **no**: apaga el BASE | sí (init) | — | — | `rootfs-overlay/etc/init.d/S11radio_daemon:10-13` |
| Broker/daemon de audio | **no existe** en la ROM | — | — | — | — | inventario `etc/init.d` en `commands.log` §3.1 |
| Guardián de #367 | máscara/guardián probados **bajo mocks**; p99 host 103,3 ms | sí (máscara) | diseño | *podría* | **no**: sin restore físico verificado | #367 `A_BLOCKED_READBACK`; #371 `safety-conclusion.md` |

**Conclusión del inventario:** el único restore real que existe es **reiniciar la GUI** (≈4 s, todo el register file,
no selectivo). No hay watchdog de audio ni proceso independiente que pueda restaurar nada.

---

## 2. Escenarios de fallo (qué pasa hoy, con el MUX conmutado a DAC)

| Escenario | Detección | Restore real | Tiempo | Riesgo |
|---|---|---|---|---|
| La GUI cierra por una ruta normal (fin de `audio_play_en`) | el propio código | `audio_play_en(false)` | ms | ok si el camino de off se ejecuta |
| **Crash/segfault de la GUI** con `voice_rec=1` | `x6200_daemon` ve terminar el proceso | reinicio de la GUI ⇒ `radio_init` pone `record=false` | **≈4 s** (backoff mínimo 2 s + init 1,7–1,9 s) | 4 s con el parlante en DAC (silencio de RX) y register file reescrito con defaults/calibración |
| **GUI colgada** (no muere) con `voice_rec=1` | **ninguna** (el supervisor espera `wait`) | **ninguno** hasta que muera o el usuario corte | **indefinido** | silencio latcheado; requiere power-cycle o intervención |
| **PulseAudio se traba** mientras suena audio de apps | la GUI tiene topes de 2 s en `audio_play`/init | `audio_play_en(false)` sólo si el hilo que lo pidió sigue vivo y llega al off | variable | si el hilo quedó bloqueado en PA, no hay off |
| **Pérdida del stream PCM** (captura/reproducción) | `pa_stream` error/hole | no hay relación con el MUX; el off depende del llamador | variable | si el MUX quedó en DAC, el RX no se oye |
| **Reinicio del servicio** (`S20gui restart`) | init script | `stop` (killall) + `sleep 1` + `start` + init | **≥2,8 s** | ídem crash |
| **Transición RX↔TX** | `radio_set_ptt()` escribe `iptt` en el **mismo reg 12** | no hay interlock "record off antes de PTT"; no hay chequeo de `voice_rec` | — | un restore externo concurrente puede **pisar el bit de PTT** (RMW con sombra stale) |
| **Corte de energía / muerte del SoC** | — | el BASE conserva su estado interno hasta su reset (firmware encriptado: efecto en el MUX **UNKNOWN**) | — | sin watchdog de audio |
| **Reset del BASE (GPIO 204)** | manual | reset completo del BASE (no selectivo) | no medido | resetea radio entera; probado y **no** recuperó audio con la GUI andando (#70) |

---

## 3. ¿Puede un daemon/servicio/broker/kernel/GPIO cumplir ≤250 ms? Evaluación honesta

### 3.1 La velocidad no es el cuello de botella

Un restore del bit son ~6 bytes por I2C a 100 kHz (≪1 ms de bus) + el `I2C_RDWR` (sub-ms). Con detección por
`pidfd`/señal de proceso (`SIGCHLD`, latencia de scheduling ≪10 ms) el **presupuesto de 250 ms es alcanzable en teoría**
por un proceso independiente. El problema es **qué** escribir y **cómo verificarlo**:

1. **No hay readback real** del reg 12: la única lectura disponible del registro devuelve la sombra RAM
   (`build/.../aether-x6200-control-v0.1.0/src/low/control.c:459-462`). R′ probó la lectura real y dio `R_INCONCLUSIVE`
   (`00 00 00 00` para `0x0030/0x0038`, #369). Sin verdad, un restore es "escribir y esperar".
2. **No hay comando por bit** evidenciado: se escribe el word completo (6 bytes, `send_regs`). Para no perder
   PTT (`0x40000`), ATU (`0x1000`/`0x2000`), calibración (`0x8000`/`0x100000`) ni `power_off` (`0x10000`) hay que conocer
   el valor actual de todos ellos — y la sombra puede estar desincronizada porque `cmd()` la actualiza **antes** de
   saber si el I2C falló (`…:445-456`) y `idle()` la reenvía cada ≤1 s (`…:473-481`, `src/radio.c:45,338`).
3. **Sin interlock de TX**: si el restore corre mientras hay PTT o ATU, un word stale puede deskeyear/keyear la radio
   (riesgo real: el PTT se escribe con RMW en el mismo reg, `…/src/control.c:156-166`; #363 ya documentó que no hay
   chequeo de orden).
4. **Sin verificación física**: aun logrando un write rápido, no hay forma de acreditar que el MUX volvió a BASE sin
   una sonda/instrumentación (#371 P1). Un restore "ciego" no es `RESTORE_VERIFIED`.

### 3.2 Qué cumpliría cada tipo de mecanismo (si existiera)

| Mecanismo | Detección | Restore | Bloqueos reales |
|---|---|---|---|
| **Daemon/broker en userspace** | buena (pidfd/heartbeat) | un write I2C | necesita **dueño exclusivo** del reg 12 (hoy la GUI es la dueña natural y reenvía 218 B ≤1 s), readback/atomicidad PTT-ATU y verificación física. Sin eso es un competidor peligroso, no un guardián |
| **Driver de kernel** | buena | write del driver | no agrega verdad ni atomicidad (el límite es el protocolo/firmware del BASE); riesgo de probe y de bloquear `i2c-dev` que usa la GUI |
| **GPIO** | n/a | reset del BASE (GPIO 204) | no es selectivo, resetea el equipo, no verificado para audio, riesgo en TX; no hay nodo DT que lo modele |
| **Watchdog del SoC** | si el kernel muere | reboot | no toca el BASE; boot >250 ms; no restaura el MUX |
| **PulseAudio/ALSA** | n/a | nada | no conocen el MUX del BASE |

### 3.3 Requisitos que un mecanismo válido debe cumplir (y estado)

| Req | Requisito | Estado hoy |
|---|---|---|
| R1 | Detección independiente del proceso GUI (crash **y** hang: heartbeat/pidfd) | **No existe** (`x6200_daemon` sólo detecta salida del proceso) |
| R2 | Presupuesto: detección + restore ≤250 ms, medido en la radio real | **No medido** (ni existe el mecanismo) |
| R3 | Dueño único/serializado del reg 12 (sin competir con `idle()` ni con setters de la GUI) | **No existe fuera de la GUI** |
| R4 | Escritura que preserve PTT/ATU (readback o write por bit con estado) | **No existe**: sin readback real; write = word completo |
| R5 | Interlock de TX/ATU (no restaurar durante PTT/tune) | **No existe** |
| R6 | Restore **selectivo** de audio (sólo `voice_rec`), sin reescribir register file ni calibración | Parcial: `audio_play_en(false)` es selectivo, pero depende del proceso; el reinicio de GUI **no** es selectivo y reescribe todo |
| R7 | Verificación física del retorno a BASE (MUX/PA) | **No existe**; requiere instrumentación presencial (#371 P1/P2) |
| R8 | Fallos cubiertos: crash GUI, hang, PA bloqueado, pérdida PCM, reinicio de servicio, RX↔TX | **No cubiertos** (ver tabla §2) |

---

## 4. Límites reales que el diseño debe asumir

1. **Sin fuente de verdad del MUX, no hay restore "verificado", sólo "probable".** La regla del épico exige
   restauración **independiente verificable**; el firmware encriptado y el protocolo sin readback la hacen inacreditable
   hoy.
2. **El registro 12 es compartido**: "sin tocar registros compartidos" es **imposible por construcción** con el
   protocolo conocido, porque el bit de audio vive en el mismo word que PTT/ATU. La única salida sería un comando de
   escritura por bit con ACK (no evidenciado) o firmware distinto del BASE (fuera de alcance).
3. **Aun un guardián perfecto de software necesitaría** una fase física de verificación (P1/P2 de #371) antes de
   habilitarse; no se puede declarar `RESTORE_VERIFIED` por mocks de host (el propio #367 quedó `A_BLOCKED_READBACK`).
4. **Reiniciar la GUI (~4 s) no es suficiente** y además es *no selectivo*: reescribe defaults y calibración de TX;
   nunca debe venderse como fail-safe del MUX.
5. **El watchdog del SoC no es un fail-safe de audio**: resetea Linux, no el BASE; el estado del MUX sobrevive al
   reboot (UNKNOWN hasta el próximo init de la GUI, que lo reescribe a `record=false` en `radio_init`).
6. **Hoy no hay nada que probar**: no existe el mecanismo; implementarlo y probarlo exige escribir el registro
   compartido, que es exactamente lo prohibido sin readback + restore probados (#363/#371).

---

## 5. Veredicto

**Recuperación: `BLOCKED`.** Ningún daemon/servicio/broker/kernel/GPIO de la ROM actual puede restaurar el MUX/audio
en ≤250 ms de forma independiente y verificable; el único mecanismo existente es reiniciar la GUI (~4 s, no
selectivo). Un diseño futuro **podría** cumplir el presupuesto temporal, pero seguiría bloqueado por la falta de
verdad de estado, atomicidad PTT/ATU, dueño serializado e interlock de TX, más la verificación física. Condiciones
concretas de desbloqueo en [`decision.md`](decision.md) (U1–U4).
