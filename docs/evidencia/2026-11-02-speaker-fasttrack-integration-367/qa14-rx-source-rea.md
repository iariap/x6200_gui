# QA14 — ¿Desaparece la RX al seleccionar DAC? REA y medición

## Etapa 1 — A/B/A con router real

Captura ALSA directa (`alsa_input.platform-sound.stereo-fallback`), 5 s por fase, se descartan 2 s.
Misma frecuencia (7.200 MHz LSB), mismo modo y volumen10. La señal RF es la RX ambiente del equipo;
**no hay generador calibrado**, por lo que la referencia es relativa y repetida A/C.

| Fase | MUX | Captura RMS | Pico |
|---|---|---:|---:|
| A BASE | BASE | **1.792,3** | 9.124 |
| B DAC+ZERO | DAC | **30,6** | 273 |
| C BASE | BASE | **1.888,9** | 9.933 |

Caída de ~35 dB al seleccionar DAC; A y C vuelven al mismo nivel. **La RX útil desaparece en DAC.**

`RX_SOURCE_IN_DAC = FAIL` (sistemático en A/B/A; sin referencia RF calibrada, validez relativa).
No se implementa AEC: no hay RX que cancelar.

## Etapa 2 — REA del camino

- Kernel desplegado: **Linux 6.1.82**, `armv7l`, DT `xiegu,x6200`/`allwinner,sun8i-r16`.
- PCM: `1c22c00.dai-sun8i-codec-aif1`; único PCM capture/playback, AIF1.
- DTS main `sun8i-r16-x6200.dts` `&sound` rutea sólo AIF1:
  - DAC: `AIF1 Slot 0` → Left/Right DAC.
  - Captura: `Left/Right ADC` → `AIF1 Slot 0 ADC`.
- `sun8i-codec-analog.c`: ADC Mixer recibe **Mic1**, `Mixer Capture`/`Mixer Reversed`; no hay entrada
  digital de RX independiente. `Mic1 Capture Switch` es la única fuente activa.
- `sun8i-codec.c`: `AIF1 Slot 0 Digital ADC Capture Switch` (loop digital de AIF1 DA0) ya **off**;
  `AIF1 Data Digital ADC Capture` on es el camino ADC normal; AIF2/AIF3 deshabilitados.
- El audio RX del X6200 llega por **Mic1 analógico**; el MCU/BASE lo silencia al seleccionar DAC.
  No existe una entrada RX digital separada expuesta al codec/SoC en el DT/driver actuales.

## Etapa 3 — Acoplamiento

- DAC_TONE con captura simultánea: 1 kHz aparece en la captura (ratio espectral ~0,72).
- Con volumen del operador en **0**, el tono sigue en captura (ratio ~0,93): el acoplamiento es
  **antes del amplificador común**, no acústico posterior al volumen. El monitor sigue con tono.
- `Mic1 Capture Switch off` elimina el tono pero **elimina toda la captura** (QA13), no es una
  solución.
- `Line In Capture` + `Mic2 Capture` habilitados durante DAC+ZERO: captura RMS29,6, sin RX útil.
  No hay entrada alternativa que recupere RX.
- Mixer restaurado exactamente; snapshot después idéntico al inicial; sin `alsactl store`.

## Etapa 4 — Corrección

`RX_SOURCE_IN_DAC = FAIL` y no hay entrada RX independiente accesible en codec/DT/kernel. No se
implementa AEC ni cancelador; no se compila kernel experimental porque no hay señal que enrutar.
Se requiere una de estas modificaciones de hardware/ruteo:

1. Mantener la RX del BASE en una entrada ADC/Mic independiente mientras el DAC está seleccionado.
2. Exponer la RX digital antes del mute del MCU/BASE hacia AIF2/AIF3 o un canal I2S independiente.
3. Aislar/silenciar la rama DAC→Mic1 para que la captura no se contamine (no existe control
   independiente en el codec actual).

`HARDWARE_ROUTE_BLOCKED`; no AEC; no soak. Radio restaurada original `e32a81f3…`, un GUI, RX,
tx=false, volumen1. `PRODUCTION_READY=NO`; sin merge.
