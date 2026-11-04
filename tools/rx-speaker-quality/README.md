# Herramienta offline — QA-SPEAKER-QUALITY

`analyze_audio_ab.py`: verifica la consistencia de la evidencia y las constantes de la cadena
(44.1 kHz, trama 441, scratch 4410, ring 400 ms, latencia NR 20 ms, writes=18→1.8 s) y ejecuta un
**modelo offline** de técnicas (pasabanda, notch, AGC) para medir magnitudes de procesamiento.
**No es el DSP del firmware ni una validación acústica**; todo resultado del modelo se etiqueta MODEL.
`tests/test_audio_ab.py`: 5 tests deterministas (constantes, bypass, atenuaciones, clipping).

Uso:
```
python3 tools/rx-speaker-quality/analyze_audio_ab.py > logs/offline-model.json
python3 tools/rx-speaker-quality/tests/test_audio_ab.py
```
No toca radio, ALSA, MUX, firmware ni archivos fuera del repo.
