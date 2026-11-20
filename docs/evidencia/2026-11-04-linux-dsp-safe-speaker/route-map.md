# Mapa de ruta actual y anterior (reconstrucción exacta)

Base 681074e0; la ruta audible NO está en la base (router experimental fuera del tree). Evidencia:
`x6200_qa/docs/evidencia/2026-11-02-speaker-fasttrack/{rx-test2-repeat.md,rx-test3-ab.md}` (sha256 en
`logs/prior-evidence-sha256.txt`), #364 PR #384 (commit `ef0d2da2`, default OFF), A′ PR #368, R′ #370.

## A. Ruta audible demostrada (2026-11-02, experimental)

```
BASE audio analógico (RX nativo)
  -> Mic1 A33 -> PulseAudio source 44.1 kHz mono S16NE   [src/audio.c:57-70; audio.h:15]
  -> audio_enh_process: notch -> AF leveler -> SpeexDSP NR (441=10 ms; 882 muestras=20 ms latencia)
  -> rx_audio_live: callback copia a ring fijo 4x4410 (400 ms) -> worker separado
       prefill 2 bloques, stream de reproducción propio -> DAC del SoC
  -> MUX BASE/DAC: bit voice_rec 0x8 del registro 12 (0x0030) -> parlante interno
  -> cleanup/suspend: cerrar stream y restaurar BASE (bit 0x8 = 0)
```

- Control de la conmutación: **un único punto** `radio_audio_route_set()` en la GUI, serializado con
  `radio_thread`/`x6200_control_idle`; escritura RMW del registro 12 (sólo se cambia 0x8; se preservan
  PTT/ATU/modem/barrido/apagado). No `audio_play_en`; HMic/IMic sin tocar.
- Medición: `writes=18 drops=0 underruns=0`; operador: `SPEAKER_AUDIO_HEARD=YES`,
  `PRESET_DIFFERENCE_HEARD=YES` (Off vs SSB voice); retorno a BASE; sin TX/PTT/ATU.
- **No producción**: rama de prueba, sin restore ante crash, sin guardián, sin supervisor.

## B. Integración posterior #364 (PR #384, default OFF, NO en base)

- `src/rx_audio_live.c/.h` (rama `feat/rx-speaker-production-364`, commit `ef0d2da2`): ring 4x4410,
  stale 200 ms, prefill 2 bloques, worker 5 ms; estados BASE/PREPARE_DAC/RX_DAC/DEGRADED_BASE/
  **BASE_RESTORE_UNCONFIRMED**; backpressure 200 ms; interlocks TX/PTT/ATU/SWR/poweroff y audio
  generado; backend real **fail-closed** `mux_ready=false`; tests host 124/0, audio callback 178/0,
  build ARM exit 0. **Default OFF, sin auto-start, sin endpoint de producción.**

## C. Cadena de escritura real del registro (base actual)

```
GUI -> libaether x6200_control_cmd(reg12,next)  [refs/X6200Control/src/control.c]
    -> all_cmd.arg[12] = next (sombra RAM)       [x6200_control_cmd]
    -> send_regs() I2C /dev/i2c-0 slave 0x72: [addr=12*4=0x0030 BE][arg LE32]
    -> MCU BASE aplica (voice_rec handler FUN_08045320: reg12/reg24 + ruteo FPGA)
x6200_control_record_set(on/off): prev=x6200_control_get(12); next=prev|0x8 / prev&~0x8  (RMW)
x6200_control_idle(): reenvía los 54 registros (218 B) desde la sombra cada <=1000 ms (hilo radio)
x6200_control_get(): devuelve la sombra RAM, NO lee el MCU
```
- Dueño único real hoy: `x6200_gui` (único con `/dev/i2c-0`, medido en #363/#367).
- Readback real: **no existe** (R′ #370: lecturas 0x0030/0x0038 uniformes 00000000 ⇒ `R_INCONCLUSIVE`;
  el control 0x2000 devolvió 0x01 ⇒ framing aceptado pero datos no atribuibles).

## D. Protecciones existentes vs faltantes

| protección | existe | límite |
|---|---|---|
| RMW del bit 0x8 preservando bits compartidos | sí (SDK + A′) | depende de la sombra; no readback |
| un solo punto de conmutación intra-GUI | sí (#364/#368) | no cubre GUI colgada/exit |
| suspensión por TX/ATU/SWR/poweroff/audio generado | sí (#364) | no verificado en hardware |
| ring acotado, sin alloc/bloqueo en callback | sí (#364) | host/mocks; falta sonda física |
| restore ante suspend/cleanup | sí | sólo dentro del proceso GUI |
| restore independiente de la GUI | **no** | supervisor sólo relanza GUI (~2 s) |
| verdad del MUX (readback/árbitro) | **no** | sombra SDK no es verdad; R′ sin readback |
| guardián lanzado en producción | **no** | A′ existe pero INACTIVO, supervisor no lo lanza |
