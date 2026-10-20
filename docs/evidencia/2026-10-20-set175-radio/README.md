# Fix de #175 verificado en la radio (commit `9d72bde1` / build `13e65963`)

- Rama `lu2dsx/x6200-rom`, merge **`9d72bde1`** ("Ajustes: una sola columna de widgets a la derecha
  (issue #175) (#178)").
- Build compilado y desplegado por este rol: **`13e6596350d2cc1b4c7883d855ae7108`**, gate del #86 OK
  (ELF32 ARM, 13.870.048 B, 3/3 marcas), GUI relanzada por el daemon, equipo sin reiniciar.

## Medido: borde izquierdo de los controles, fila por fila (radio real, 800x480)

| Pagina | Fila | Antes | **Ahora** |
|---|---|---|---|
| Audio | Line-in, Line-out | 499 | **499** |
| Audio | Play,Rec gain | 499 | **499** |
| Audio | Audio enhance ("SSB voice") | 499 | **499** |
| Audio | **Speaker mode** | **643** | **499** ✓ |
| Audio | Voice mode | 499 | **499** |
| Operation | Freq acceleration / Bluetooth / Discoverable | 499 / 643 / 643 | **499 / 500 / 500** ✓ |

**Conclusion**: todos los controles de una sola pieza arrancan ahora en **x≈499** (la columna
`COL_CONTROLES` del commit), el origen x≈643 desaparecio y las filas de 2 piezas usan 499 + 644 como
antes. Es lo que pide #175.

## Otros chequeos

- Barrido de 23 apps: **24/24 ok, 0 "REINICIO", 0 "SE CAYO", 0 "no responde"** (esta vez sin los
  transitorios de SSH que dieron falsos positivos en el barrido del build anterior).
- `verifica-radio`: `TODO OK`.
- Capturas: `pag-0..4.png` (Operation, Audio, Spectrum, Clock, More).

## Estado dejado

- Frecuencia/modo/filtro del dueno restaurados (**7.173.000 LSB BW 2980**), pagina GEN, RX, sin TX.
