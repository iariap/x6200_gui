# QA30 — Configuración FPGA/DSP de la rama DAC y candidato de mute

Fecha: 2026-11-03. BASE 1.0.8 con candidato flasheado; no se volvió a escribir el MCU.

## Escrituras reales del handler

Handler `0x08045320` (Thumb):

- Rama BASE (`voice_rec=0`): escribe `reg12=0x08`, `reg24=0x28` y llama `0x080450C8`.
- Rama DAC (`voice_rec=1`): escribe `reg12=0x0A`, `reg24=0x00` y llama
  `0x08043840`, `0x080439B4`, `0x080432A8`, `0x080439B4`, `0x080432A8`.

El candidato cambia sólo `reg24` de `0x00` a `0x28`; las demás escrituras quedan iguales.
Ningún otro caller de `0x08044470` escribe reg12/24.

## Secuencia DAC (valores/máscaras)

`0x08043840(r0, 1, 0)`:

- `field80 &= ~0x10`, `field82 &= ~0x10`.
- Escribe `reg 0x34 = field80`, `reg 0x36 = field82`.
- `0x100` es la cabecera de comando; no es un registro de audio.

`0x080439B4(r0, 1, 1)`:

- `field80 |= 0x40`, `field82 |= 0x40`.
- Escribe `reg 0x34 = field80`, `reg 0x36 = field82`.

`0x080439B4(r0, 2, 4)`:

- Escribe `reg 0x37 = field83`, `reg 0x39 = field85`.

`0x080432A8(r0, 1, clamp(byte[0xCC],0,0x5F))`:

- Escribe `reg 0x3B = clamp(byte[0xCC])`.

`0x080432A8(r0, 2, clamp(byte[0xD0],0,0x5F))`:

- Escribe `reg 0x3C = clamp(byte[0xD0])`.

## Secuencia BASE `0x080450C8`

Es un switch de rutas 0..5:

| Caso | reg0x34 | reg0x36 | reg0x37 | reg0x39 | filtros |
|---|---:|---:|---:|---:|---|
| 0 | 4 | 0x40 | 4 | 0x40 | 0x3B=0; 0x3C=byte[0xD0] |
| 1 | 0x40 | 0x40 | — | — | 0x3B=byte[0xCC] |
| 2 | 0x10 | 0x50 | — | — | 0x3B=byte[0xCE] |

La rama DAC deja `reg0x34` con el bit `0x40` puesto por `0x080439B4`; si el valor de
`field80` es 4, pasa a **0x44**. BASE usa 4 en el caso 0. `reg0x36` queda 0x40 en ambos.
`0x37/0x39` y los filtros también cambian.

**Candidato principal (no confirmado): bit 6 (`0x40`) de `reg0x34`.**
No se atribuye el mute sólo por correlación: `reg0x34` también usa 0x40 en el caso 1 del
ruteo BASE y 0x10 en el caso 2; la semántica exacta del registro no está mapeada.

## Otros usos

- Los únicos escritores de reg0x34/0x36/0x37/0x39 son `0x080439B4` (DAC) y `0x080450C8`
  (rutas BASE); no hay otras escrituras en el firmware que permitan inferir un quinto estado.
- `0x080450C8` tiene seis casos; los valores 4/0x40/0x10 indican un selector de ruta, pero
  no se conoce un valor combinado BASE+DAC.
- No se encontró una entrada RX digital independiente en kernel/DT (QA14/15): la RX llega
  por Mic1 analógico; el MCU/BASE decide el mute.

## Verificación DAC independiente

Herramienta ALSA directa `pcm_status`, `hw:0,0`, s16le/48k/2ch, 96000 frames:

- BASE: `OPEN ok`, `WRITE frames=96000`, estado `RUNNING`, `DRAIN=0`, `AVAIL=256`.
- DAC: idéntico `OPEN/WRITE/RUNNING/DRAIN=0`, con `record_set(1)` del helper.
- PCM aceptado por ALSA y transferencia completa en ambos casos.
- MUX/DAC seleccionado por el helper (`voice_rec`), sin readback del registro.
- **Audio audible: NOT_VERIFIED** (no hay micrófono externo).

Captura A/B/C (QA29) sigue mostrando B≈142 frente a A≈2086: el PCM sale por DAC, pero RX
no se conserva en la captura.

## Conclusión / próximo experimento

**SOFTWARE_ONLY_INCONCLUSIVE.** La modificación de `reg24` no es suficiente. El próximo
cambio mínimo bajo hipótesis (no generado ni flasheado):

> H1: en `0x080439B4`, omitir el `orr r3,#0x40` de `0x080439CA` para no poner el bit 6 de
> `reg0x34`, manteniendo reg12/reg24, reg0x36, reg0x37/0x39 y filtros.

Efecto esperado si el bit 0x40 es el mute de RX: `reg0x34` vuelve a 4 y RX podría conservarse;
riesgo: si el bit 0x40 habilita la salida DAC, se pierde la reproducción. No hay evidencia
suficiente para afirmar ninguna de las dos cosas.

Evidencia que falta:

1. mapa de reg0x34/0x36/0x37/0x39 y del bit 0x40;
2. confirmar si existe un estado BASE+DAC combinado;
3. readback/verificación de ejecución del parche;
4. confirmación acústica o de flujo DAC independiente.

No se hizo barrido de registros ni escritura FPGA desconocida; no se generó firmware nuevo.
