# Preflight de #378

## OEM y hardware

- APP **V1.0.8** (Aug 17 2026, 16:21:06)
- FIRMWARE/BASE **V1.0.8** (Aug 19 2026, 16:13:08)
- FPGA **V1.0.0**; HARDWARE **V1.0.0**
- OEM RX/TX confirmados por el propietario antes de la prueba.

## Imagen nueva

- `sha256 = 1e3a9748754a758d4ac83aab673812a7313371a48ec0793884d2f66717e2339b`
- 571.473.920 B; MBR; p1 FAT32 32 MiB; p2 ext4 512 MiB; sin p3.
- GUI: `03356797959d21c35500d95c23bbc04d8d1f0ac922a89ec3681215d4fdcb982e`
  (PR #381 `57f671e8`; `sub #126` y `veor q12` verificados).
- Lib X6200Control: `e93d79a376a974bea61301ee5c90eb0f951d79c00f06918ccdc7827e59d6cad5`.
- `S01create_data`: `da7d19b13d853cbe6e764f60b62194203b704cb595e5f0f3837a3b8f398c6963`.

## Material

- SD OEM 1.0.8 de rollback.
- SD de prueba (la imagen nueva).
- Fuente/alimentación según manual.
- Acceso físico al apagado.

## Fuera de alcance

TX/PTT/ATU, MUX experimental, cambios de BASE/MCU, calibración, I²C/UART raw.
