#!/usr/bin/env python3
"""
Medicion de las capturas del owner para el issue #292 (LSB: la senal audible
aparece a la derecha de la raya).

No requiere radio: analiza los PNG/raw que el owner ya guardo (scratch local,
ver README de esta carpeta). Mide, en la region del waterfall de cada captura:

  - la raya central (x),
  - el pico de cada senal (columna mas brillante), en px y en Hz (+ = derecha),
  - la barra del passband (columnas calidas, R>G,B, en la banda del espectro),
  - y, entre pares de capturas, el corrimiento horizontal por correlacion.

Usa el eje calibrado del issue: span 48 kHz sobre 800 px = 60 Hz/px; la raya se
mide en x~398..400 segun la captura (se reporta el offset respecto de la raya
medida, no de un 400 asumido).

Uso:
    python3 measure_captures.py <dir-de-capturas>

SPDX-License-Identifier: LGPL-2.1-or-later
"""

import os
import sys

import numpy as np
from PIL import Image

HZ_PER_PX = 60.0            # 48000 Hz / 800 px (barra 2,7k = 45 px)
WF_Y0, WF_Y1 = 235, 405     # banda del waterfall en la vista 800x480
SPEC_Y0, SPEC_Y1 = 66, 137  # banda del espectro (donde se dibuja la barra)


def load_gray(path):
    return np.asarray(Image.open(path).convert("L")).astype(float)


def load_rgb(path):
    return np.asarray(Image.open(path).convert("RGB"))


def load_fb_raw(path):
    """Framebuffer crudo del owner: 800x480x4 BGRA almacenado rotado."""
    a = np.fromfile(path, dtype=np.uint8)
    if a.size != 800 * 480 * 4:
        return None
    img = a.reshape(800, 480, 4)[:, :, [2, 1, 0]]
    return np.asarray(Image.fromarray(img, "RGB").transpose(Image.ROTATE_270))


def wf_profile(gray):
    return gray[WF_Y0:WF_Y1].mean(axis=0)


def ray_x(col):
    masked = col.copy()
    masked[:50] = 0
    masked[-50:] = 0
    return int(np.argmax(masked))


def signals(col, ray, thr_k=3.0):
    c = col.copy()
    c[ray - 4:ray + 5] = 0
    c[:20] = 0
    c[-20:] = 0
    thr = c.mean() + thr_k * c.std()
    peaks = [x for x in range(1, len(c) - 1)
             if c[x] > thr and c[x] >= c[x - 1] and c[x] >= c[x + 1]]
    merged = []
    for p in peaks:
        if merged and p - merged[-1][-1] <= 6:
            merged[-1].append(p)
        else:
            merged.append([p])
    centers = [int(np.mean(g)) for g in merged]
    centers.sort(key=lambda x: -c[x])
    return sorted(centers[:5])


def band_px(rgb):
    """Barra del passband: columnas calidas (R > G,B) en la banda del espectro."""
    a = rgb.astype(int)
    r, g, b = a[:, :, 0], a[:, :, 1], a[:, :, 2]
    m = (r > 30) & (r > g + 8) & (r > b + 4)
    hx = m[SPEC_Y0:SPEC_Y1].sum(axis=0)
    cols = [x for x in range(len(hx)) if hx[x] > 20]
    return (min(cols), max(cols)) if cols else None


def xcorr_lag(pa, pb, rad=90):
    pa = pa - pa.mean()
    pb = pb - pb.mean()
    w = len(pa)
    best = None
    for lag in range(-rad, rad + 1):
        if lag >= 0:
            s = float(np.dot(pa[: w - lag], pb[lag:]))
        else:
            s = float(np.dot(pa[-lag:], pb[: w + lag]))
        if best is None or s > best[1]:
            best = (lag, s)
    return best[0]


def main():
    d = sys.argv[1] if len(sys.argv) > 1 else "."

    pngs = ["captura-usuario-2252.png", "fb-AM.png", "fbm-LSB.png",
            "fbm-USB.png", "fbx-LSB.png", "fbx-USB.png"]
    raw_fbs = [("fA.raw", "LSB 7.074.000"), ("fB.raw", "LSB 7.076.000")]

    print("== Eje calibrado: %.0f Hz/px (span 48 kHz / 800 px) ==" % HZ_PER_PX)
    print()
    print("-- picos y barra por captura (px/Hz respecto de la raya) --")

    profiles, rays = {}, {}
    for fn in pngs:
        p = os.path.join(d, fn)
        if not os.path.exists(p):
            continue
        rgb = load_rgb(p)
        col = wf_profile(load_gray(p))
        r = ray_x(col)
        sig = signals(col, r)
        profiles[fn], rays[fn] = col, r
        desc = ", ".join("%d(%+d,%+dHz)" % (x, x - r, round((x - r) * HZ_PER_PX))
                         for x in sig)
        b = band_px(rgb)
        bdesc = "" if b is None else " | barra[%d..%d] (%+d..%+dHz)" % (
            b[0], b[1], round((b[0] - r) * HZ_PER_PX), round((b[1] - r) * HZ_PER_PX))
        print("  %-26s raya x=%d | %s%s" % (fn, r, desc, bdesc))

    for raw, label in raw_fbs:
        p = os.path.join(d, raw)
        if not os.path.exists(p):
            continue
        rgb = load_fb_raw(p)
        if rgb is None:
            continue
        col = wf_profile(rgb.mean(axis=2))
        r = ray_x(col)
        sig = signals(col, r)
        profiles[raw], rays[raw] = col, r
        desc = ", ".join("%d(%+d,%+dHz)" % (x, x - r, round((x - r) * HZ_PER_PX))
                         for x in sig)
        b = band_px(rgb)
        bdesc = "" if b is None else " | barra[%d..%d]" % b
        print("  %-26s raya x=%d | %s%s   [%s]" % (raw, r, desc, bdesc, label))

    print()
    print("-- corrimientos por correlacion (lag<0: la 2da corrio a la izquierda) --")

    def lag(f1, f2, label):
        if f1 in profiles and f2 in profiles:
            def mask(p, r):
                q = p.copy()
                q[r - 5:r + 6] = q.mean()
                return q

            l = xcorr_lag(mask(profiles[f1], rays[f1]), mask(profiles[f2], rays[f2]))
            print("  %-36s lag=%+d px (%+d Hz)" % (label, l, -round(l * HZ_PER_PX)))

    lag("fbx-LSB.png", "fbx-USB.png", "fbx-LSB vs fbx-USB (mismo 7.074)")
    lag("fA.raw", "fB.raw", "fA vs fB (calibracion, freq +2 kHz)")


if __name__ == "__main__":
    main()
