# Preflight del updater vendor — 2026-11-03

## Objetivo

Verificar que el entorno vendor aislado inicia la GUI/updater **sin IAP, erase ni escritura de
firmware**, con control exclusivo del BASE, y restaurar la GUI original.

## Procedimiento ejecutado

1. SD vendor original montada **ro**; overlay temporal en `/tmp`; bind `/dev,/proc,/sys,/run`.
2. Se detuvo la GUI original (`x6200_gui`/daemon) para dejar **un solo dueño** del BASE.
3. Se lanzó `x6200_ui_v100` 1.0.8 desde el chroot aislado.
4. Se observaron logs/procesos durante 12 s. **No se envió input, no se pulsó Update.**
5. Se cerró el vendor GUI, se desmontó el overlay y se reinició la GUI original.
6. Se restauró ALSA al snapshot previo (diff vacío).

## Resultado

- GUI vendor: **arrancó**; `BaseCommIIC` y `BaseComm` iniciaron; `[INFO] Baseband is ready`;
  leyó calibración (`[OK] Read calibrate data`); audio `XAudio` inicializado.
- **No** aparecen `Start IAP`, `Erasing Flash`, `XMWorker` ni XMODEM en el log.
- **No** se envió erase/XMODEM; no se escribió firmware.
- Control exclusivo: se detuvo la GUI original antes y se restauró después.
- GUI original restaurada: MD5 `e32a81f3563dab27e5b30111a47a7d12`, PID único, RX, `tx=false`.
- ALSA restaurado exacto (`diff` vacío).

## Limitaciones

- **No se navegó a la pantalla de selección de firmware** (no se inyectó input); se verificó el
  arranque del updater, no la selección de archivo.
- **No se probó entry/salida IAP**: el flujo de actualización lleva a erase; no hay handshake sin
  escritura.
- El arranque vendor modificó transitoriamente el mixer (Mic1 Playback/AIF2 ADC/DAC Playback on);
  se restauró a off. El `params.db` cambió de hash al reiniciar nuestra GUI (side effect de la GUI
  original, no de la REA); no se revirtió a un backup antiguo porque habría cambiado ajustes del
  operador (p. ej. volumen).
- `freq_bg` quedó en 7143000 en vez de 7200000 tras el reinicio de la GUI; es estado de UI, no
  firmware.

## Clasificación

`FLASH_PREFLIGHT = PASS_STARTUP` (arranque/no-auto-write/control exclusivo/restauración);
`FIRMWARE_SELECTION = NOT_EXERCISED`; `IAP_ENTRY = UNVERIFIED`.
No se escribió el MCU.

## Intento de selección por VNC (no completado)

- Se recreó el overlay, se copió el candidato a `/usr/firmware` del overlay:
  SHA256 verificado `cc7b4acae26887d0620f633f6a8a695419460f3201554c163120a3960ac2b6b9`.
- Se arrancó el vendor GUI con el plugin Qt VNC (`vnc:port=5900,size=480x800`); el puerto 5900
  quedó escuchando.
- Al conectar un cliente VNC (`vncdotool`), el servidor cerró la conexión y el proceso vendor
  terminó; no se pudo capturar la pantalla ni inyectar input.
- **No se envió ninguna tecla/click; no se pulsó Update/Confirm; no se inició IAP/erase/XMODEM.**
- Se cerró el entorno, se restauraron GUI original, ALSA (diff vacío), volumen0 y frecuencia
  7143000; sin montajes residuales.

### Flujo estático de selección

`UiFwUpgrade::exec()` llama a `updateFunctionButtons`; el único caller de
`UiFwUpgrade::upgradeFirmware` es `UiFwUpgrade::slot_pushButtonClicked` (acción de botón
**UPGRADE**). La selección de archivo (lista PREV/NEXT) no llama a `upgradeFirmware`; la escritura
sólo se inicia desde el botón UPGRADE. La selección runtime del candidato **no se ejercitó**.

`FIRMWARE_SELECTION = UNVERIFIED`.

## Autorización recibida y bloqueo operativo del flash

El propietario autorizó explícitamente escribir
`x6200-108-voice-keep-rx.xgf` SHA256 `cc7b4aca…`.

Preflight:

- hash del candidato verificado en el overlay;
- firmware oficial 1.0.8 y vendor rootfs disponibles;
- GUI original/params/SD respaldados;
- alimentación por AC/batería en carga;
- un solo GUI; sin montajes residuales.

Intento de control del updater:

- vendor GUI arrancó aislada y el plugin Qt VNC abrió el puerto 5900;
- el servidor ofreció **VNC Authentication** (tipo 1);
- `vncdotool` cerró la conexión y el proceso vendor terminó;
- handshake RFB 3.3 crudo quedó bloqueado esperando el challenge; el plugin no entregó una sesión
  interactiva utilizable;
- no hay `/dev/uinput` ni módulos para inyectar input; no se pudo navegar/seleccionar.

**No se envió IAP, erase ni XMODEM; no se escribió el MCU.**

`FLASH_PREFLIGHT=FAIL` por **control operativo del updater**, no por el candidato.
El candidato sigue autorizado pero el flash queda pendiente de una vía de interacción funcional
(VNC/input/manual) o de un cliente IAP equivalente.

Radio restaurada: GUI original, un proceso, RX, tx=false, ALSA diff vacío, volumen0, frecuencia
7143000; sin montajes.

## Intento RFC 6143 corregido y aborto

Se corrigió el cliente: en RFB 3.3, security type `1` = **None**; se envía ClientInit directo,
sin challenge ni SecurityResult.

Resultados:

- handshake inicial: servidor `RFB 003.003`, seguridad `1`, `ServerInit` recibido en un intento;
- luego no se recibió `FramebufferUpdate`; en un segundo intento se agotó el tiempo esperando
  `ServerInit` y el proceso/estado VNC no fue utilizable;
- el servidor/proceso vendor terminó al desconectar clientes;
- no se pudo capturar pantalla ni navegar; **no se envió input ni Update/Confirm**;
- **no se envió IAP/erase/XMODEM; el MCU no fue escrito.**

Se abortó el flash según el handoff. `FLASH_PREFLIGHT=FAIL` por VNC/interacción, no por el
candidato. Candidato autorizado sigue sin ejecutarse.

Restauración: GUI original, un proceso, RX, tx=false, ALSA diff vacío, vol0, freq7143000; sin
montajes.

## Reintento RFB 3.3 corregido — fallo de framebuffer

- Se completó el handshake con security type `1` (None) y `ServerInit` 480×800 en un intento.
- Se envió `SetPixelFormat` (formato del servidor), `SetEncodings` RAW y
  `FramebufferUpdateRequest` no incremental.
- No se recibió `FramebufferUpdate`; los reintentos limitados agotaron tiempo. En un segundo arranque
  el `ServerInit` ya no llegó.
- El proceso vendor terminó al desconectar el cliente; no se pudo capturar pantalla ni enviar input.
- **No se envió Update/Confirm, IAP, erase ni XMODEM. El MCU no fue escrito.**
- Restaurado: GUI original, ALSA diff vacío, vol0, freq7143000; `freq_bg` quedó en 7043000 por
  estado de UI del vendor, no firmware.

## Control autónomo: VNC software, Xvfb/XCB, LinuxFB — evaluación final

- VNC con `QT_QUICK_BACKEND=software`, `QT_OPENGL=software`, `LIBGL_ALWAYS_SOFTWARE=1`:
  handshake `RFB 003.003`, security `1` (None); el proceso vendor/`VNC` terminó al conectar o
  antes de `ServerInit`; no hubo framebuffer utilizable.
- Xvfb/X11: **no hay `Xvfb` en el host** y el rootfs vendor **no incluye `libqxcb.so`**; no es
  viable sin añadir un plugin Qt/xcb ARM compatible, que excede el preflight.
- LinuxFB: `/dev/fb0` existe, pero **no hay `/dev/uinput` ni módulo uinput**; no hay inyección
  virtual de input. No se puede navegar sin controles físicos.
- No se implementó un cliente IAP improvisado.

**No se envió Update/Confirm, IAP, erase ni XMODEM. El MCU no fue escrito.**
`FLASH_PREFLIGHT=FAIL` por imposibilidad de control gráfico autónomo.
Radio restaurada: GUI original, un proceso, RX, tx=false, ALSA diff vacío, vol0, freq7143000;
`freq_bg=7143000`/estado de UI; sin montajes.
