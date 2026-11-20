# Métricas offline (voz natural + degradaciones sintéticas)

Resumen por condición/modo en `summary.csv`; detalle por muestra/speaker/nivel en `per_sample.csv`
(480 filas: 8 frases × 15 degradaciones × 4 modos; y esta corrida incluye clean).

- **STOI/ESTOI: BLOCKED** — `pystoi`/`scipy` no instalados y pip bloqueado en el entorno; no se
  sustituye por una métrica inventada. Falta exacta: dependencia autorizada + corpus limpio (ya hay).
- Métricas válidas usadas: mejora de SNR vs referencia limpia, correlación de envolvente, clipping,
  ganancia RMS, brillo, pumping, longitud/continuidad. No son inteligibilidad perceptual.
- Hallazgos: C y D2 casi idénticos; D1 mejora SNR en band/impulsos pero **baja** correlación de
  envolvente y **aumenta** pumping (especialmente impulsos, 1.40 vs 0.96) ⇒ no mejora clara; B bypass
  conserva envolvente pero no reduce ruido (referencia).
- **No se declara superioridad física del parlante** ni mejor inteligibilidad a partir de estas
  métricas; la validación perceptual requiere el paquete ciego + operador autorizado.
