# Validación offline (honesta)

## Qué se validó (ejecutado)

`tools/native-dsp-rea/tests/test_register_mapping.py` — **9 tests OK** (`logs/qa19-tests.txt`):
1. índices de registro (`x6200_cmd_enum_t`) y perfiles 45–50;
2. empaquetado SSB `low<<16|high` (50/2950 → `0x00320B86`) y preservación al set_low/set_high;
3. modos simétricos CW/AM/NFM/WFM `(-high)<<16|high` (CW 250 → `0xFF0600FA`; AM 4500 → `0xEE6C1194`);
4. campos NR/NB (enable bit24/25, niveles/ancho) → `0x030A1405` para el caso de prueba;
5. DNF centro/ancho/modo (1000/50/auto → `0x02032000|1000`);
6. AGC knee/slope/hang (defaults -60/6/off → byte `0xC4`, slope 6) y hang;
7. RX EQ 5×5 bits + enable, decodificación signed-5 (`-16..15`) y máscara `0x1F` para negativos;
8. defaults de modo del GUI (LSB 50..2950; CW -250..250; AM/NFM/WFM low=0; espejo LSB) y regla
   "negativo ignorado en DB excepto CW";
9. defaults reales de la GUI (`src/cfg/cfg.c`: knee -60, slope 6, dnf 1000/50/off, nb 10/10, nr 0,
   EQ 0).

Esto valida la **reconstrucción del mapeo GUI→X6200Control→registro** (VERIFIED); no el DSP.

## Qué NO se puede validar offline (PENDING)

- **Respuesta en frecuencia/atenuación/distorsión** de DFL/NR/NB/DNF/AGC/EQ: el algoritmo corre en el
  firmware nativo (FPGA/DSP) y **no hay bitstream FPGA** en el set de fábrica; reconstruir una
  aproximación y presentarla como medida del firmware sería engañoso. No se simula.
- **Latencia** audible y **efecto subjetivo**: requieren radio + owner (prueba física autorizada).
- **Efecto real de cada parámetro**: el MCU marca dirty y aplica en un lazo no resuelto; los bits están
  verificados, pero su semántica física (Hz/dB/tiempo de cada escala) es UNKNOWN.

## Qué medir en la validación física futura (cuando se autorice)

1. A/B por parámetro con señal débil/ruido/interferente y captura externa: DFL (ancho/edges),
   NR on/off+nivel, NB on/off+nivel/ancho, DNF manual/auto centro/ancho, AGC knee/slope/hang,
   RX EQ banda a banda y WFM; respuesta aproximada por barrido de tono.
2. Continuidad de RX y parlante durante cambios de parámetro; sin USB/BT/externo.
3. Registro de configuración antes/después y restauración a defaults.
No se declara ningún resultado audible en este PR.
