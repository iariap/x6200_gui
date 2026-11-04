# X6200 — MCU RX path evidence (QA37/QA38)

Documental aislado. No incluye los documentos históricos de PR #386; se referencian allí.
Evidencia previa preservada en la rama `archive/pr388-qa37-full`.

- `qa37-mcu-rx-path.md`: QA37 original (histórico; corregido por QA38).
- `qa38-spi3-i2s.md`: QA38 — SPI3=I2S3, DMA handles, buffers SRAM1, underruns UNKNOWN.
- `scan.json`: escaneo literal de QA37; no prueba de ausencia de referencias calculadas.
- `qa38-flow.json`: verificación de flujo I2SCFGR/DMA/buffers.
- `qa39-i2s3-pipeline.md`: QA39 — DMA RX/TX confirmado; RX=0x30000000, TX=0x30000C00; formato/tasa UNKNOWN.
