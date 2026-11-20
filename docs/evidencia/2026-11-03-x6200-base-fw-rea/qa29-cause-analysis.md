# QA29 — Por qué el parche reg24 no conserva RX

Fecha: 2026-11-03. Candidato `x6200-108-voice-keep-rx.xgf` ya flasheado; BASE 1.0.8.
No se volvió a flashear.

## 1. Verificación del parche en el binario

La dirección virtual es `0x0804535C`; con imagen base `0x08020000` el offset de archivo es
`0x2535C`. `cmp -l` da una única diferencia en el byte 152413 (1-based) = offset `0x2535D`,
segundo byte de `00 23` → `28 23` (`movs r3,#0` → `movs r3,#0x28`).

Handler correcto (Thumb, `0x08045320`):

```text
cmp r0,#0
BASE  (r0==0):
  reg12 = 0x08; reg24 = 0x28; call 0x080450C8; flags=0
DAC   (r0!=0):
  reg12 = 0x0A; reg24 = 0x00; call 0x08043840; 0x080439B4; 0x080432A8;
  call 0x080439B4 (r1=2,r2=4); 0x080432A8; flags=1
```

El candidato cambia únicamente el `reg24=0x00` de la rama DAC a `0x28`. `reg12` sigue 0x0A.

## 2. Escrituras posteriores

Se escanearon todos los callers del helper de escritura `0x08044470` (`reg12/24`):
sólo aparecen cuatro llamadas, todas dentro del handler `0x08045320`. **No hay otra escritura
posterior a reg12/24** que revierta el parche.

La rama DAC además configura el FPGA/DSP con `0x08043840`, `0x080439B4` y `0x080432A8`
(registros 0x34/0x36/0x37/0x39 y filtros); la rama BASE usa `0x080450C8` para reconfigurar
0x34/0x36/0x37/0x39. Esas llamadas son el candidato más fuerte para el mute restante, pero no
están nominadas en el código.

## 3. Medición directa, sin PulseAudio

GUI y daemon detenidos; PulseAudio detenido; helper ARM `libaether_x6200_control` con
`x6200_control_record_set(BASE/DAC)`; captura directa `arecord -D hw:0,0` (s16le/48k/2ch),
3 s; misma radio/frecuencia/volumen.

| Fase | RMS | Pico |
|---|---:|---:|
| A BASE | 2086,0 | 27945 |
| B DAC+ZERO | **142,3** | 1663 |
| C BASE | 1515,0 | 32767 |

La captura directa reproduce el resultado de PulseAudio (B≈142): **no es un artefacto de PA**.
B no contiene RX útil; A/C recuperan señal (C algo menor, mismo comportamiento de BASE).

Prueba de tono DAC: no se confirmó entrega al sink (monitor PA suspendido/sin datos);
`DAC_FEEDBACK_IN_CAPTURE` queda **INCONCLUSIVE**.

## 4. Ejecución del parche

El log del updater mostró `open firmware file "/usr/firmware/x6200-108-voice-keep-rx.xgf"`,
IAP erase, 512 paquetes XMODEM y `UiFwUpgrade finished 0`. **No hay readback del MCU**, por lo
que no se puede demostrar por lectura que el byte modificado esté ejecutándose; sólo se verificó
la escritura del archivo y el arranque posterior.

## 5. Conclusión

- `reg24=0x28` **no es el único mecanismo** de mute; el parche es insuficiente.
- La hipótesis original “reg24 corta RX hacia ADC” quedó **incompleta**: no hay evidencia de que
  reg24 sea la compuerta de RX, y el cambio no restaura RX.
- El mute restante está en `reg12=0x0A` o en la configuración FPGA/DSP de la rama DAC
  (`0x08043840`/`0x080439B4`/`0x080432A8`), pero no hay mapa de registros que permita una
  corrección mínima segura.
- **Decisión: SOFTWARE_ONLY_INCONCLUSIVE**, no “hardware limitation confirmed” todavía.

Evidencia faltante para una solución software-only:

1. mapa funcional de `reg12` (BASE 0x08 / DAC 0x0A) y de los registros FPGA 0x34/0x36/0x37/0x39;
2. confirmar si existe un modo combinado BASE+DAC o si reg12 es un selector excluyente;
3. readback o verificación de ejecución del parche en el MCU;
4. confirmación independiente del flujo PCM DAC (tono/monitor) sin depender de la captura Mic1.

## 6. Estado

Radio restaurada: GUI original `e32a81f3…`, un proceso, RX, tx=false, ALSA diff vacío,
vol2, freq7143000 (`freq_bg` 7105000 por estado de UI), sin montajes. Sin TX/PTT/ATU/Bluetooth.
No se escribió el MCU otra vez.
