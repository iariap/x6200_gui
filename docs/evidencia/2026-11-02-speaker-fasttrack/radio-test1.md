# Prueba 1 en radio — camino de reproducción Linux existente

Fecha: 2026-11-02 17:32 UTC. Base `main` 681074e0. Rama `test/speaker-fasttrack`.
Operador presente; radio en RX (7.091200 MHz LSB-D, 40m), antena conectada; sin TX/PTT/ATU.

## Build

- `x6200_gui` de prueba: md5 `9efc75f53e3d0df2a5bc2835c6fb26b0`, ELF32 ARM, 14.005.984 B.
- Rollback guardado: `/usr/sbin/x6200_gui` original md5 `e32a81f3563dab27e5b30111a47a7d12`, copia en
  `/mnt/backups/x6200_gui.pre-speaker-test` y host.
- Gate `verifica-firmware-port.py`: OK.
- El binario de prueba se retiró y la GUI original fue restaurada; la radio quedó en RX.

## Secuencia ejecutada por socket de control

1. `{"cmd":"get"}`: RX, `vol=8`, `tx=false`; HMic=20, IMic=30.
2. `{"cmd":"speaker_test"}`: una sola prueba; el test hace
   `audio_play_en(true)` -> tono 440 Hz de 1 s por `audio_play()` -> `audio_play_en(false)`.
3. Después: `{"cmd":"get"}`: RX, mismo `vol`, `tx=false`; la GUI seguía viva.

## Resultado medido

```
START rx=1
BEFORE reg12=0x00001000 voice_rec=0 rxvol=8 hmic=20 imic=30 state=0
AFTER  reg12=0x00001000 voice_rec=0 rxvol=8 hmic=20 imic=30 state=0 written=17640 failed=1
RESULT FAIL_OR_DEGRADED
DONE
```

Interpretación:
- El test escribió **17.640 muestras = 0,4 s** al stream existente; al intentar seguir, `audio_play()`
  devolvió -1. No llegó a reproducir los 44.100 samples previstos.
- `reg12` quedó **idéntico** antes/después (`0x00001000`, voice_rec=0); `rxvol` y HMic/IMic no
  cambiaron; estado `rx=1`, `tx=false`; no hubo PTT/ATU.
- La GUI se restauró y quedó operativa.
- Control independiente: `paplay` de un WAV de 1 s al sink devolvió **rc=0**; el sink/DAC por ALSA
  consume audio (sin MUX). El problema observado es del buffer/consumo del `play_stm` de la GUI, no
  del sink ALSA.

## Clasificación (honesta)

- `SPEAKER_AUDIO_HEARD`: **PENDIENTE de confirmación del operador** (pudo haberse oído un tono de
  0,4 s; no se asume).
- `NOMINAL_BASE_RETURN_PASS`: **parcial**: el registro y el estado de radio volvieron a BASE, pero la
  escucha de retorno del audio RX aún no fue confirmada por el operador.
- `CRASH_RECOVERY_UNVERIFIED`: la GUI no crasheó; no se probó un crash.
- `PRODUCTION_NOT_READY`: no se declara.

## Decisión

No se avanza a la ruta RX continua hasta saber si el operador oyó el tono de 0,4 s. Si no se oyó,
el camino existente falla y hay que diagnosticar `play_stm`; si se oyó, el MUX y el DAC funcionan y
el límite es el buffer de `audio_play()` (0,4 s), que el router continuo debe resolver con su propio
stream/ring y un retorno explícito a BASE.
