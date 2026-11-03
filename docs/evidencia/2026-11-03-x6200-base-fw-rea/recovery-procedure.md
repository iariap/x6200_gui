# Procedimiento de flasheo experimental y recuperación — candidato `cc7b4aca…`

## Estado de autorización

- SD/Linux: pruebas autorizadas.
- **MCU: no flasheado.** `BASE_EXPERIMENTAL_FLASHED=NO`.
- `MCU_RECOVERY=UNVERIFIED`.
- Este documento prepara el procedimiento; no ejecuta la escritura.

## Artefactos

| Artefacto | SHA256 | Uso |
|---|---|---|
| Candidato `.xgf` | `cc7b4acae26887d0620f633f6a8a695419460f3201554c163120a3960ac2b6b9` | flash experimental |
| Candidato descifrado | `75339acd697f8f5b34f2bd69bbaac5a73119a548aa24422c7b79d7d9449cb389` | diff/auditoría |
| Oficial 1.0.8 `.xgf` | `9a19f97a5ad9e141a3663d222f984073bf0b18b38d4662604372a0036f9daa23` | fallback |
| Oficial 1.0.6 `.xgf` | `dca90387b70d9550448006310a69ea65625bec87f402c10df789deeb13ff12f3` | referencia |
| Vendor GUI 1.0.8 | `26623266bfcd3ed4d2e08b03b9f023cef809579f5ed497899670fc01f66e9b2b` | IAP/updater |
| GUI original radio | `e32a81f3…` | rollback Linux |

## Verificaciones offline ya hechas

- Candidato = oficial 1.0.8 descifrado + **1 byte** en `0x0804535C`.
- Reencriptado AES-256-ECB; round-trip exacto; tamaño 524288 B.
- Vector table/SP/reset coherentes; no se tocaron vectores, tablas ni otros writes.
- **No se puede verificar aceptación del bootloader** sin el MCU.

## Compatibilidad con el BASE instalado

- El candidato está construido sobre **1.0.8**.
- **No se pudo leer la versión exacta del MCU instalado** por IAP (no hay comando de lectura/versión).
- Si el MCU instalado no es 1.0.8, el candidato podría no ser compatible.
- **Gate**: confirmar versión/lectura del MCU antes de escribir.

## Procedimiento propuesto (no ejecutado)

### Antes

1. Radio con fuente estable, no batería baja; sin TX/PTT/ATU.
2. Respaldar GUI/params/eMMC conocidos.
3. Verificar hashes de candidato y oficiales.
4. Disponer de **vendor SD/updater** o un cliente IAP funcional para el fallback.
5. Confirmar que el IAP funciona y que se puede entrar con el MCU actual.
6. Aceptar por escrito el riesgo de MCU irrecuperable.

### Flash

1. Entrar al modo IAP del BASE con el updater vendor.
2. Erase app flash; transferir `cc7b4aca…` por XMODEM.
3. Verificar que el updater termina sin error (si expone estado).
4. Reiniciar; comprobar Linux, I2C BASE, RX nativa.

### Validación

- Si arranca: BASE+ZERO (RX), luego DAC+ZERO, DAC+TONO, router, retorno a BASE.
- Si no arranca o no hay audio: intentar **oficial 1.0.8** por el mismo IAP.
- Si el IAP no responde: **no hay recuperación por software conocida**; se requiere SWD/BOOT0.

## Riesgo específico

- El `.xgf` parece contener la app en `0x08020000`; el bootloader está fuera de la imagen.
- **Pero no está demostrado** que el bootloader quede intacto, que acepte la imagen ni que el IAP funcione si la app no arranca.
- El firmware oficial no incluye necesariamente calibración/OTP única.
- Sin lectura full-flash previa no hay backup MCU real.

## Decisión

No se ejecuta el flash hasta:

1. confirmar la versión/estado del MCU;
2. tener un fallback IAP probado, o
3. autorización específica del propietario para este candidato, asumiendo que la SD **no** recupera el MCU.
