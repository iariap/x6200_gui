# Rollback preparado — firmware oficial 1.0.8

Estado: **PREPARADO, NO EJECUTADO**. Requiere autorización específica del propietario.

## Artefacto

- Archivo: `x6200_mcu_20260820141208.xgf`
- SHA256: `9a19f97a5ad9e141a3663d222f984073bf0b18b38d4662604372a0036f9daa23`
- Tamaño: 524288 B
- Ruta local: `fw108-scratch/ext108/fs108/x6200_mcu_20260820141208.xgf`
- Ruta vendor esperada: `/usr/firmware/x6200_mcu_20260820141208.xgf`

## Por qué

- El primer candidato (`cc7b4aca…`) no recuperó RX de captura.
- H1 (`df1e274f…`) tampoco recuperó RX (QA32: B=143,0 vs A=2160,7).
- H1 probablemente además deshabilitó DAC (QA11b previo sí oía DAC+TONE; QA35 no), aunque no
  está confirmado.
- Ningún experimento produjo una mejora funcional demostrada; se vuelve al oficial 1.0.8.

## Precondiciones

1. Autorización explícita del propietario para el SHA oficial.
2. Alimentación estable por AC; batería en carga.
3. Backups: GUI, `/mnt/params.db`, snapshot ALSA y estado actual guardados.
4. Oficial 1.0.8 verificado por SHA256.
5. Control exclusivo del BASE; una sola GUI.
6. No TX/PTT/ATU/Bluetooth.

## Procedimiento (una sola escritura)

1. Verificar SHA256 del archivo y que sea el único `.xgf` visible en el overlay vendor.
2. Detener GUI/daemon originales; montar overlay vendor con **sólo** el oficial; bind
   `/dev,/proc,/sys,/run`.
3. Arrancar la GUI vendor con EGLFS original; navegar a Firmware Upgrade.
4. Verificar visualmente el nombre y que la lista tenga **sólo** `x6200_mcu_20260820141208.xgf`.
5. Confirmar en el log `open firmware file ".../x6200_mcu_20260820141208.xgf"`.
6. Ejecutar un único UPGRADE; no interrumpir.
7. Esperar fin de XMODEM y reiniciar.
8. Verificar BASE/RX; restaurar GUI, PulseAudio, ALSA y ajustes del operador.

## Aborto

- Si el archivo visible no es el oficial, no iniciar.
- Si la alimentación es inestable, no iniciar.
- Si el updater falla antes de IAP, detenerse.
- Si el IAP queda en curso, no interrumpir.
- Si no arranca, sólo intentar IAP si responde; no improvisar.

## Autorización necesaria

> “Autorizo escribir `x6200_mcu_20260820141208.xgf` con SHA256
> `9a19f97a5ad9e141a3663d222f984073bf0b18b38d4662604372a0036f9daa23` para restaurar el
> firmware oficial 1.0.8, aceptando el riesgo de brick y la posible recuperación física.”

No se reutiliza la autorización de H1 ni la de `cc7b4aca…`.
