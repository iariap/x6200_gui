# Prueba del estado actual en la radio (2026-10-14)

Build probado: rama `lu2dsx/x6200-rom`, commit `cc55931`. Verificado que la radio corre **exactamente**
ese build: `x6200_gui` md5 `71a10c9aeab9` y `liblvgl.so` md5 `a1181ba6368d`, iguales a la imagen.

## Lo que funciona (a la vista en `01-principal.png`)

- Barra superior con **iconos reales**: bateria y enchufe en el reloj de potencia, glifo de WiFi (fix
  de UI-011 validado).
- Fila de frecuencia **legible** (`7.002 / 7.098.000 / 7.194`, fix de UI-010 validado) y celda
  `VFO-B` completa (fix de UI-012 validado).
- Pie (`VOL 1:3`, Volume, Squelch, RF gain, TX power), etiquetas CW/DIG y espectro.
- Panel del decodificador CW **decodificando** texto real.
- Barrido de las 23 apps: **18 ok / 5 caidas** (ver
  `../2026-10-14-caidas-aleatorias/prueba-23-postfix.log`).

## Lo que NO funciona: la GUI se cae

Medido con los timestamps de los logs: **mediana 46 s** de vida por arranque de GUI (min 20 s, max
122 s, 19 ciclos) y **los 57 logs retenidos terminan en crash**. El equipo **no** se reinicia (uptime
11 h): se reinicia la GUI, que el supervisor relanza en ~1 s.

Causa raiz (issue #19): `*** buffer overflow detected ***` de glibc en el camino de audio ->
`cw_put_audio_samples` -> `cw_decoder_signal`.

Disparador, reproducido a voluntad:

| Prueba | Caidas en 150 s |
|---|---|
| GUI recien reiniciada, sin usar apps CW | **0** |
| Despues de abrir y cerrar `CwTrainer` | **5** (una cada ~30 s) |

## Conclusion

Con el build actual la radio **no es usable** mientras se usen las apps CW: entra en un bucle de
reinicio de la GUI cada ~30-46 s. Reiniciar la GUI la deja estable (150 s sin caidas) hasta que
vuelvan a usarse esas apps. Todo lo demas que se probo funciona.
