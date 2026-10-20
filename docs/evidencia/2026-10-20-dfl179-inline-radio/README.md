# Editor DFL inline (#179 / PR #181) verificado en la radio

- Rama `lu2dsx/x6200-rom`, merge **`f81ef7f9`** ("DFL #179: editor inline en el waterfall, operado por
  la rueda VOL (sin ventana flotante) (#181)").
- Build que ya corria en la radio: **`49db3db478402d69cb7f48fb50432f15`** (deploy del otro agente).
- Radio real, tema Aurora, RX, sin TX, 7.120.653 kHz LSB, Filter BW 2140.

## Verificado por control remoto

| Item | Resultado | Evidencia |
|---|---|---|
| Abrir el panel DFL (5.a celda "DFL editor") | **panel inline** bajo el waterfall | `02-editor-inline.png` |
| Ventana flotante / titulo / boton X | **no hay** (es integrado) | `02-editor-inline.png` |
| Rotulo y restablecer | "DFL LSB" + boton **Restablecer** | `02-editor-inline.png` |
| Filas seleccionables con valor | **LOW 5 Hz** (resaltada) / HIGH 2145 / BW 2140 | `02-editor-inline.png` |
| Grafico | "Esquema de banda pasante" con escala **0..5000** (mas fina que la 0..10000 anterior) | `02-editor-inline.png` |
| Banda pasante en el espectro | conserva la del PR #174 (fill + 2 lineas, texto "2140") | `02-editor-inline.png` |
| Celdas de abajo | **LOW · HIGH · BW · Eq · Waterfall** (sin "Cerrar DFL") | `02-editor-inline.png` |
| 5.a celda "Waterfall" | **colapsa** el panel a la fila compacta (Filter low/high/BW + Eq + **"Filtro grafico"**) = segunda vista | `03-waterfall.png` |
| Volver / cerrar | cambiar de pagina (GEN) cierra el panel y deja la pantalla normal | `04-cierre.png` |
| Estabilidad | **PID 18936 sin cambios**, 0 logs nuevos, sin `CRASHLOG` | — |

## No cubierto por este rol (lo valida el owner en la radio)

- **La rueda VOL (rotary1)** editando la fila seleccionada: el control remoto no gira el encoder.
- **Touch** en filas/handles y en "Restablecer" / "Waterfall" / "Filtro grafico": solo se pueden pulsar
  las 5 celdas de abajo por socket, no coordenadas.

## Estado dejado

Pagina **GEN**, editor cerrado, frecuencia/modo/filtro del dueno **intactos** (7.120.653 LSB, BW 2140),
RX, sin TX.
