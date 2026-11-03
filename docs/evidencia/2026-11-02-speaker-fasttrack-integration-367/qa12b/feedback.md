# QA12b — causa del eco RAW: la captura se contamina con el DAC

Grabación simultánea de la fuente de captura (`alsa_input.platform-sound.stereo-fallback`) y del
monitor del sink durante cada fase; análisis 2–6 s.

| Captura durante | RMS | Autocorrelación |
|---|---:|---|
| BASE (sin DAC) | 1.988 | sin peaks >0.05 |
| ZERO con DAC | 26,7 | solo ruido de baja amplitud |
| RAW con DAC | **7.185** | **0.403 s = 0,637; 0.806 s = 0,698** |
| TONE con DAC | 326 | **80,6% de la energía espectral en 1 kHz** |

- En BASE la captura es normal y no tiene periodicidad.
- En TONE, el tono sintético del DAC **aparece en la fuente de captura** (1 kHz dominante): la
  selección `voice_rec=1` está realimentando la salida DAC hacia la captura.
- En RAW, como el router realimenta la captura al DAC, el lazo captura→DAC→captura produce un
  eco con retardo de ~0,40 s; la autocorrelación de la captura muestra las réplicas 0,403/0,806 s.
- El selector BASE/DAC es correcto (QA12 ZERO/TONE limpios); el defecto es el **lazo de captura**,
  no el MUX ni el DSP.

## Clasificación

`ANALOG_FEEDBACK_LOOP = CONFIRMED`; `HARDWARE_ROUTE_BLOCKED` para la ruta actual: no existe en el
SDK/controlador un mute independiente de la rama DAC→captura, y no se puede cerrar el lazo desde
la app sin cambiar el hardware/ruteo. No se modifica producción. El fade NR es otro problema.

Radio restaurada `e32a81f3…`, un GUI, RX, tx=false, volumen1. `PRODUCTION_READY=NO`; sin merge.
