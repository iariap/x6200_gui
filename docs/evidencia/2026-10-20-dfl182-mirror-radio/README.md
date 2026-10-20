# Espejo del editor DFL en LSB/LSB-D (#182 / PR #184) verificado en la radio

- Rama `lu2dsx/x6200-rom`, merge **`d32b59e8`** ("DFL #182: el editor espeja la banda pasante en
  LSB/LSB-D (HIGH a la izquierda) (#184)").
- Build compilado y desplegado por este rol: **`88196e442ddc6a67150da4890d9f1ed1`**, gate del #86 OK
  (ELF32 ARM, 13.875.308 B, 3/3 marcas), GUI relanzada, equipo sin reiniciar.

## Verificado en la radio

| Caso | Esperado | Medido |
|---|---|---|
| **LSB** (7.120 kHz, BW 2400, LOW -425 / HIGH 1975) | eje **2000 -> 0** y **HIGH a la izquierda** | ✔ eje 2000->0, HIGH (marcador) a la izquierda, LOW a la derecha |
| **USB** con el panel abierto (mismo BW) | eje **0 -> 2000** y **LOW a la izquierda** | ✔ eje 0->2000, LOW a la izquierda, HIGH a la derecha |
| Cambio LSB<->USB con el panel abierto | re-espeja en el mismo frame | ✔ (capturas 01 vs 02) |
| Textos de las filas | siguen siendo LOW / HIGH / BW con los valores DSP | ✔ LOW -425 / HIGH 1975 / BW 2400 en los dos modos |
| Banda en el espectro (#174) | se conserva a escala | ✔ "2.4k", izquierda en LSB, derecha en USB |
| Estabilidad | sin crash | PID 21227 estable, 0 CRASHLOG |

## Nota sobre el barrido de esta pasada

El barrido marco **BT** como "REINICIO (dejo de responder)" **falso positivo** (otra vez el SSH vacio,
`uptime_radio()` va por SSH): el PID no cambio, **no aparecio ningun log guardado nuevo** durante el
barrido (el 694 es del deploy) y abrir **BT** 14 s a mano dejo **PID 21227 estable** sin log nuevo.
Los otros **23 apps** dieron ok.

## Estado dejado

Frecuencia/modo/filtro del dueno restaurados (**7.119.000 LSB, BW 1540**), pagina GEN, RX, sin TX.
