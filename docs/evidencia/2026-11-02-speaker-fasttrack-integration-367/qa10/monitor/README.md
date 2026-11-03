# QA10 monitor objective recording

`parec` grabbed the PulseAudio sink monitor (`alsa_output...monitor`, s16le/44.1k/mono) while each
QA10b phase ran; the phase started ~1 s into the recording and lasted 5 s. This measures the PCM
that reached the PulseAudio sink, not the acoustic speaker output.

| Recording | Duration | Median RMS (1.5–6 s) | Blocks below half median | Min/Max RMS |
|---|---:|---:|---:|---:|
| ZERO | 7.74 s | 0.0 | 0.0 | 0 / 0 |
| RAW | 7.78 s | ~9.100 | ~0.0 | ~2.000 / ~11.900 |
| PROCESSED | 7.68 s | ~340 | ~0.63 | ~18 / ~7.263 |

- ZERO: silence after the startup burst; the RX the operator hears in ZERO is **not in the
  PulseAudio sink** → extra native/MUX path.
- RAW: continuous, no fade in the sink.
- PROCESSED: strongly gated/faded envelope in the **PCM delivered to the sink**; the audible
  fade is measurable in the processed stream, not just a listening impression.

See `analysis.json` for exact numbers. Raw recordings are in the external evidence scratch
(`fw108-scratch/evidence/rx-router-367/`).
