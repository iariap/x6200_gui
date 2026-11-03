# QA13 — Loopback DAC→captura por ALSA

## Controles relevantes (card 0 `sun8i-a33-audio`)

Snapshot completo en `qa13/amixer-before.txt`; restauración exacta verificada con
`qa13/amixer-after.txt` (diff vacío).

| numid | Control | Valor inicial |
|---:|---|---|
| 18 | AIF1 Slot 0 Digital ADC Capture Switch | **off,off** |
| 20 | AIF1 Data Digital ADC Capture Switch | on,on |
| 19 | AIF2 Digital ADC Capture Switch | off,off |
| 36 | Mixer Capture Switch | off,off |
| 37 | Mixer Reversed Capture Switch | off,off |
| 39 | Mic1 Capture Switch | **on,on** |
| 30 | ADC Digital DAC Playback Switch | off,off |
| 31 | DAC Playback Switch | off,off |

Los switches de loopback digital ya estaban apagados. La única captura activa asociada es
Mic1/ADC/AIF1 Data.

## Prueba

`DAC_TONE` QA12b, volumen10, captura y monitor simultáneos:

| Condición | Captura RMS | Captura 1 kHz | Monitor 1 kHz |
|---|---:|---:|---:|
| Baseline | 330,0 | **0,722** | 0,731 |
| Mic1 Capture Switch off | **0,0** | **0,0** | **0,746** |

- Con Mic1 Capture apagado el tono desaparece de la captura, pero **también desaparece toda la
  captura**: RMS0. No sirve para operar el RX.
- El monitor sigue con el tono (parlante/DAC normal).
- Restaurado Mic1 Capture on: captura BASE vuelve a RMS 1580,7 (RX útil).
- Snapshot ALSA final idéntico al inicial (diff vacío); sin `alsactl store`.

## Clasificación

`ALSA_LOOPBACK_FIX = NOT_CONFIRMED`: no hay un control independiente que corte DAC→captura sin
matar la captura RX. El lazo es analógico/acústico o comparte la única ruta ADC/Mic1; no se fuerza
ningún control más allá del prueba y se restaura el mixer.

`HARDWARE_ROUTE_BLOCKED`; `PRODUCTION_READY=NO`; no se ejecutó soak porque el criterio de captura
RX útil no se cumple con el único control que eliminaba el tono. Radio final original
`e32a81f3…`, un GUI, RX, tx=false, volumen1.
