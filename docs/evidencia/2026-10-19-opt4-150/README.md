# Evidencia QEMU — OPT-4 #150 (2026-10-19)

Capturas del firmware **real** del commit de código `923e4dea` corriendo con
`qemu-arm-static` (`QEMU_CPU=cortex-a7`) sobre un chroot del rootfs de la imagen,
comandado por el socket de control (`tools/captura-banco.sh`).

- `fuente: md5 a3c8a64e7943` (`src/` + `lvgl/src`)
- `binario: md5 9a26b607c67d` (`/usr/sbin/x6200_gui`, 13 855 068 B, gate
  `verifica-firmware-port.py` 3/3 marcas)

| PNG | Orden por socket | Bytes | sha256 |
|---|---|---|---|
| `banco-home.png` | handshake `{"cmd":"get"}` | 20891 | `a59f3b16…` |
| `banco-settings.png` | `{"cmd":"app","text":"Settings"}` | 28183 | `7ae1a753…` |
| `banco-bands.png` | `{"cmd":"app","text":"Bands"}` | 23596 | `de8dda4b…` |

QEMU user-mode **no** es un Cortex-A7 ni mide batería; sólo prueba que el binario
arranca, dibuja y responde al control por socket con los cambios de #150.
