# QA — Calidad de voz RX en parlante interno (Linux DSP) — offline

**Base verificada:** `681074e0` (`fork/main`) · **PRs relacionados:** #398, #397, #396, #395, #394, #393;
épica #362. **Tipo:** bench/offline con DSP real; sin radio/I2C/MUX/QA activa, sin deploy.
**PR:** `[AUDIO][LINUX DSP] Optimize RX voice intelligibility for internal speaker`.

## Decisión
- Harness A/B/C/D real construido y ejecutado offline; **no hay candidato que justifique cambiar el DSP
  por defecto** con métricas objetivas sobre fixtures sintéticas (no hay STOI/ESTOI válidos sin corpus
  de voz autorizado, ni validación perceptual).
- **No se modifica código de DSP/presets/defaults.** Se mantiene la cadena actual (SSB Voice) y el
  nativo BASE como fallback. Candidatos D1/D2 quedan documentados para escucha autorizada.
- Clasificación: `OFFLINE_PASS` (harness/regression/integridad), `AUDIBLE_PENDING`,
  `PRODUCTION_BLOCKED`; `PRODUCTION_READY=FAIL`.
- Limitaciones: fixtures sintéticas ≠ voz/radio; CPU QEMU ≠ CPU física; sin grabación acústica ni A/B
  perceptual; no se declara inteligibilidad mejorada.

## Contenido
`benchmark-industry.md` (patrones adoptar/evitar), `dsp-chain-map.md`, `ab-harness.md`,
`results-current-vs-candidates.md`, `listening-protocol.md`, `bench-results.json`,
`fixtures.sha256`, `integrity.txt`, `native-tests.log`, logs de build/run.
