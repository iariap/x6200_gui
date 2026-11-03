# Gate de integridad del firmware BASE X6200

## Alcance

Análisis offline de las imágenes 1.0.6/1.0.8 descifradas, del updater vendor y del candidato.
**No se escribió el MCU; no se enviaron comandos IAP.**

## 1. `calCRC32_STM32` en la GUI vendor

- 1.0.6: `xcrc::calCRC32_STM32` @ `0x3DF14`.
- 1.0.8: `xcrc::calCRC32_STM32` @ `0x3E57C`.
- **Único llamador en cada binario**: `0x3D3AC` / `0x3DA14`, dentro de
  **`DialogFactoryMode`** (modo fábrica/calibración).
- Calcula CRC sobre **203 words** en `objeto+0x98` y guarda en `objeto+0x3C4`.
- **No hay llamadas desde `UiFwUpgrade`, `XMWorker` ni `Xmodem`.**

Conclusión: esa función valida **datos de calibración/fábrica**, no la imagen de firmware.

## 2. Actualizador

- `UiFwUpgrade::upgradeFirmware` lee el `.xgf` completo y lo entrega a `XMWorker`.
- `XMWorker` usa `Xmodem::crc16` **sólo para los paquetes XMODEM**; no calcula CRC/hash de la
  imagen completa.
- No hay imports de SHA/AES/OpenSSL en la GUI; el AES y la aceptación están en el MCU/bootloader.
- **No se encontró** un campo de CRC/hash/firma global que la GUI calcule o transmita.

## 3. Imágenes descifradas

- 524288 B exactos; padding final de `0xFF`.
- Al final, tras el padding, hay **4 bytes no-FF**:
  - 1.0.6: `b7 0c 5f c6` (LE `0xC65F0CB7`)
  - 1.0.8: `79 46 9e 24` (LE `0x249E4679`)
- Se probaron, sin coincidencias reproducibles:
  - CRC32 estándar/reflejado (zlib), init 0 y `0xFFFFFFFF`, xorout 0/`0xFFFFFFFF`;
  - CRC-32/MPEG-2 (polinomio `0x04C11DB7`, no reflejado);
  - CRC-32/STM32 con procesamiento **word-wise** (como `calCRC32_STM32`);
  - regiones: imagen completa, imagen sin últimos 4 B, últimos 4 B puestos a cero, datos sin padding,
    mitades y prefijos plausibles.
- **No se identificó** el campo de 4 bytes; podría ser checksum, metadato o dato de cola.
  No se actualizó el candidato.

## 4. Estructura

- Vector table al inicio (SP/reset); código; datos; padding final FF; sin cabecera de longitud,
  firma o tabla de hashes visible.
- 1.0.6 y 1.0.8 difieren en código/datos; el padding y la cola de 4 B no alcanzan para identificar
  un checksum global.

## 5. Gate

| Gate | Resultado | Evidencia |
|---|---|---|
| `XGF_INTEGRITY` | **PARTIAL** | AES/round-trip verificado; checksum global no identificado |
| `ADDITIONAL_CHECKSUM` | **INCONCLUSIVE** | cola de 4 B no reproducible; no caller de updater |
| `IAP_ACCEPTANCE_CONTRACT` | **UNKNOWN** | no hay lectura/verify; bootloader no disponible |
| `MCU_VERSION` | **UNKNOWN** | no hay comando de versión/lectura seguro |
| `IAP_ENTRY` | **UNVERIFIED** | no se ejecutó handshake; updater no probado |
| `MCU_RECOVERY` | **UNVERIFIED** | SD sólo recupera Linux; sin SWD/BOOT0 |
| `BASE_PATCH_READY` | **NO** | integridad/aceptación/recuperación sin verificar |

## 6. Decisión

**NO-GO** para el primer flasheo.

No hay un checksum reproducible que actualizar, pero tampoco está demostrado que el bootloader no
valide algo. La incertidumbre de aceptación, versión instalada y recuperación impide un flash
responsable.

## 7. Preflight

- GUI original `e32a81f3…` respaldada; `params.db` con backup; SD vendor y oficiales 1.0.6/1.0.8
  con hashes.
- Updater vendor no se inició desde el Linux actual (faltan dependencias Qt/vendor); no se probó
  handshake IAP ni entry.
- No se enviaron erase/XMODEM.
- El fallback oficial sólo sería útil si el bootloader/IAP sigue accesible; no está verificado.


## 8. Referencia pública G90Tools

`G90Tools` documenta que el bootloader G90 recibe firmware **AES-256-ECB** por UART, lo descifra y
lo escribe; no documenta CRC/firma de imagen ni un checksum de 4 bytes. Es una referencia
metodológica, **no** prueba del bootloader X6200.

## 9. Dependencias/entrada IAP

`x6200_ui_v100` 1.0.8 requiere Qt5/qwt/NE10/liquid y otras libs que **no están** en el Linux actual;
no se pudo iniciar el updater vendor. No se ejecutó handshake ni entry IAP. Ver `iap-analysis.md`.

## 10. Incógnitas que requieren escritura/lectura

- valor/semántica real de los 4 bytes finales;
- si el bootloader valida checksum/firma;
- si el bootloader entra a IAP sin la app;
- versión BASE instalada;
- recuperación MCU.

No se presentan como “rechazo”: son límites de la evidencia offline.


## 11. Ronda adicional — 4 bytes finales

Se ampliaron las pruebas sin fuerza bruta:

- CRC32C, JAMCRC, CRC32/POSIX, Adler-32, FNV-1a y suma de bytes sobre imagen completa, sin últimos
  4 B, últimos 4 B a cero y región sin padding: **sin coincidencia**.
- CRC32/MPEG-2 word-wise (equivalente al bucle de `calCRC32_STM32`) sobre **todos los prefijos**
  word-aligned de ambas imágenes: **sin coincidencia**.
- CRC sobre imagen **encriptada** y variantes: **sin coincidencia**.
- La cola de 4 B sigue **no identificada**. No se modificó el candidato.
- `ADDITIONAL_CHECKSUM=INCONCLUSIVE`; no hay evidencia suficiente para afirmar que sea un checksum
  ni para actualizarlo.


## 12. Hipótesis de residuo CRC (imagen completa, con footer)

Se calculó el CRC de la **imagen completa incluyendo los 4 bytes finales** para 1.0.6/1.0.8:

| Variante | 1.0.6 | 1.0.8 |
|---|---:|---:|
| CRC-32/ISO-HDLC (zlib) | `0x79F3C62A` | `0xA12FCFB3` |
| CRC-32/MPEG-2 | `0xDA8EACCB` | `0x2FE9E2AC` |
| STM32 word-wise | `0x6EAECD5A` | `0x2C076777` |

No comparten un residuo constante (ni 0, ni `0xFFFFFFFF`, ni un mismo valor) en ninguna variante
justificada. **La hipótesis de residuo CRC no se confirma.**
