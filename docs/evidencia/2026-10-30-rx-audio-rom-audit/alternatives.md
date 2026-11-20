# Alternativas reales — #362 (ROM Linux completa)

**Objetivo evaluado:** que el audio RX capturado por Linux y procesado por SpeexDSP se reproduzca en el **parlante
del X6200** de forma **segura** (sin poner en riesgo radio, MUX, PTT/ATU ni dejarla sorda tras un fallo).

**Resultado:** **no existe una alternativa que cumpla el objetivo completo con la ROM actual.** Sí existen dos
caminos de **validación** que no tocan el parlante/MUX y que se pueden hacer sin riesgo: (c) A2DP BT y validación
offline. Esta sección evalúa en el orden pedido: (a) ruteo Linux sin reg 12, (b) coordinación del MUX con restore
independiente verificable, (c) A2DP como validación audible.

Criterios usados: **P** = ¿llega al parlante del X6200? · **S** = ¿es seguro (no escribe MUX/PTT/ATU, falla sin dejar
el audio del equipo tomado)? · **V** = ¿es verificable (se puede acreditar con evidencia)? · **E** = ¿existe/hay
componentes en la ROM hoy?

---

## (a) Ruteo seguro con componentes Linux existentes **sin** escribir el registro 12

Se revisaron **todas** las salidas de audio disponibles en la ROM compilada:

| # | Camino candidato | ¿Existe en la ROM? | ¿Llega al parlante? | Evidencia / límite | Veredicto |
|---|---|---|---|---|---|
| a1 | **DAC del SoC por el stream de reproducción existente** (`play_stm` → AIF1 DA0 → DAC → mixer → HP) | Sí: la cadena ALSA/PulseAudio está armada y medida (`sink RUNNING`, #363 A0) | **Depende del MUX del BASE** (que hoy está en audio de RX); no hay by-pass Linux | La salida del códec termina en los pines HP y de ahí en el MUX del BASE (`audio-route-map.md` §2); `speaker-test` sin sonido con la GUI parada (12/10, sin captura conservada) es la única observación previa y no se verificó. No hay driver/DT/GPIO de un amplificador o switch Linux | **NO** para el objetivo (no es seguro/verificable) |
| a2 | **Conector S/P 3,5 mm** (AUDIO/NC/GND) | Es una **salida** del BASE | Es la misma salida del MUX/PA del BASE; **no** es una entrada por donde inyectar audio Linux | Manual X6200 Rev.05 y #371 `wiring.md`: el S/P es salida; inyectar audio no está permitido ni cableado | **NO** |
| a3 | **A2DP Bluetooth** | Sí: `module-loopback`/bluez5 + `btaudio-ctl rx on` (medido 13/10) | **No**: suena en los auriculares BT | No toca el BASE; el loopback toma la captura ALSA **pre-SpeexDSP** (`rootfs/usr/sbin/btaudio-ctl:23,80-116`) | **Sí, como validación** (ver (c)); **NO** para el parlante |
| a4 | **USB audio host** (`CONFIG_SND_USB_AUDIO=m`; DTS `dr_mode = "host"`) | Sí en kernel/DT, **sin** hardware | **No** (suena en un DAC USB externo) | `build/build/linux-6.1.82/.config` §SND_USB_AUDIO; `sun8i-r16-x6200.dts:540-543`; requeriría DAC USB + adaptador | **Sí, como validación** con hardware; **NO** para el parlante |
| a5 | **Sinks virtuales** (`module-null-sink`, `snd-aloop=m`, `snd-dummy=m`) | Sí (módulos presentes en rootfs/kernel) | **No** (no hay transductor) | `commands.log` §3.2/§2.1; utilidad: validar pipeline y latencias sin audio físico | **Sí, offline**; **NO** para el parlante |
| a6 | **Line-in/line-out del BASE** (reg 20 `ling_loutg_imicg_hmicg`) | Sí, la lib lo expone | **Desconocido**: no hay evidencia de que el BASE inyecte line-in al camino de RX/parlante; usarlo implica escribir URLs del register file opaco | `refs/X6200Control/.../control.h:85-86`; `src/radio.c:793-794`; **UNKNOWN** su ruteo físico | **NO** (UNKNOWN, sin evidencia) |

**Conclusión (a):** **no hay ninguna ruta Linux→parlante que evite el registro 12.** El único componente Linux que hoy
"habla" con el camino del parlante es `audio_play_en(true)` (`src/audio.c:458-478`), que **escribe exactamente el bit
`voice_rec`** del registro compartido — es decir, no cumple la condición "sin reg 12". Todo lo demás o no llega al
parlante o es una variante de validación.

> **No se declara la ruta Linux→parlante implementada/validada.** `audio_play_en()` se usa hoy para voz/recorder/TX,
> no para el RX procesado, y su seguridad como ruta de audio de recepción es justamente lo que #363 bloqueó.

---

## (b) Coordinación del MUX desde Linux con restauración independiente verificable

### b.1 Qué exige el objetivo (requisitos duros)

| Req | Requisito | Estado hoy | Evidencia |
|---|---|---|---|
| **B-R1** | Fuente de verdad del estado del MUX (readback real o equivalente) | **Ausente**: `get()` es sombra RAM; R′ devolvió `00 00 00 00` en `0x0030/0x0038` | `build/…/src/low/control.c:459-462`; #369 `R_INCONCLUSIVE`; `commands.log` §5.1/§6.2 |
| **B-R2** | Dueño serializado del reg 12 (un solo escritor; la GUI hoy es dueña de `/dev/i2c-0` y reenvía 218 B ≤1 s) | **Ausente fuera de la GUI** | `src/radio.c:45,338`; `…/control.c:473-481`; #363 A2 |
| **B-R3** | Escritura segura que no pierda PTT/ATU (mismo word: `iptt=0x40000`, `atue=0x1000`, `atu_tune=0x2000`, `power_off=0x10000`) | **Ausente**: no hay comando por bit ni readback; RMW sobre sombra desincronizable; una escritura externa puede keyear/deskeyear la radio | `…/src/control.c:69-81,156-166`; `refs/X6200Control/…/low/control.h:108-125` |
| **B-R4** | Detección de fallo independiente del proceso GUI (crash/hang) y restore **≤250 ms** | **Ausente**: ver `recovery-analysis.md` (GUI ≳4 s; sin broker; watchdog SoC; GPIO invasivo) | `recovery-analysis.md` |
| **B-R5** | Verificación física del retorno a BASE y del cambio de MUX | **Ausente**: requiere instrumentación presencial (#371 P1/P2) | #371 `safety-conclusion.md` |
| **B-R6** | Interlock de TX (no conmutar/restaurar durante PTT/ATU) | **Ausente**: el bit de PTT vive en el mismo registro y no hay interlock en el protocolo | `…/src/control.c:156-166` |

### b.2 Arquitecturas posibles y por qué no alcanzan

| Arquitectura | Cómo funcionaría | Qué resolvería | Por qué NO alcanza hoy |
|---|---|---|---|
| **b.2.1 Dentro de la GUI** (el camino de #364/#365) | La GUI, único escritor actual, agrega la ruta procesada al parlante con `audio_play_en(true/false)` y restaura al salir | Dueño único natural; no compite con nadie | No hay readback ni verdad del MUX (B-R1), ni verify físico (B-R5); el restore depende de que el proceso siga vivo; un crash deja el MUX en DAC sin restore ⇒ exactamente A2/A3 de #363 = **BLOCKED_SAFE_MUX** |
| **b.2.2 Broker/daemon externo dueño del reg 12** | Un servicio nuevo (o `x6200_daemon` extendido) centraliza TODAS las escrituras del reg 12; la GUI le delega; el broker hace watchdog y restore | Podría restaurar más rápido que reiniciar la GUI (en teoría) | Requiere readback real (B-R1), atomicidad conservando PTT/ATU (B-R3), handoff sin carreras con `idle()`/setters de la GUI (B-R2) y verificación física (B-R5). Ninguno existe. Peor: cualquier RMW con sombra stale puede **pisar PTT** ⇒ riesgo de TX no comandado. **NO-GO** |
| **b.2.3 Driver de kernel para el BASE** | Un driver de plataforma/i2c que modele el reg 12 y exponga una API segura; la GUI deja de usar `i2c-dev` crudo | Serialización a nivel kernel, posibles notificaciones | No crea readback ni verdad física (el MCU sigue sin devolver estado); el problema B-R1/B-R3/B-R5 es del **protocolo/firmware del BASE**, no de quién escribe en Linux. Añade riesgo de probe en el bus y de bloquear `i2c-dev` que usa la lib. **NO-GO** |
| **b.2.4 Cambiar el firmware del MCU/FPGA** | Reimplementar el MUX con protocolo con estado/ACK y restore por bit | Resolvería la raíz | **Fuera de alcance e imposible con los medios actuales**: firmware encriptado, sin fuente ni documentación (#362/`linux-vs-base.md`); no se toca firmware del BASE |

### b.3 Veredicto (b)

**`BLOCKED_SAFE_MUX` / NO_GO.** La coordinación del MUX desde Linux **podría** ser segura sólo si existiera una fuente
de verdad independiente del estado físico y una escritura atómica/verificable que no toque PTT/ATU, más un restore
físicamente verificado ≤250 ms. Las cuatro cosas están ausentes y dependen del firmware del BASE o de instrumentación
física, no de software que podamos escribir hoy.

---

## (c) A2DP Bluetooth **sólo como validación audible**, manteniendo el parlante BASE

### c.1 Lo que ya existe y está medido

| Pieza | Evidencia | Estado |
|---|---|---|
| `btaudio-ctl rx on` = `module-loopback source=alsa_input.platform-sound.stereo-fallback sink=bluez_sink.<MAC>.a2dp_sink` | `rootfs/usr/sbin/btaudio-ctl:23,68,80-116` | **CODE** |
| PulseAudio 16.1 con `module-loopback.so`, `module-null-sink.so`, módulos bluez5 | `build/target/usr/lib/pulseaudio/modules/` | **CODE** (rootfs compilado) |
| Política A2DP-only: sin autoswitch, sin HFP/HSP, sin mover el sink por defecto | `rootfs/etc/pulse/system.pa.d/btaudio.pa:1-18` | **CODE** |
| A2DP real, `loop=1`, auriculares Pixel Buds Pro 2, volumen del sink movido por la perilla | `HALLAZGOS §25` (13/10, evidencia previa) | **MEASURED-PRIOR** |
| Mute del parlante mientras hay auriculares vía `rxvol=0` (no MUX) | `src/btaudio.c:133-141` + `src/radio.c:869-873`; `HALLAZGOS §25` | **CODE** + medido prior |

### c.2 Límite central

El loopback toma la **captura ALSA cruda** (la misma que la GUI lee antes de `audio_enh_process()`), así que **no valida
el stream procesado por SpeexDSP**. Para validar *el procesado* de forma audible hace falta que el PCM post-enhance
llegue a un sink: eso **no está implementado** (la GUI sólo escribe `play_stm` con audio de apps/TX).

### c.3 Propuesta mínima de validación (no implementada en esta fase)

1. La GUI escribe el PCM **después** de `audio_enh_process()` (mismo buffer de `capture_deliver`) en un **stream de
   reproducción dedicado**, por ejemplo hacia un `module-null-sink` (`x6200_enh`) cuando hay auriculares BT conectados.
   No toca el stream compartido `play_stm` (que manda TX: voz/FT8/JS8 — `HALLAZGOS §17/§25` advierten no moverlo).
2. `btaudio-ctl` (o el operador) agrega un `module-loopback source=x6200_enh.monitor sink=bluez_sink…` para escuchar el
   procesado en los auriculares.
3. Validación: escucha con operador + `parec` del monitor del null-sink para hashes/latencia (procesado real), y
   `btaudio-ctl list` para el estado A2DP. **No** se toca ningún registro del BASE ni el MUX.
4. Criterio de éxito: audio procesado audible en BT, sin cambios en el parlante del equipo (que sigue con su RX), sin
   TX/PTT/ATU, y hash del monitor reproducible para un mismo estímulo. Criterio de aborto: cualquier pérdida de audio
   del equipo, inestabilidad o intervención de PTT/ATU.

Esto **no acredita** el parlante ni el MUX; acredita que el pipeline de procesamiento es audible/correcto por un sink
Linux. Es una validación de **calidad**, no de **ruteo físico**.

### c.4 Veredicto (c)

**GO acotado** como validación audible/offline: usa componentes que ya están en la ROM, no escribe el registro 12, no
conmuta el MUX y un fallo no deja el parlante tomado. **No resuelve el objetivo** (parlante del X6200) y no habilita
#364/#365. Requiere una decisión/issue del Owner para el cambio de GUI propuesto (esta auditoría no abre issues).

---

## Comparación final y qué alternativa queda

| Alternativa | Llega al parlante (P) | Segura (S) | Verificable (V) | Existe hoy (E) | Veredicto |
|---|---|---|---|---|---|
| (a) ruteo Linux sin reg 12 | **No** | — | — | Parcial (DAC/BT/USB) | **NO_GO** para el objetivo |
| (b) coordinación del MUX + restore independiente | Sí (si se hiciera) | **No demostrable** | **No** | **No** | **BLOCKED_SAFE_MUX / NO_GO** |
| (c) A2DP como validación audible | **No** (auriculares) | **Sí** (no toca BASE/MUX) | **Sí** (hash/latencia/escucha) | **Sí** (pre-Speex medido) | **GO acotado** (validación, no solución) |
| (c+) A2DP con stream procesado dedicado | **No** (auriculares) | **Sí** (sólo GUI/PulseAudio) | **Sí** | **No** (propuesta) | **GO condicionado** a aprobación e implementación acotada |
| (d) validación offline host (tests nativos + banco #351) | No | Sí | Sí | Sí | **GO** (ya disponible) |
| (e) parlante del X6200 | Sí | **No** | **No** | **No** | **NO_GO / BLOCKED** |

**Si no hay alternativa (para el objetivo):** no la hay, y se documenta: **la ROM Linux completa no ofrece hoy una
ruta segura al parlante del X6200 para el RX procesado**. Las únicas acciones que se pueden hacer sin riesgo son
validaciones por sinks Linux (A2DP/USB/offline) que no acreditan el parlante. Cualquier paso hacia el parlante exige
primero las condiciones de [`decision.md`](decision.md).
