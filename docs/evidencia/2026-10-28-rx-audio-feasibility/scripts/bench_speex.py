#!/usr/bin/env python3
"""Banco de SpeexDSP REAL sobre src/audio_enhancement.c (issue #345, tarea B).

Orquesta:
  1. docker build de scripts/bench/Dockerfile (Debian bookworm pinneado +
     libspeexdsp-dev=1.2.1-1) — el host NO tiene libspeexdsp.
  2. docker run con el worktree montado read-only: el contenedor copia
     src/audio_enhancement.c VERBATIM, lo compila contra la lib real y corre la
     matriz de casos (scripts/bench/run_in_container.sh).
  3. analisis en el host de las salidas PCM: RMS, peak, clipping, saltos en los
     bordes de chunk, muestras del tail que quedan intactas, lag por
     correlacion y sha256.
  4. escribe measurements.csv (columnas exactas del issue) y bench-results.json.

Si el contenedor no se puede construir o el bench falla, el script NO inventa
numeros: escribe las filas BLOCKED con la causa y devuelve exit 1.

Uso:
    python3 bench_speex.py            # corre todo
    python3 bench_speex.py --no-build # reusa la imagen ya construida

SPDX-License-Identifier: LGPL-2.1-or-later
"""

from __future__ import annotations

import argparse
import array
import csv
import hashlib
import json
import math
import subprocess
import sys
import wave
from pathlib import Path

EVIDENCE_DIR = Path(__file__).resolve().parent.parent
WT = EVIDENCE_DIR.parents[2]
BENCH_DIR = EVIDENCE_DIR / "scripts" / "bench"
PCM_DIR = EVIDENCE_DIR / "artifacts" / "pcm"
OUT_DIR = EVIDENCE_DIR / "artifacts" / "bench"
CSV_PATH = EVIDENCE_DIR / "measurements.csv"
RESULTS_PATH = EVIDENCE_DIR / "bench-results.json"
PCM_MANIFEST = EVIDENCE_DIR / "pcm-manifest.json"
SRC_FILE = WT / "src" / "audio_enhancement.c"

IMAGE = "x6200-speexdsp-bench:1.2.1-1"
BASE_IMAGE = "debian:bookworm-slim@sha256:99c0b25c46d0214d5a2091092e7908907dcaecd6a962cde0ac700acee222b6f3"
DATA_RATE = 44100
FRAME = 256
CLIP_LEVEL = 32767

COLUMNS = [
    "case",
    "source",
    "rate_hz",
    "frame_samples",
    "buffer_samples",
    "processed_samples",
    "unprocessed_samples",
    "latency_ms",
    "cpu_wall_ms",
    "rms_dbfs",
    "peak_dbfs",
    "clip_count",
    "sha",
    "command",
    "exit_code",
    "evidence_class",
    "notes",
]

SIGNAL_FILES = {
    "silence": "silence_44100_s16.wav",
    "sine1k": "sine_1k_44100_s16.wav",
    "multisine": "multisine_44100_s16.wav",
    "noise": "white_noise_44100_s16.wav",
}

try:  # numpy solo acelera el lag por correlacion; el resto es stdlib
    import numpy as _np
except ImportError:  # pragma: no cover - el host del factory lo tiene
    _np = None


# --------------------------------------------------------------------------- #
# utilidades
# --------------------------------------------------------------------------- #


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def read_wav(path: Path) -> array.array:
    with wave.open(str(path), "rb") as w:
        if w.getnchannels() != 1 or w.getsampwidth() != 2:
            raise SystemExit(f"{path}: se esperaba mono S16")
        n = w.getnframes()
        raw = w.readframes(n)
    samples = array.array("h")
    samples.frombytes(raw)
    if sys.byteorder != "little":
        samples.byteswap()
    return samples


def read_raw(path: Path) -> array.array:
    samples = array.array("h")
    samples.frombytes(path.read_bytes())
    if sys.byteorder != "little":
        samples.byteswap()
    return samples


def db(value: float) -> float | None:
    if value <= 0.0:
        return None
    return 20.0 * math.log10(value / 32768.0)


def fmt(value: float | None, digits: int = 3) -> str:
    if value is None:
        return "UNKNOWN"
    if isinstance(value, float) and (math.isinf(value) or math.isnan(value)):
        return "UNKNOWN"
    return f"{value:.{digits}f}"


def pcm_stats(samples: array.array) -> dict:
    peak = max(abs(s) for s in samples)
    acc = sum(float(s) * s for s in samples)
    rms = math.sqrt(acc / len(samples))
    clip = sum(1 for s in samples if abs(s) >= CLIP_LEVEL)
    return {
        "peak_abs": peak,
        "rms": rms,
        "rms_dbfs": db(rms),
        "peak_dbfs": db(float(peak)),
        "clip": clip,
    }


def max_jump(samples: array.array, indices: range | list[int]) -> int:
    return max((abs(samples[i] - samples[i - 1]) for i in indices), default=0)


def boundary_indices(n: int, grid: int) -> range:
    return range(grid, n, grid)


def tail_identity(inp: array.array, out: array.array, block: int) -> tuple[int, int]:
    """Muestras del tail de cada callback (block % 256) que quedaron identicas."""
    tail = block % FRAME
    if tail == 0:
        return (0, 0)
    same = 0
    total = 0
    for start in range(0, len(inp) - block + 1, block):
        for i in range(start + block - tail, start + block):
            total += 1
            if inp[i] == out[i]:
                same += 1
    return (same, total)


def lag_samples(
    inp: array.array, out: array.array, window: int = 44100, max_lag: int = 512
) -> tuple[int | None, bool]:
    """Lag (muestras) del pico de correlacion normalizada, y si es ambiguo.

    Una senal periodica correlaciona casi igual en lags separados un periodo:
    ahi el pico no identifica el delay (se marca ambiguo y no se reporta).
    """
    if _np is None:
        return (None, True)
    n = min(window, len(inp))
    if n <= max_lag * 2:
        return (None, True)
    x = _np.frombuffer(inp[:n].tobytes(), dtype="<i2").astype(_np.float64)
    y = _np.frombuffer(out[:n].tobytes(), dtype="<i2").astype(_np.float64)
    if not x.any() or not y.any():
        return (None, True)
    size = 1 << (2 * n - 1).bit_length()
    cc = _np.fft.irfft(_np.fft.rfft(y, size) * _np.conj(_np.fft.rfft(x, size)), size)
    cc = _np.concatenate([cc[-(n - 1) :], cc[:n]])
    lags = _np.arange(-(n - 1), n)
    mask = (lags >= -max_lag) & (lags <= max_lag)
    cand = lags[mask]
    corr = cc[mask]
    energy = _np.empty(len(cand))
    for k, lag in enumerate(cand):
        a = x[max(0, int(lag)) : n + min(0, int(lag))]
        b = y[max(0, -int(lag)) : n - max(0, int(lag))]
        energy[k] = math.sqrt(float(_np.dot(a, a)) * float(_np.dot(b, b)))
    corr = corr / _np.maximum(energy, 1e-9)
    best = int(_np.argmax(corr))
    peak = float(corr[best])
    far = _np.abs(cand - cand[best]) > 16
    second = float(corr[far].max()) if far.any() else 0.0
    ambiguous = peak <= 0.0 or (second / peak) > 0.95
    return (int(cand[best]), ambiguous)


def run(cmd: list[str], log: list[str], check: bool = True) -> subprocess.CompletedProcess:
    log.append("$ " + " ".join(cmd))
    proc = subprocess.run(cmd, capture_output=True, text=True)
    log.append(f"[exit {proc.returncode}]")
    if proc.stdout.strip():
        log.append(proc.stdout.rstrip())
    if proc.stderr.strip():
        log.append(proc.stderr.rstrip())
    if check and proc.returncode != 0:
        raise RuntimeError(f"fallo: {' '.join(cmd)} (exit {proc.returncode})")
    return proc


# --------------------------------------------------------------------------- #
# filas del CSV
# --------------------------------------------------------------------------- #


def arithmetic_rows() -> list[dict]:
    """Derivacion aritmetica de src/audio_enhancement.c:293-317 (no es medicion)."""
    rows = []
    cases = []
    for rate in (44100, 48000):
        for block in (1, 256, 257, 4410, 8820):
            cases.append((rate, block))
    cases.append((48000, 4800))  # 100 ms reales a la tasa declarada

    for rate, block in cases:
        frames = block // FRAME
        processed = frames * FRAME
        unprocessed = block - processed
        frame_ms = FRAME / rate * 1000.0
        buffer_ms = block / rate * 1000.0
        rows.append(
            {
                "case": f"arith_{rate}_{block}",
                "source": "src/audio_enhancement.c:293-317 (apply_noise_reduction)",
                "rate_hz": rate,
                "frame_samples": FRAME,
                "buffer_samples": block,
                "processed_samples": processed,
                "unprocessed_samples": unprocessed,
                "latency_ms": fmt(frame_ms, 4),
                "cpu_wall_ms": "UNKNOWN",
                "rms_dbfs": "UNKNOWN",
                "peak_dbfs": "UNKNOWN",
                "clip_count": "UNKNOWN",
                "sha": "UNKNOWN",
                "command": "derivacion aritmetica (ver speex-audit.md, seccion 1)",
                "exit_code": 0,
                "evidence_class": "CODE",
                "notes": (
                    f"CODE: {frames}x256+{unprocessed}={block} muestras por buffer "
                    f"({buffer_ms:.2f} ms a {rate} Hz); {unprocessed}/{block} = "
                    f"{100.0 * unprocessed / block:.2f}% sin NR; frame = {frame_ms:.4f} ms"
                ),
            }
        )
    return rows


def blocked_rows(reason: str | None) -> list[dict]:
    rows = [
        {
            "case": "latency_end_to_end_rx_to_speaker",
            "source": "PulseAudio + BASE + parlante/auriculares",
            "rate_hz": DATA_RATE,
            "frame_samples": FRAME,
            "buffer_samples": 4410,
            "processed_samples": "UNKNOWN",
            "unprocessed_samples": "UNKNOWN",
            "latency_ms": "UNKNOWN",
            "cpu_wall_ms": "UNKNOWN",
            "rms_dbfs": "UNKNOWN",
            "peak_dbfs": "UNKNOWN",
            "clip_count": "UNKNOWN",
            "sha": "UNKNOWN",
            "command": "N/A",
            "exit_code": "N/A",
            "evidence_class": "BLOCKED",
            "notes": (
                "BLOCKED: la latencia RX->parlante incluye PulseAudio (fragsize 100 ms), "
                "el DSP del BASE y el trayecto electrico; nada de eso se mide en este host "
                "ni se toca la radio en esta tarea"
            ),
        },
        {
            "case": "cpu_on_radio",
            "source": "GUI x6200 en la radio (R16)",
            "rate_hz": DATA_RATE,
            "frame_samples": FRAME,
            "buffer_samples": 4410,
            "processed_samples": "UNKNOWN",
            "unprocessed_samples": "UNKNOWN",
            "latency_ms": "UNKNOWN",
            "cpu_wall_ms": "UNKNOWN",
            "rms_dbfs": "UNKNOWN",
            "peak_dbfs": "UNKNOWN",
            "clip_count": "UNKNOWN",
            "sha": "UNKNOWN",
            "command": "N/A",
            "exit_code": "N/A",
            "evidence_class": "BLOCKED",
            "notes": (
                "BLOCKED: el CPU del banco es un x86_64 de host; el R16 de la radio es otra "
                "arquitectura y otra carga. No se extrapola presupuesto de CPU."
            ),
        },
        {
            "case": "audible_intelligibility",
            "source": "senales sinteticas (tonos y ruido)",
            "rate_hz": DATA_RATE,
            "frame_samples": FRAME,
            "buffer_samples": 4410,
            "processed_samples": "UNKNOWN",
            "unprocessed_samples": "UNKNOWN",
            "latency_ms": "UNKNOWN",
            "cpu_wall_ms": "UNKNOWN",
            "rms_dbfs": "UNKNOWN",
            "peak_dbfs": "UNKNOWN",
            "clip_count": "UNKNOWN",
            "sha": "UNKNOWN",
            "command": "N/A",
            "exit_code": "N/A",
            "evidence_class": "BLOCKED",
            "notes": (
                "BLOCKED: tonos y ruido sinteticos prueban integridad de procesamiento, no "
                "inteligibilidad de voz SSB/AM; hace falta escucha con voz real y la radio"
            ),
        },
    ]

    if reason is not None:
        rows.append(
            {
                "case": "bench_real_libspeexdsp",
                "source": "contenedor docker (scripts/bench/Dockerfile)",
                "rate_hz": DATA_RATE,
                "frame_samples": FRAME,
                "buffer_samples": 4410,
                "processed_samples": "UNKNOWN",
                "unprocessed_samples": "UNKNOWN",
                "latency_ms": "UNKNOWN",
                "cpu_wall_ms": "UNKNOWN",
                "rms_dbfs": "UNKNOWN",
                "peak_dbfs": "UNKNOWN",
                "clip_count": "UNKNOWN",
                "sha": "UNKNOWN",
                "command": "python3 scripts/bench_speex.py",
                "exit_code": 1,
                "evidence_class": "BLOCKED",
                "notes": f"BLOCKED: {reason}",
            }
        )

    return rows


# --------------------------------------------------------------------------- #
# main
# --------------------------------------------------------------------------- #


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--no-build", action="store_true", help="no reconstruye la imagen")
    args = ap.parse_args()

    log: list[str] = []
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    meta = {
        "issue": "iariap/x6200_gui#345",
        "task": "B",
        "image": IMAGE,
        "base_image": BASE_IMAGE,
        "source_file": str(SRC_FILE.relative_to(WT)),
        "source_sha256": sha256_file(SRC_FILE) if SRC_FILE.exists() else "UNKNOWN",
        "pcm_manifest_sha256": sha256_file(PCM_MANIFEST) if PCM_MANIFEST.exists() else "UNKNOWN",
        "numpy": None if _np is None else _np.__version__,
    }

    missing = [f for f in SIGNAL_FILES.values() if not (PCM_DIR / f).exists()]
    if missing:
        print("faltan WAV sinteticos; corro gen_pcm.py", file=sys.stderr)
        run([sys.executable, str(EVIDENCE_DIR / "scripts" / "gen_pcm.py")], log)

    # 1) imagen
    inspect = subprocess.run(["docker", "image", "inspect", IMAGE], capture_output=True, text=True)
    if inspect.returncode != 0 or not args.no_build:
        build = run(
            ["docker", "build", "-t", IMAGE, "-f", str(BENCH_DIR / "Dockerfile"), str(BENCH_DIR)],
            log,
            check=False,
        )
        if build.returncode != 0:
            reason = "no se pudo construir la imagen del banco (docker build fallo; ver commands.log)"
            _write_blocked(reason, log, meta)
            print(f"BLOCKED: {reason}", file=sys.stderr)
            return 1
    else:
        log.append(f"$ docker image inspect {IMAGE}  # imagen ya construida, se reusa")

    image_id = subprocess.run(
        ["docker", "image", "inspect", "--format", "{{.Id}}", IMAGE], capture_output=True, text=True
    ).stdout.strip()
    meta["image_id"] = image_id

    # 2) bench dentro del contenedor
    docker_run = [
        "docker",
        "run",
        "--rm",
        "-v",
        f"{WT}:/repo:ro",
        "-v",
        f"{OUT_DIR}:/out",
        "-w",
        "/build",
        IMAGE,
        "sh",
        "/repo/docs/evidencia/2026-10-28-rx-audio-feasibility/scripts/bench/run_in_container.sh",
    ]
    bench = run(docker_run, log, check=False)
    (OUT_DIR / "container.log").write_text("\n".join(log) + "\n", encoding="utf-8")

    if bench.returncode != 0:
        reason = f"el bench dentro del contenedor fallo (exit {bench.returncode}); ver artifacts/bench/container.log"
        _write_blocked(reason, log, meta)
        print(f"BLOCKED: {reason}", file=sys.stderr)
        return 1

    results_file = OUT_DIR / "results.jsonl"
    lines = [json.loads(line) for line in results_file.read_text(encoding="utf-8").splitlines() if line.strip()]
    streams = [r for r in lines if r.get("kind") == "stream"]
    probes = [r for r in lines if r.get("kind") in ("polarity", "rate", "level")]
    n_cases = len({r["case"] for r in streams})
    if n_cases != 17 or len(probes) != 12:
        reason = f"resultados incompletos: {n_cases} casos y {len(probes)} probes (se esperaban 17 y 12)"
        _write_blocked(reason, log, meta)
        print(f"BLOCKED: {reason}", file=sys.stderr)
        return 1

    toolchain = {
        "gcc": _first_line(bench.stdout, "gcc"),
        "speexdsp_pkgconfig": _speex_version(bench.stdout),
    }
    meta["toolchain"] = toolchain
    meta["determinism"] = "noise_p2_b4410 repetido: cmp OK" if "determinismo: noise_p2_b4410" in bench.stdout else "FALLO"

    rows = arithmetic_rows()
    measurements = {}

    by_case: dict[str, list[dict]] = {}
    for r in streams:
        by_case.setdefault(r["case"], []).append(r)

    for name in sorted(by_case):
        runs = by_case[name]
        r = runs[0]
        walls = sorted(x["wall_ms"] for x in runs)
        users = sorted(x["user_ms"] for x in runs)
        max_cbs = sorted(x["max_callback_ms"] for x in runs)
        wall_ms = walls[len(walls) // 2]
        user_ms = users[len(users) // 2]
        sys_ms = sorted(x["sys_ms"] for x in runs)[len(runs) // 2]
        max_cb_ms = max_cbs[len(max_cbs) // 2]
        max_cb_abs_ms = max_cbs[-1]
        wav_name = _wav_for_case(name)
        wav_path = PCM_DIR / wav_name
        out_path = OUT_DIR / f"{name}.raw"
        inp = read_wav(wav_path)
        out = read_raw(out_path)
        st_in = pcm_stats(inp)
        st_out = pcm_stats(out)
        block = int(r["block"])
        jump_block = max_jump(out, boundary_indices(len(out), block))
        jump_frame = max_jump(out, boundary_indices(len(out), FRAME))
        jump_global = max_jump(out, range(1, len(out)))
        in_jump = max_jump(inp, range(1, len(inp)))
        same, total = tail_identity(inp, out, block)
        diff_samples = sum(1 for a, b in zip(inp, out) if a != b)
        lag, lag_ambiguous = lag_samples(inp, out)
        if lag_ambiguous:
            lag = None
        attenuation = (
            None if st_in["rms"] <= 0.0 or st_out["rms"] <= 0.0 else 20.0 * math.log10(st_out["rms"] / st_in["rms"])
        )
        real_time_ms = r["seconds"] * 1000.0
        cpu_pct = 100.0 * wall_ms / real_time_ms

        measurements[name] = {
            "runs": len(runs),
            "block": block,
            "preset": r["preset_name"],
            "in_rms_dbfs": st_in["rms_dbfs"],
            "out_rms_dbfs": st_out["rms_dbfs"],
            "attenuation_db": attenuation,
            "out_peak_dbfs": st_out["peak_dbfs"],
            "out_clip": st_out["clip"],
            "jump_block": jump_block,
            "jump_frame": jump_frame,
            "jump_global": jump_global,
            "in_jump": in_jump,
            "diff_samples": diff_samples,
            "tail_same": same,
            "tail_total": total,
            "lag_samples": lag,
            "lag_ambiguous": lag_ambiguous,
            "wall_ms": wall_ms,
            "wall_ms_min": walls[0],
            "wall_ms_max": walls[-1],
            "user_ms": user_ms,
            "sys_ms": sys_ms,
            "max_callback_ms": max_cb_ms,
            "max_callback_abs_ms": max_cb_abs_ms,
            "cpu_pct_of_realtime": cpu_pct,
            "sha256": sha256_file(out_path),
            "processed": r["processed"],
            "unprocessed": r["unprocessed"],
            "rss_preinit_kb": r["rss_preinit_kb"],
            "rss_start_kb": r["rss_start_kb"],
            "rss_end_kb": r["rss_end_kb"],
        }

        notes = [
            f"MEDIDO: {r['seconds']} s de audio procesados en {wall_ms:.1f} ms de wall "
            f"(mediana de {len(runs)} corridas, min {walls[0]:.1f} / max {walls[-1]:.1f}; "
            f"{cpu_pct:.3f}% del tiempo real), max por callback {max_cb_ms:.4f} ms "
            f"(mediana de los maximos por corrida; absoluto {max_cb_abs_ms:.4f} ms). "
            f"El maximo por callback es wall y puede incluir preempcion del host",
            f"preset {r['preset_name']}, callback de {block} muestras",
        ]
        if attenuation is not None:
            notes.append(f"atenuacion RMS {attenuation:+.2f} dB")
        if total:
            notes.append(f"tail sin NR identico al input: {same}/{total} muestras")
        if diff_samples == 0:
            notes.append("salida bit-exacta al input (0 muestras distintas)")
        notes.append(
            f"salto max entre muestras consecutivas: input {in_jump}, output global {jump_global}, "
            f"en borde de callback {jump_block}, en borde de frame 256 {jump_frame}"
        )
        if lag is not None:
            lag_note = f"{lag} muestras ({lag / DATA_RATE * 1000.0:+.4f} ms)"
        elif st_in["rms"] <= 0.0:
            lag_note = "UNKNOWN (senal nula, no hay correlacion posible)"
        else:
            lag_note = (
                "UNKNOWN (pico de correlacion ambiguo por periodicidad de la senal; "
                "el delay inequivoco se mide con ruido blanco)"
            )
        notes.append("lag de muestras por correlacion: " + lag_note)
        notes.append(
            f"RSS antes de audio_enh_init {r['rss_preinit_kb']} kB, con el estado creado "
            f"{r['rss_start_kb']} kB, al final {r['rss_end_kb']} kB"
        )
        if r["processed"] == 0 and r["nr"] == 0:
            notes.append("NR no habilitado en este preset: 0 muestras procesadas por Speex")
        elif r["processed"] == 0:
            notes.append(
                "NR habilitado pero el callback no completa ningun frame de 256: 0 muestras procesadas por Speex"
            )
        elif r["unprocessed"]:
            notes.append(
                f"las {r['unprocessed']} muestras del tail por callback quedan sin NR "
                f"({100.0 * r['unprocessed'] / (r['processed'] + r['unprocessed']):.2f}% por buffer)"
            )

        rows.append(
            {
                "case": name,
                "source": wav_name,
                "rate_hz": DATA_RATE,
                "frame_samples": FRAME,
                "buffer_samples": block,
                "processed_samples": r["processed"],
                "unprocessed_samples": r["unprocessed"],
                "latency_ms": fmt(None if lag is None else lag / DATA_RATE * 1000.0, 4),
                "cpu_wall_ms": fmt(wall_ms, 3),
                "rms_dbfs": fmt(st_out["rms_dbfs"], 3),
                "peak_dbfs": fmt(st_out["peak_dbfs"], 3),
                "clip_count": st_out["clip"],
                "sha": sha256_file(out_path),
                "command": (
                    "docker run --rm -v <worktree>:/repo:ro -v <artifacts>/bench:/out -w /build "
                    f"{IMAGE} sh .../run_in_container.sh  ->  "
                    f"./bench --case {name} --in {wav_name} --preset {r['preset']} "
                    f"--block {block} --seconds {r['seconds']}"
                ),
                "exit_code": 0,
                "evidence_class": "MEASURED-HOST",
                "notes": "; ".join(notes),
            }
        )

    probe_notes = []
    for p in probes:
        probe_notes.append(p)
        if p["kind"] == "level":
            command = f"./bench --probe level --in {SIGNAL_FILES['noise']}"
            notes = (
                f"audio_enh_nr_set_level({p['level']}) -> SPEEX_PREPROCESS_SET_NOISE_SUPPRESS="
                f"{p['requested_db']} dB (formula -30+level*20/100 de src/audio_enhancement.c:156); "
                f"estado a {p['init_rate']} Hz, {p['frames']} frames de 256; in RMS "
                f"{fmt(p.get('in_rms_dbfs'), 2)} dBFS, out RMS {fmt(p.get('out_rms_dbfs'), 2)} dBFS, "
                f"atenuacion medida {p['atten_db']:+.2f} dB"
            )
        elif p["kind"] == "rate":
            command = f"./bench --probe rate --in {SIGNAL_FILES['noise']} --suppress {p['requested']}"
            notes = (
                f"estado Speex inicializado a {p['init_rate']} Hz, {p['frames']} frames de 256; "
                f"SET_NOISE_SUPPRESS={p['requested']} -> GET={p['readback']}; "
                f"in RMS {fmt(p.get('in_rms_dbfs'), 2)} dBFS, out RMS {fmt(p.get('out_rms_dbfs'), 2)} dBFS, "
                f"atenuacion {p['atten_db']:+.2f} dB"
            )
        else:
            command = f"./bench --probe polarity --in {SIGNAL_FILES['noise']}"
            notes = (
                f"estado Speex inicializado a {p['init_rate']} Hz, {p['frames']} frames de 256; "
                f"SET_NOISE_SUPPRESS={p['requested']} -> GET={p['readback']}; "
                f"denoise={p.get('denoise', 1)}; in RMS {fmt(p.get('in_rms_dbfs'), 2)} dBFS, "
                f"out RMS {fmt(p.get('out_rms_dbfs'), 2)} dBFS, atenuacion {p['atten_db']:+.2f} dB"
            )

        rows.append(
            {
                "case": p["case"],
                "source": SIGNAL_FILES["noise"],
                "rate_hz": p["init_rate"],
                "frame_samples": FRAME,
                "buffer_samples": "UNKNOWN",
                "processed_samples": "UNKNOWN",
                "unprocessed_samples": "UNKNOWN",
                "latency_ms": "UNKNOWN",
                "cpu_wall_ms": "UNKNOWN",
                "rms_dbfs": fmt(p.get("out_rms_dbfs"), 3),
                "peak_dbfs": "UNKNOWN",
                "clip_count": "UNKNOWN",
                "sha": "UNKNOWN",
                "command": command,
                "exit_code": 0,
                "evidence_class": "MEASURED-HOST",
                "notes": notes,
            }
        )

    rows.extend(blocked_rows(None))

    meta["probes"] = probe_notes
    meta["measurements"] = measurements

    with CSV_PATH.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=COLUMNS)
        writer.writeheader()
        writer.writerows(rows)

    RESULTS_PATH.write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    print(f"OK: {n_cases} casos medidos ({len(streams)} corridas) con libspeexdsp real, {len(probes)} sondas")
    print(f"imagen {image_id}")
    print(f"speexdsp {toolchain['speexdsp_pkgconfig']}, {toolchain['gcc']}")
    print(f"fuente {meta['source_sha256']} ({meta['source_file']})")
    print(f"CSV: {CSV_PATH}")
    print(f"resultados: {RESULTS_PATH}")
    return 0


def _write_blocked(reason: str, log: list[str], meta: dict) -> None:
    rows = arithmetic_rows() + blocked_rows(reason)
    with CSV_PATH.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=COLUMNS)
        writer.writeheader()
        writer.writerows(rows)
    meta["blocked"] = reason
    RESULTS_PATH.write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    (OUT_DIR / "container.log").write_text("\n".join(log) + "\n", encoding="utf-8")


def _first_line(text: str, needle: str) -> str:
    for line in text.splitlines():
        if needle in line:
            return line.strip()
    return "UNKNOWN"


def _speex_version(text: str) -> str:
    lines = [ln.strip() for ln in text.splitlines()]
    for i, line in enumerate(lines):
        if line == "--- toolchain ---" and i + 2 < len(lines):
            return lines[i + 2]
    return "UNKNOWN"


def _wav_for_case(name: str) -> str:
    for prefix, wav in SIGNAL_FILES.items():
        if name.startswith(prefix):
            return wav
    raise SystemExit(f"caso sin WAV asociado: {name}")


if __name__ == "__main__":
    sys.exit(main())
