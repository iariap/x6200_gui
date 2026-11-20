# Flash experimental autorizado — candidato voice-keep-rx

## Autorización

El propietario autorizó explícitamente escribir:

- archivo: `x6200-108-voice-keep-rx.xgf`
- SHA256: `cc7b4acae26887d0620f633f6a8a695419460f3201554c163120a3960ac2b6b9`
- un único flash experimental, aceptando riesgo de brick.

## Incidente: primer flash usó el archivo equivocado

La primera ejecución del updater abrió y escribió el firmware **oficial**:

`/usr/firmware/x6200_mcu_20260820141208.xgf`

Log: `open firmware file  "/usr/firmware/x6200_mcu_20260820141208.xgf" ok.`
IAP: 524288 bytes, erase + 512 paquetes XMODEM, `XMWorker finished`,
`UiFwUpgrade finished 0`.

El BASE dejó de responder tras el IAP; se reinició la radio y volvió a comunicar:
`BASE fw version: 1.0.8`, `BASE fw date: Aug 19 2026,16:13:08`.
No hubo brick y no se escribió el candidato.

## Segundo flash: candidato correcto

Se recreó el entorno vendor con EGLFS original (`/etc/profile`, `QT_QPA_EGLFS_ROTATION=90`,
`/etc/qtkmsconfig.json`, `card1`, modo 800x480), frutas/fontconfig y captura por DRM
(`DRM_IOCTL_MODE_GETFB2` + PRIME/mmap, no `/dev/fb0`).

El updater abrió:

`open firmware file  "/usr/firmware/x6200-108-voice-keep-rx.xgf" ok.`

Log de programación:

- `XMWorker(0x3cc070) started, 524288`
- `[OK] Get IAP respond`
- `[OK] Start IAP connection`
- `[OK] Start IAP erase app flash`
- `[OK] Start IAP xmodem transfer`
- 512 paquetes de 1024 bytes, 100 %, `rem: 0`
- `[OK] xmodem send finished.`
- `XMWorker finished.`
- `UiFwUpgrade finished 0`
- `XMWorker destroyed.`

El hash del archivo copiado al overlay `/usr/firmware` era:
`cc7b4acae26887d0620f633f6a8a695419460f3201554c163120a3960ac2b6b9`.

Después del IAP se reinició la radio. BASE volvió a comunicar con `BASE fw version: 1.0.8`
(misma versión; el parche es de comportamiento, no cambia la versión).

## Estado

- Candidato aplicado por updater vendor; sin cliente IAP alternativo.
- Radio arranca, GUI original operativa, BASE comunica.
- Sin TX/PTT/ATU/Bluetooth.
- No se hizo readback del MCU ni se verificó aún el comportamiento de `voice_rec` en audio.
