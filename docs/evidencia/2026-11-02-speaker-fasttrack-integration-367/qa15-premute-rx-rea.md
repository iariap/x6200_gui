# QA15 — Búsqueda de una fuente RX pre-mute para Linux

Fecha: 2026-11-03. Base PR #385 `eb17ec86`; GUI main `681074e0`; Buildroot main `745af77e`.
Sin cambios de kernel, firmware BASE, audio, MUX ni hardware.

## 1. Topología reconstruida (con marcas de evidencia)

```
                    X6200 BASE / placa
  RF → demodulador ──► audio RX analógico
                         │
                         ├──► PA / parlante nativo                         [E14 HYPOTHESIS]
                         │
                         └──► MIC1? ─► ADC Mixer ─► ADC ─► AIF1 ADC
                                      (sync codec)        │
                                                         └──► Linux capture
                                                              `alsa_input...stereo-fallback`
                                                              (BASE: RX útil)

  Linux playback ──► AIF1 DA0 ──► codec DAC ──► PA / parlante            [MEASURED QA11/12]
                                      │
                                      └──► acoplamiento a MIC1/ADC        [MEASURED QA14:
                                           (tone in capture; volume0 no lo quita)
  voice_rec=1 (reg12 0x8): el BASE deja de entregar RX útil a la captura  [MEASURED QA14]
```

- `E14` (RX demod → parlante) y `E15` (codec DA0 → BASE/parlante) siguen sin esquema;
  no se declaran conocidos.
- El codec sólo expone **AIF1** (captura+playback), **MIC1** como fuente activa del ADC,
  **Mic2/LineIn** sin señal útil y los loops digitales apagados (QA13).
- El acoplamiento DAC→captura persiste con volumen 0 y no desaparece al apagar Mic1 sin perder
  toda la captura (QA13/14).

## 2. Evidencia por componente

| Componente | Evidencia | Resultado |
|---|---|---|
| Kernel desplegado | `uname -a` en radio: Linux 6.1.82, armv7l; DT `xiegu,x6200`/`allwinner,sun8i-r16` | único PCM AIF1 |
| DTS main | `br2_external/board/X6200/dts/sun8i-r16-x6200.dts` `&sound`: AIF1 Slot0 ↔ Left/Right DAC; ADC ↔ AIF1 Slot0 ADC | sin AIF2/3 ni entrada RX digital |
| Driver ASoC | `build/build/linux-6.1.82/sound/soc/sunxi/sun8i-codec.c`: `AIF1 Slot 0 Digital ADC Capture` (loopback AIF1 DA0) y AIF2/AIF3 off; `AIF1 Data Digital ADC Capture` es el camino ADC normal | sin loop digital activo |
| Driver analog | `sun8i-codec-analog.c`: ADC Mixer ← Mic1, Mixer Capture/Reversed; `Mic1 Capture` es la única fuente activa | no hay entrada RX separada |
| ALSA real | Card0 `sun8i-a33-audio`; snapshot `qa13/amixer-before.txt`: AIF1 Slot0 off, Mixer Capture off, AIF2 off, Mic1 on | coincide con el driver |
| SDK | `refs/X6200Control`: `record_set` sólo RMW bit `voice_rec=0x8` de reg12; no hay API de ruta RX/DAC independiente. `spmode` sólo speaker/phone; `linein_set` es gain; `iqout` es aux IQ gen | no hay contrato independiente |
| GUI vendor 1.0.6/1.0.8 | REA #376: escribe el bloque workparams (reg12 incluye 0x8), sin readback/ACK/restore; audio/MUX sin cambio entre versiones | no hay control nuevo de ruta |
| MCU/BASE | `x6200_mcu_20250111134652.xgf` 524288 B SHA256 `dca90387...` (y 1.0.8 `9a19f97a...`): entropía 7.47–7.75, sin símbolos; `.xgf` opaco | **MCU_OPAQUE**; no se puede afirmar ruta interna |
| Telemetría flow | `x6200_flow_t.samples[512]` es `uint8_t` de espectro/waterfall, no PCM; docs `X6100_vs_X6200_HARDWARE.md:183-192` | no sirve como audio RX |
| Comportamiento | QA14 A/B/A: captura BASE 1.792/1.889 RMS; DAC+ZERO 30,6 RMS (−35 dB); QA12b DAC tone aparece en captura; volumen0 no lo elimina | RX útil desaparece en DAC; acoplamiento antes del amplificador |

## 3. Búsqueda de una entrada RX independiente

- **Mic2 y LineIn**: habilitados durante DAC+ZERO → captura RMS 146,5 vs ~1.800 BASE; no recuperan
  RX (QA13/14). No hay evidencia de conexión de placa a esas entradas.
- **AIF2/AIF3**: drivers/DT no los exponen como PCM; controles apagados; no hay señal física
  documentada.
- **Digital RX pre-mute**: no existe en DT/driver/SoC; el MCU/BASE es opaco y la GUI vendor no
  expone una API independiente de `voice_rec`.
- **`samples[512]`**: es espectro de 8 bits, no PCM; un contrato nuevo haría falta para usarlo, y no
  está demostrado.

## 4. Clasificación

- `PRE_MUTE_RX_FOUND = NO` (no identificado ni confirmado).
- `BASE_FIRMWARE_ROUTE_FOUND = NO` (MCU opaco; vendor GUI sólo `voice_rec` bit 0x8).
- `NO_ACCESSIBLE_SOFTWARE_ROUTE = YES`.
- **`PHYSICAL_ACCESS_REQUIRED`**: sólo una inspección de placa/esquema puede localizar el nodo de
  audio RX antes del mute y verificar si puede llevarse a LINEIN/Mic2.
- No se implementa AEC; no se compila kernel experimental; no hay señal que enrutar.
- `PRODUCTION_READY=NO`; sin merge.

## 5. Protocolo físico (para técnico; no ejecutado por el agente)

**Precondiciones**: equipo apagado, batería/fuente desconectadas, SD/eMMC respaldadas, ESD, sin
soldar ni cortar pistas. Instrumento: osciloscopio/analizador con sonda 10×, 1 MΩ, AC coupling,
límite de entrada verificado; masa de referencia en GND de placa; **no** conectar sondas a nodos
energizados sin esquema/servicio. Sin TX/PTT/ATU.

1. Obtener esquema/servicio/boardview del X6200 o fotografías de alta resolución de la zona del
   códec A33 y del conector BASE.
2. Identificar el nodo de audio RX demodulado **antes** del mute de `voice_rec` y los pines
   MIC1/LINEIN/Mic2 del códec.
3. Con el equipo en RX y una señal RF estable de referencia, medir RMS/espectro en ambos nodos;
   verificar continuidad y niveles (mVpp), impedancia y polarización DC.
4. Repetir con DAC activo y PCM cero: confirmar si el nodo pre-mute conserva RX y si el códec lo
   recibe; no modificar el equipo.
5. Si el nodo es compatible con LINEIN/Mic2 (nivel, AC coupling, bias), preparar una modificación
   documentada de ruteo (no ejecutarla sin autorización) y el cambio ASoC/DTS correspondiente.
6. Si no hay nodo accesible, el límite es de hardware/BASE; no hay corrección de software.

No se soldó, cortó, sonda ni energizó nada en esta corrida.

## 6. Camino de corrección (mínimo, condicionado a Etapa 5)

- Preferencia 1: RX analógico pre-mute → **LINEIN** (o Mic2) del códec → ADC/AIF1 → Linux DSP →
  DAC → parlante. Requiere señal física compatible y ruta ASoC/DT.
- Preferencia 2: RX digital independiente antes del mute (AIF2/AIF3/I2S), sólo si existe señal.
- Preferencia 3: modificación del firmware BASE que mantenga RX y DAC simultáneos; el `.xgf` está
  cifrado/opaco y no hay mecanismo de recuperación demostrado; no se intenta.
- AEC: **no aplica** mientras la RX desaparezca en DAC.
