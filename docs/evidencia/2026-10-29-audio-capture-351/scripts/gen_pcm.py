#!/usr/bin/env python3
"""Generador determinista de PCM sintetico mono S16 44100 Hz (issue #351, parte 3a).

Copiado del generador pinneado de #345 (docs/evidencia/2026-10-28-rx-audio-feasibility/
scripts/gen_pcm.py) sin cambiar formulas ni semilla, para que las senales de este banco
sean byte a byte las mismas y los dos conjuntos de mediciones sean comparables. Este
archivo es autonomo: no lee nada del directorio historico.

Produce cuatro senales de 10 s, semilla fija 6200, sin dependencias fuera de la
stdlib (nada de numpy): el mismo binario de Python debe dar los mismos bytes en
cualquier host. Escribe un WAV por senal y un manifiesto JSON con sha256, RMS,
peak, clipping y duracion.

Las senales verifican INTEGRIDAD DE PROCESAMIENTO (que la cadena no recorte,
no desplace y no invente muestras). NO verifican inteligibilidad de voz: son
tonos y ruido sinteticos, no habla.

Uso:
    python3 gen_pcm.py                 # escribe artifacts/pcm/*.wav + pcm-manifest.json
    python3 gen_pcm.py --out DIR       # otro destino para los WAV
    python3 gen_pcm.py --verify        # regenera en memoria y compara contra el manifiesto
    python3 gen_pcm.py --quiet         # sin resumen por archivo

SPDX-License-Identifier: LGPL-2.1-or-later
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import random
import struct
import sys
from pathlib import Path

SEED = 6200
RATE_HZ = 44100
DURATION_S = 10
SAMPLES = RATE_HZ * DURATION_S
FULL_SCALE = 32768.0
CLIP_LEVEL = 32767

EVIDENCE_DIR = Path(__file__).resolve().parent.parent
DEFAULT_OUT = EVIDENCE_DIR / "artifacts" / "pcm"
MANIFEST = EVIDENCE_DIR / "pcm-manifest.json"


def _round_half_away(x: float) -> int:
    """Redondeo simetrico explicito (no depende del banker's rounding de round())."""
    return math.floor(x + 0.5) if x >= 0.0 else -math.floor(-x + 0.5)


def _dbfs(value: float, ref: float = FULL_SCALE) -> float:
    if value <= 0.0:
        return float("-inf")
    return 20.0 * math.log10(value / ref)


def signal_silence() -> list[int]:
    return [0] * SAMPLES


def signal_sine_1k() -> list[int]:
    """Seno 1 kHz a -18 dBFS de pico (amplitud lineal 10^(-18/20) del fondo de escala)."""
    amp = FULL_SCALE * 10.0 ** (-18.0 / 20.0)
    w = 2.0 * math.pi * 1000.0 / RATE_HZ
    return [_round_half_away(amp * math.sin(w * n)) for n in range(SAMPLES)]


def signal_multisine() -> list[int]:
    """300 + 1000 + 2400 Hz, cada componente a 1/3 de la amplitud de -18 dBFS.

    La suma de las tres amplitudes queda en -18 dBFS de pico teorico (peor caso
    de alineacion de fases), asi que la senal no recorta.
    """
    amp = FULL_SCALE * 10.0 ** (-18.0 / 20.0) / 3.0
    freqs = (300.0, 1000.0, 2400.0)
    out = []
    for n in range(SAMPLES):
        acc = 0.0
        for f in freqs:
            acc += amp * math.sin(2.0 * math.pi * f * n / RATE_HZ)
        out.append(_round_half_away(acc))
    return out


def signal_white_noise() -> list[int]:
    """Ruido blanco uniforme con RMS exacto de -30 dBFS.

    Uniforme en [-a, a] con a = FS * 10^(-30/20) * sqrt(3) => RMS = a/sqrt(3) =
    -30 dBFS. Uniforme (no gaussiano) para que el pico este acotado y no recorte.
    """
    rms_target = FULL_SCALE * 10.0 ** (-30.0 / 20.0)
    a = rms_target * math.sqrt(3.0)
    rnd = random.Random(SEED)
    return [_round_half_away((rnd.random() * 2.0 - 1.0) * a) for _ in range(SAMPLES)]


SIGNALS = (
    {
        "name": "silence",
        "file": "silence_44100_s16.wav",
        "description": "silencio digital (todas las muestras en 0)",
        "formula": "x[n] = 0",
        "build": signal_silence,
    },
    {
        "name": "sine_1k_minus18dbfs",
        "file": "sine_1k_44100_s16.wav",
        "description": "seno 1 kHz a -18 dBFS de pico",
        "formula": "x[n] = round(32768 * 10^(-18/20) * sin(2*pi*1000*n/44100))",
        "build": signal_sine_1k,
    },
    {
        "name": "multisine_300_1000_2400",
        "file": "multisine_44100_s16.wav",
        "description": "mezcla 300/1000/2400 Hz, cada componente a 1/3 de -18 dBFS",
        "formula": (
            "x[n] = round(sum_f (32768*10^(-18/20)/3) * sin(2*pi*f*n/44100)), f in {300, 1000, 2400}"
        ),
        "build": signal_multisine,
    },
    {
        "name": "white_noise_minus30dbfs",
        "file": "white_noise_44100_s16.wav",
        "description": "ruido blanco uniforme con RMS -30 dBFS (semilla 6200)",
        "formula": (
            "x[n] = round((2*u[n]-1) * 32768*10^(-30/20)*sqrt(3)), u[n] = random.Random(6200).random()"
        ),
        "build": signal_white_noise,
    },
)


def to_bytes(samples: list[int]) -> bytes:
    return struct.pack(f"<{len(samples)}h", *samples)


def wav_bytes(samples: list[int]) -> bytes:
    """WAV canonico de 44 bytes de cabecera, sin metadatos ni timestamps."""
    payload = to_bytes(samples)
    header = b"RIFF" + struct.pack("<I", 36 + len(payload)) + b"WAVE"
    header += b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, RATE_HZ, RATE_HZ * 2, 2, 16)
    header += b"data" + struct.pack("<I", len(payload))
    return header + payload


def _db_or_null(value: float) -> float | None:
    """-inf no es JSON valido: una senal exactamente nula se publica como null."""
    db = _dbfs(value)
    return None if math.isinf(db) else round(db, 4)


def stats(samples: list[int]) -> dict:
    peak = max(abs(s) for s in samples)
    rms = math.sqrt(sum(float(s) * s for s in samples) / len(samples))
    clip = sum(1 for s in samples if abs(s) >= CLIP_LEVEL)
    return {
        "samples": len(samples),
        "duration_s": len(samples) / RATE_HZ,
        "rms_linear": round(rms, 4),
        "rms_dbfs": _db_or_null(rms),
        "peak_dbfs": _db_or_null(float(peak)),
        "peak_abs": peak,
        "clip_count": clip,
        "notes": "silencio digital: RMS lineal 0, dBFS null" if peak == 0 else "",
    }


def build_manifest() -> tuple[dict, dict[str, bytes]]:
    files = []
    blobs: dict[str, bytes] = {}
    for spec in SIGNALS:
        samples = spec["build"]()
        blob = wav_bytes(samples)
        blobs[spec["file"]] = blob
        entry = {
            "name": spec["name"],
            "file": spec["file"],
            "description": spec["description"],
            "formula": spec["formula"],
            "rate_hz": RATE_HZ,
            "channels": 1,
            "sample_format": "S16LE",
            "seed": SEED,
            "sha256": hashlib.sha256(blob).hexdigest(),
        }
        entry.update(stats(samples))
        files.append(entry)
    manifest = {
        "generator": "gen_pcm.py",
        "generator_version": 1,
        "issue": "iariap/x6200_gui#351",
        "scope": "senales sinteticas de integridad; NO son voz ni prueban inteligibilidad",
        "seed": SEED,
        "rate_hz": RATE_HZ,
        "channels": 1,
        "sample_format": "S16LE",
        "duration_s": DURATION_S,
        "samples": SAMPLES,
        "dbfs_reference": FULL_SCALE,
        "dependencies": "stdlib only (wave/struct/random/math/hashlib)",
        "files": files,
    }
    return manifest, blobs


def write_all(out_dir: Path, quiet: bool) -> dict:
    manifest, blobs = build_manifest()
    out_dir.mkdir(parents=True, exist_ok=True)
    for entry in manifest["files"]:
        path = out_dir / entry["file"]
        path.write_bytes(blobs[entry["file"]])
        if not quiet:
            rms = "null" if entry["rms_dbfs"] is None else f"{entry['rms_dbfs']:+.2f}"
            peak = "null" if entry["peak_dbfs"] is None else f"{entry['peak_dbfs']:+.2f}"
            print(
                f"{entry['file']}: {entry['samples']} muestras, "
                f"RMS {rms} dBFS, peak {peak} dBFS, "
                f"clip {entry['clip_count']}, sha256 {entry['sha256']}"
            )
    MANIFEST.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def verify() -> int:
    if not MANIFEST.exists():
        print(f"FALLO: no existe {MANIFEST}", file=sys.stderr)
        return 1
    expected = json.loads(MANIFEST.read_text(encoding="utf-8"))
    actual, _ = build_manifest()
    failures = 0
    if expected != actual:
        exp = {f["file"]: f["sha256"] for f in expected.get("files", [])}
        act = {f["file"]: f["sha256"] for f in actual["files"]}
        for name in sorted(set(exp) | set(act)):
            if exp.get(name) != act.get(name):
                failures += 1
                print(f"FALLO {name}: manifiesto {exp.get(name)} != regenerado {act.get(name)}")
    for entry in actual["files"]:
        print(f"{entry['file']}: sha256 {entry['sha256']}")
    if failures:
        return 1
    print("OK: los cuatro WAV regenerados coinciden byte a byte con el manifiesto")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT, help="directorio de los WAV")
    ap.add_argument("--verify", action="store_true", help="compara contra pcm-manifest.json sin escribir WAV")
    ap.add_argument("--quiet", action="store_true", help="no imprime el detalle por archivo")
    args = ap.parse_args()

    if args.verify:
        return verify()

    manifest = write_all(args.out, args.quiet)
    if not args.quiet:
        print(f"manifiesto: {MANIFEST}")
    print("sha256 " + " ".join(f["sha256"] for f in manifest["files"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
