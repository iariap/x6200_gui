# IAP/updater vendor — dependencias y secuencia (sin escribir)

## Dependencias del updater

`x6200_ui_v100` 1.0.8 (ELF ARM, 26623266…) requiere, entre otras:

`libQt5Core/Gui/Widgets/Quick/Multimedia/Charts/SerialPort/VirtualKeyboard`, `libqwt.so.6`,
`libNE10.so.10`, `libliquid.so`, `libasound.so.2`, `libnm`, `libKF5NetworkManagerQt`,
`libgpiod`, `libspeex`, `libssl1.1`.

En el Linux actual de la radio **no están instaladas** esas bibliotecas Qt/qwt. El binario vendor no
se puede ejecutar desde el rootfs del port; habría que arrancar el rootfs vendor desde SD o montar
un entorno aislado con sus libs.

## Secuencia estática

`UiFwUpgrade::upgradeFirmware`:

1. abre el `.xgf` y lee **todo** el archivo;
2. deshabilita la UI;
3. muestra “Erasing Flash”;
4. crea `XMWorker` y ejecuta IAP:
   - conexión IAP (`Wait IAP respond`, `Get IAP respond`, `Start IAP connection`);
   - **erase app flash**;
   - **XMODEM** (`Start IAP xmodem transfer`);
5. actualiza progreso/“Writing Flash”.

No hay un comando de **lectura/verify** en el GUI. `Xmodem::crc16` protege cada paquete; un ACK de
XMODEM sólo confirma recepción del paquete, **no** aceptación de la imagen ni arranque.

## Lo que no se pudo probar

- Entrada/salida del IAP sin erase: no hay un botón “solo handshake”; iniciar el updater lleva a
  erase. No se ejecutó.
- Independencia del bootloader respecto de la app: desconocida.
- Versión BASE instalada: no hay interfaz de consulta en el Linux actual; no se usó I2C crudo.
- Aceptación de imagen: el bootloader no está en el `.xgf` y no se puede analizar sin lectura.

## Límites de fallback

- SD/U-Boot recupera **Linux**, no el MCU.
- El `.xgf` oficial 1.0.8 sólo puede reescribirse por IAP **si el bootloader sigue accesible**.
- Si la app no arranca y el bootloader no expone IAP, no hay recuperación por software.
