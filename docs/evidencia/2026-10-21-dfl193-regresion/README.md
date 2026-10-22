# #193 verificado en la radio: el editor DFL inline **ya no se cierra** al cambiar de modo en la misma familia

- Rama `lu2dsx/x6200-rom`; merge **`c888308e`** ("DFL: el editor inline ya no se cierra al cambiar de modo en
  la misma familia (regresión de #183) + el gate de contratos no pasa como verde si no evaluó (#193)").
- Build en la radio: **`3ef20224dbd95d4eef6d14d53ca1c352`** (deploy del otro agente, GUI relanzada 18:11).

## Medido

| Paso | Resultado | Evidencia |
|---|---|---|
| Editor inline abierto en **USB** | "DFL USB", eje **0 → 5000**, LOW 400 / HIGH 2800 / BW 2400 | `04-*.png` |
| Cambiar a **LSB** con el panel abierto | **el panel NO se cierra**; pasa a "DFL **LSB**" y **re-espeja** (eje **5000 → 0**, LOW a la derecha) | `06-*.png` |
| Valores | intactos (400 / 2800 / 2400) | ambas |
| Extra | también se vio que la página **Memorias** ("Más") no se cierra al cambiar de modo | `02-*.png` |

## Nota de metodo

Primero entré por una ruta equivocada (btn 4 en la pagina de favoritos abre **"Más"/Memorias**, no el editor).
La ruta correcta: **DFL corto → Ajustar → Gráfico**. Las capturas utiles son `04` (USB) y `06` (LSB).

## Estado dejado

7.082.988 kHz LSB, BW 2800, pagina GEN, RX, sin TX.
