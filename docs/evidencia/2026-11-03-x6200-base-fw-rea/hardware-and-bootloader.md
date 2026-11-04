# X6200 — mapa de hardware, arranque y bootloader (evidencia parcial)

## 1. Identificación del MCU

- **Referencia pública** (handoff): placa X6200 con **STM32H725**, FPGA **Intel Cyclone 10**,
  codec USB **CM108B** y SoC **Allwinner A33**. Grado: **PUBLIC_REFERENCE**, no verificado
  localmente contra fotografías/boardview porque no hay acceso físico.
- **Evidencia local**:
  - Linux corre en A33 (`sun8i-r16`, `armv7l`); DT sólo describe A33.
  - A33 USB host: únicamente WLAN RTL8723DU; **CM108B no enumera** en Linux.
  - ALSA: una sola tarjeta `sun8i-a33-audio` (AIF1); no hay tarjeta USB de audio.
  - H725/FPGA no aparecen en DT de Linux: están del lado BASE, no mapeados por A33.
- **Modelo exacto/revisión**: no confirmado. H725 asumido por referencia pública; no se aplican
  direcciones ni periféricos de STM32F427.
- **H725 (según datasheet ST)**: Cortex-M7, **1 MB flash dual-bank**, 564 KB RAM, AES/HASH,
  USB, USART, SWD/JTAG; BOOT0 selecciona flash o bootloader ROM; RDP 0/1/2 y option bytes.
  La imagen `.xgf` de 512 KB encaja con **un banco/app** de 512 KB, pero no se probó.

## 2. Arranque y programación

- **A33/Linux**: U-Boot 2017.11 (SPL) desde eMMC/SD; kernel 6.1.82; DTB
  `sun8i-r16-x6200.dtb`; recuperación Linux por SD/U-Boot/`install_emmc.sh` (vendor).
- **BASE/MCU**: actualización desde la GUI vendor (`UiFwUpgrade` + `XMWorker`) por IAP/XMODEM.
  No hay bootloader documentado, ni lector, ni vector table, ni mapa de flash.
- **Recuperación MCU**: **no verificada**. La SD/U-Boot sólo recupera Linux, no el MCU.
- **RDP/SWD/BOOT0**: no se intentó desbloquear, leer option bytes ni borrar memoria.

## 3. `.xgf` — análisis estructural (herramienta `tools/xgf_analysis.py`)

| Métrica | 1.0.6 | 1.0.8 |
|---|---:|---:|
| Tamaño | 524288 | 524288 |
| SHA256 | `dca90387…` | `9a19f97a…` |
| Entropía global | 7,7484 | 7,4704 |
| Bloques 16 B repetidos | 9002 | 11791 |
| Bloque ciphertext más repetido | `6c954341…` ×5087 | idem ×9640 |
| Segundo más repetido | `7a491eeb…` ×3214 | idem ×1936 |
| Compresión estándar | no | no |

Hallazgo clave: hay **regiones de 512/4096 B donde un mismo bloque de 16 B se repite
exactamente** (p. ej. offset 348160 y 446464), con entropía 4,0 y 255/358 bloques de baja
entropía. Eso es compatible con **AES-ECB sobre regiones de relleno constante** (o con datos
constantes sin cifrar), no con un flujo CBC/CTR continuo. La hipótesis **AES-ECB** explica:

- alta entropía en código/datos;
- bloques de 16 B idénticos repetidos en zonas de padding;
- regiones de 4 KB idénticas entre 1.0.6 y 1.0.8 (mismo plaintext constante, misma clave).

No se dispone de clave ni de imagen clara; **no se demostró** el algoritmo, sólo su compatibilidad
estructural. No se intentó fuerza bruta ni se importaron claves de otros equipos.

## 4. IAP

`UiFwUpgrade` lee `*.xgf` completo, `XMWorker` abre UART/IAP, borra app flash y transfiere por
**XMODEM** (CRC16 de transporte). El GUI **no** contiene imports de AES/OpenSSL/mbedtls: el
descifrado/validación ocurre en el MCU/bootloader. No hay comando de lectura/verify documentado;
la aceptación final (AES/CRC/firma) no se conoce. El CRC16 de XMODEM no autentica la imagen.

## 5. Arquitectura digital RX y CM108B

- Cadena probable según referencia y mediciones: RF ADC → **FPGA Cyclone 10** (DDC/demod) → PCM →
  **STM32H725** (audio/control) → salida analógica → **MIC1 del codec A33** → ADC/AIF1 → Linux.
- El **parlante interno** es conmutado por el lado BASE/H725: BASE+ZERO suena RX; DAC+ZERO silencio;
  DAC+TONO sólo tono; BASE+TONO sólo RX. `voice_rec=0x8` cambia ese selector y además deja sin RX
  útil al ADC de Linux.
- **CM108B** no enumera en A33 (`lsusb`: sólo WLAN), no hay tarjeta ALSA USB ni segunda captura.
  No se puede usar como fuente RX independiente en Linux con el estado actual.
- **`samples[512]`** del flujo BASE es espectro uint8, no PCM; no se usa.

## 6. Conclusión

No se localizó el código del mute: no hay imagen MCU clara ni mapa. La evidencia apunta a que el
mute/selector está en el **H725/BASE**, no en A33; no se puede parchear sin clave/lectura o
depuración física. `BASE_PATCH_READY=NO`, `MCU_RECOVERY=UNVERIFIED`, `PRODUCTION_READY=NO`.
