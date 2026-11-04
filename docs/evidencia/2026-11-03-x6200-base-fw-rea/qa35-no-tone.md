# QA35 — Sin tono audible incluso a volumen máximo

Fecha: 2026-11-03. H1 instalado. Sin nuevo flash.

## Prueba

- GUI/daemon/PulseAudio detenidos para usar ALSA directo; la UI se reinició después, como en
  las pruebas anteriores (no fue un reinicio del sistema).
- `DAC Playback Switch = on`, `DAC Playback Volume = 192` (máximo), amplitud PCM 28000.
- **BASE:** tono grave continuo 1 kHz, 1,5 s (`frames_total=96000`, `drain=0`).
- **DAC:** tres tonos ascendentes 1,2/2,4/3,6 kHz, 0,7 s (`frames_total=168000`, `drain=0`).
- Retorno a BASE, codec restaurado, GUI/PA reiniciados, ALSA restaurado.
- Uptime de la radio: 15510 s; no hubo reinicio del sistema, sólo la UI controlada.

## Resultado

- El propietario **no escuchó ningún tono**, ni en BASE ni en DAC.
- El QSO nativo sí era audible.
- PCM aceptado por ALSA y codec habilitado → el fallo está en la **ruta PCM→parlante**,
  no en la aceptación de muestras.
- No se puede atribuir el silencio al DAC ni al mute del MCU: **incluso BASE no produjo tono**.

## Gates

- `TONE_HEARD = NO (reporte del propietario)`
- `PCM_ACCEPTED = YES`
- `DAC_MIXER_ENABLED = YES` (switch on, volume máximo)
- `DAC_MUX_SELECTED = YES` (helper voice_rec)
- `SPEAKER_AUDIO_HEARD = YES` (sólo RX nativo/QSO)
- `LINUX_DSP_SPEAKER = FAIL` para la ruta PCM probada
- `BASE_RX_PRESERVED_WITH_DAC = NO` (captura ADC)
- `DAC_FEEDBACK_IN_CAPTURE = INCONCLUSIVE`
- `LINUX_DSP_RX_CAPTURE = FAIL`
- `BASE_RETURN = PASS` (RX nativo restaurado; UI original)
- `H1_FLASHED = YES`
- `H1_RESULT = FAIL_FUNCTIONAL`

## Conclusión

El audio que el propietario escuchaba era el **RX nativo del BASE**, no la reproducción PCM de
Linux. La ruta Linux→codec→parlante no queda demostrada ni siquiera en BASE con el codec
habilitado; el objetivo de DSP Linux por el parlante interno no se alcanzó.

No se harán más pruebas de tono ni otro flash sin una hipótesis nueva y autorización.
