# Rollback ejecutado — firmware oficial 1.0.8

Fecha: 2026-11-03. Autorización del propietario recibida en contexto (“tienes mi autorización, hazlo”)
para el rollback oficial previamente especificado.

## Artefacto

- `x6200_mcu_20260820141208.xgf`
- SHA256 `9a19f97a5ad9e141a3663d222f984073bf0b18b38d4662604372a0036f9daa23`
- 524288 B.

## Procedimiento

- Snapshot previo de estado, `/mnt/params.db`, ALSA y log.
- Overlay vendor **sólo** con el oficial; se ocultaron todos los demás `.xgf`.
- Hash del archivo visible verificado.
- GUI/daemon originales detenidos; updater vendor con EGLFS original.
- Log: `open firmware file "/usr/firmware/x6200_mcu_20260820141208.xgf" ok.`
- IAP: `Start IAP erase app flash`; 512 paquetes XMODEM; 100 %; `XMWorker finished`;
  `UiFwUpgrade finished 0`.
- Reinicio de la radio.

## Resultado

- BASE: `BASE fw version: 1.0.8`; `BASE fw date: Aug 19 2026,16:13:08`.
- GUI original `e32a81f3…`; un proceso; RX; `tx=false`.
- Operador: `27.565.000 Hz AM`, volumen 25, preservados.
- ALSA: snapshot antes/después **idéntico** (`diff` vacío).
- Sin montajes vendor/SD.
- H1 ya **no** está instalado; H1_FLASHED queda como evento histórico.
- Sin nuevo flash posterior.

## Gates finales

- `ROLLBACK_FLASHED = YES`
- `ROLLBACK_RESULT = SUCCESS`
- `OFFICIAL_1_0_8 = YES`
- `H1_INSTALLED = NO`
- `H1_FLASHED = YES (histórico)`
- `H1_RESULT = FAIL_FUNCTIONAL`
- `BASE_RX_PRESERVED_WITH_DAC = NO` (mediciones QA32)
- `H1_DAC_ROUTE_DISABLED = LIKELY, no confirmado`
- `LINUX_PCM_PATH_FAULT = UNKNOWN`
- `PREVIOUS_AUDIBLE_EVIDENCE = VALID` (QA11b)
- `COMBINED_RX_DAC_ROUTE = UNPROVEN`
- `LINUX_DSP_RX_CAPTURE = FAIL`
- `LINUX_DSP_SPEAKER = FAIL` para la ruta PCM probada
- `BASE_RETURN = PASS`
- `SIN TX/PTT/ATU/BLUETOOTH`
