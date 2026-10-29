# Handoff de hardware / #363 — qué falta para `A_PASS`

**Este PR es DRAFT y deja el camino activo OFF.** No hay PASS de hardware: G2 es host con bus mock y
G4 no ejecutó la etapa activa (no hay readback real). El MUX **no se activó** en la radio.

## 1. Estado de la seguridad (A′)

| pieza | estado | evidencia |
|---|---|---|
| lectura real del reg 12 | **NO EXISTE** (`R_BLOCKED_NO_HARDWARE_TRUTH`) | `readback-re.md` |
| dueño único / coordinador | implementado, **fail-closed** (sin readback no escribe) | `ownership.md` |
| guardián independiente | implementado, INACTIVO, ARM+qemu, mocks G2 ≤250 ms | `guardian-contract.md` |
| flags físicos disponibles | `x6200_flow_t.flag.tx/atu_status/sql_mute` (BASE→GUI serial) | `readback-re.md` §5 |
| MUX experimental | **OFF** (no se escribe reg 12) | G4 snapshot `logs/g4-radio-readonly.txt` |

## 2. Qué necesita #363 para volver a `A_PASS`

1. **Readback real** del registro 12 documentado (dev/addr/request/response/endianness/index/mask) o
   una fuente de verdad físicamente equivalente que mida el MUX; sin eso el restore no es verificable
   desde software.
2. Autorización y protocolo de prueba física A4: tono/fixture propio ≤ −24 dBFS por stream dedicado
   (no `audio_play_en`), conmutación transitoria ≤2 s, restore automático verificado ≤250 ms,
   instrumentación del parlante/jack o escucha humana autorizada (el owner no está hasta 12/11).
3. Verificar que el restore deja el BASE en el estado nativo (audio de RX vuelve) y que PTT/ATU/mic
   no cambiaron — con los bits compartidos preservados (el módulo ya lo garantiza en mock).
4. Repetir la corrida de #367 con el bus real (**sin** segundo dueño) cuando exista la lectura real.

## 3. Qué NO se debe hacer (recordatorio del issue)

- No activar el MUX, no tocar TX/PTT/ATU, no `audio_play_en` para RX, no HMic/IMic, no flash, no
  calibración, no I2C crudo desde un segundo proceso, no instalar helper/supervisor experimental en
  la radio antes de los gates.
- No usar `module-loopback`, LD_PRELOAD ni un segundo dueño compitiendo con `idle()` como atajo.
- No declarar PASS con `parec`, sink RUNNING o monitor del DAC.

## 4. G5 (físico) sigue en #363

`A_PASS` sólo puede salir de #363 con medición física real; este PR no lo reemplaza. #364 sigue
bloqueado; #365 depende de #364.

## 5. Radio (sólo lectura, 2026-10-29T23:03Z)

`tx=false`, `radio=RX`, LSB 7.135 MHz (el owner cambió frecuencia/volumen; no el agente), GUI/lib con
los mismos sha256 del #363, único dueño de `/dev/i2c-0` = `x6200_gui`. No se escribió el registro 12.
