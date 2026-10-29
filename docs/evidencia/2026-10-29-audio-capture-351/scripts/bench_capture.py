#!/usr/bin/env python3
"""Banco de captura con libspeexdsp REAL sobre src/audio_enhancement.c (issue #351, parte 3a).

Orquesta:
  1. gen_pcm.py: WAV sinteticos deterministas (semilla 6200, 10 s) + manifiesto sha256.
  2. docker build de scripts/bench/Dockerfile (Debian bookworm pinneado por digest +
     libspeexdsp-dev=1.2.1-1) — el host NO tiene libspeexdsp.
  3. docker run con el worktree montado read-only: el contenedor copia
     src/audio_enhancement.c VERBATIM, lo compila con -Werror contra la lib real y corre
     la matriz Off / SSB voice / AM broadcast sobre las tres senales (scripts/bench/
     run_in_container.sh).
  4. analisis en el host: RMS, peak, clipping, seams en los bordes de callback y de trama,
     retardo por correlacion, muestras producidas, tiempos de proceso, bit-exactitud por
     particion y mapa level -> supresion efectiva.
  5. escribe measurements.csv, bench-results.json y commands.log.

Si el contenedor no se puede construir, la lib real falta o una verificacion obligatoria
falla, el script NO inventa numeros: escribe las filas BLOCKED con la causa y devuelve 1.

Uso:
    python3 bench_capture.py              # corre todo
    python3 bench_capture.py --no-build   # reusa la imagen ya construida

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
COMMANDS_LOG = EVIDENCE_DIR / "commands.log"
PCM_MANIFEST = EVIDENCE_DIR / "pcm-manifest.json"
SRC_FILE = WT / "src" / "audio_enhancement.c"
SRC_HEADER = WT / "src" / "audio_enhancement.h"

IMAGE = "x6200-speexdsp-capture-bench:1.2.1-1"
BASE_IMAGE = "debian:bookworm-slim@sha256:99c0b25c46d0214d5a2091092e7908907dcaecd6a962cde0ac700acee222b6f3"
DATA_RATE = 44100
FRAME = 441
CLIP_LEVEL = 32767

COLUMNS = [
    "case",
    "source",
    "rate_hz",
    "frame_samples",
    "block_pattern",
    "suppression_db",
    "latency_samples",
    "latency_ms",
    "rms_dbfs",
    "peak_dbfs",
    "clip_count",
    "seam_callback_lsb",
    "seam_frame_lsb",
    "seam_input_lsb",
    "wall_ms",
    "cb_p99_ms",
    "in_samples",
    "out_samples",
    "bit_exact_vs_input",
    "partition_invariant",
    "sha256",
    "command",
    "exit_code",
    "evidence_class",
    "notes",
]

SIGNAL_FILES = {
    "noise": "white_noise_44100_s16.wav",
    "multisine": "multisine_44100_s16.wav",
    "sine1k": "sine_1k_44100_s16.wav",
}

BLOCK_TAGS = {
    "b4410": "4410",
    "b441": "441",
    "bmixed": "1,440,441,442,257",
    "b8820": "8820",
}

PRESETS = {0: "OFF", 2: "SSB_VOICE", 3: "AM_BROADCAST"}

# Presets donde el issue EXIGE bit-exactitud entre particiones.
BIT_EXACT_REQUIRED = (0, 2)

try:  # numpy solo acelera el lag por correlacion; el resto es stdlib
    import numpy as _np
except ImportError:  # pragma: no cover
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
        raw = w.readframes(w.getnframes())
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


def max_jump(samples: array.array, indices) -> int:
    return max((abs(samples[i] - samples[i - 1]) for i in indices if i > 0), default=0)


def pattern_boundaries(pattern: str, n: int) -> list[int]:
    """Posiciones de inicio de callback que produce el patron ciclado."""
    sizes = [int(v) for v in pattern.split(",")]
    out = []
    pos = 0
    k = 0
    while pos < n:
        size = sizes[k % len(sizes)]
        out.append(pos)
        pos += size
        k += 1
    return out


def lag_samples(inp: array.array, out: array.array, window: int = 44100, max_lag: int = 2048):
    """Lag (muestras) del pico de correlacion normalizada, y si es ambiguo.

    Una senal periodica correlaciona casi igual en lags separados un periodo: ahi el
    pico no identifica el delay (se marca ambiguo y no se reporta).
    """
    if _np is None:
        return (None, True)
    n = min(window, len(inp), len(out))
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
# filas BLOCKED (lo que este banco no puede medir)
# --------------------------------------------------------------------------- #


def blocked_rows(reason: str | None) -> list[dict]:
    rows = [
        {
            "case": "audible_quality_rx_to_speaker",
            "source": "ruta RX->parlante/auriculares del BASE",
            "evidence_class": "BLOCKED",
            "notes": (
                "BLOCKED: la cadena de captura NO alimenta el parlante; la ruta audible y "
                "cualquier retorno seguro de RX procesado siguen NOT_VERIFIED (#347). Este "
                "banco no mide calidad audible."
            ),
        },
        {
            "case": "latency_end_to_end_rx_to_speaker",
            "source": "PulseAudio + BASE + parlante/auriculares",
            "evidence_class": "BLOCKED",
            "notes": (
                "BLOCKED: la latencia RX->parlante incluye PulseAudio (fragsize 100 ms), el "
                "DSP del BASE y el trayecto electrico; aca solo se mide el retardo "
                "algoritmico de la copia de captura."
            ),
        },
        {
            "case": "cpu_on_radio",
            "source": "GUI x6200 en la radio (R16)",
            "evidence_class": "BLOCKED",
            "notes": (
                "BLOCKED: el CPU del banco es un x86_64 de host; el R16 es otra arquitectura "
                "y otra carga. No se extrapola presupuesto de CPU."
            ),
        },
        {
            "case": "audible_intelligibility",
            "source": "senales sinteticas (tonos y ruido)",
            "evidence_class": "BLOCKED",
            "notes": (
                "BLOCKED: tonos y ruido sinteticos prueban integridad de procesamiento, no "
                "inteligibilidad de voz SSB/AM; hace falta escucha con voz real y la radio."
            ),
        },
    ]

    if reason is not None:
        rows.append(
            {
                "case": "bench_real_libspeexdsp",
                "source": "contenedor docker (scripts/bench/Dockerfile)",
                "evidence_class": "BLOCKED",
                "notes": f"BLOCKED: {reason}",
                "exit_code": 1,
            }
        )

    for row in rows:
        row.setdefault("exit_code", "N/A")
        for col in COLUMNS:
            row.setdefault(col, "UNKNOWN")
    return rows


def write_rows(rows: list[dict]) -> None:
    with CSV_PATH.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=COLUMNS)
        writer.writeheader()
        writer.writerows(rows)


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
        "issue": "iariap/x6200_gui#351",
        "part": "3a",
        "image": IMAGE,
        "base_image": BASE_IMAGE,
        "source_file": str(SRC_FILE.relative_to(WT)),
        "source_sha256": sha256_file(SRC_FILE) if SRC_FILE.exists() else "UNKNOWN",
        "source_header_sha256": sha256_file(SRC_HEADER) if SRC_HEADER.exists() else "UNKNOWN",
        "numpy": None if _np is None else _np.__version__,
        "frame": FRAME,
        "rate_hz": DATA_RATE,
    }

    # 1) PCM sintetico determinista.
    missing = [f for f in SIGNAL_FILES.values() if not (PCM_DIR / f).exists()]
    if missing:
        gen = run([sys.executable, str(EVIDENCE_DIR / "scripts" / "gen_pcm.py")], log, check=False)
        if gen.returncode != 0:
            reason = "no se pudieron generar los WAV sinteticos (ver commands.log)"
            _write_blocked(reason, log, meta)
            print(f"BLOCKED: {reason}", file=sys.stderr)
            return 1
    else:
        log.append(f"# WAV ya presentes en {PCM_DIR} (gen_pcm.py --verify a continuacion)")
        run([sys.executable, str(EVIDENCE_DIR / "scripts" / "gen_pcm.py"), "--verify"], log, check=False)

    meta["pcm_manifest_sha256"] = sha256_file(PCM_MANIFEST) if PCM_MANIFEST.exists() else "UNKNOWN"

    # 2) imagen del banco.
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

    meta["image_id"] = subprocess.run(
        ["docker", "image", "inspect", "--format", "{{.Id}}", IMAGE], capture_output=True, text=True
    ).stdout.strip()

    # 3) matriz dentro del contenedor.
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
        "/repo/docs/evidencia/2026-10-29-audio-capture-351/scripts/bench/run_in_container.sh",
    ]
    bench = run(docker_run, log, check=False)

    if bench.returncode != 0:
        reason = f"el bench dentro del contenedor fallo (exit {bench.returncode}); ver commands.log"
        _write_blocked(reason, log, meta)
        print(f"BLOCKED: {reason}", file=sys.stderr)
        return 1

    results_file = OUT_DIR / "results.jsonl"
    lines = [json.loads(line) for line in results_file.read_text(encoding="utf-8").splitlines() if line.strip()]
    streams = [r for r in lines if r.get("kind") == "stream"]
    checks = [r for r in lines if r.get("kind") == "partition_check"]
    probes = [r for r in lines if r.get("kind") == "level"]
    speex_only = [r for r in lines if r.get("kind") == "speex_only"]

    expected_cases = len(SIGNAL_FILES) * len(PRESETS) * len(BLOCK_TAGS)
    n_cases = len({r["case"] for r in streams})
    if (
        n_cases != expected_cases
        or len(checks) != len(PRESETS)
        or len(probes) != 4
        or len(speex_only) != 1
    ):
        reason = (
            f"resultados incompletos: {n_cases} casos (se esperaban {expected_cases}), "
            f"{len(checks)} partition-check (3), {len(probes)} probes (4) y "
            f"{len(speex_only)} sonda speex-only (1)"
        )
        _write_blocked(reason, log, meta)
        print(f"BLOCKED: {reason}", file=sys.stderr)
        return 1

    meta["toolchain"] = {
        "gcc": _first_line(bench.stdout, "gcc"),
        "speexdsp_pkgconfig": _speex_version(bench.stdout),
        "noise_suppress_constants": [
            ln.strip() for ln in bench.stdout.splitlines() if "SPEEX_PREPROCESS_" in ln and "define" in ln
        ],
    }

    # 4) analisis en el host.
    repeat_shas: dict[str, list[str]] = {}
    repeats_file = OUT_DIR / "repeats.sha256"
    if repeats_file.exists():
        for line in repeats_file.read_text(encoding="utf-8").splitlines():
            parts = line.split()
            if len(parts) == 2:
                repeat_shas.setdefault(parts[1], []).append(parts[0])

    by_case: dict[str, list[dict]] = {}
    for r in streams:
        by_case.setdefault(r["case"], []).append(r)

    rows: list[dict] = []
    measurements: dict[str, dict] = {}
    failures: list[str] = []

    def fail(msg: str) -> None:
        failures.append(msg)

    # Retardo propio de SpeexDSP, medido con un estado SOLO de la lib: separa el
    # retardo de la maquina de tramas del modulo (una trama) del que pone la libreria.
    noise_in = read_wav(PCM_DIR / SIGNAL_FILES["noise"])
    speex_out = read_raw(OUT_DIR / "probe_speex_only.raw")
    speex_lag, speex_lag_ambiguous = lag_samples(noise_in, speex_out)
    if speex_lag_ambiguous:
        fail("sonda speex-only: pico de correlacion ambiguo, no se puede separar el retardo")
    meta["speex_only_delay"] = {
        "samples": speex_lag,
        "ms": None if speex_lag is None else speex_lag / DATA_RATE * 1000.0,
        "ambiguous": speex_lag_ambiguous,
        "frame": FRAME,
        "rate_hz": DATA_RATE,
        "suppression_db": speex_only[0]["suppression_db"],
        "frames": speex_only[0]["frames"],
        "note": (
            "estado SpeexPreprocessState directo, sin la cadena del modulo: mide el retardo "
            "algoritmico propio de libspeexdsp (un frame)"
        ),
    }
    expected_nr_lag = None if speex_lag is None else FRAME + speex_lag

    for name in sorted(by_case):
        runs = by_case[name]
        r = runs[0]
        walls = sorted(x["wall_ms"] for x in runs)
        p99s = sorted(x["cb_p99_ms"] for x in runs)
        wall_ms = walls[len(walls) // 2]
        p99_ms = p99s[len(p99s) // 2]
        max_cb_ms = max(x["cb_max_ms"] for x in runs)
        wav_name = _wav_for_case(name)
        wav_path = PCM_DIR / wav_name
        out_path = OUT_DIR / f"{name}.raw"
        inp = read_wav(wav_path)
        out = read_raw(out_path)
        st_in = pcm_stats(inp)
        st_out = pcm_stats(out)
        pattern = r["blocks"]

        bounds = pattern_boundaries(pattern, len(out))
        seam_cb = max_jump(out, bounds)
        seam_frame = max_jump(out, range(FRAME, len(out), FRAME))
        seam_in = max_jump(inp, range(1, len(inp)))
        diff = sum(1 for a, b in zip(inp, out) if a != b)
        lag, lag_ambiguous = lag_samples(inp, out)
        if lag_ambiguous:
            lag = None

        attenuation = (
            None
            if st_in["rms"] <= 0.0 or st_out["rms"] <= 0.0
            else 20.0 * math.log10(st_out["rms"] / st_in["rms"])
        )
        real_time_ms = r["seconds"] * 1000.0
        cpu_pct = 100.0 * wall_ms / real_time_ms

        # Verificaciones obligatorias por caso.
        if r["total_in"] != r["total_out"] or len(out) != len(inp):
            fail(f"{name}: N in/N out distinto ({r['total_in']} -> {r['total_out']}, raw {len(out)})")

        if r["preset"] in (0, 4) and diff != 0:
            fail(f"{name}: preset {r['preset_name']} no es bit-exacto al input ({diff} muestras distintas)")

        if st_out["clip"] != 0:
            fail(f"{name}: {st_out['clip']} muestras en clipping")

        if name.startswith("noise_"):
            if r["nr"] and lag != expected_nr_lag:
                fail(
                    f"{name}: retardo medido {lag} muestras, se esperaba {expected_nr_lag} "
                    f"({FRAME} de la maquina de tramas + {speex_lag} de la lib)"
                )
            if not r["nr"] and lag not in (0, None):
                fail(f"{name}: preset sin NR con retardo {lag} muestras (se esperaba 0)")

        if r["nr"] and r["suppression_db"] != _expected_suppress(r["nr_level"]):
            fail(
                f"{name}: supresion efectiva {r['suppression_db']} dB != "
                f"{_expected_suppress(r['nr_level'])} dB para level {r['nr_level']}"
            )

        shas = repeat_shas.get(name, [])
        if len(shas) == 3 and len(set(shas)) != 1:
            fail(f"{name}: las 3 corridas no son deterministas ({shas})")

        notes = [
            f"MEDIDO: 10 s de audio en {wall_ms:.1f} ms de wall (mediana de {len(runs)} corridas), "
            f"{cpu_pct:.3f}% del tiempo real; p99 por callback {p99_ms * 1000.0:.1f} us, "
            f"maximo {max_cb_ms * 1000.0:.1f} us (wall del host x86_64, no del R16)",
            f"preset {r['preset_name']}, particion {pattern} ({r['callbacks']} callbacks)",
            f"N in {r['total_in']} -> N out {r['total_out']} (trama {r['frame']} muestras a {r['rate']} Hz)",
        ]
        if r["nr"]:
            notes.append(
                f"NR on, level {r['nr_level']} -> supresion efectiva leida de la lib real "
                f"{r['suppression_db']} dB"
            )
        else:
            notes.append(
                f"NR off: no se aplica supresion (el estado Speex existe con su default de la "
                f"lib, {r['suppression_db']} dB, y no se usa)"
            )
        if diff == 0:
            notes.append("salida bit-exacta al input (0 muestras distintas)")
        elif r["preset"] in (0, 4):
            notes.append(f"{diff} muestras distintas del input en un preset que deberia ser passthrough")
        if attenuation is not None:
            notes.append(f"atenuacion RMS {attenuation:+.2f} dB")
        notes.append(
            f"seam maximo entre muestras consecutivas: input {seam_in} LSB, salida en borde de "
            f"callback {seam_cb} LSB, en borde de trama 441 {seam_frame} LSB"
        )
        if lag is not None:
            notes.append(
                f"retardo por correlacion: {lag} muestras ({lag / DATA_RATE * 1000.0:.3f} ms)"
                + (
                    f" = {FRAME} (maquina de tramas) + {speex_lag} (retardo propio de libspeexdsp)"
                    if r["nr"] and speex_lag is not None
                    else ""
                )
            )
        elif st_in["rms"] <= 0.0:
            notes.append("retardo por correlacion: UNKNOWN (senal nula)")
        else:
            notes.append(
                "retardo por correlacion: UNKNOWN (pico ambiguo por periodicidad de la senal; "
                "el delay inequivoco se mide con ruido blanco)"
            )

        measurements[name] = {
            "runs": len(runs),
            "preset": r["preset_name"],
            "block_pattern": pattern,
            "callbacks": r["callbacks"],
            "frame": r["frame"],
            "rate_hz": r["rate"],
            "enh": r["enh"],
            "nr": r["nr"],
            "nr_level": r["nr_level"],
            "suppression_db": r["suppression_db"],
            "in_samples": r["total_in"],
            "out_samples": r["total_out"],
            "in_rms_dbfs": st_in["rms_dbfs"],
            "out_rms_dbfs": st_out["rms_dbfs"],
            "attenuation_db": attenuation,
            "out_peak_dbfs": st_out["peak_dbfs"],
            "out_clip": st_out["clip"],
            "seam_callback_lsb": seam_cb,
            "seam_frame_lsb": seam_frame,
            "seam_input_lsb": seam_in,
            "diff_samples": diff,
            "lag_samples": lag,
            "lag_ambiguous": lag_ambiguous,
            "wall_ms": wall_ms,
            "wall_ms_min": walls[0],
            "wall_ms_max": walls[-1],
            "cb_p99_ms": p99_ms,
            "cb_max_ms": max_cb_ms,
            "cpu_pct_of_realtime": cpu_pct,
            "sha256": sha256_file(out_path),
            "repeat_shas": shas,
        }

        rows.append(
            {
                "case": name,
                "source": wav_name,
                "rate_hz": r["rate"],
                "frame_samples": r["frame"],
                "block_pattern": pattern,
                "suppression_db": r["suppression_db"] if r["nr"] else "N/A",
                "latency_samples": "UNKNOWN" if lag is None else lag,
                "latency_ms": fmt(None if lag is None else lag / DATA_RATE * 1000.0, 4),
                "rms_dbfs": fmt(st_out["rms_dbfs"], 3),
                "peak_dbfs": fmt(st_out["peak_dbfs"], 3),
                "clip_count": st_out["clip"],
                "seam_callback_lsb": seam_cb,
                "seam_frame_lsb": seam_frame,
                "seam_input_lsb": seam_in,
                "wall_ms": fmt(wall_ms, 3),
                "cb_p99_ms": fmt(p99_ms, 6),
                "in_samples": r["total_in"],
                "out_samples": r["total_out"],
                "bit_exact_vs_input": 1 if diff == 0 else 0,
                "partition_invariant": "UNKNOWN",
                "sha256": sha256_file(out_path),
                "command": (
                    "docker run --rm -v <worktree>:/repo:ro -v <evidence>/artifacts/bench:/out "
                    f"-w /build {IMAGE} sh .../run_in_container.sh  ->  "
                    f"./bench --case {name} --in {wav_name} --preset {r['preset']} "
                    f"--blocks {pattern} --seconds 10"
                ),
                "exit_code": 0,
                "evidence_class": "MEASURED-HOST",
                "notes": "; ".join(notes),
            }
        )

    # Invariante de particion: mismo input, distintas particiones.
    invariance: dict[str, dict] = {}
    for sig in SIGNAL_FILES:
        for preset in PRESETS:
            shas = {}
            for tag in BLOCK_TAGS:
                name = f"{sig}_p{preset}_{tag}"
                if name in measurements:
                    shas[tag] = measurements[name]["sha256"]
            unique = sorted(set(shas.values()))
            invariant = len(unique) == 1
            invariance[f"{sig}_p{preset}"] = {"shas": shas, "invariant": invariant}
            for tag in shas:
                row = next(r for r in rows if r["case"] == f"{sig}_p{preset}_{tag}")
                row["partition_invariant"] = 1 if invariant else 0
            if preset in BIT_EXACT_REQUIRED and not invariant:
                fail(f"{sig}_p{preset}: la salida cambia con la particion (se exigia bit-exacta)")
            if preset not in BIT_EXACT_REQUIRED and not invariant:
                base = measurements[f"{sig}_p{preset}_b4410"]
                mixed = measurements[f"{sig}_p{preset}_bmixed"]
                same_as_base = sorted(t for t, s in shas.items() if s == base["sha256"])
                invariance[f"{sig}_p{preset}"]["differing_samples"] = _count_diff(
                    OUT_DIR / f"{sig}_p{preset}_b4410.raw", OUT_DIR / f"{sig}_p{preset}_bmixed.raw"
                )
                invariance[f"{sig}_p{preset}"]["max_abs_diff_lsb"] = _max_abs_diff(
                    OUT_DIR / f"{sig}_p{preset}_b4410.raw", OUT_DIR / f"{sig}_p{preset}_bmixed.raw"
                )
                invariance[f"{sig}_p{preset}"]["identical_to_4410"] = same_as_base
                invariance[f"{sig}_p{preset}"]["note"] = (
                    "no se exige bit-exacto: el nivelador AF mide el pico por bloque interno "
                    f"(<= {FRAME} muestras), asi que una particion que fuerza chunks mas cortos "
                    f"cambia la ganancia; identicas a 4410: {', '.join(same_as_base)}; "
                    f"base sha {base['sha256'][:12]}, mixed sha {mixed['sha256'][:12]}"
                )

    # Particion en un proceso con reset explicito entre pases.
    reset_checks: dict[str, dict] = {}
    for c in checks:
        preset = c["preset"]
        a = read_raw(OUT_DIR / f"noise_p{preset}_reset_a.raw")
        b = read_raw(OUT_DIR / f"noise_p{preset}_reset_b.raw")
        same = a == b
        differing = sum(1 for x, y in zip(a, b) if x != y)
        reset_checks[f"noise_p{preset}"] = {
            "preset": c["preset_name"],
            "blocks_a": c["blocks_a"],
            "blocks_b": c["blocks_b"],
            "identical": same,
            "differing_samples": differing,
            "max_abs_diff_lsb": max((abs(x - y) for x, y in zip(a, b)), default=0),
            "length_preserved": bool(c["length_preserved"]),
            "suppression_db": c["suppression_db"],
        }
        if preset == 0 and not same:
            fail(
                f"noise_p{preset}: tras el reset, la particion {c['blocks_b']} difiere de "
                f"{c['blocks_a']} en {differing} muestras"
            )
        if preset != 0 and not same:
            # No es un fallo: se documenta. audio_enh_preset_reset()/el cambio de preset
            # descarta la trama pendiente de la maquina NR, pero NO reinicia la adaptacion
            # interna de libspeexdsp (estimacion de ruido) ni la ganancia suavizada del
            # nivelador AF. La bit-exactitud entre particiones se mide partiendo de estado
            # recien inicializado (la matriz), que es el estado que fija el contrato.
            reset_checks[f"noise_p{preset}"]["note"] = (
                "reset de preset != re-init: libspeexdsp conserva su estimacion adaptativa"
                + (
                    " y el nivelador AF conserva su ganancia suavizada"
                    if preset == 3
                    else ""
                )
                + " entre los dos pases, por eso el segundo pase no es bit-exacto al primero"
            )
        rows.append(
            {
                "case": f"noise_p{preset}_reset",
                "source": SIGNAL_FILES["noise"],
                "rate_hz": c["rate"],
                "frame_samples": c["frame"],
                "block_pattern": f"{c['blocks_a']} | reset | {c['blocks_b']}",
                "suppression_db": c["suppression_db"],
                "latency_samples": "UNKNOWN",
                "latency_ms": "UNKNOWN",
                "rms_dbfs": "UNKNOWN",
                "peak_dbfs": "UNKNOWN",
                "clip_count": "UNKNOWN",
                "seam_callback_lsb": "UNKNOWN",
                "seam_frame_lsb": "UNKNOWN",
                "seam_input_lsb": "UNKNOWN",
                "wall_ms": "UNKNOWN",
                "cb_p99_ms": "UNKNOWN",
                "in_samples": c["input_samples"],
                "out_samples": f"{c['out_samples_a']}/{c['out_samples_b']}",
                "bit_exact_vs_input": "UNKNOWN",
                "partition_invariant": 1 if same else 0,
                "sha256": sha256_file(OUT_DIR / f"noise_p{preset}_reset_b.raw"),
                "command": (
                    f"./bench --partition-check noise_p{preset}_reset --in {SIGNAL_FILES['noise']} "
                    f'--preset {preset} --blocks-a "{c["blocks_a"]}" --blocks-b "{c["blocks_b"]}"'
                ),
                "exit_code": 0,
                "evidence_class": "MEASURED-HOST",
                "notes": (
                    f"mismo input procesado dos veces en un proceso, con audio_enh_preset_reset() "
                    f"entre los pases: {'identicas' if same else f'{differing} muestras distintas'} "
                    f"(max |diff| {max((abs(x - y) for x, y in zip(a, b)), default=0)} LSB); "
                    f"N in {c['input_samples']} -> N out {c['out_samples_a']}/{c['out_samples_b']}"
                ),
            }
        )

    # Mapa level -> supresion efectiva (lib real).
    level_probe: dict[str, dict] = {}
    for p in probes:
        level_probe[str(p["level"])] = {
            "formula_db": p["formula_db"],
            "suppression_db": p["suppression_db"],
            "in_rms_dbfs": p.get("in_rms_dbfs"),
            "out_rms_dbfs": p.get("out_rms_dbfs"),
            "atten_db": p["atten_db"],
            "frames": p["frames"],
        }
        if p["suppression_db"] != p["formula_db"]:
            fail(
                f"level {p['level']}: supresion efectiva {p['suppression_db']} dB != "
                f"formula {p['formula_db']} dB"
            )
        rows.append(
            {
                "case": f"probe_level_{p['level']}",
                "source": SIGNAL_FILES["noise"],
                "rate_hz": p["rate"],
                "frame_samples": p["frame"],
                "block_pattern": "4410 (100 ms)",
                "suppression_db": p["suppression_db"],
                "latency_samples": "UNKNOWN",
                "latency_ms": "UNKNOWN",
                "rms_dbfs": fmt(p.get("out_rms_dbfs"), 3),
                "peak_dbfs": "UNKNOWN",
                "clip_count": "UNKNOWN",
                "seam_callback_lsb": "UNKNOWN",
                "seam_frame_lsb": "UNKNOWN",
                "seam_input_lsb": "UNKNOWN",
                "wall_ms": "UNKNOWN",
                "cb_p99_ms": "UNKNOWN",
                "in_samples": "UNKNOWN",
                "out_samples": "UNKNOWN",
                "bit_exact_vs_input": "UNKNOWN",
                "partition_invariant": "UNKNOWN",
                "sha256": "UNKNOWN",
                "command": f"./bench --probe levels --in {SIGNAL_FILES['noise']}",
                "exit_code": 0,
                "evidence_class": "MEASURED-HOST",
                "notes": (
                    f"audio_enh_nr_set_level({p['level']}) -> formula {p['formula_db']} dB, "
                    f"SPEEX_PREPROCESS_GET_NOISE_SUPPRESS real {p['suppression_db']} dB; "
                    f"{p['frames']} tramas de {p['frame']}; atenuacion RMS medida "
                    f"{p['atten_db']:+.2f} dB"
                ),
            }
        )

    # Monotonia de la atenuacion con el nivel.
    ordered = [level_probe[k] for k in ("0", "40", "60", "100") if k in level_probe]
    if len(ordered) == 4:
        att = [x["atten_db"] for x in ordered]
        if not all(att[i] >= att[i + 1] for i in range(3)):
            fail(f"la atenuacion no crece con el nivel: {att}")
        meta["level_attenuation_monotonic"] = True

    # Retardo propio de la lib, medido con un estado Speex solo.
    st_speex = pcm_stats(speex_out)
    rows.append(
        {
            "case": "probe_speex_only_delay",
            "source": SIGNAL_FILES["noise"],
            "rate_hz": DATA_RATE,
            "frame_samples": FRAME,
            "block_pattern": f"{FRAME} (estado Speex directo, sin la cadena del modulo)",
            "suppression_db": speex_only[0]["suppression_db"],
            "latency_samples": "UNKNOWN" if speex_lag is None else speex_lag,
            "latency_ms": fmt(None if speex_lag is None else speex_lag / DATA_RATE * 1000.0, 4),
            "rms_dbfs": fmt(st_speex["rms_dbfs"], 3),
            "peak_dbfs": fmt(st_speex["peak_dbfs"], 3),
            "clip_count": st_speex["clip"],
            "seam_callback_lsb": "UNKNOWN",
            "seam_frame_lsb": "UNKNOWN",
            "seam_input_lsb": "UNKNOWN",
            "wall_ms": "UNKNOWN",
            "cb_p99_ms": "UNKNOWN",
            "in_samples": len(noise_in),
            "out_samples": len(speex_out),
            "bit_exact_vs_input": 0,
            "partition_invariant": "UNKNOWN",
            "sha256": sha256_file(OUT_DIR / "probe_speex_only.raw"),
            "command": f"./bench --probe speex-only --in {SIGNAL_FILES['noise']} --out probe_speex_only.raw",
            "exit_code": 0,
            "evidence_class": "MEASURED-HOST",
            "notes": (
                f"SpeexPreprocessState directo a {FRAME} muestras / {DATA_RATE} Hz, "
                f"NOISE_SUPPRESS {speex_only[0]['suppression_db']} dB, "
                f"{speex_only[0]['frames']} frames: retardo propio de la lib "
                f"{speex_lag} muestras ({0 if speex_lag is None else speex_lag / DATA_RATE * 1000.0:.3f} ms); "
                f"la cadena del modulo agrega una trama mas (total medido {expected_nr_lag} muestras)"
            ),
        }
    )

    rows.extend(blocked_rows(None))

    meta["measurements"] = measurements
    meta["partition_invariance"] = invariance
    meta["partition_reset_checks"] = reset_checks
    meta["level_probe"] = level_probe
    meta["checks"] = {
        "cases": n_cases,
        "repeats": len(streams),
        "partition_checks": len(checks),
        "level_probes": len(probes),
        "failures": failures,
        "ok": not failures,
    }

    write_rows(rows)
    RESULTS_PATH.write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    COMMANDS_LOG.write_text("\n".join(log) + "\n", encoding="utf-8")

    print(
        f"casos {n_cases} ({len(streams)} corridas), partition-check {len(checks)}, "
        f"probes {len(probes)} con libspeexdsp real"
    )
    print(f"imagen {meta['image_id']}")
    print(f"speexdsp {meta['toolchain']['speexdsp_pkgconfig']}, {meta['toolchain']['gcc']}")
    print(f"fuente {meta['source_sha256']} ({meta['source_file']})")
    print(f"CSV: {CSV_PATH}")
    print(f"resultados: {RESULTS_PATH}")
    print(f"comandos: {COMMANDS_LOG}")

    if failures:
        print(f"FALLO: {len(failures)} verificaciones obligatorias", file=sys.stderr)
        for f in failures:
            print(f"  - {f}", file=sys.stderr)
        return 1

    print("OK: N in/N out conservado, Off bit-exacto, Off/SSB bit-exactos por particion, "
          "sin clipping, supresion efectiva == formula")
    return 0


def _expected_suppress(level: int) -> int:
    return -10 - (int(level) * 20) // 100


def _count_diff(a: Path, b: Path) -> int:
    x = read_raw(a)
    y = read_raw(b)
    return sum(1 for i, j in zip(x, y) if i != j)


def _max_abs_diff(a: Path, b: Path) -> int:
    x = read_raw(a)
    y = read_raw(b)
    return max((abs(i - j) for i, j in zip(x, y)), default=0)


def _write_blocked(reason: str, log: list[str], meta: dict) -> None:
    write_rows(blocked_rows(reason))
    meta["blocked"] = reason
    meta["checks"] = {"ok": False, "failures": [reason]}
    RESULTS_PATH.write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    COMMANDS_LOG.write_text("\n".join(log) + "\n", encoding="utf-8")


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
