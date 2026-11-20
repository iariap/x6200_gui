# Parche mínimo propuesto — 1.0.8

## Cambio

En **`0x0804535C`** (rama `voice_rec=1`), instrucción `movs r3,#0` → `movs r3,#0x28`:

```
before: 00 23   movs r3, #0
after:  28 23   movs r3, #40
```

Es **un solo byte** del stream de instrucciones (el segundo byte de la instrucción Thumb); el
archivo descifrado difiere en **1 byte** respecto del 1.0.8 original.

## Artefacto candidato

- Descifrado modificado:
  `x6200-108-voice-keep-rx.dec.bin`
  SHA256 `75339acd697f8f5b34f2bd69bbaac5a73119a548aa24422c7b79d7d9449cb389`
- Reencriptado AES-256-ECB:
  `x6200-108-voice-keep-rx.xgf`
  SHA256 **`cc7b4acae26887d0620f633f6a8a695419460f3201554c163120a3960ac2b6b9`**
- Tamaño 524288 B; round-trip criptográfico verificado.
- El binario está **fuera del repositorio**; sólo hashes/diff en evidencia.

## Validación offline

- Diff: **1 byte**, en la instrucción esperada.
- Desensamblado verificado: `movs r3,#40`; el resto de la rama intacto.
- No se modificaron vectores, longitudes, tablas de calibración ni otros writes.
- **No se validó funcionalmente** (no hay emulador H725).

## Riesgos / flash

- **Aceptación del bootloader desconocida**: si valida AES/CRC/firma, puede rechazar la imagen.
- **Recovery MCU no verificado**: no hay SWD/JTAG/BOOT0 validado; restaurar Linux/SD no revierte el MCU.
- Si la imagen es aceptada pero el registro 24 no era el mute, el audio puede seguir igual o quedar
  en un estado no previsto.
- **No se flasheó**. `BASE_EXPERIMENTAL_FLASHED=NO`; `MCU_RECOVERY=UNVERIFIED`.

## Antes de un flash autorizado

1. Confirmar que el MCU en la radio es H725 y que el `.xgf` 1.0.8 corresponde a la versión activa.
2. Verificar la vía IAP/vendor GUI y una recuperación por firmware oficial.
3. Tener `.xgf` oficial 1.0.8 y 1.0.6 aparte, con hashes.
4. Si no hay recuperación verificada, se requiere autorización específica para esta imagen
   concreta y aceptar el riesgo de brick del MCU.
