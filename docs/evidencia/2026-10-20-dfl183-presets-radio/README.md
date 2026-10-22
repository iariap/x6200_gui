# Presets y memorias del DFL (#183 / PR #185) verificados en la radio

- Rama `lu2dsx/x6200-rom`, merge **`6666806e`** ("DFL #183: presets al primer toque, ajuste manual con
  VOL y memorias persistentes (#185)"), publicado en `90bdf8e7`.
- Build en la radio: **`e43f7f6c4cbb6086572030f914eb44cb`** (deploy del otro agente), PID 23741,
  equipo sin reiniciar.

## Verificado por control remoto

| Item | Resultado | Evidencia |
|---|---|---|
| DFL corto abre la pagina de **FAVORITOS** del modo | si: celdas **Wide 3300 Hz / Normal 2700 Hz / DX 2100 Hz / Ajustar / Mas** | `01-favoritos.png` |
| Filtro que no coincide con un preset | avisa **"Personalizado"** | `01-favoritos.png` |
| **Un toque aplica el preset** (par LOW/HIGH, BW derivado) | **Wide**: `filter=3300, flo=100, fhi=3400` (3300 = 3400-100) | `02-preset-wide.png` |
|  | **DX**: `filter=2100, flo=300, fhi=2400` (2100 = 2400-300) | `03-preset-dx.png` |
| Preset activo resaltado | la celda **DX** queda en acento y **desaparece** "Personalizado" | `03-preset-dx.png` |
| Cerrar | volver a la pagina GEN cierra el panel | `04-cerrado.png` |
| Barrido de 23 apps | **24/24 ok, 0 "REINICIO", 0 "SE CAYO", 0 "no responde"** | log del barrido |
| `verifica-radio` | `TODO OK` | — |

## No cubierto por este rol (validacion fisica del owner)

- **Memorias Mem 1/2** ("Mas"), la confirmacion de sobrescritura, el **hold sobre un favorito**
  ("Elegir origen"), y el **ajuste manual con la rueda VOL**: requieren tocar la pantalla o girar el
  encoder, que el control remoto no expone.
- Presets de **DIGI / CW / AM** (solo se probo la familia **SSB** del modo actual).

## Estado dejado

`7.094.288 kHz LSB, BW 2715`, pagina GEN, RX, sin TX. Nota: el par LOW/HIGH previo del dueño
(30/2745) **no se puede restaurar por socket** (solo se fija el BW); quedo en -7/2708 para el mismo BW.
