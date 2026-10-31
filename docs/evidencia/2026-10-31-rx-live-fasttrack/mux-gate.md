# Gate del MUX — estado verificado en esta unidad

## A. Verdad del estado físico — **BLOCKED_NO_HARDWARE_TRUTH**

No existe una lectura física verificable del reg 12/MUX:

- `x6200_control_get()` devuelve la **sombra RAM**; `x6200_control_idle()` reenvía 216 B desde la
  sombra cada ≤1 s.
- #363/PR #366 (merged) documentó `BLOCKED_SAFE_MUX`.
- #369/PR #370 (`R_INCONCLUSIVE`): el último sondeo read-only del reg 12 devolvió `00000000` en las
  direcciones probadas; no se acreditó readback. No se repite el sondeo.
- La RE del BASE 1.0.8 no encontró ACK/readback ni mecanismo independiente de restore; el MCU está
  opaco/encapsulado.
- El S-meter/telemetría y la sombra no son evidencia física del MUX.

## B. Dueño único del registro compartido — **PARTIAL / offline**

PR #368 contiene:

- módulo puro `rxmux_*` (plan/restore/máscara sólo del bit `0x8`, preserva PTT/ATU/modem);
- coordinador serializado bajo `radio_lock` (`radio_audio_mux_request`);
- guardián IPC/FSM, lease ≤100 ms y presupuesto 250 ms, con tests host de 9188 comprobaciones.

Pero PR #368 está **DRAFT, CONFLICTING** contra `main`, no se mergeó y su guardián está OFF. En el
main actual no existe `radio_audio_mux_request`. Por eso el router nuevo usa un backend MUX
fail-closed y no hay segundo escritor.

## C. Restauración independiente — **BLOCKED**

No hay restore físico verificado en ≤250 ms:

- el único restore conocido es reiniciar la GUI/daemon (~4 s), que reescribe el register file;
- el guardián de PR #368 sólo se probó con bus mock y escritor detenido por SIGKILL;
- no hay fuente de verdad posterior que confirme BASE, ni sonda/estado externo del MUX.

## D. Prueba física — **NOT_EXECUTED**

No se conmuta el MUX, no se reproduce audio Linux por el parlante y no se toca TX/PTT/ATU.

## Decisión

`BLOCKED_SAFE_MUX`. El router queda inhabilitado por construcción (`mux_ready=false`). Para avanzar:

1. fuente de verdad física independiente del estado MUX;
2. coordinador único integrado desde PR #368 (rebased) con contrato de audio generado/TX;
3. restore independiente medido en hardware con peor caso ≤250 ms y verificación posterior;
4. recién entonces reemplazar el backend fail-closed del router y habilitar el experimento D.

El desarrollo offline del router NO reemplaza estos gates.
