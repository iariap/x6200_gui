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

## 6. Software original descargado de la radio (SD vendor, sólo lectura)

Se montó `/dev/mmcblk2p2` (rootfs vendor original, 383 MB usados) en modo **ro** y se descargaron
selectivamente artefactos a `fw108-scratch/qaref-vendor-sd/` (fuera del repositorio), sin publicar
binarios ni datos privados.

| Artefacto vendor | SHA256 |
|---|---|
| `usr/app_qt/x6200_ui_v100` (1.0.8) | `26623266bfcd3ed4d2e08b03b9f023cef809579f5ed497899670fc01f66e9b2b` |
| `usr/firmware/x6200_mcu_20260820141208.xgf` | `9a19f97a5ad9e141a3663d222f984073bf0b18b38d4662604372a0036f9daa23` |
| `etc/asound.conf` | `574a78285b71006e6e5ced69fbc6f8360c4a7372a702273155e8123cd49a1d02` |
| `usr/share/support/update` | `e48e378a8d7627a596d8f96a2c658f83e867015bb59177e29cb427bd71746c86` |
| `S51amixer` | `2f96676a9cec789b23739de89b70bd556c9591cf436e556bd9398fe6628a81aa` |
| `emmc_sources/install_emmc.sh` | `25b8f41e01e916c5dd7966d33613ce951536ac96f35b22c174a8608f2b2dd401` |
| `emmc_sources/u-boot-sunxi-with-spl.bin` | `ccf63f9a558c725eb644b3246f786628b6c1c7cbecfb7f1a627fb54fafa2053e` |

### Lo que confirma

- **`S51amixer` vendor**: `AIF1 Data Digital ADC Capture on`, `Mic1 Capture on`, `Mic2 mute`,
  `AIF1 Slot 0 Digital DAC Playback on`, `DAC/Headphone on`. Es exactamente la misma única fuente
  Mic1/AIF1 que el port; **no aparece LINEIN/Mic2 ni un control RX pre-mute**.
- **`/etc/asound.conf` vendor**: dmix/dsnoop sobre `hw:0,0`; no crea una segunda captura RX.
- **`/usr/share/support/update` vendor**: sólo actualiza kernel/rootfs de la eMMC con
  `install_emmc.sh` (U-Boot + zImage + DTB + `rootfs.tar`); **no actualiza ni recupera el MCU**.
- **`.xgf` vendor**: mismo contenedor opaco de 524288 B; el actualizador GUI lo envía por
  IAP/XMODEM. No hay imagen en claro, fuente, llave ni lector.
- La SD vendor sirve como **imagen de recuperación Linux** si se corrompe la eMMC, no como
  recuperación MCU.

### Consecuencia

Descargar el software original no cambia la conclusión: el MCU sigue siendo una caja opaca y la
ruta RX pre-mute no está expuesta. `MCU_RECOVERY` sigue **UNVERIFIED**; no se flasheó ni se escribió
en la SD (montaje ro). No se publica firmware propietario completo ni claves.


## 7. Deep REA 2026-11-03 (H725/FPGA, AES-ECB, CM108)

Se añadió `hardware-and-bootloader.md` y la herramienta reproducible `tools/xgf_analysis.py`.

- Referencia pública: H725 + Cyclone 10 + CM108B + A33; verificación local limitada a A33 (única
  tarjeta ALSA, CM108B no enumera en USB).
- `.xgf`: hay bloques de 16 B **exactamente repetidos** (5087/9640 veces) y regiones de 512/4096 B
  con entropía 4,0; compatible con **AES-ECB sobre padding constante**, no con CBC/CTR continuo.
  No hay clave/imagen clara; no se intentó fuerza bruta.
- IAP: GUI lee el archivo y lo envía por XMODEM; el GUI no descifra; la transformación/aceptación
  está en el bootloader MCU. Sin lectura/verify ni recuperación MCU verificada.
- Arquitectura probable: RF ADC → Cyclone 10 → H725 → analógico → MIC1 A33 → Linux; el mute de
  `voice_rec` está del lado BASE/H725, no en A33. CM108B no está disponible en A33.
- Clasificación sin cambios: `MCU_IMAGE_CLEAR=NO`, `IAP_ACCEPTANCE_CONTRACT=UNKNOWN`,
  `MCU_RECOVERY=UNVERIFIED`, `BASE_PATCH_READY=NO`, `BASE_EXPERIMENTAL_FLASHED=NO`,
  `INDEPENDENT_RX_CAPTURE=FAIL`, `LINUX_DSP_SPEAKER=NOT_TESTED`, `PRODUCTION_READY=NO`.


## 8. Gate de integridad 2026-11-03

Ver `integrity-gate.md`. `XGF_INTEGRITY=PARTIAL`; `ADDITIONAL_CHECKSUM=INCONCLUSIVE`;
`IAP_ACCEPTANCE_CONTRACT=UNKNOWN`; `MCU_VERSION=UNKNOWN`; `IAP_ENTRY=UNVERIFIED`;
`MCU_RECOVERY=UNVERIFIED`; `BASE_PATCH_READY=NO`. **NO-GO**; no se flasheó.


## 9. Preflight vendor 2026-11-03

Ver `vendor-preflight.md`: GUI vendor aislada arrancó, Baseband ready, sin IAP/erase; control
exclusivo; GUI original/ALSA restaurados. No se navegó la selección de firmware ni se probó IAP
entry. `FLASH_PREFLIGHT=PASS_STARTUP`; `FIRMWARE_SELECTION=NOT_EXERCISED`; `IAP_ENTRY=UNVERIFIED`.


## 10. Selección de firmware

Se intentó VNC para seleccionar el candidato sin Update; el plugin VNC abrió el puerto 5900 pero el
proceso cerró al conectar el cliente. No se inyectó input ni se pulsó Update. Flujo estático:
`upgradeFirmware` sólo es llamado por `slot_pushButtonClicked` (botón UPGRADE); selección no inicia
escritura. `FIRMWARE_SELECTION=UNVERIFIED`.


## 11. Autorización y bloqueo del flash

Autorización explícita recibida para `cc7b4aca…`. El updater vendor arrancó, pero la interacción
VNC/input no quedó operativa (auth tipo1, cliente cerrado; sin uinput). **No se envió IAP/erase/XMODEM
ni se escribió el MCU.** `FLASH_PREFLIGHT=FAIL` por control del updater; candidato no ejecutado.
Radio restaurada.


## 12. RFC 6143 corregido y aborto

Security type 1 = None; ClientInit directo. Aun así, el servidor VNC no entregó un
FramebufferUpdate utilizable y terminó; no se capturó pantalla ni se navegó. No se envió
input/Update/IAP/erase/XMODEM. Abortado; radio restaurada. `FLASH_PREFLIGHT=FAIL`.


## 13. RFB 3.3 corregido: sin framebuffer

Handshake None/ServerInit OK en un intento; SetPixelFormat/RAW/UpdateRequest sin respuesta; segundo
intento sin ServerInit. Proceso vendor terminó. No se envió input/Update/IAP/erase/XMODEM. Flash
abortado; radio restaurada. `FLASH_PREFLIGHT=FAIL`.


## 14. Control gráfico autónomo: sin vía viable

VNC software no entrega framebuffer y termina; Xvfb no está en host y vendor no tiene `libqxcb.so`;
LinuxFB no tiene `/dev/uinput` para input. No se envió IAP/erase/XMODEM. Flash abortado; radio
restaurada. `FLASH_PREFLIGHT=FAIL`.


## 15. evdev + LinuxFB: captura/injecção OK, labels corregidos, updater no alcanzado

`evdev_write` funciona sobre `/dev/input/event0`; framebuffer capturado/rotado; labels corregidos con
`QT_QPA_FONTDIR`. Se alcanzó el menú SETTING (GEN/AUDIO), pero no se localizó/activó NEXT hacia
Firmware Upgrade. No se seleccionó firmware ni se pulsó UPGRADE; sin IAP/erase/XMODEM. Radio
restaurada. `FLASH_PREFLIGHT=FAIL_UI_NAVIGATION`.


## 17. UI vendor girada 90°: opciones fuera de pantalla

Framebuffer 480×800; UI vendor girada; `/etc/profile` vendor fija EGLFS_ROTATION=90. Se probaron
rotaciones EGLFS/LinuxFB/fb0 y fuentes; las capturas no reflejan EGLFS y LinuxFB sigue girado.
NEXT/PREV no verificable; sin UPGRADE/IAP/erase/XMODEM; radio restaurada.


## 18. Flash autorizado del candidato

Ver `authorized-flash.md`. Primer intento escribió el oficial por error (`x6200_mcu_...xgf`);
la radio reinició y volvió a comunicar (BASE 1.0.8). Segundo intento escribió el candidato
`x6200-108-voice-keep-rx.xgf` (SHA256 `cc7b4aca…`) con IAP/XMODEM vendor: 524288 bytes, 512
paquetes, 100 %, `UiFwUpgrade finished 0`. Radio reiniciada y BASE 1.0.8. Sin TX/PTT/ATU/Bluetooth.
Prueba de audio DSP pendiente.
