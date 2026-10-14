# Banco de pruebas sin radio

Corre el `x6200_gui` **real** (el binario ARM que va en la imagen) bajo `qemu-arm-static`, dentro de
un chroot del rootfs, con un framebuffer falso, y lo comanda por su socket de control. No hace falta
la radio.

- `fakefb.c` -> `libfakefb.so`: intercepta el framebuffer y escribe `480x800x32 BGRA` en
  `$CHROOT/tmp/fakefb0`, que es de donde se sacan las capturas.
- `crash.c` -> `libcrash.so`: si la GUI muere, deja el backtrace en `$CHROOT/crash.txt`.
- `nobuf.c` -> `libx6200trace.so`: traza los `open`, `fopen` y `pthread_create`.

Uso: `tools/captura-banco.sh` (y el skill `.agents/pruebas-sin-radio/SKILL.md`, que explica todo el
procedimiento, como comandarla desde afuera y que garantias tiene).
