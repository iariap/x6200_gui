# Decisión — #362 (auditoría ROM Linux completa para audio RX)

**Pregunta:** ¿la ROM Linux completa que compilamos permite reproducir audio RX procesado por SpeexDSP en el parlante
del X6200 de forma segura?

## Decisión

> ### `BLOCKED_SAFE_MUX` — **NO_GO** para la ruta Linux→parlante.
>
> La ROM **captura** el RX y **procesa** con SpeexDSP (medido en banco, #351), pero **no tiene una ruta segura al
> parlante**: el único camino pasa por el MUX del BASE, seleccionado por `voice_rec` (bit `0x8` del registro 12), y ese
> registro compartido **no tiene readback real, no tiene dueño serializado fuera de la GUI y contiene PTT/ATU**.
> Tampoco existe un restore independiente ≤250 ms verificable. Se mantiene `GATE_A = BLOCKED_SAFE_MUX` (#363) y
> **#364/#365 no arrancan**.

**Veredictos por componente:**

| Componente / gate | Veredicto | Razón |
|---|---|---|
| Ruta Linux→parlante (objetivo del épico) | **BLOCKED / NO_GO** | requiere `voice_rec`; sin verdad de estado ni restore verificado |
| Ruteo Linux **sin** reg 12 (alternativa a) | **NO_GO** | no existe camino audible al parlante; el DAC/HP queda detrás del MUX; S/P es salida del mismo MUX; BT/USB no son el parlante |
| Coordinación del MUX desde Linux (alternativa b) | **BLOCKED / NO_GO** | faltan R1–R6 de `alternatives.md §b.3`: readback, dueño serializado, atomicidad PTT/ATU, detección independiente, restore físico verificado, interlock de TX |
| A2DP como validación audible (alternativa c) | **GO acotado** (no resuelve el objetivo) | usa componentes ya presentes y medidos (13/10, `HALLAZGOS §25`), no escribe el reg 12 ni conmuta el MUX; suena en auriculares BT, no en el parlante, y hoy el loopback es pre-SpeexDSP |
| A2DP con stream procesado dedicado (c+) | **GO condicionado** a aprobación/implementación acotada | sólo GUI/PulseAudio; no toca el BASE; requiere un cambio de GUI no incluido en esta fase |
| Validación offline host (d) | **GO** | tests nativos y banco #351 disponibles; no acredita audio físico |
| `AUDIO_OUTPUT_VERIFIED` (#371) | **NO** | no hay medición física en esta fase |
| `MUX_SELECT_PHYSICALLY_VERIFIED` (#371) | **NO** | reg 12 sin readback real (`R_INCONCLUSIVE`) |
| `RESTORE_VERIFIED` (#371) | **NO** | no hay mecanismo independiente; reiniciar GUI ≳4 s no es restore |
| #367 guardián (DRAFT #368) | **no habilitar** | `A_BLOCKED_READBACK`; sin restore físico verificado |
| #364 / #365 | **no iniciar** | dependen de `GATE_A=PASS`, que no existe |

---

## Condiciones objetivas de desbloqueo (todas necesarias)

Si y sólo si se cumplen **U1–U4** (aportadas de forma externa/verificable, o por una fase física autorizada) se podría
revisar el gate. Ninguna se cumple hoy.

| # | Condición | Cómo se acreditaría | Estado |
|---|---|---|---|
| **U1** | **Fuente de verdad del estado del MUX**: readback real del reg 12 (no la sombra RAM) o equivalente instrumentado | documentación OEM que especifique la ventana de lectura con semántica conocida y contrastable contra un valor conocido (como falló R′), o sonda externa no invasiva del MUX (#371 P1) | **AUSENTE** |
| **U2** | **Dueño serializado del reg 12** con interlock de TX/ATU y sin competir con el `idle()` (218 B/≤1 s) ni con los setters de la GUI | diseño que centralice las escrituras y demuestre bajo fallos que no se pierde PTT/ATU (hoy: solo la GUI, RMW sobre sombra, `cmd()` sombra-antes-de-ioctl) | **AUSENTE** |
| **U3** | **Restore independiente y verificable ≤250 ms** ante crash/hang de GUI, PA bloqueado, pérdida de PCM, reinicio de servicio y transición RX/TX, preservando PTT/ATU | mecanismo nuevo (broker/kernel/otro) + inyección de fallos + medición en la radio real del peor caso ≤250 ms + verificación física del retorno a BASE | **AUSENTE** (hoy: reinicio de GUI ≈4 s, no selectivo) |
| **U4** | **Verificación física** del cambio de MUX y de la restauración (DAC→parlante y vuelta a BASE), con instrumentación y operador | fase presencial #371 P1/P2 (osciloscopio/sonda/analizador según `#371 equipment.md`), registro de señales y hashes | **AUSENTE** |

**Nota de seguridad:** U1 y U2 **no se pueden suplir con software**. El firmware del MCU/FPGA del BASE está encriptado
(`.xgf`, sha256 `dca90387…12f3`), no expone estado y no tiene comando de escritura por bit evidenciado. Cualquier
"restore" sin U1/U2 es una escritura ciega sobre un registro que también comanda **PTT** y **ATU**.

---

## Propuesta mínima si hay vía segura

**Hoy no hay vía segura al parlante**, así que la propuesta mínima es **no implementar la ruta al parlante** y hacer
sólo lo que no toca el BASE:

1. **Validación audible A2DP del stream procesado (c+)** — recomendada como próximo paso de bajo riesgo, sujeta a
   aprobación del Owner y a un issue propio (esta auditoría **no** abre issues):
   - la GUI escribe el PCM post-`audio_enh_process()` en un stream dedicado (null-sink `x6200_enh`) cuando hay
     auriculares BT; `btaudio-ctl`/operador agrega `module-loopback source=x6200_enh.monitor sink=bluez_sink…`;
   - no se toca ningún registro del BASE (ni `voice_rec`, ni `rxvol`, ni `spmode`);
   - validación: escucha + `parec` del monitor con hashes/latencia; el parlante del equipo sigue con su RX intacto;
   - **no acredita** el parlante ni el MUX; sirve como validación de calidad y como demostración de que el pipeline
     Linux completo (captura → SpeexDSP → sink) funciona de punta a punta.
2. **Documentar/registrar** esta auditoría como evidencia del épico (este PR) y **mantener** `BLOCKED_SAFE_MUX`,
   #364/#365 y el guardián #367 sin habilitar.
3. **No** escribir I2C, no conmutar el MUX, no deployar, no reiniciar la GUI por esta fase (no se hizo).

**Si en una fase futura se cumplen U1–U4**, el orden mínimo sería: (i) demostrar readback/verdad de estado,
(ii) implementar dueño serializado + interlock en el lado Linux (o documentar que el protocolo no lo permite),
(iii) demostrar restore ≤250 ms con inyección de fallos y verificación física, (iv) sólo entonces re-evaluar
#363/#367 y autorizar #364/#365. Nada de eso está autorizado hoy.

---

## Qué NO significa esta decisión

- **No** significa que el pipeline Linux de audio esté roto: la captura y el SpeexDSP funcionan/están medidos (#347,
  #351) y la GUI los usa para decoders/recorder.
- **No** significa que "no se puede hacer nunca": significa que **hoy no se puede hacer de forma segura y verificable
  con lo que la ROM controla**; la raíz está del lado del firmware del BASE (o de la instrumentación física).
- **No** acredita `AUDIO_OUTPUT_VERIFIED`, `MUX_SELECT_PHYSICALLY_VERIFIED` ni `RESTORE_VERIFIED`: los tres siguen
  **NO**. Un sink `RUNNING`, un `parec`, un A2DP audible o un QEMU no cambian nada de eso.

## Frase para citar en el PR / handoff

> “La ROM Linux completa captura y procesa el RX, pero su parlante depende de un MUX del MCU/FPGA del BASE sin readback
> real, sin dueño serializado fuera de la GUI y sin restore independiente verificado: por eso **`BLOCKED_SAFE_MUX`**
> sigue siendo la decisión correcta, y la única validación audible que no toca el equipo es A2DP.”
