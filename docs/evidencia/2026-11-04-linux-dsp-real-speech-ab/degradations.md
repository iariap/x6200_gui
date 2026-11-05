# Degradaciones simuladas (RF sintética, no radio real)

Generador `prepare.py` (numpy, seed determinístico 20261104), por muestra de 1.2 s:
- `white_noise` 3 niveles (−12/−6/0 dB rel. RMS de voz)
- `band_noise` 3 niveles (ruido 300–3000 Hz)
- `adjacent_qrm` 3 niveles (tono ~1800 Hz + armónico)
- `impulses` 3 niveles (impulsos periódicos)
- `level_steps` 3 niveles (cambios de ganancia)
Cada WAV degradado y su hash queda en el manifiesto; todos los candidatos procesan **exactamente la
misma entrada**. Etiquetado: degradación sintética; no se presenta como captura de radio.
