# RX procesado → DAC → parlante: primer intento real

Fecha: 2026-11-02 17:49:54 UTC (14:49 local). Rama `test/rx-speaker-fasttrack`, base `main` 681074e0.
Radio en RX, 7.091200 MHz LSB-D, 40 m, antena conectada, volumen 30. Operador presente.

## Ruta usada

`captura PulseAudio -> audio_enh_process (SSB voice forzado temporalmente) -> rx_audio_live (ring
4x4410 + worker + stream propio) -> DAC del SoC -> MUX -> parlante`.

- El test usa `radio_audio_route_set()`: único punto que escribe `voice_rec` dentro de la GUI,
  serializado con `radio_thread`/`x6200_control_idle`; **no** usa `audio_play_en` y no toca
  HMic/IMic.
- Antes de habilitar hace un retorno explícito a BASE. Después del test vuelve a BASE y restaura el
  preset efectivo y el volumen.
- Sin TX/PTT/ATU; sin guardianes externos; sin I²C directo.

## Medición

```
BEFORE reg12=0x00001000 voice_rec=0 rxvol=30 hmic=20 imic=30 state=RX preset=OFF
WAIT state=BASE pending=0
DAC state=RX_DAC dac=1
AFTER reg12=0x00001000 voice_rec=0 rxvol=30 hmic=20 imic=30 state=RX preset=OFF
writes=17 drops=0 underruns=319
RESULT PASS_BASE_RESTORED
```

- El router abrió su stream y llegó a `RX_DAC`; el MUX se seleccionó durante ~1,5 s.
- `writes=17` bloques de 0,1 s = 1,7 s de PCM aceptados; `drops=0`.
- `underruns=319` (contador del worker cuando no hay bloque; el audio pudo tener cortes).
- El registro volvió idéntico, HMic/IMic y volumen sin cambios; radio en RX; sin TX/ATU.
- La GUI original `e32a81f3…` fue restaurada; un solo proceso `x6200_gui`.

## Clasificación

- `SPEAKER_AUDIO_HEARD`: **pendiente de confirmación del operador**.
- `NOMINAL_BASE_RETURN_PASS`: sí a nivel registro/estado; falta confirmar auditivamente el retorno.
- `CRASH_RECOVERY_UNVERIFIED`: no se probó crash; el test terminó normal.
- `PRODUCTION_NOT_READY`: no se declara.

Si el operador confirma que escuchó RX procesado, el siguiente paso es estabilizar el stream
(underruns) y repetir una prueba corta; si no, el fallo está en el tramo DAC/PA/MUX o en el stream
del router.
