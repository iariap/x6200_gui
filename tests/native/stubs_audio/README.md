# Stubs del callback de captura (issue #351, parte 2)

Arbol de headers contra el que se compila `src/audio.c` para
`tests/native/test_audio_callback.c` (ver el bloque `#351` del final de
`tests/native/run_native_tests.sh`). `src/audio.c` se incluye por texto desde un
directorio propio, asi que sus `#include "..."` (`audio.h`, `dsp.h`, `meter.h`,
`screen_rec.h`, `audio_enhancement.h`) caen aca y no en `src/`: el mismo patron
de staging que clock, backlight, hkey, radio y el resto de la suite.

## Que reemplaza y que observa

El callback de PulseAudio no tiene otra entrada observable que el stream, asi
que el test no puede ser una prueba estatica: se llaman los `pa_*` de este stub
con una respuesta programable y se cuentan/registran los efectos.

- `pulse/pulseaudio.h` + `tests/native/audio_callback_stubs.c`: la superficie que
  usa `src/audio.c`. `pa_stream_peek()` devuelve `(rc, data, nbytes)` fijados por
  el test; `pa_stream_drop()`, `pa_stream_write()`,
  `pa_threaded_mainloop_{lock,unlock}()` y los `x6200_control_*` llevan contador
  y entrada en el log de eventos. El resto es no-op.
- `alsa/`: `asoundlib.h`/`mixer.h` minimos. `mixer_setup()` y los setters de
  volumen compilan y linkean, pero el test nunca los ejecuta (y el contador
  prueba que el callback tampoco).
- `lvgl/lvgl.h`: solo los `LV_LOG_*`, dirigidos al contador del stage. El
  contrato exige que el error de peek/drop y el `nbytes` impar se registren, asi
  que aca los logs NO se silencian como en los otros stubs.
- `dsp.h`, `meter.h`, `screen_rec.h`, `audio_enhancement.h`: los dos
  consumidores del PCM y la cadena de enhancement, reducidos a lo que el callback
  llama. `audio_enh_process()` escribe **in place** (como Speex) para que el
  canario del fragmento detecte que el codigo viejo tocaba la memoria de
  PulseAudio, y `audio_enh_nr_reset_pending()` limpia la bandera que el hole
  tiene que resetear.
- `params/params.h`: `params.hmic`/`params.imic` y las tres escrituras de control
  de la radio. El resto de `params_t` no existe a proposito: este stage no
  compila `params.c` ni el SDK de la BASE.

## Sin submodulo

Ningun `-I` de este stage apunta al `lvgl/` del repo (solo `$AUDIO_STAGE` y
`tests/native`), asi que `--ci-parity` lo corre igual que en CI.

## Heap

El binario se linkea con `-Wl,--wrap=malloc`, `--wrap=calloc`, `--wrap=realloc`
y `--wrap=free`: el test compara el contador antes y despues del callback y falla
si aparece cualquier reserva. El contrato A prohibe heap dentro del callback.
