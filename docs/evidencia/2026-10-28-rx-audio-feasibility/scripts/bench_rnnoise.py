#!/usr/bin/env python3
"""Banco A/B offline RNNoise vs SpeexDSP vs OFF (issue #345, anexo vinculante, tanda D).

Orquesta y analiza:
  1. Extrae el PCM crudo de los cuatro WAV sinteticos (gen_pcm.py, semilla 6200).
  2. docker build de scripts/bench/Dockerfile.rnnoise (Debian bookworm pinneado,
     libspeexdsp 1.2.1-1) y docker run con el clone de RNNoise montado read-only
     desde /tmp (fuera del repo): el contenedor verifica el commit pinneado,
     descarga y valida el modelo, compila upstream y corre la matriz
     (scripts/bench/rnnoise_build.sh).
  3. Analisis en el host de las salidas: RMS/peak/clipping, discontinuidades en
     las posiciones de empalme de cada cadena, latencia por correlacion, sha256,
     y verificacion de los controles (off bit-exacto, repetibilidad, prefijo de
     10 s, fallback sin RNNoise).
  4. Escribe rnnoise-vs-speex.csv, rnnoise-bench.json, rnnoise-manifest.json,
     los WAV de audio/ y AGREGA sus filas a measurements.csv.

Ningun numero se inventa: si el build, la descarga del modelo o el banco fallan,
escribe las filas BLOCKED con la causa y devuelve exit 1.

Uso:
    python3 bench_rnnoise.py             # corre todo
    python3 bench_rnnoise.py --no-build  # reusa la imagen ya construida
    python3 bench_rnnoise.py --analyse   # solo re-analiza artifacts/rnnoise

SPDX-License-Identifier: LGPL-2.1-or-later
"""

from __future__ import annotations

import argparse
import array
import csv
import hashlib
import json
import math
import struct
import subprocess
import sys
import wave
from pathlib import Path

# Mismos helpers y mismas columnas que el banco de la tarea B: este script vive
# al lado de bench_speex.py, asi que el import resuelve por el directorio del
# script (sys.path[0]) sin tocar el path a mano.
from bench_speex import COLUMNS as MEASUREMENTS_COLUMNS
from bench_speex import fmt, lag_samples, pcm_stats, sha256_file

EVIDENCE_DIR = Path(__file__).resolve().parent.parent
WT = EVIDENCE_DIR.parents[2]
BENCH_DIR = EVIDENCE_DIR / "scripts" / "bench"
PCM_DIR = EVIDENCE_DIR / "artifacts" / "pcm"
OUT_DIR = EVIDENCE_DIR / "artifacts" / "rnnoise"
IN_DIR = OUT_DIR / "in"
AUDIO_DIR = EVIDENCE_DIR / "audio"

CSV_PATH = EVIDENCE_DIR / "rnnoise-vs-speex.csv"
JSON_PATH = EVIDENCE_DIR / "rnnoise-bench.json"
MANIFEST_PATH = EVIDENCE_DIR / "rnnoise-manifest.json"
MEASUREMENTS_PATH = EVIDENCE_DIR / "measurements.csv"

IMAGE = "x6200-rnnoise-bench:70f1d25-1"
BASE_IMAGE = "debian:bookworm-slim@sha256:99c0b25c46d0214d5a2091092e7908907dcaecd6a962cde0ac700acee222b6f3"
RNNOISE_REPO = "https://github.com/xiph/rnnoise"
RNNOISE_COMMIT = "70f1d256acd4b34a572f999a05c87bf00b67730d"
RNNOISE_MODEL_URL = (
    "https://media.xiph.org/rnnoise/models/"
    "rnnoise_data-0a8755f8e2d834eff6a54714ecc7d75f9932e845df35f8b59bc52a7cfe6e8b37.tar.gz"
)
RNNOISE_MODEL_SHA = "0a8755f8e2d834eff6a54714ecc7d75f9932e845df35f8b59bc52a7cfe6e8b37"
RNNOISE_MODEL_BYTES = 58603099
SRC_RNNOISE = Path("/tmp/opencode/rnnoise-src")

IN_RATE = 44100
UNIT_IN = 441  # 10 ms a 44100 Hz
CLIP_LEVEL = 32767
SECONDS = 60
REPS = 5
WARMUPS = 3

VECTORS = {
    "silence": ("silence_44100_s16.wav", "silencio digital"),
    "sine1k": ("sine_1k_44100_s16.wav", "seno 1 kHz a -18 dBFS de pico"),
    "multisine": ("multisine_44100_s16.wav", "mezcla 300/1000/2400 Hz a -18 dBFS de pico"),
    "noise": ("white_noise_44100_s16.wav", "ruido blanco uniforme a -30 dBFS (semilla 6200)"),
}

METHODS = ("off", "resample", "speex", "rnnoise", "rnnoise_norm")

# Vectores periodicos: el pico de correlacion cae en cualquier multiplo del
# periodo, asi que no identifica el retardo de la cadena.
PERIODIC = ("sine1k", "multisine")

# Sonda de latencia: la cadena Speex con un frame por callback no empalma
# segmentos procesados y sin procesar, asi que su retardo es unico.
# (caso, vector, metodo, bloque por callback)
PROBES: tuple[tuple[str, str, str, int], ...] = (("noise_speex_b256", "noise", "speex_b256", 256),)

VERSIONS = {
    "off": "passthrough del banco (sin resampling)",
    "resample": "resampler propio polifasico taps=64 Kaiser beta=8 (bench_rnnoise.c)",
    "speex": "libspeexdsp 1.2.1-1 + src/audio_enhancement.c (preset SSB_VOICE, callback 441)",
    "speex_b256": "libspeexdsp 1.2.1-1 + src/audio_enhancement.c (preset SSB_VOICE, callback 256)",
    "rnnoise": f"rnnoise {RNNOISE_COMMIT[:8]} + modelo {RNNOISE_MODEL_SHA[:8]} (escala S16)",
    "rnnoise_norm": f"rnnoise {RNNOISE_COMMIT[:8]} + modelo {RNNOISE_MODEL_SHA[:8]} (control /32768)",
}

COMPARATIVE_COLUMNS = [
    "vector",
    "method",
    "version",
    "rate_hz",
    "samples",
    "rms_dbfs",
    "peak_dbfs",
    "clip_count",
    "discontinuities",
    "latency_ms",
    "frame_ms_p50",
    "frame_ms_p95",
    "frame_ms_p99",
    "total_ms",
    "peak_rss_kb",
    "pct_frames_over_10ms",
    "resample_up_ms_p50",
    "rnnoise_ms_p50",
    "speex_ms_p50",
    "resample_down_ms_p50",
    "sha256",
    "audio_destination",
    "evidence_class",
    "notes",
]

AUDIO_DESTINATION = "OFFLINE_CAPTURE_ONLY"
COMMITTED_WAVS = ("multisine", "noise")


# --------------------------------------------------------------------------- #
# utilidades
# --------------------------------------------------------------------------- #


def read_raw(path: Path) -> array.array:
    samples = array.array("h")
    samples.frombytes(path.read_bytes())
    if sys.byteorder != "little":
        samples.byteswap()
    return samples


def _byteswapped(samples: array.array) -> bytes:
    copy = array.array("h", samples)
    copy.byteswap()
    return copy.tobytes()


def wav_bytes(samples: array.array) -> bytes:
    payload = samples.tobytes() if sys.byteorder == "little" else _byteswapped(samples)
    header = b"RIFF" + struct.pack("<I", 36 + len(payload)) + b"WAVE"
    header += b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, IN_RATE, IN_RATE * 2, 2, 16)
    header += b"data" + struct.pack("<I", len(payload))
    return header + payload


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


def median(values: list[float]) -> float:
    return sorted(values)[len(values) // 2]


def max_jump(samples: array.array) -> int:
    return max((abs(samples[i] - samples[i - 1]) for i in range(1, len(samples))), default=0)


def rms(samples: array.array) -> float:
    if not samples:
        return 0.0
    return math.sqrt(sum(float(s) * s for s in samples) / len(samples))


def _processed(measurement: dict) -> int:
    """Muestras que entraron a un motor de NR (misma semantica que la tarea B)."""
    if measurement["method"].startswith("speex"):
        return measurement["units"] * 256  # audio_enh_process solo procesa frames de 256
    if measurement["method"].startswith("rnnoise"):
        return measurement["out_samples"]
    return 0


def _unprocessed(measurement: dict) -> int:
    return measurement["out_samples"] - _processed(measurement)


def prepare_inputs(log: list[str]) -> None:
    missing = [name for name, _ in VECTORS.values() if not (PCM_DIR / name).exists()]
    if missing:
        run([sys.executable, str(EVIDENCE_DIR / "scripts" / "gen_pcm.py")], log)
    IN_DIR.mkdir(parents=True, exist_ok=True)
    for name, _ in VECTORS.values():
        raw = IN_DIR / (Path(name).stem + ".raw")
        with wave.open(str(PCM_DIR / name), "rb") as handle:
            if handle.getnchannels() != 1 or handle.getsampwidth() != 2:
                raise RuntimeError(f"{name}: se esperaba mono S16")
            payload = handle.readframes(handle.getnframes())
        raw.write_bytes(payload)
        log.append(f"$ extraccion PCM {name} -> {raw.name} ({len(payload)} bytes)")


def check_pinned_source(log: list[str]) -> str:
    """Verifica en el host que el clone de /tmp esta en el commit pinneado."""
    if not SRC_RNNOISE.exists():
        raise RuntimeError(f"no existe el clone de RNNoise en {SRC_RNNOISE}")
    head = subprocess.run(
        ["git", "-C", str(SRC_RNNOISE), "rev-parse", "HEAD"], capture_output=True, text=True
    )
    log.append(f"$ git -C {SRC_RNNOISE} rev-parse HEAD")
    log.append(f"[exit {head.returncode}]")
    log.append(head.stdout.strip())
    if head.returncode != 0 or head.stdout.strip() != RNNOISE_COMMIT:
        raise RuntimeError(
            f"el clone de RNNoise no esta en {RNNOISE_COMMIT} (obtenido {head.stdout.strip()!r})"
        )
    return head.stdout.strip()


# --------------------------------------------------------------------------- #
# discontinuidades y latencia
# --------------------------------------------------------------------------- #


def seam_positions(n: int, method: str, block: int) -> set[int]:
    """Posiciones donde la cadena empalma segmentos, segun como trocee la senal.

    - speex: borde de callback (cada `block`) y corte interno del frame de 256
      dentro del callback (audio_enh_process procesa solo frames completos).
    - rnnoise: borde de frame de 10 ms (441 muestras a 44.1 kHz).
    - off/resample: la cadena no trocea (filtro continuo / copia exacta).
    """
    positions: set[int] = set()
    if method.startswith("speex"):
        for start in range(0, n, block):
            if start:
                positions.add(start)
            if start + 256 < n:
                positions.add(start + 256)
    elif method.startswith("rnnoise"):
        positions.update(range(block, n, block))
    return positions


def seam_metric(out: array.array, method: str, block: int) -> dict:
    """Discontinuidades autoclaibradas en las posiciones de empalme.

    Un empalme es discontinuo si su salto entre muestras supera el maximo salto
    que aparece en la MISMA salida fuera de las posiciones de empalme (+-3
    muestras). Asi el umbral se adapta a cada senal y no hay que fijar un valor
    en dB a ojo.
    """
    n = len(out)
    seams = seam_positions(n, method, block)
    if not seams:
        return {
            "seams": 0,
            "jump_seam_max": 0,
            "jump_nonseam_max": 0,
            "discontinuities": 0,
        }

    near: set[int] = set()
    for position in seams:
        for delta in range(-3, 4):
            near.add(position + delta)

    seam_max = 0
    nonseam_max = 0
    over = 0
    for i in range(1, n):
        jump = abs(out[i] - out[i - 1])
        if i in seams:
            seam_max = max(seam_max, jump)
        elif i not in near:
            nonseam_max = max(nonseam_max, jump)

    for i in seams:
        if i < n and abs(out[i] - out[i - 1]) > nonseam_max:
            over += 1

    return {
        "seams": len(seams),
        "jump_seam_max": seam_max,
        "jump_nonseam_max": nonseam_max,
        "discontinuities": over,
    }


def analyse(vector: str, method: str, inp: array.array, out: array.array, block: int) -> dict:
    st_in = pcm_stats(inp)
    st_out = pcm_stats(out)
    diff = sum(1 for a, b in zip(inp, out, strict=True) if a != b)
    attenuation = (
        None
        if st_in["rms"] <= 0.0 or st_out["rms"] <= 0.0
        else 20.0 * math.log10(st_out["rms"] / st_in["rms"])
    )

    if diff == 0:
        lag: int | None = 0
        lag_reason = "bit-exacto (0 muestras distintas): retardo 0"
    elif vector in PERIODIC:
        lag = None
        lag_reason = (
            "vector periodico: el pico de correlacion cae en cualquier multiplo del periodo, "
            "no identifica el retardo (se mide con el vector de ruido)"
        )
    elif method.startswith("speex") and block == UNIT_IN:
        lag = None
        lag_reason = (
            "la cadena empalma 256 muestras procesadas (retardo de un frame) con 185 copiadas "
            "sin procesar: no tiene un retardo unico; la sonda speex_b256 lo mide con un frame "
            "por callback"
        )
    else:
        measured, ambiguous = lag_samples(inp, out, window=IN_RATE, max_lag=2048)
        if ambiguous:
            lag = None
            lag_reason = "pico de correlacion ambiguo para esta senal"
        else:
            lag = measured
            lag_reason = ""

    return {
        "in_rms_dbfs": st_in["rms_dbfs"],
        "out_rms_dbfs": st_out["rms_dbfs"],
        "out_peak_dbfs": st_out["peak_dbfs"],
        "out_clip": st_out["clip"],
        "attenuation_db": attenuation,
        "jump_in_max": max_jump(inp),
        "jump_out_max": max_jump(out),
        **seam_metric(out, method, block),
        "lag_samples": lag,
        "lag_reason": lag_reason,
        "diff_samples": diff,
        "sha256": sha256_file(OUT_DIR / f"{vector}_{method}.raw"),
    }


# --------------------------------------------------------------------------- #
# filas BLOCKED (si algo no se puede medir)
# --------------------------------------------------------------------------- #


def blocked_rows(reason: str) -> list[dict]:
    return [
        {
            "vector": "n/a",
            "method": "rnnoise",
            "version": "n/a",
            "rate_hz": IN_RATE,
            "samples": "UNKNOWN",
            "rms_dbfs": "UNKNOWN",
            "peak_dbfs": "UNKNOWN",
            "clip_count": "UNKNOWN",
            "discontinuities": "UNKNOWN",
            "latency_ms": "UNKNOWN",
            "frame_ms_p50": "UNKNOWN",
            "frame_ms_p95": "UNKNOWN",
            "frame_ms_p99": "UNKNOWN",
            "total_ms": "UNKNOWN",
            "peak_rss_kb": "UNKNOWN",
            "pct_frames_over_10ms": "UNKNOWN",
            "resample_up_ms_p50": "UNKNOWN",
            "rnnoise_ms_p50": "UNKNOWN",
            "speex_ms_p50": "UNKNOWN",
            "resample_down_ms_p50": "UNKNOWN",
            "sha256": "UNKNOWN",
            "audio_destination": AUDIO_DESTINATION,
            "evidence_class": "BLOCKED",
            "notes": f"BLOCKED: {reason}",
        }
    ]


# --------------------------------------------------------------------------- #
# main
# --------------------------------------------------------------------------- #


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--no-build", action="store_true", help="no reconstruye la imagen")
    ap.add_argument("--analyse", action="store_true", help="no corre el contenedor, solo re-analiza")
    args = ap.parse_args()

    log: list[str] = []
    bench = None
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    meta: dict = {
        "issue": "iariap/x6200_gui#345",
        "task": "D (anexo vinculante RNNoise)",
        "image": IMAGE,
        "base_image": BASE_IMAGE,
        "rnnoise_repo": RNNOISE_REPO,
        "rnnoise_commit": RNNOISE_COMMIT,
        "rnnoise_model_url": RNNOISE_MODEL_URL,
        "rnnoise_model_sha256": RNNOISE_MODEL_SHA,
        "audio_destination": AUDIO_DESTINATION,
        "protocol": {
            "seconds": SECONDS,
            "reps": REPS,
            "warmups": WARMUPS,
            "unit_in_samples": UNIT_IN,
            "unit_ms": 1000.0 * UNIT_IN / IN_RATE,
            "clock": "CLOCK_MONOTONIC",
            "state": "el estado se crea al inicio de cada pasada y NO se resetea entre repeticiones",
        },
    }

    try:
        prepare_inputs(log)
        check_pinned_source(log)

        if not args.analyse:
            inspect = subprocess.run(["docker", "image", "inspect", IMAGE], capture_output=True, text=True)
            if inspect.returncode != 0 or not args.no_build:
                build = run(
                    [
                        "docker",
                        "build",
                        "-t",
                        IMAGE,
                        "-f",
                        str(BENCH_DIR / "Dockerfile.rnnoise"),
                        str(BENCH_DIR),
                    ],
                    log,
                    check=False,
                )
                if build.returncode != 0:
                    raise RuntimeError("docker build del banco RNNoise fallo (ver commands.log)")

            docker_run = [
                "docker",
                "run",
                "--rm",
                "-v",
                f"{WT}:/repo:ro",
                "-v",
                f"{SRC_RNNOISE}:/src/rnnoise:ro",
                "-v",
                f"{OUT_DIR}:/out",
                "-w",
                "/build",
                IMAGE,
                "sh",
                "/repo/docs/evidencia/2026-10-28-rx-audio-feasibility/scripts/bench/rnnoise_build.sh",
            ]
            bench = run(docker_run, log, check=False)
            (OUT_DIR / "container.log").write_text("\n".join(log) + "\n", encoding="utf-8")
            if bench.returncode != 0:
                raise RuntimeError(
                    f"el banco dentro del contenedor fallo (exit {bench.returncode}); "
                    "ver artifacts/rnnoise/container.log"
                )
        else:
            log.append("$ (--analyse) se reusa artifacts/rnnoise/matrix.jsonl y las salidas .raw")

        results = _load_matrix()
        log_path = OUT_DIR / "container.log"
        meta["toolchain"] = _toolchain(
            bench.stdout
            if bench is not None
            else (log_path.read_text(encoding="utf-8") if log_path.exists() else "")
        )
        meta["rnnoise_lib_sha256"] = _first_hash(OUT_DIR / "rnnoise-lib.sha256")
        meta["rnnoise_frame_size"] = int((OUT_DIR / "rnnoise-frame-size.txt").read_text().strip())
        meta["bench_bin_sha256"] = _hash_map(OUT_DIR / "bench-bin.sha256")
        meta["source_sha256"] = _hash_map(OUT_DIR / "source.sha256")
        meta["rnnoise_source_sha256"] = _hash_map(OUT_DIR / "rnnoise-source.sha256")
        meta["rnnoise_weights_sha256"] = _hash_map(OUT_DIR / "rnnoise-model.sha256")
        meta["image_id"] = subprocess.run(
            ["docker", "image", "inspect", "--format", "{{.Id}}", IMAGE], capture_output=True, text=True
        ).stdout.strip()

        measurements, comparative, controls = _build_tables(results, meta)
        meta["controls"] = controls
        meta["measurements"] = measurements

        _write_comparative(comparative)
        _write_measurements(measurements)
        _write_manifest(meta)
        _write_audio()
        JSON_PATH.write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        (OUT_DIR / "container.log").write_text("\n".join(log) + "\n", encoding="utf-8")

        _print_summary(comparative, controls, meta)
        return 0

    except (RuntimeError, OSError, KeyError, ValueError) as exc:
        reason = str(exc)
        with CSV_PATH.open("w", newline="", encoding="utf-8") as handle:
            writer = csv.DictWriter(handle, fieldnames=COMPARATIVE_COLUMNS)
            writer.writeheader()
            writer.writerows(blocked_rows(reason))
        meta["blocked"] = reason
        _append_blocked_measurement(reason)
        JSON_PATH.write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        (OUT_DIR / "container.log").write_text("\n".join(log) + "\n", encoding="utf-8")
        print(f"BLOCKED: {reason}", file=sys.stderr)
        return 1


def _load_matrix() -> dict:
    path = OUT_DIR / "matrix.jsonl"
    if not path.exists():
        raise RuntimeError("no existe artifacts/rnnoise/matrix.jsonl")
    rows = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
    if not rows:
        raise RuntimeError("matrix.jsonl esta vacio")
    by_case: dict[str, list[dict]] = {}
    for row in rows:
        by_case.setdefault(row["case"], []).append(row)
    return by_case


def _toolchain(text: str) -> dict:
    out = {"gcc": "UNKNOWN", "speexdsp": "UNKNOWN", "arch": "UNKNOWN", "cpu": "UNKNOWN"}
    lines = [line.strip() for line in text.splitlines()]
    for i, line in enumerate(lines):
        if line == "### toolchain":
            for candidate in lines[i + 1 : i + 10]:
                if candidate.startswith("gcc "):
                    out["gcc"] = candidate
                elif candidate.replace(".", "").isdigit():
                    out["speexdsp"] = candidate
                elif candidate in ("x86_64", "aarch64", "armv7l"):
                    out["arch"] = candidate
                elif candidate.startswith("model name"):
                    out["cpu"] = candidate.split(":", 1)[1].strip()
    out["flags"] = (
        "-O2 -std=c11 -Wall -Wextra -Werror; rnnoise: ./configure --disable-examples "
        "--disable-x86-rtcd --enable-static CFLAGS=-O2"
    )
    return out


def _first_hash(path: Path) -> str:
    if not path.exists():
        return "UNKNOWN"
    return path.read_text(encoding="utf-8").split()[0]


def _hash_map(path: Path) -> dict:
    if not path.exists():
        return {}
    out = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        parts = line.split()
        if len(parts) == 2:
            out[parts[1]] = parts[0]
    return out


def _notes_for(vector: str, method: str, measurement: dict, analysis: dict, runs: int) -> list[str]:
    notes = [
        f"MEDIDO: 60 s de PCM (6 repeticiones del corpus de 10 s sin resetear estado) en "
        f"{measurement['total_ms']:.1f} ms de wall (mediana de {runs} pasadas, min "
        f"{measurement['total_ms_min']:.1f} / max {measurement['total_ms_max']:.1f}; "
        f"{measurement['cpu_pct_of_realtime']:.3f}% del tiempo real)",
        f"por frame de {measurement['block'] / IN_RATE * 1000.0:.2f} ms: "
        f"p50 {measurement['frame_p50_ms']:.4f} / "
        f"p95 {measurement['frame_p95_ms']:.4f} / p99 {measurement['frame_p99_ms']:.4f} ms; "
        f"{measurement['pct_over_10ms']:.2f}% de las tramas supera 10 ms de PCM",
        f"costo separado por frame (p50): resampling subida {measurement['resample_up_p50_ms']:.4f} ms, "
        f"RNNoise {measurement['rnnoise_p50_ms']:.4f} ms, resampling bajada "
        f"{measurement['resample_down_p50_ms']:.4f} ms, Speex {measurement['speex_p50_ms']:.4f} ms",
        f"RSS pico {measurement['peak_rss_kb']:.0f} kB",
        f"discontinuidades: {analysis['discontinuities']} de {analysis['seams']} posiciones de "
        f"empalme superan el maximo salto no-empalme de la salida "
        f"({analysis['jump_nonseam_max']} LSB); salto max en empalme "
        f"{analysis['jump_seam_max']} LSB; salto max de la entrada {analysis['jump_in_max']} LSB",
    ]

    if analysis["out_rms_dbfs"] is None:
        notes.append("silencio digital: RMS lineal 0, dBFS no definido (null)")
    else:
        notes.append(
            f"RMS {analysis['out_rms_dbfs']:.2f} dBFS (entrada {fmt(analysis['in_rms_dbfs'], 2)}), "
            f"peak {fmt(analysis['out_peak_dbfs'], 2)} dBFS, clipping {analysis['out_clip']} muestras"
        )

    if analysis["lag_samples"] is not None:
        notes.append(
            f"latencia por correlacion {analysis['lag_samples']} muestras "
            f"({analysis['lag_samples'] / IN_RATE * 1000.0:.4f} ms)"
        )
    else:
        notes.append(f"latencia por correlacion: UNKNOWN ({analysis['lag_reason']})")

    if analysis["diff_samples"] == 0:
        notes.append("salida bit-exacta a la entrada (0 muestras distintas)")
    if method == "rnnoise_norm":
        notes.append(
            "control de escala: la entrada se dividio por 32768 (lectura de la API en [-1,1]); "
            "ver rnnoise-vs-speex.md, seccion 4"
        )
    if vector in PERIODIC and method.startswith("rnnoise"):
        notes.append(
            "artefacto: RNNoise es un denoiser de voz y trata un tono puro como ruido; la "
            "atenuacion de este vector no es una virtud ni una promesa de calidad"
        )
    return notes


def _build_tables(results: dict, meta: dict) -> tuple[dict, list[dict], dict]:
    measurements: dict = {}
    comparative: list[dict] = []

    for vector, (wav_name, description) in VECTORS.items():
        inp = read_raw(IN_DIR / (Path(wav_name).stem + ".raw"))
        for method in METHODS:
            case = f"{vector}_{method}"
            runs = results.get(case)
            if not runs or len(runs) != REPS:
                raise RuntimeError(f"faltan repeticiones para {case}: {0 if not runs else len(runs)}")
            for warmup in (1, 2, 3):
                if results.get(f"{case}_warmup{warmup}") is None:
                    raise RuntimeError(f"falta el warmup {warmup} de {case}")

            out = read_raw(OUT_DIR / f"{case}.raw")
            if len(out) != len(inp):
                raise RuntimeError(f"{case}: salida {len(out)} != entrada {len(inp)} muestras")

            analysis = analyse(vector, method, inp, out, UNIT_IN)
            first = runs[0]
            totals = sorted(r["wall_ms"] for r in runs)
            measurements[case] = {
                "vector": vector,
                "vector_description": description,
                "method": method,
                "method_effective": first["method_effective"],
                "fallback": first["fallback"],
                "reps": len(runs),
                "warmups": WARMUPS,
                "units": first["units"],
                "block": first["block"],
                "in_samples": first["in_samples"],
                "out_samples": first["out_samples"],
                "rnnoise_frame_size": first["rnnoise_frame_size"],
                "rnnoise_frames": first["rnnoise_frames"],
                "total_ms": median([r["wall_ms"] for r in runs]),
                "total_ms_min": totals[0],
                "total_ms_max": totals[-1],
                "user_ms": median([r["user_ms"] for r in runs]),
                "sys_ms": median([r["sys_ms"] for r in runs]),
                "cpu_pct_of_realtime": 100.0 * median([r["wall_ms"] for r in runs]) / (SECONDS * 1000.0),
                "frame_p50_ms": median([r["frame_p50_ms"] for r in runs]),
                "frame_p95_ms": median([r["frame_p95_ms"] for r in runs]),
                "frame_p99_ms": median([r["frame_p99_ms"] for r in runs]),
                "frame_max_ms": median([r["frame_max_ms"] for r in runs]),
                "resample_up_p50_ms": median([r["up_p50_ms"] for r in runs]),
                "rnnoise_p50_ms": median([r["nr_p50_ms"] for r in runs]),
                "resample_down_p50_ms": median([r["down_p50_ms"] for r in runs]),
                "speex_p50_ms": median([r["speex_p50_ms"] for r in runs]),
                "pct_over_10ms": median([r["pct_over_10ms"] for r in runs]),
                "peak_rss_kb": median([r["rss_peak_kb"] for r in runs]),
                "rss_start_kb": first["rss_start_kb"],
                "rss_end_kb": first["rss_end_kb"],
                **analysis,
            }
            comparative.append(_comparative_row(vector, method, measurements[case], analysis, len(runs)))

    for case, vector, method, block in PROBES:
        runs = results.get(case)
        if not runs:
            raise RuntimeError(f"falta la sonda {case}")
        inp = read_raw(IN_DIR / (Path(VECTORS[vector][0]).stem + ".raw"))
        out = read_raw(OUT_DIR / f"{case}.raw")
        if len(out) != len(inp):
            raise RuntimeError(f"{case}: salida {len(out)} != entrada {len(inp)} muestras")
        analysis = analyse(vector, method, inp, out, block)
        measurements[case] = {
            "vector": vector,
            "vector_description": VECTORS[vector][1],
            "method": method,
            "method_effective": runs[0]["method_effective"],
            "fallback": runs[0]["fallback"],
            "reps": 1,
            "warmups": 0,
            "units": runs[0]["units"],
            "block": runs[0]["block"],
            "in_samples": runs[0]["in_samples"],
            "out_samples": runs[0]["out_samples"],
            "rnnoise_frame_size": runs[0]["rnnoise_frame_size"],
            "rnnoise_frames": runs[0]["rnnoise_frames"],
            "total_ms": runs[0]["wall_ms"],
            "total_ms_min": runs[0]["wall_ms"],
            "total_ms_max": runs[0]["wall_ms"],
            "user_ms": runs[0]["user_ms"],
            "sys_ms": runs[0]["sys_ms"],
            "cpu_pct_of_realtime": 100.0 * runs[0]["wall_ms"] / (SECONDS * 1000.0),
            "frame_p50_ms": runs[0]["frame_p50_ms"],
            "frame_p95_ms": runs[0]["frame_p95_ms"],
            "frame_p99_ms": runs[0]["frame_p99_ms"],
            "frame_max_ms": runs[0]["frame_max_ms"],
            "resample_up_p50_ms": runs[0]["up_p50_ms"],
            "rnnoise_p50_ms": runs[0]["nr_p50_ms"],
            "resample_down_p50_ms": runs[0]["down_p50_ms"],
            "speex_p50_ms": runs[0]["speex_p50_ms"],
            "pct_over_10ms": runs[0]["pct_over_10ms"],
            "peak_rss_kb": runs[0]["rss_peak_kb"],
            "rss_start_kb": runs[0]["rss_start_kb"],
            "rss_end_kb": runs[0]["rss_end_kb"],
            **analysis,
        }
        row = _comparative_row(vector, method, measurements[case], analysis, 1)
        # Sonda: un frame por callback de 256 muestras (5.8 ms), no 10 ms.
        for key in ("frame_ms_p50", "frame_ms_p95", "frame_ms_p99"):
            row[key] = "UNKNOWN"
        row["notes"] += (
            "; SONDA DE LATENCIA: 1 pasada de 60 s con callbacks de 256 muestras (5.8 ms por "
            "callback), por eso no hay p50/p95/p99 por frame de 10 ms"
        )
        comparative.append(row)

    controls = _check_controls(results, measurements)
    return measurements, comparative, controls


def _comparative_row(vector: str, method: str, measurement: dict, analysis: dict, runs: int) -> dict:
    return {
        "vector": vector,
        "method": method,
        "version": VERSIONS[method],
        "rate_hz": IN_RATE,
        "samples": measurement["out_samples"],
        "rms_dbfs": fmt(analysis["out_rms_dbfs"], 3),
        "peak_dbfs": fmt(analysis["out_peak_dbfs"], 3),
        "clip_count": analysis["out_clip"],
        "discontinuities": analysis["discontinuities"],
        "latency_ms": fmt(
            None if analysis["lag_samples"] is None else analysis["lag_samples"] / IN_RATE * 1000.0,
            4,
        ),
        "frame_ms_p50": fmt(measurement["frame_p50_ms"], 5),
        "frame_ms_p95": fmt(measurement["frame_p95_ms"], 5),
        "frame_ms_p99": fmt(measurement["frame_p99_ms"], 5),
        "total_ms": fmt(measurement["total_ms"], 3),
        "peak_rss_kb": int(measurement["peak_rss_kb"]),
        "pct_frames_over_10ms": fmt(measurement["pct_over_10ms"], 4),
        "resample_up_ms_p50": fmt(measurement["resample_up_p50_ms"], 5),
        "rnnoise_ms_p50": fmt(measurement["rnnoise_p50_ms"], 5),
        "speex_ms_p50": fmt(measurement["speex_p50_ms"], 5),
        "resample_down_ms_p50": fmt(measurement["resample_down_p50_ms"], 5),
        "sha256": analysis["sha256"],
        "audio_destination": AUDIO_DESTINATION,
        "evidence_class": "MEASURED-HOST",
        "notes": "; ".join(_notes_for(vector, method, measurement, analysis, runs)),
    }


def _check_controls(results: dict, measurements: dict) -> dict:
    controls: dict = {}
    prefix = UNIT_IN * (IN_RATE // 10)

    # 1. off bit-exacto contra la entrada, en los cuatro vectores.
    off_exact = {}
    for vector, (wav_name, _) in VECTORS.items():
        source = IN_DIR / (Path(wav_name).stem + ".raw")
        out_path = OUT_DIR / f"{vector}_off.raw"
        inp = read_raw(source)
        out = read_raw(out_path)
        off_exact[vector] = {
            "sha_igual_a_entrada": sha256_file(out_path) == hashlib.sha256(source.read_bytes()).hexdigest(),
            "muestras_distintas": sum(1 for a, b in zip(inp, out, strict=True) if a != b),
            "bytes": out_path.stat().st_size,
        }
    controls["off_bit_exacto"] = off_exact

    # 2. repetibilidad de la pasada completa (misma entrada, misma version).
    repeat = {}
    for method in ("speex", "rnnoise"):
        main_sha = sha256_file(OUT_DIR / f"noise_{method}.raw")
        rep_sha = sha256_file(OUT_DIR / f"noise_{method}_repeat60.raw")
        repeat[method] = {
            "sha_pasada": main_sha,
            "sha_repeticion": rep_sha,
            "iguales": main_sha == rep_sha,
        }
    controls["repetibilidad_60s"] = repeat

    # 3. la pasada de 10 s es el prefijo de la de 60 s (estado fresco al inicio).
    prefix_check = {}
    for method in ("speex", "rnnoise"):
        full = read_raw(OUT_DIR / f"noise_{method}.raw")[:prefix]
        single = read_raw(OUT_DIR / f"noise_{method}_10s.raw")
        again = read_raw(OUT_DIR / f"noise_{method}_10s_repeat.raw")
        prefix_check[method] = {
            "prefijo_10s_igual_a_pasada_10s": list(full) == list(single),
            "10s_repetible": list(single) == list(again),
        }
    controls["prefijo_10s"] = prefix_check

    # 4. fallback sin RNNoise: bit-exacto, tamano correcto, sin silencio artificial.
    fallback = {}
    for case, source in (
        ("noise_rnnoise_absent", "white_noise_44100_s16.raw"),
        ("silence_rnnoise_absent", "silence_44100_s16.raw"),
        ("noise_rnnoise_simulated_missing", "white_noise_44100_s16.raw"),
    ):
        out_path = OUT_DIR / f"{case}.raw"
        src_path = IN_DIR / source
        out = read_raw(out_path)
        expected = read_raw(src_path)
        row = results.get(case)
        fallback[case] = {
            "salida_bit_exacta": out_path.read_bytes() == src_path.read_bytes(),
            "bytes": out_path.stat().st_size,
            "bytes_esperados": src_path.stat().st_size,
            "muestras": len(out),
            "rms_salida": rms(out),
            "rms_entrada": rms(expected),
            "exit_code": 0 if row else None,
            "fallback_flag": row[0]["fallback"] if row else None,
            "method_effective": row[0]["method_effective"] if row else None,
        }
    controls["fallback_sin_rnnoise"] = fallback

    # 5. coherencia del metodo efectivo en la matriz principal.
    controls["matriz_sin_fallback"] = all(
        m["method_effective"] == m["method"] and not m["fallback"]
        for m in measurements.values()
        if m["method"] in METHODS
    )
    return controls


def _write_comparative(comparative: list[dict]) -> None:
    with CSV_PATH.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=COMPARATIVE_COLUMNS)
        writer.writeheader()
        writer.writerows(comparative)


def _write_measurements(measurements: dict) -> None:
    """Agrega las filas de esta tanda a measurements.csv (no pisa las de la tarea B)."""
    existing: list[dict] = []
    if MEASUREMENTS_PATH.exists():
        with MEASUREMENTS_PATH.open(newline="", encoding="utf-8") as handle:
            existing = [row for row in csv.DictReader(handle) if not row["case"].startswith("rxd_")]

    rows = []
    for case, m in measurements.items():
        rows.append(
            {
                "case": f"rxd_{case}",
                "source": VECTORS[m["vector"]][0],
                "rate_hz": IN_RATE,
                "frame_samples": m["block"],
                "buffer_samples": m["block"],
                "processed_samples": _processed(m),
                "unprocessed_samples": _unprocessed(m),
                "latency_ms": fmt(
                    None if m["lag_samples"] is None else m["lag_samples"] / IN_RATE * 1000.0, 4
                ),
                "cpu_wall_ms": fmt(m["total_ms"], 3),
                "rms_dbfs": fmt(m["out_rms_dbfs"], 3),
                "peak_dbfs": fmt(m["out_peak_dbfs"], 3),
                "clip_count": m["out_clip"],
                "sha": m["sha256"],
                "command": (
                    "docker run --rm -v <worktree>:/repo:ro -v /tmp/opencode/rnnoise-src:/src/rnnoise:ro "
                    "-v <artifacts>/rnnoise:/out -w /build x6200-rnnoise-bench:70f1d25-1 sh "
                    ".../scripts/bench/rnnoise_build.sh  ->  "
                    f"./bench_rnnoise --case {case} --in {Path(VECTORS[m['vector']][0]).stem}.raw "
                    f"--method {m['method']} --block {m['block']} --seconds 60"
                ),
                "exit_code": 0,
                "evidence_class": "MEASURED-HOST",
                "notes": (
                    f"MEDIDO: cadena {m['method']} sobre {VECTORS[m['vector']][1]}; 60 s en "
                    f"{m['total_ms']:.1f} ms de wall ({m['cpu_pct_of_realtime']:.3f}% del tiempo real); "
                    f"p50/p95/p99 por frame de {m['block'] / IN_RATE * 1000.0:.2f} ms "
                    f"{m['frame_p50_ms']:.4f}/{m['frame_p95_ms']:.4f}/{m['frame_p99_ms']:.4f} ms; "
                    f"{m['pct_over_10ms']:.2f}% de tramas > 10 ms; RSS pico {m['peak_rss_kb']:.0f} kB; "
                    f"costo separado resampling subida {m['resample_up_p50_ms']:.4f} / RNNoise "
                    f"{m['rnnoise_p50_ms']:.4f} / resampling bajada {m['resample_down_p50_ms']:.4f} / "
                    f"Speex {m['speex_p50_ms']:.4f} ms; clipping {m['out_clip']}; discontinuidades "
                    f"{m['discontinuities']} de {m['seams']} empalmes; sha256 de la salida de 60 s; "
                    f"audio_destination {AUDIO_DESTINATION}"
                ),
            }
        )

    rows.append(
        {
            "case": "rxd_voice_noise_ab",
            "source": "mezcla voz + ruido (no existe audio de voz con licencia en este entorno)",
            "rate_hz": IN_RATE,
            "frame_samples": UNIT_IN,
            "buffer_samples": UNIT_IN,
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
            "evidence_class": "SKIPPED",
            "notes": (
                "SKIPPED: el anexo pide una mezcla voz + ruido SOLO si existe audio de voz con "
                "licencia/consentimiento valido; en este entorno no hay ninguna grabacion de voz "
                "utilizable y el issue prohibe inventar voz o usar grabaciones de terceros. "
                "Las cuatro senales sinteticas miden integridad de procesamiento, no inteligibilidad"
            ),
        }
    )
    rows.append(
        {
            "case": "rxd_live_route_gate1",
            "source": "RX -> procesador -> audio audible en la radio",
            "rate_hz": IN_RATE,
            "frame_samples": UNIT_IN,
            "buffer_samples": UNIT_IN,
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
                "BLOCKED (gate 1 del anexo): no hay ruta audible demostrada de RX procesado; "
                "read_callback() solo procesa la copia de captura (src/audio.c:63) y el unico "
                "escritor de play_stm es audio_play() (src/audio.c:281). Dictamen live: NO-GO"
            ),
        }
    )
    rows.append(
        {
            "case": "rxd_hardware_budget_gate2",
            "source": "GUI x6200 en la radio (R16)",
            "rate_hz": IN_RATE,
            "frame_samples": UNIT_IN,
            "buffer_samples": UNIT_IN,
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
                "BLOCKED (gate 2 del anexo): el presupuesto medido es de un x86_64 de host con "
                "codigo portable C; el R16 es otra arquitectura y otra carga. Sin medicion en "
                "radio no se declara apto para tiempo real"
            ),
        }
    )
    rows.append(
        {
            "case": "rxd_intelligibility",
            "source": "senales sinteticas (tonos y ruido)",
            "rate_hz": IN_RATE,
            "frame_samples": UNIT_IN,
            "buffer_samples": UNIT_IN,
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
                "BLOCKED: RNNoise es una red entrenada con voz general; sin habla HF real y A/B "
                "ciego en la misma cadena fisica no hay promesa de inteligibilidad. El resultado "
                "offline se reporta como RESEARCH/PROTOTYPE OFFLINE, nunca como feature"
            ),
        }
    )

    with MEASUREMENTS_PATH.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=MEASUREMENTS_COLUMNS)
        writer.writeheader()
        writer.writerows([*existing, *rows])


def _append_blocked_measurement(reason: str) -> None:
    """Deja constancia del intento fallido en measurements.csv, sin borrar la tarea B."""
    existing: list[dict] = []
    if MEASUREMENTS_PATH.exists():
        with MEASUREMENTS_PATH.open(newline="", encoding="utf-8") as handle:
            existing = [row for row in csv.DictReader(handle) if not row["case"].startswith("rxd_")]
    row = {
        "case": "rxd_bench_rnnoise",
        "source": "contenedor docker (scripts/bench/Dockerfile.rnnoise)",
        "rate_hz": IN_RATE,
        "frame_samples": UNIT_IN,
        "buffer_samples": UNIT_IN,
        "processed_samples": "UNKNOWN",
        "unprocessed_samples": "UNKNOWN",
        "latency_ms": "UNKNOWN",
        "cpu_wall_ms": "UNKNOWN",
        "rms_dbfs": "UNKNOWN",
        "peak_dbfs": "UNKNOWN",
        "clip_count": "UNKNOWN",
        "sha": "UNKNOWN",
        "command": "python3 scripts/bench_rnnoise.py",
        "exit_code": 1,
        "evidence_class": "BLOCKED",
        "notes": f"BLOCKED: {reason}",
    }
    with MEASUREMENTS_PATH.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=MEASUREMENTS_COLUMNS)
        writer.writeheader()
        writer.writerows([*existing, row])


def _write_manifest(meta: dict) -> None:
    manifest = {
        "issue": "iariap/x6200_gui#345",
        "scope": "bench offline de evidencia; NO es una dependencia del producto",
        "rnnoise": {
            "repo": RNNOISE_REPO,
            "commit": RNNOISE_COMMIT,
            "license": "BSD-3-Clause (COPYING del commit pinneado)",
            "license_sha256": meta["rnnoise_source_sha256"].get("COPYING", "UNKNOWN"),
            "vendored_in_repo": False,
            "build": {
                "configure": [
                    "--disable-examples",
                    "--disable-x86-rtcd",
                    "--disable-shared",
                    "--enable-static",
                    "CFLAGS=-O2",
                ],
                "why_no_rtcd": (
                    "el default del commit ya es sin rtcd; se declara explicito para que la ruta "
                    "medida sea la portable C (determinista y comparable con un build ARM), no la "
                    "que despacha AVX2 segun la CPU del host"
                ),
                "container": BASE_IMAGE,
                "lib_sha256": meta.get("rnnoise_lib_sha256", "UNKNOWN"),
            },
            "frame_size": meta.get("rnnoise_frame_size"),
            "model": {
                "url": RNNOISE_MODEL_URL,
                "sha256": RNNOISE_MODEL_SHA,
                "sha256_verificado_por": "download_model.sh del commit (compara con model_version)",
                "bytes": RNNOISE_MODEL_BYTES,
                "files": meta.get("rnnoise_weights_sha256", {}),
                "redistributed_in_repo": False,
                "in_firmware": False,
                "license_note": (
                    "el tarball no trae un archivo de licencia propio; se distribuye desde "
                    "media.xiph.org como parte del proyecto rnnoise, cuyo COPYING de ese commit es "
                    "BSD-3-Clause. En esta evidencia no se redistribuye: los pesos quedan fuera del "
                    "repo y fuera de la ROM, y se descargan solo en el contenedor de prueba"
                ),
            },
        },
        "bench": {
            "source": "scripts/bench/bench_rnnoise.c",
            "binaries_sha256": meta.get("bench_bin_sha256", {}),
            "speex_source_sha256": meta.get("source_sha256", {}),
        },
        "audio_destination": AUDIO_DESTINATION,
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def _write_audio() -> None:
    """Commitea dos vectores x las 3 cadenas (minimo exigido: uno)."""
    AUDIO_DIR.mkdir(parents=True, exist_ok=True)
    written = []
    for vector in COMMITTED_WAVS:
        for method in ("off", "speex", "rnnoise"):
            samples = read_raw(OUT_DIR / f"{vector}_{method}.raw")[: UNIT_IN * (IN_RATE // 10)]
            path = AUDIO_DIR / f"{vector}_{method}_10s.wav"
            path.write_bytes(wav_bytes(samples))
            written.append(path.name)
    (AUDIO_DIR / "sha256sums.txt").write_text(
        "".join(f"{sha256_file(AUDIO_DIR / name)}  {name}\n" for name in sorted(written)),
        encoding="utf-8",
    )


def _print_summary(comparative: list[dict], controls: dict, meta: dict) -> None:
    print(f"OK: {len(comparative)} filas medidas, {REPS} repeticiones por caso (warmup {WARMUPS})")
    print(f"imagen {meta.get('image_id', 'UNKNOWN')}")
    print(
        f"RNNoise {RNNOISE_COMMIT} frame_size={meta.get('rnnoise_frame_size')} "
        f"lib sha256={meta.get('rnnoise_lib_sha256', 'UNKNOWN')[:16]}"
    )
    print(f"modelo {RNNOISE_MODEL_SHA} ({RNNOISE_MODEL_URL})")
    print()
    print(
        f"{'vector':10} {'metodo':13} {'rms':>8} {'lat_ms':>8} {'p50':>9} {'p95':>9} {'p99':>9} "
        f"{'total':>9} {'%>10ms':>7} {'rss_kB':>7} {'disc':>5}"
    )
    for row in comparative:
        print(
            f"{row['vector']:10} {row['method']:13} {row['rms_dbfs']:>8} {row['latency_ms']:>8} "
            f"{row['frame_ms_p50']:>9} {row['frame_ms_p95']:>9} {row['frame_ms_p99']:>9} "
            f"{row['total_ms']:>9} {row['pct_frames_over_10ms']:>7} {row['peak_rss_kb']:>7} "
            f"{row['discontinuities']:>5}"
        )
    print()
    print("controles:")
    for key, value in controls.items():
        print(f"  {key}: {json.dumps(value, ensure_ascii=False)}")
    print()
    print(f"CSV: {CSV_PATH}")
    print(f"JSON: {JSON_PATH}")
    print(f"manifest: {MANIFEST_PATH}")


if __name__ == "__main__":
    sys.exit(main())
