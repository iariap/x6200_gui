# `voice_rec` — lógica localizada en la imagen 1.0.8

## Flujo

1. El GUI Linux escribe el bloque de workparams por I2C3.
2. La app decodifica el **registro 12** en **`0x08041C3A`**:
   - lee `global_A+0x30` (`global_A=0x240276E0`);
   - **bit 3 (`0x8`) = `voice_rec`** → se guarda en **`global_B+0x75`**
     (`global_B=0x240277B8`, byte `0x2402782D`);
   - otros bits se copian a flags de configuración;
   - bit `0x1000` se trata por separado con `0x080468DC`.
3. Un consumidor en **`0x080428F0`** detecta el cambio del byte `+0x75` y llama a
   **`0x08045320(voice_rec)`**.
4. **Handler `0x08045320`** (pseudocódigo):

```c
void voice_rec(int on) {
    if (!on) {                         // BASE
        spi_write(handle, 1, 12, 0x08);
        spi_write(handle, 1, 24, 0x28); // 40
        audio_config_base();
        flag = 0;
    } else {                           // DAC
        spi_write(handle, 1, 12, 0x0A);
        spi_write(handle, 1, 24, 0x00); // 0  <-- corta la RX hacia el ADC
        configure_dac_path();          // 0x08043840/0x080439B4/0x080432A8
        flag = 1;
    }
}
```

## Causa del mute

La diferencia observable es el **segundo write SPI: registro 24 = 0x28 en BASE vs 0x00 en DAC**.
La hipótesis más fuerte es que **registro 24 controla el camino RX hacia la captura** (mute), y
**registro 12 (0x08/0x0A)** selecciona la fuente de audio/parlante. El **registro 12 no se toca**
en el parche candidato: sigue en 0x0A para conservar DAC en parlante.

**Nivel de evidencia**: código + registros + comportamiento QA11–14. No hay confirmación física
del nombre del registro ni del FPGA; `RX_MUTE_CAUSE = IDENTIFIED_OFFLINE` (no `MEASURED`).

## Consecuencia

- **BASE original**: reg24=0x28, RX disponible.
- **DAC original**: reg24=0x00, RX desaparece.
- **Parche candidato**: en la rama DAC, reg24=0x28 en lugar de 0x00, manteniendo reg12=0x0A.
- **No se cambia** TX/PTT/ATU, calibración, flags de la rama BASE ni la restauración.
