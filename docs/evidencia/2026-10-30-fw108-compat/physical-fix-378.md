# Prueba física #378: corrección del flujo 1.0.7/1.0.8 y waterfall

Fecha: 2026-10-30 (America/Buenos_Aires). Issue [#378](https://github.com/iariap/x6200_gui/issues/378),
epic [#374](https://github.com/iariap/x6200_gui/issues/374). PR: [#381](https://github.com/iariap/x6200_gui/pull/381).

## Síntoma

La ROM de prueba arrancó en la radio con BASE 1.0.8; la UI quedó fluida y el
audio RX funcionó, pero **waterfall y espectro no mostraban ruido/señales en
ningún tema** (fondo de un solo color). Capturas:

- [antes: Sunlit, waterfall plano](screens/01-antes-sunlit.png)
- [después: Redshift, ruido y trazas](screens/02-despues-redshift.png)

## Causa raíz, medida en la radio

La conversión de muestras del PR #381 era `(byte - 150)` (offset 0x96 tomado de
otro payload de la APP vendor). En el flujo real de BASE 1.0.8 las muestras son
**dB referidos a 126**: bytes medidos en RX ~`14..47`, no PCM centrado en 150.
Con `-150` todos los valores quedaban negativos; el promedio en `dsp.cpp` era
negativo; `log10` producía `NaN`. `dsp_update_min_max` alimentaba `grid_min` /
`grid_max` con `NaN` y el `lpf` los dejaba envenenados: todas las filas del
waterfall caían en un solo índice de paleta. La inspección del proceso mostró
`grid_min = grid_max = NaN`, `height = 284` y filas de caché constantes
(`0x00` o `0xFF`), mientras el reloj y el resto de la UI seguían vivos.

La GUI upstream v0.1.2 ya tenía la compatibilidad de flujo 1.0.7/1.0.8 en el
commit `c74df4a3` ("Fixes for 1.0.7 BASE"): `radio.c` usa `pack->samples[i] -
126.0f` y `dbm = -pack->dbm + 4`; `dsp.cpp` convierte dB→potencia en
`AveragedPSD::add_samples`, vuelve a dB con `10·log10` y calcula min/max con
ventana de 2.7 kHz + offsets de Minimum Statistics. Nuestro port tenía el código
anterior (BASE 1.0.6) porque el rebase no había traído esa parte.

## Corrección aplicada

- `src/radio_samples.h`: `RADIO_FLOW_SAMPLE_BIAS = 126`.
- `src/radio.c`: `int16_t dbm = -(int16_t)pack->dbm + 4`;
  `dsp_samples(samples, RADIO_SAMPLES, pack->flag.tx, dbm)`.
- `src/dsp.cpp`: dB→potencia y `10·log10` en `AveragedPSD`;
  `dsp_update_min_max` de upstream (`fft_width`, ventana 2700 Hz, offsets
  14.83/9.47/5.57/3.24 dB); `+8/-16` bins de borde.
- `tests/native/test_radio_samples.c`: contrato actualizado (0→-126,
  126→0, 255→129; bytes medidos 14..47→-112..-79 dB).

## Verificación

- `tests/native/run_native_tests.sh`: **exit 0**; stage de muestras incluido.
- Build real de la GUI con Buildroot/toolchain del owner: **exit 0**. Binario
  compilado: md5 `7316d73830e94871b11a94b7f2d0f133`, sha256
  `f3c4c40c3076e8f68361301c5525c600cc9d6669fda986ab6f18aa5873d71423`.
- Despliegue temporal en la SD de prueba (sin tocar BASE/MCU, sin TX/PTT/ATU):
  reemplazo del binario `/usr/sbin/x6200_gui` y reinicio de la GUI.
- Tras el fix: `grid_min ≈ -49.9 dB`, `grid_max ≈ 0 dB` (finitos), y capturas
  consecutivas del waterfall con hashes distintos, `std ≈ 22`, ~1.2k colores
  (antes: región idéntica y plana). Captura después: ver
  [02-despues-redshift.png](screens/02-despues-redshift.png).
- UI fluida, audio RX OK, sin crash en el log de la corrida.

## Límites y pendientes

- La imagen `sdcard.img` original (`sha256 7ec5d8cc…`) **no** incluye esta
  corrección. La prueba se hizo con el binario compilado del PR desplegado
  temporalmente en la SD; para el artefacto reproducible de #378 falta rearmar
  la imagen con este binario y verificar su SHA. No se modificó el firmware
  OEM/BASE/MCU.
- `payload_authenticity=UNVERIFIED`; la corrección se basa en la medición del
  flujo en la radio y en el commit upstream `c74df4a3`, no en una captura
  autenticada del wire 1.0.8.
- La imagen/rom sigue `production_ready=false`; PR #381 y #14 sin merge.
