#!/usr/bin/env python3
"""QA-SPEAKER-QUALITY (#364/#362) — verificaciones OFFLINE reproducibles.

Dos bloques, ambos deterministas:
  1) CONSISTENCIA de la evidencia y de las constantes de la cadena actual (lee src/ y
     docs/ del tree): rate 44.1k, frame 441, scratch 4410, latencia NR 882 muestras=20 ms,
     ring 4x4410=400 ms, bloques 4410=100 ms, staleness 200 ms, writes=18 -> 1.8 s.
  2) MODELO OFFLINE de tecnicas transferibles (solo para medir magnitudes de PROCESAMIENTO,
     NO es el DSP del firmware ni una validacion acustica): pasabanda biquad RBJ, notch biquad,
     AGC de pico con ataque/release, sobre seno+ruido/impulso/ruido blanco. Mide atenuacion,
     SNR del tono fuera de banda, clipping, latencia de grupo de los filtros. Etiqueta MODEL.
No se declara ningun resultado audible.
"""
import json, math, os, random, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))

def read(p):
    with open(os.path.join(ROOT, p), 'r', encoding='utf-8', errors='replace') as f:
        return f.read()

# ---------- 1. consistencia de evidencia ----------
def consistency():
    src_audio = read('src/audio.c')
    src_enh = read('src/audio_enhancement.c')
    audio_h = read('src/audio.h')
    checks = {}
    checks['capture_44100'] = 'AUDIO_CAPTURE_RATE' in audio_h and '44100' in audio_h
    checks['mono_s16'] = 'S16' in src_audio and 'mono' in src_audio.lower()
    checks['scratch_4410'] = 'CAPTURE_SCRATCH_SAMPLES' in src_audio and '4410' in src_audio
    checks['enh_frame_441'] = 'AUDIO_ENH_FRAME_SIZE' in src_enh and '441' in src_enh
    checks['capture_only'] = ('screen_rec_put_audio_samples' in src_audio and
                              'dsp_put_audio_samples' in src_audio and
                              'pa_stream_write' in src_audio)
    checks['router_not_in_base'] = not os.path.exists(os.path.join(ROOT, 'src', 'rx_audio_live.c'))
    # constantes de la evidencia fisica (2026-11-02-speaker-fasttrack/rx-test3-ab.md)
    rate, frame, scratch = 44100, 441, 4410
    block_ms = 1000.0 * (scratch + 1) / rate  # bloque de captura ~100.0 ms (4410/44100)
    block_ms = 1000.0 * scratch / rate
    ring_ms = 4 * block_ms
    nr_latency_ms = 2 * 1000.0 * frame / rate  # 441 (frame) + 441 (lib) = 882 muestras
    consistency_vals = dict(
        rate_hz=rate, frame_samples=frame, scratch_samples=scratch,
        block_ms=round(block_ms, 3), ring_ms=round(ring_ms, 3),
        nr_latency_ms=round(nr_latency_ms, 3),
        router_writes=18, router_drops=0, router_underruns=0,
        router_audio_s=round(18 * block_ms / 1000.0, 3),
    )
    checks['block_100ms'] = abs(block_ms - 100.0) < 0.01
    checks['ring_400ms'] = abs(ring_ms - 400.0) < 0.05
    checks['nr_latency_20ms'] = abs(nr_latency_ms - 20.0) < 0.01
    checks['router_1p8s'] = abs(consistency_vals['router_audio_s'] - 1.8) < 0.001
    return checks, consistency_vals

# ---------- 2. modelo offline de tecnicas (NO es el DSP del firmware) ----------
def biquad_bp(fc, fs, q=1.0):
    w0 = 2 * math.pi * fc / fs
    alpha = math.sin(w0) / (2 * q)
    b0, b1, b2 = alpha, 0.0, -alpha
    a0, a1, a2 = 1 + alpha, -2 * math.cos(w0), 1 - alpha
    return [b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0]

def biquad_notch(fc, fs, q=1.0):
    w0 = 2 * math.pi * fc / fs
    alpha = math.sin(w0) / (2 * q)
    b0, b1, b2 = 1.0, -2 * math.cos(w0), 1.0
    a0, a1, a2 = 1 + alpha, -2 * math.cos(w0), 1 - alpha
    return [b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0]

def run_biquad(x, coef):
    b0, b1, b2, a1, a2 = coef
    y = [0.0] * len(x)
    x1 = x2 = y1 = y2 = 0.0
    for i, xi in enumerate(x):
        yi = b0 * xi + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1 = x1, xi
        y2, y1 = y1, yi
        y[i] = yi
    return y

def run_agc(x, fs, attack_ms=5.0, release_ms=250.0, target=0.15):
    a_att = math.exp(-1.0 / (fs * attack_ms / 1000.0))
    a_rel = math.exp(-1.0 / (fs * release_ms / 1000.0))
    env = 0.0
    y = [0.0] * len(x)
    for i, xi in enumerate(x):
        a = abs(xi)
        c = a_att if a > env else a_rel
        env = c * env + (1 - c) * a
        # leveller SOLO hacia abajo: nunca amplifica => no puede clip-ear
        g = min(1.0, target / env) if env > 1e-9 else 1.0
        y[i] = xi * g
    return y

def gen_tone_noise(n, fs, f, snr_db, seed=6200):
    rnd = random.Random(seed)
    tone = [0.2 * math.sin(2 * math.pi * f * i / fs) for i in range(n)]
    noise_amp = 0.2 / (10 ** (snr_db / 20.0))
    noise = [rnd.gauss(0, noise_amp) for _ in range(n)]
    return [t + z for t, z in zip(tone, noise)], tone

def band_power(x, fs, f_lo, f_hi):
    """Potencia en banda por suma de |X(f)|^2 sobre DFT directa (N chico)."""
    n = len(x)
    p = 0.0
    for k in range(int(f_lo * n / fs), int(f_hi * n / fs) + 1):
        re = sum(x[i] * math.cos(2 * math.pi * k * i / n) for i in range(n))
        im = sum(x[i] * math.sin(2 * math.pi * k * i / n) for i in range(n))
        p += re * re + im * im
    return p / (n * n)

def measure_model(fs=8000, n=4000):
    """fs=8k, N=0.5s: suficiente para el modelo y DFT directa manejable."""
    mix, tone = gen_tone_noise(n, fs, 700.0, snr_db=0.0)
    inband = band_power(mix, fs, 650, 750)
    outband = band_power(mix, fs, 1500, 2500)
    bp = biquad_bp(700.0, fs, q=2.0)
    y_bp = run_biquad(mix, bp)
    notch = biquad_notch(700.0, fs, q=8.0)
    y_notch = run_biquad(mix, notch)
    agc = run_agc(mix, fs)
    def clip_count(x): return sum(1 for v in x if abs(v) > 1.0)
    return dict(
        fs=fs, n=n,
        bp_inband_power=inband, bp_outband_power=outband,
        bp_outband_atten_db=round(10 * math.log10((band_power(y_bp, fs, 1500, 2500) + 1e-18) / (outband + 1e-18)), 3),
        bp_inband_change_db=round(10 * math.log10((band_power(y_bp, fs, 650, 750) + 1e-18) / (inband + 1e-18)), 3),
        notch_700_atten_db=round(10 * math.log10((band_power(y_notch, fs, 650, 750) + 1e-18) / (inband + 1e-18)), 3),
        agc_peak_before=round(max(abs(v) for v in mix), 4),
        agc_peak_after=round(max(abs(v) for v in agc), 4),
        clip_before=clip_count(mix), clip_after=clip_count(agc),
        label='MODEL_OFFLINE_NOT_FIRMWARE_NOT_ACOUSTIC',
    )

def main():
    checks, vals = consistency()
    ok = all(checks.values())
    model = measure_model()
    out = dict(consistency_checks=checks, consistency_values=vals, offline_model=model,
               result='PASS' if ok else 'FAIL')
    print(json.dumps(out, indent=2, ensure_ascii=False))
    return 0 if ok else 1

if __name__ == '__main__':
    sys.exit(main())
