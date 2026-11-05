# QA — Real-speech corpus y validación ciega de inteligibilidad RX (offline)

**Base:** `681074e0` (`fork/main`) · **PR:** `[AUDIO][LINUX DSP] Real speech corpus and blind RX intelligibility validation`
**Tipo:** validación offline con voz natural y DSP real; sin radio/MUX/QA activa, sin TX, sin deploy.

## Decisión
- **No se cambia el DSP por defecto**: C=SSB Voice actual se mantiene. D1 (leveler+NR30) y D2 (NR55)
  **no se recomiendan** como default: en métricas objetivas no superan a C con mejora clara y D1 muestra
  mayor pumping/impulsos y menor correlación de envolvente.
- `STOI/ESTOI = BLOCKED`: dependencia (`pystoi`/scipy) no disponible en el entorno y pip bloqueado; **no
  se sustituye** por proxy ni se inventan valores.
- Paquete ciego C/D1/D2/bypass **generado y nivelado** (72 muestras/288 segmentos, orden aleatorio,
  answer key separada), pero **no ejecutado con operador**: `AUDIBLE_PENDING`.
- Clasificación: `OFFLINE_PASS` (pipeline real, determinismo, integridad, corpus/degradaciones,
  paquete ciego), `AUDIBLE_PENDING`, `PRODUCTION_BLOCKED`; `PRODUCTION_READY=FAIL`.

## Contenido
`corpus.md` (licencia/atribución/hashes), `degradations.md`, `harness.md`, `metrics.md`,
`blind-package.md`, `decision.md`, `commands.log`; `summary.csv`, `per_sample.csv`,
`manifest-generated.csv`, `corpus.sha256`, `native-tests.log`. Los WAV no se versionan (reproducibles
por scripts); el manifiesto registra hashes.
