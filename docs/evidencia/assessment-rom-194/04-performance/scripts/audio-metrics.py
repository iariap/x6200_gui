#!/usr/bin/env python3
"""audio-metrics.py — #198: measure the WAVs captured by tools/captura-banco.sh.

The capture is what the GUI wrote to its named PulseAudio sink (null sink) inside
the private chroot, recorded with `parec`; the silence runs use CAP_VOZ= (no
voice test). This reports sample rate, channels, frames, duration, RMS/peak dBFS
and non-zero samples per channel, plus SHA256/size. It does not re-encode and it
does not claim RF, mixer gain, underruns or speaker quality.

Usage: audio-metrics.py <audio-dir> > capturas/audio-metrics.json
"""
from __future__ import annotations

import hashlib
import json
import math
import sys
import wave
from pathlib import Path


def db(value: float) -> float | None:
    if value <= 0:
        return None
    return 20.0 * math.log10(value / 32767.0)


def measure(path: Path) -> dict:
    w = wave.open(str(path), "rb")
    frames = w.getnframes()
    channels = w.getnchannels()
    rate = w.getframerate()
    raw = w.readframes(frames)
    w.close()
    samples = memoryview(raw).cast("h")
    per = [samples[c::channels] for c in range(channels)]
    out = {
        "file": path.name,
        "bytes": path.stat().st_size,
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "sample_rate_hz": rate,
        "channels": channels,
        "frames": frames,
        "duration_s": round(frames / rate, 4) if rate else None,
        "channels_identical": len(per) < 2 or bytes(per[0]) == bytes(per[1]),
        "channels_metrics": [],
    }
    for i, channel in enumerate(per):
        n = len(channel)
        rms = math.sqrt(sum(float(v) * v for v in channel) / n) if n else 0.0
        peak = max((abs(v) for v in channel), default=0)
        out["channels_metrics"].append(
            {
                "channel": i,
                "rms_dbfs": None if rms == 0 else round(db(rms), 3),
                "peak_dbfs": None if peak == 0 else round(db(float(peak)), 3),
                "nonzero": sum(1 for v in channel if v != 0),
                "samples": n,
            }
        )
    return out


def main() -> None:
    directory = Path(sys.argv[1] if len(sys.argv) > 1 else "audio")
    files = sorted(directory.glob("*.wav"))
    result = {"layer": "QEMU_USER", "coverage": "SYNTHETIC",
              "source": "GUI voice test (X6200_VOICE_TEST) -> PulseAudio null sink -> parec",
              "files": [measure(p) for p in files]}
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
