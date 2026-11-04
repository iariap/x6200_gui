# Integración DSP — comparación A/B/C (labels VERIFIED / INFERRED / UNKNOWN)

| criterio | **A: DSP nativo controlado desde Linux** | B: extender DSP del STM32 | C: DSP Linux → parlante |
|---|---|---|---|
| Calidad | la del firmware nativo Xiegu (VERIFIED que existe; efecto audible PENDING) | podría añadir etapas propias (INFERRED) | la del DSP Linux (VERIFIED para captura; no para audible) |
| Latencia | la nativa, sin etapa extra (INFERRED) | +latencia MCU en buffer circular (UNKNOWN) | captura→A33→DAC/MUX (QA28: B≈142 vs A≈2067, sin RX útil) |
| CPU | ~0 extra en A33 (sólo escrituras I2C) (VERIFIED por API) | carga DSP Cortex-M7 (UNKNOWN) | decenas de % de un core A33 (INFERRED) |
| Batería | sin cambio por el filtro; sólo I2C (INFERRED/VERIFIED parcial) | impacto no medido (UNKNOWN) | no medido (UNKNOWN) |
| Complejidad | baja: X6200Control ya expone NR/NB/DNF/AGC/EQ/DFL (VERIFIED) | alta: flash MCU, formato/rate/DMA (UNKNOWN) | alta: ALSA/PulseAudio + selector BASE/DAC (falló) |
| Compatibilidad | oficial 1.0.8, sin firmware nuevo (VERIFIED) | rompe firmware oficial (NO-GO sin recuperación) | requiere MUX/DAC y convivencia con RX (no verificada) |
| Riesgo TX | nulo: son controles RX (VERIFIED a nivel registro) | riesgo calibración/TX si se parchea MCU (UNKNOWN) | nulo en TX, pero pierde RX útil (QA28/32/35) |
| Continuidad RX | continua: el filtro nativo no reconfigura el selector ni el front-end (VERIFIED por diseño del firmware) | depende del parche (UNKNOWN) | QA28/QA32/QA35: RX capturada se pierde con DAC (VERIFIED fallo) |
| Bypass / recuperación | volver a valores por defecto/off desde GUI (VERIFIED API) | requiere reflash/rollback no verificado (NO-GO) | volver a BASE (VERIFIED) pero sin DSP audible |
| Mantenimiento | depende del firmware vendor (opaco) (INFERRED) | propio, pero alto costo (INFERRED) | propio (INFERRED) |
| Decisión | **GO — recomendada** | NO-GO (no justificado) | NO-GO (no verificado) |

## Arquitectura elegida: A

`RECOMMENDED_ARCHITECTURE = A`: reutilizar los filtros nativos controlando desde Linux los registros
que ya usa el GUI. No se implementa B ni C porque A ya cubre el objetivo (RX continuo + parlante
interno) y B/C añaden riesgos sin evidencia de necesidad.

La cadena Linux-side **no audible** (Speex NR, notch, AF leveler sobre la copia de captura) se conserva
para grabadora/decoders; no sustituye al DSP audible nativo.

## Evidencia del fallo de C (histórica, citada)

QA28 (`B≈142 vs A≈2067`), QA32 (`H1 B=143,0 vs A=2160,7`), QA35 (sin tono audible ni en BASE ni DAC),
QA36 (`BASE_RX_PRESERVED_WITH_DAC=NO`), QA390 (endpoint I2S3 TX→parlante UNKNOWN). No se reutilizan los
parches fallidos reg12/reg24/FPGA 0x34.
