# Ownership del registro 12 — coordinador único y fail-closed

## 1. Inventario de escritores del reg 12 en esta base (`cb9ea287`)

Todas las escrituras de la lib son **RMW sobre la sombra RAM** (`x6200_control_get` + máscara +
`x6200_control_cmd`); ninguno relee el MCU.

| camino | bit(s) | llamador | ¿bajo `radio_lock`? |
|---|---|---|---|
| `x6200_control_record_set(true/false)` | `0x8` voice_rec | `src/audio.c:468/473` (`audio_play_en`), `src/radio.c:782` (init) | **no** (audio_play_en corre desde UI) |
| `x6200_control_ptt_set()` | `0x40000` iptt | `src/radio.c:1368/1372` | sí |
| `x6200_control_modem_set()` | `0x4000` | `src/radio.c:1400` | sí |
| `x6200_control_atu_tune()` | `0x2000` | `src/radio.c:262/276` | sí |
| `x6200_control_atu_set()` | `0x1000` | `src/radio.c:724` | no (observer) |
| `x6200_control_split_set()` | `0x2` | `src/radio.c:702` | no (observer) |
| `x6200_control_swrscan_set()` | `0x10` | `src/radio.c:1024/1035` | sí |
| `x6200_control_poweroff()` | `0x10000` | `src/radio.c:293` | no |
| `x6200_control_idle()` | **todos** | `src/radio.c:338` (cada ≤1000 ms) | sí, pero reenvía 216 B desde la sombra |

Consecuencias que fundan el diseño:
1. Un proceso externo que escriba el reg 12 es **pisado en ≤1 s** por `idle()`; no puede ser dueño.
2. La sombra puede quedar **falsa** (`cmd` actualiza sombra antes del ioctl y no propaga error).
3. `audio_play_en()` no es neutral: además del bit 0x8 cambia HMic/IMic (`0x3dca4/0x3dcac/0x3dcb4`
   del binario desplegado). **No se modificó.**

## 2. Cambio mínimo implementado

- `src/rx_audio_mux_guard.h/.c` (módulo nuevo): lógica pura + protocolo + FSM del guardián.
- `src/radio.c/.h` (mínimo): único punto nominal
  `rxmux_result_t radio_audio_mux_request(bool dac)` bajo `radio_lock`.
  Precondiciones: `radio_control_ok`, `state == RADIO_RX`, sin dueño extranjero del bit (la sombra
  muestra `0x8` y el coordinador no lo prendió ⇒ TTS/mensaje de voz/arranque), y **readback real
  disponible**. Si falta cualquiera ⇒ devuelve `RXMUX_BLOCKED_*` **sin escribir**.
- `radio_audio_mux_set_bus_ops()`: sólo para tests/instrumentación. **En producción nunca se registra
  ⇒ `RXMUX_BLOCKED_NO_READBACK` siempre** (camino activo OFF).
- No se tocó `audio_play_en()`, `src/audio.c`, Settings/UI, VFO, DSP, `audio_enhancement.*`, ni
  configuraciones TX.

## 3. Contrato del coordinador (estado actual)

```
radio_audio_mux_request(dac):
  if !radio_control_ok                      -> BLOCKED_NOT_READY        (sin escritura)
  lock(radio_lock)
  if state != RADIO_RX                      -> BLOCKED_TX_ACTIVE        (sin escritura)
  if foreign_owner(shadow)                  -> BLOCKED_FOREIGN_OWNER    (sin escritura)
  if !bus_ops_registered                    -> BLOCKED_NO_READBACK     (sin escritura)  <-- HOY
  real = bus.read_reg12()
  if has_tx_activity(real)                  -> BLOCKED_TX_ACTIVE        (sin escritura)
  next = (real | 0x8) o (real & ~0x8)
  write(next); back = read(); if back != next -> BLOCKED_VERIFY
  owner = dac
  unlock
```

Invariantes verificados por `tests/native/test_rx_audio_mux_guard.c`:
- sólo cambia el bit `0x8`: `(next & ~0x8) == (real & ~0x8)` (forzado en el módulo y testeado en
  ≥2009 valores + 1000 eventos interleaved);
- `dac=true` es imposible con `iptt/atu/modern/swrscan/poweroff` presentes en el valor **real**;
- readback mismatch ⇒ `BLOCKED_VERIFY` (no se asume nada, no se repite el write);
- sin readback ⇒ **nunca** se llama a `write_reg12`.

## 4. Por qué no hay dueño externo (y por qué el guardián no escribe hoy)

- El único proceso que abre `/dev/i2c-0` es `x6200_gui` (medido en la radio: pid 4716), y `idle()`
  reenvía el bloque completo cada ≤1 s. Un guardián externo que escriba mientras la GUI corre no
  tiene ownership real.
- El restore de emergencia del guardián exige: detener al escritor (SIGKILL), **verificar que el bus
  quedó libre** (proceso muerto/FD cerrado), y recién entonces `readback → clear 0x8 → write →
  readback`. Como **no hay readback real** (Fase R), el helper se queda INACTIVO y **no escribe**: se
  identificó `RXMUX_GUARD_REFUSE_NO_READBACK`.
- El supervisor `x6200_daemon` **no** se modifica: lanzar el helper hoy no agregaría seguridad
  (no puede restaurar sin readback) y el contrato pide no instalarlo antes de los gates.

## 5. Evidencia de test (host, bus mock; G1)

`tests/native/run_native_tests.sh` stage "A-prime #367" → `logs/native-tests.log`:
- 9188 comprobaciones, **0 fallos** (el stage completo, incluye G1/G2/G3);
- 2009+ valores de máscara/plan con los bits compartidos;
- 1000 eventos interleaved (idle/PTT/ATU/record/RX-TX) sin tocar bits fuera de `0x8` y sin escribir
  con TX/ATU activos;
- readback mismatch y errors de bus no habilitan ni dejan estado "asumido".

**Limitación**: es host con bus mock, no hardware. La seguridad física la sigue cerrando #363 (A4).
