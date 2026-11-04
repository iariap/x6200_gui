# Corpus de voz natural — licencia y reproducibilidad

- **Corpus:** CMU ARCTIC (Festival/CMU), muestras WAV individuales de 4 speakers: `bdl` (m),
  `slt` (f), `rms` (m), `clb` (f), frases `a0001`/`a0002`. Fuente:
  `http://festvox.org/cmu_arctic/cmu_arctic/cmu_us_<speaker>_arctic/wav/arctic_<phrase>.wav`.
- **Licencia/atribución:** CMU ARCTIC se distribuye para uso libre por el proyecto Festvox
  (http://festvox.org/cmu_arctic/); se cita la fuente. **No se reclama permiso adicional**: los WAV no
  se versionan en este PR; el script `fetch_corpus.sh` los adquiere reproduciblemente.
- **Hashes de origen** (sha256) en `corpus.sha256`; 8 archivos verificados al momento de la auditoría.
- **Procesamiento:** resample 16 kHz → 44.1 kHz mono S16 (documentado, sin normalización); 1.2 s.
  WAVs de origen son voz natural; las degradaciones son sintéticas y se etiquetan como tales.
- No se usan grabaciones de comunicaciones, datos personales ni corpus de terceros no autorizados.
