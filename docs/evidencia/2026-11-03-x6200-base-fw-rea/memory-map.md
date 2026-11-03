# X6200 BASE — mapa de memoria recuperado

## Imagen

- `.xgf` / imagen descifrada: **524288 B** (512 KiB).
- **Base de ejecución: `0x08020000`** (no `0x08000000`; corregido al alinear el reset).
- Rango: `0x08020000`–`0x0809FFFF` (un banco/app de 512 KiB dentro de H725 de 1 MiB).
- **SP inicial** `0x24050000`; **reset handler** `0x08024880` (vector 0x08024881, Thumb).
- `0x08000000`–`0x0801FFFF`: 128 KiB previos no incluidos (bootloader/región de arranque
  probable; no verificado).

## Tabla de vectores (archivo offset 0, cargada en 0x08020000)

- offset 0x000: `0x24050000` (SP)
- offset 0x004: `0x08024881` (reset)
- offset 0x008–0x03C: excepciones (`0x0802…`)
- offset 0x040: IRQ0; handlers no-default dentro de la app.

## Rutas identificadas

| Símbolo/lógica | Dirección | Evidencia |
|---|---|---|
| Reset/startup | `0x08024880` | SP load + copia de datos + BSS + llamada a init |
| Init principal | `0x08022A18` | llamadas a SPI3, UARTs, I2C3, FMC, DAC, I2C1/2/4 |
| I2C3 init (slave addr `0xE4` = 0x72<<1) | `0x08021620` | handle `0x24005930`; 0xE4; HAL |
| I2C3 command/IRQ handler | `0x08044B54` | parser de comandos 0x1000/0x1100/0x1200/0xF000 |
| Decode de registro 12 | `0x08041C3A` | lee `+0x30`, bits 3/4/5/6… y 0x1000 |
| Bandera `voice_rec` | `0x2402782D` | byte `B+0x75`, `B=0x240277B8` |
| Consumidor de bandera | `0x080428F0` | llama a `0x08045320(voice_rec)` |
| Handler `voice_rec` | `0x08045320` | ramas 0/1; SPI writes; audio path |
| SPI write helper | `0x08044470` | handle `0x24005310`; escribe registros 12/24 |
| DAC init/channel | `0x08020F9C` / `0x08021020` / `0x08021098` | DAC base 0x40007400 |
| FMC init | `0x08023E68` | FMC base 0x52004000 (FPGA) |
| SPI3 init | `0x08021A58` | incluye 48000 Hz |

## Limitaciones

- No hay mapa de periféricos FPGA ni símbolos; nombres de registros 12/24 son **inferidos**.
- `0x08000000`–0x0801FFFF no está en la imagen; no se leyó el bootloader.
