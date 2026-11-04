# QA31 — Candidato H1 dirigido a reg0x34 bit0x40

Fecha: 2026-11-03. Candidato preparado **offline**. No se flasheó.

## Modificación

Base: candidato actual `x6200-108-voice-keep-rx.dec.bin`
(SHA256 `75339acd697f8f5b34f2bd69bbaac5a73119a548aa24422c7b79d7d9449cb389`).
Conserva `reg24=0x28` en `0x0804535C` (offset `0x2535D`).

H1: en `0x080439B4`, instrucción `0x080439CA`:

```text
original: 43 f0 40 03   orr r3, r3, #0x40   ; field80 |= 0x40
H1:       00 bf 00 bf   nop; nop             ; no altera field80
```

- Offset de archivo: `0x239CA`.
- Sólo se reemplaza esa instrucción de 4 bytes por dos NOP de 16 bits.
- Se conservan `reg12=0x0A`, `reg24=0x28`, `reg0x36`, `reg0x37`, `reg0x39` y filtros.
- Diff H1 vs candidato actual: **4 bytes** (`0x239CA..0x239CD`).
- Diff H1 vs oficial 1.0.8: **5 bytes** (los 4 anteriores + `0x2535D` del reg24).
- AES round-trip verificado.

## Aislamiento

`0x080439B4` tiene exactamente dos callers, ambos dentro del handler `0x08045320`:

- `0x0804537A`: `r1=1`, `r2=1` — voice_rec=1; ejecuta el ORR.
- `0x08045396`: `r1=2`, `r2=4` — segundo pase; con r5=4 no ejecuta el ORR.

No hay otros callers directos del helper en el firmware. Por tanto el NOP sólo elimina
el bit `0x40` de `reg0x34` en la configuración DAC; no cambia las rutas BASE ni el
segundo pase que escribe `reg0x37/0x39`.

## Valores esperados

| Configuración | reg0x34 | reg0x36 | reg0x37 | reg0x39 | filtros |
|---|---:|---:|---:|---:|---|
| BASE (caso 0) | 4 | 0x40 | 4 | 0x40 | 0x3B=0; 0x3C=byte[0xD0] |
| DAC actual (cc7b4aca) | field80\|0x40 (≈0x44) | field82\|0x40 (≈0x40) | field83 | field85 | 0x3B=clamp(CC); 0x3C=clamp(D0) |
| DAC H1 | **field80 (≈4)** | field82\|0x40 (≈0x40) | field83 | field85 | iguales |

`field80/82/83/85` dependen del estado de audio previo; los valores entre paréntesis
son el caso de partida conocido (4/0x40). No hay readback del FPGA/MCU.

## Artefactos

- H1 `.xgf`: `x6200-108-h1-reg34.xgf`
  SHA256 **`df1e274f9e6fe59c00c1ebe6aeec5dced3a07f398ecd04ed503380df75c5d488`**
- H1 descifrado: `x6200-108-h1-reg34.dec.bin`
  SHA256 `f6409d36fccf4c13c81460600036e52e9db5d7dc4348450d4be85b4b6fb957af`
- Tamaño 524288 B; AES round-trip exacto.
- **No autorizado y no flasheado.**

## Riesgo

- Si `reg0x34` bit0x40 es el **mute de RX**, H1 debería conservar RX.
- Si ese bit habilita la **salida DAC/MUX**, H1 podría perder la reproducción aunque el PCM
  sea aceptado por ALSA.
- No hay mapa de registros ni readback que permita afirmar cuál de las dos cosas ocurre.
- El bootloader ya aceptó la estructura de este `.xgf` con el candidato anterior; el
  checksum final sigue sin verificarse.

## Ensayo A/B/A propuesto (no ejecutado)

1. Preflight: AC estable, backups, oficial 1.0.8, control exclusivo, GUI detenida.
2. A: BASE; captura ALSA directa `arecord -D hw:0,0` (s16le/48k/2ch, 3 s).
3. B: H1 con `record_set(1)`; misma captura; `pcm_status` para PCM aceptado.
4. C: BASE; misma captura.
5. Comparar RMS/espectro; B debe conservar una fracción sustancial de A y no ser sólo ruido.
6. Abortar y volver a BASE si no hay retorno, si B es silencio persistente o si la
   comunicación BASE se pierde.
7. No declarar audio audible sin micrófono externo.

`H1_PATCH_READY = YES` offline; **requiere autorización específica del SHA256 H1**.
