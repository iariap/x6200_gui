# Recomendación y decisión

- **Mantener DSP actual (SSB Voice)** como default; no crear presets ni cambiar defaults/persistencia/UI.
- D1/D2 **no** se recomiendan como default: sin STOI/ESTOI y sin resultados ciegos, y con tradeoffs
  objetivos (D1 más pumping/impulsos, menor envolvente; D2 ≈ C). Un `max-NR` automático queda rechazado.
- Si en el futuro una escucha ciega autorizada muestra beneficio reproducible de D1/D2 en
  comprensión/claridad **sin** regresión de naturalidad/estabilidad (separando limpio/débil/interferencia),
  podría proponerse como preset explícito; nunca auto.
- `PRODUCTION_READY=FAIL`; `AUDIBLE_PENDING`; `PRODUCTION_BLOCKED`; sin cambios de producción.
