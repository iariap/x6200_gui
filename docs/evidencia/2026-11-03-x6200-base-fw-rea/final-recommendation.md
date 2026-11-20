# Recomendación final — flash de `cc7b4aca…`

## Hecho verificado

- AES-256-ECB y round-trip de ambas imágenes: **verificado**.
- Base de ejecución `0x08020000`; `voice_rec` localizado; handler `0x08045320`.
- Candidato: 1 byte en `0x0804535C`; AES/round-trip; hash
  `cc7b4acae26887d0620f633f6a8a695419460f3201554c163120a3960ac2b6b9`.
- `calCRC32_STM32`: sólo calibración/fábrica; no updater.
- 4 bytes finales: **no identificados**; hipótesis de residuo CRC constante **no confirmada**.
- Entorno vendor aislado por overlay desde SD: dependencias Qt/qwt/NE10/liquid **resueltas**;
  no se inició la GUI ni el updater.
- No se escribió MCU, SD ni option bytes; no se envió erase/XMODEM.

## Riesgos demostrados

1. **No hay recuperación MCU verificada**: SD sólo recupera Linux; no SWD/BOOT0.
2. **No hay readback IAP**: no se puede confirmar la escritura ni restaurar la flash completa.
3. **IAP entry no probado**: el updater no se ejecutó y no hay evidencia de que el bootloader
   entre al modo IAP sin la aplicación.
4. **Versión BASE instalada desconocida**.
5. **No hay backup MCU** (calibración/OTP incluidas).

## Incertidumbres (no son pruebas de incompatibilidad)

- semántica de los 4 bytes finales;
- si el bootloader valida CRC/firma;
- si el bootloader es independiente de la app;
- si el `.xgf` oficial 1.0.8 puede restaurar tras un fallo.

No se encontró un checksum reproducible; eso **no demuestra** que el bootloader rechace la imagen.

## Recomendación

**GO condicional para un único flash experimental**, únicamente si el propietario autoriza
explícitamente **este archivo** y acepta:

- brick del MCU;
- imposibilidad de recuperación por SD;
- necesidad de abrir la radio como último recurso;
- ausencia de backup MCU/calibración.

**NO-GO autónomo** hasta que exista autorización específica. El checksum desconocido no es el
motivo del NO-GO; el motivo es la ausencia de recuperación/entry IAP verificados.

## Procedimiento si se autoriza

1. fuente estable; sin TX/PTT/ATU;
2. verificar hashes del candidato y oficiales;
3. tener la imagen oficial 1.0.8 y el updater vendor listos;
4. escribir el candidato por IAP/XMODEM;
5. verificar arranque; si falla, intentar oficial 1.0.8 sólo si el IAP responde;
6. si el IAP no responde, detener escrituras y evaluar recuperación física.
