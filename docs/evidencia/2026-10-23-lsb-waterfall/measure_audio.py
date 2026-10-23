#!/usr/bin/env python3
"""
Medicion del audio del owner para el issue #292 (la senal AUDIBLE aparece a la
derecha de la raya). No requiere radio: analiza los .raw de audio que el owner
ya guardo (mono int16 @ 48 kHz, ver README).

Para cada archivo reporta los tonos dominantes (200..4000 Hz) de la primera
ventana. NO alcanza para decidir H1/H2/H3 por si solo (falta el modo/estacion
de cada grabacion y su sincronia con la pantalla), pero funda el orden de
magnitud del tono audible del issue (~1,85 kHz).

Uso:
    python3 measure_audio.py <dir-de-audio>

SPDX-License-Identifier: LGPL-2.1-or-later
"""

import os
import sys

import numpy as np

RATE = 48000


def dominant_tones(path, n_fft=1 << 16, lo=200.0, hi=4000.0, top=4):
    x = np.fromfile(path, dtype=np.int16).astype(np.float64)
    if x.size < n_fft:
        n_fft = 1 << int(np.floor(np.log2(x.size)))
    x = x[:n_fft]
    x = x - x.mean()
    w = np.hanning(n_fft)
    X = np.abs(np.fft.rfft(x * w))
    f = np.fft.rfftfreq(n_fft, 1 / RATE)
    band = (f >= lo) & (f <= hi)
    Xb = np.where(band, X, 0.0)
    peak = Xb.max()
    idx = np.argsort(-Xb)[:top]
    return x.size / RATE, [(float(f[i]), 20 * np.log10(Xb[i] / peak + 1e-12))
                           for i in sorted(idx)]


def main():
    d = sys.argv[1] if len(sys.argv) > 1 else "."
    files = ["audio4.raw", "ftAudio.raw", "ff-LSB.raw", "ff-USB.raw",
             "calAudio.raw", "fAud.raw"]

    print("== tonos dominantes del audio del owner (200..4000 Hz) ==")
    for fn in files:
        p = os.path.join(d, fn)
        if not os.path.exists(p):
            continue
        dur, tones = dominant_tones(p)
        desc = ", ".join("%.0f Hz (%.1f dB)" % t for t in tones)
        print("  %-14s %.2fs | %s" % (fn, dur, desc))


if __name__ == "__main__":
    main()
