# QA28 — Validación A/B/A de RX con parche MCU

Fecha: 2026-11-03. Radio con candidato `x6200-108-voice-keep-rx.xgf` flasheado
(SHA256 `cc7b4aca…`), BASE 1.0.8. Sin volver a flashear.

## Método

- GUI detenida; `x6200_daemon` detenido.
- Helper ARM propio contra `libaether_x6200_control`: `x6200_control_init()` y
  `x6200_control_record_set(true/false)` para seleccionar DAC/BASE sin stream Linux.
- Captura PulseAudio: `parec --device=alsa_input.platform-sound.stereo-fallback`,
  s16le/44.1k/2ch.
- Frecuencia 7.143 MHz LSB, RX exclusivo; volumen10 durante la prueba.
- A: BASE+ZERO; B: DAC+ZERO (voice_rec); C: BASE+ZERO.
- Sin TX/PTT/ATU/Bluetooth.

## Resultados

| Fase | Ruta | RMS estable (2 s) | Pico | Notas |
|---|---|---|---:|---:|---|
| A | BASE | 2067,6 | 9916 | referencia |
| B | DAC+ZERO | **142,1** | 22384 (transitorio) | **sin RX útil**; RMS total 1417 por transitorio de conmutación |
| C | BASE | 2072,2 | 10079 | recuperación |

- A y C: bloques estables ~2060–2430; espectro con señal.
- B: bloques estables 141,9–144,0; espectro sin señal RX.
- La referencia QA14 previa era B≈30,6; ahora B≈142 pero **no conserva RX real**.
- Prueba de tono 1 kHz durante DAC+ZERO: no apareció 1 kHz en la captura de fuente
  (`1k_ratio≈0.001`). La entrega del tono al sink no quedó confirmada por monitor
  (sink SUSPENDED, captura de monitor sin datos tras el arranque), así que
  `DAC_FEEDBACK_IN_CAPTURE` se clasifica **INCONCLUSIVE**, no YES.
- ALSA restaurado exacto (`diff` vacío); params.db restaurado desde snapshot previo;
  GUI original reiniciada; un proceso; `tx=false`; sin montajes.

## Gates

- `BASE_PATCH_FLASHED = YES`
- `BASE_RX_PRESERVED_WITH_DAC = NO`
- `DAC_FEEDBACK_IN_CAPTURE = INCONCLUSIVE`
- `LINUX_DSP_RX_CAPTURE = FAIL`
- `LINUX_DSP_SPEAKER = NOT_VERIFIED` (sin micrófono externo)
- `BASE_RETURN = PASS` (C RMS 2072; estado/ALSA restaurados)

## Incidente de selección previo

El primer flash escribió el oficial `x6200_mcu_20260820141208.xgf` en vez del candidato.
Causa: la navegación por EVDEV seleccionó el segundo ítem de la lista antes de UPGRADE.
Medidas para evitar repetición: verificar el nombre resaltado en pantalla; no enviar
NEXT/PREV sin captura; fijar el candidato como único `.xgf` en el overlay (el oficial
debe quedar fuera) y verificar el `open firmware file` antes de IAP.
