# X6200 BASE firmware REA — `.xgf`, IAP y mute RX

Fecha: 2026-11-03. Rama `research/x6200-base-audio-rea`, base **main remoto
`e1476ecef355e8c3e5084e577ecc35689d4f326d`** (el handoff citaba `681074e0`; el remoto avanzó
antes de esta corrida; se documenta la diferencia). Buildroot main `745af77e` verificado; no se
modificó Buildroot.

Referencias: Epic #362, PR experimental #385 (head `68c501a8`, sin merge).

## Artefactos verificados

| Artefacto | Ruta | Bytes | SHA256 |
|---|---|---:|---|
| MCU 1.0.6 | `refs/vendor-fw/x6200_mcu_20250111134652.xgf` | 524288 | `dca90387b70d9550448006310a69ea65625bec87f402c10df789deeb13ff12f3` |
| MCU 1.0.8 | `fw108-scratch/ext108/fs108/x6200_mcu_20260820141208.xgf` | 524288 | `9a19f97a5ad9e141a3663d222f984073bf0b18b38d4662604372a0036f9daa23` |
| GUI vendor 1.0.6 | `refs/vendor-fw/x6200_ui_v100` | 1649256 | `b76331c5aa859ae98dc1819b1079a3e40489e9986c9656e0dc2c553aeb28871b` |
| GUI vendor 1.0.8 | `fw108-scratch/ext108/fs108/x6200_ui_v100` | 1684984 | `26623266bfcd3ed4d2e08b03b9f023cef809579f5ed497899670fc01f66e9b2b` (paquete **UNVERIFIED_VENDOR_PAYLOAD**; discrepancia documental de hash ya registrada en #374/#376) |

## Hallazgos

### `.xgf`

- Ambos son **524288 B exactos**, sin cabecera/magic reconocible; `file` = `data`.
- Entropía 7,75 (1.0.6) / 7,47 (1.0.8); ASCII 38,8%/39,5%; sin símbolos legibles.
- No son gzip/zlib/bzip2/lzma estándar.
- 1.0.6 y 1.0.8 no son idénticos; la diferencia es extensa. Alineados a 128 B (tamaño múltiplo de
  XMODEM) pero **no se pudo demostrar** que sea texto plano.
- **Hipótesis principal**: imagen cifrada/obfuscada para el bootloader; no se asume que alta
  entropía pruebe cifrado.

### GUI/IAP

- `UiFwUpgrade::upgradeFirmware` lee `/usr/firmware/*.xgf` **completo y sin transformación** y lo
  entrega a `XMWorker::slot_start(QByteArray,int)`.
- `XMWorker` habla **IAP/XMODEM**: strings `[OK] Start IAP connection`, `Start IAP erase app flash`,
  `Start IAP xmodem transfer`, `Writing Flash`; símbolos `Xmodem::crc16`, `xcrc::calCRC32_STM32`,
  `send_cmd`, `uart`.
- **No hay símbolos/imports de AES/OpenSSL/mbedtls ni función de cifrado en el GUI**: la
  transformación, si existe, ocurre en el MCU/bootloader.
- El CRC16 de XMODEM sólo protege el transporte; no autentica la imagen. La validación final
  (CRC32/STM32, firma, cifrado) no está documentada.
- **MCU**: los indicios `xcrc::calCRC32_STM32` + XMODEM/IAP apuntan a un STM32-class, pero la
  identidad exacta y el bootloader no están confirmados.
- **Recuperación**: la única vía conocida es el IAP oficial desde la GUI. No hay BOOT/ISP/SWD/JTAG
  verificado ni procedimiento de recuperación si el MCU queda inválido. Restaurar Linux/SD no
  revierte el MCU.

## Etapa B — mute RX

No se pudo obtener una imagen MCU en claro, mapa de memoria, vector table ni bootloader. Sin
código no se puede localizar la lógica que mutea la RX, ni distinguir A/B/C/D de la hipótesis
`voice_rec`. El único contrato observable sigue siendo el bit `0x8` del registro 12; no hay
readback ni ACK de estado físico.

## Decisión

- No se fabrica un `.xgf` ni se parchea el MCU: sin imagen clara, sin contrato de aceptación y sin
  recuperación verificada sería una escritura ciega e irreversible.
- No se modifica Linux/kernel/DTB: el codec/driver no tienen una segunda entrada RX.
- No AEC: la RX desaparece; no hay referencia que cancelar.

Clasificación:

- `MCU_IMAGE_CLEAR = NO`
- `IAP_ACCEPTANCE_CONTRACT = UNKNOWN`
- `MCU_RECOVERY = UNVERIFIED`
- `BASE_PATCH_READY = NO`
- `BASE_EXPERIMENTAL_FLASHED = NO`
- `INDEPENDENT_RX_CAPTURE = FAIL` (QA14)
- `LINUX_DSP_SPEAKER = NOT_TESTED` (sin fuente RX independiente)
- `PRODUCTION_READY = NO`

## Límite técnico

Para avanzar legal y técnicamente hace falta al menos uno de:

1. imagen MCU descifrada/source o bootloader documentado;
2. llave/contrato de aceptación IAP;
3. recuperación BOOT/ISP/SWD/JTAG verificada.

Sin eso no hay modificación mínima demostrable. No se publica firmware propietario, claves ni
calibraciones; sólo hashes y análisis.
