# #292 — validación de banda lateral con portadora de noche (2026-10-28 21:00 local)

Radio: X6200, ROM v0.1.11 (GUI `5ef35ec0b807`). Método: `tools/lsb-sideband-test.sh --carrier`
(portadora AM de 790 kHz, capturas parec de 3 s y tono de 1 kHz contra el piso 2–4 kHz).

## Script oficial (790 kHz)

```
789-LSB  tono1k= 24.7 dB   rms=3729
789-USB  tono1k= 33.1 dB   rms=3953
791-LSB  tono1k= 33.0 dB   rms=4064
791-USB  tono1k= 22.8 dB   rms=3695
RESULTADO: FAIL (criterio absoluto del script: <48 / >55 dB)
```

La **dirección del patrón es correcta** (LSB filtra 789 y USB filtra 791, al revés en 791), pero el
criterio absoluto del script está calibrado para portadora fuerte y el 790 estuvo débil esta noche
(ningún valor llegó a 48/55 dB).

## Barrido AM de la noche (dB, `get`)

Más fuertes: **1280 kHz (−69 dB)**, 760 (−82), 1270 (−83), 640 (−85), 1290 (−85), 750 (−87).

## Variante del mismo método sobre 760 kHz (criterio relativo ±8 dB)

```
759-LSB  tono1k= 26.1 dB   rms=3788
759-USB  tono1k= 39.6 dB   rms=3850
761-LSB  tono1k= 36.3 dB   rms=3687
761-USB  tono1k= 24.9 dB   rms=3861
RESULTADO: PASS (lados opuestos; contraste 13.5 dB y 11.4 dB)
```

1280 kHz no mostró contraste (estación modulada), no sirve como portadora.

**Conclusión**: #292 sigue correcto en la ROM v0.1.11 (no hay regresión por los cambios de SWR/curva/BT/UI);
el FAIL del script oficial es por debilidad de la portadora de 790 kHz esta noche, no por la banda lateral.
Radio restaurada en 7.140.000 LSB (estado del dueño), `crashlog 0`.
