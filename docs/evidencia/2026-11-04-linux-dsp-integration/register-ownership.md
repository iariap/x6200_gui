# Propiedad del registro 12 y bits compartidos

Registro 12 (`x6200_sple_atue_trx`, I2C `addr=12*4=0x0030` BE + arg LE32):

| bit | uso | regla con la feature ON |
|---|---|---|
| 0x00000008 | `voice_rec` (BASE/DAC) | **sólo el árbitro** lo cambia; GUI no puede enviarlo por `APPLY_SHARED` |
| 0x00000010 | swrscan | preservado; si aparece, disable-first (TX mask) |
| 0x00001000/0x2000 | ATUE/ATU tune | preservado; disable-first |
| 0x00004000 | modem | preservado; disable-first |
| 0x00010000 | poweroff | preservado; disable-first |
| 0x00040000 | iptt | preservado; disable-first antes de aplicar |
| resto | otros campos | preservados bit a bit desde la sombra autoritativa |

- La sombra se actualiza **sólo** por escrituras exitosas del árbitro; `apply_shared` rechaza targets
  con `voice_rec` y hace `disable_rx` antes de cualquier bit de TX.
- Sin readback del MCU (R′ #370), el árbitro no reconstruye bits por suposición: si la sombra es
  inválida bloquea RX_LINUX; si el bus falla ⇒ FAULT (no se asume valor físico).
- El supervisor no escribe el registro (prohibido): pide restore al árbitro; si no responde ⇒ FAULT.
